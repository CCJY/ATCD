#include "tao/Transport_Connector.h"
#include "tao/Transport.h"
#include "tao/ORB_Core.h"
#include "tao/MProfile.h"
#include "tao/Profile.h"
#include "tao/Environment.h"
#include "tao/Thread_Lane_Resources.h"
#include "tao/debug.h"
#include "tao/Connect_Strategy.h"
#include "tao/LF_Multi_Event.h"
#include "tao/Client_Strategy_Factory.h"
#include "tao/Connection_Handler.h"
#include "tao/Profile_Transport_Resolver.h"
#include "tao/Wait_Strategy.h"
#include "tao/SystemException.h"
#include "tao/Endpoint.h"
#include "tao/Base_Transport_Property.h"

#include "ace/OS_NS_string.h"

//@@ TAO_CONNECTOR_SPL_INCLUDE_ADD_HOOK

#if !defined (__ACE_INLINE__)
# include "tao/Transport_Connector.inl"
#endif /* __ACE_INLINE__ */

ACE_RCSID (tao,
           Connector,
           "$Id$")

namespace
{
  class TransportCleanupGuard
  {
  public:

    TransportCleanupGuard (TAO_Transport *tp, bool awake=false)
      : tp_ (tp)
      , awake_ (awake)
    {
    }

    ~TransportCleanupGuard ()
    {
      if (this->awake_ && this->tp_)
        {
          // Purge from the connection cache.  If we are not in the
          // cache, this does nothing.
          this->tp_->purge_entry ();

          // Close the handler.
          this->tp_->close_connection ();

          this->tp_->remove_reference ();
        }
    }

    void awake ()
    {
      this->awake_ = true;
    }

  private:

    TAO_Transport * const tp_;
    bool awake_;

  };
}

TAO_BEGIN_VERSIONED_NAMESPACE_DECL

// Connector
TAO_Connector::TAO_Connector (CORBA::ULong tag)
  : active_connect_strategy_ (0),
    tag_ (tag),
    orb_core_ (0)
{
}

TAO_Connector::~TAO_Connector (void)
{
  delete this->active_connect_strategy_;
}

TAO_Profile *
TAO_Connector::corbaloc_scan (const char *str,
                              size_t &len
                              ACE_ENV_ARG_DECL)
{
  if (this->check_prefix (str) != 0)
    return 0;
  const char *comma_pos = ACE_OS::strchr (str,',');
  const char *slash_pos = ACE_OS::strchr (str,'/');
  if (comma_pos == 0 && slash_pos == 0)
    {
      if (TAO_debug_level)
        ACE_DEBUG ((LM_DEBUG,
                    ACE_TEXT("TAO (%P|%t) TAO_Connector::corbaloc_scan warning: ")
                    ACE_TEXT("supplied string contains no comma or slash: %s\n"),
                    str));
      len = ACE_OS::strlen (str);
    }
  else if (comma_pos == 0 || comma_pos > slash_pos)
    len = (slash_pos - str);
  else len = comma_pos - str;
  return this->make_profile(ACE_ENV_SINGLE_ARG_PARAMETER);
}

int
TAO_Connector::make_mprofile (const char *string,
                              TAO_MProfile &mprofile
                              ACE_ENV_ARG_DECL)
{
  // This method utilizes the "Template Method" design pattern to
  // parse the given URL style IOR for the protocol being used
  // and create an mprofile from it.
  //
  // The methods that must be defined by all Connector sub-classes are:
  //      make_profile
  //      check_prefix

  // Check for a valid string
  if (!string || !*string)
    {
      ACE_THROW_RETURN (CORBA::INV_OBJREF (
                          CORBA::SystemException::_tao_minor_code (
                            0,
                            EINVAL),
                          CORBA::COMPLETED_NO),
                        -1);
    }

  // Check for the proper prefix in the IOR.  If the proper prefix isn't
  // in the IOR then it is not an IOR we can use.
  if (this->check_prefix (string) != 0)
    {
      return 1;
      // Failure: not the correct IOR for this protocol.
      // DO NOT throw an exception here since the Connector_Registry
      // should be allowed the opportunity to continue looking for
      // an appropriate connector.
    }

  if (TAO_debug_level > 0)
    {
      ACE_DEBUG ((LM_DEBUG,
                  ACE_TEXT ("TAO (%P|%t) - TAO_Connector::make_mprofile ")
                  ACE_TEXT ("<%s>\n"),
                  ACE_TEXT_CHAR_TO_TCHAR (string)));
    }

  ACE_CString ior;

  ior.set (string, ACE_OS::strlen (string), 1);

  // Find out where the protocol ends
  ACE_CString::size_type ior_index = ior.find ("://");

  if (ior_index == ACE_CString::npos)
    {
      ACE_THROW_RETURN (CORBA::INV_OBJREF (), -1);
      // No colon ':' in the IOR!
    }
  else
    {
      ior_index += 3;
      // Add the length of the colon and the two forward slashes `://'
      // to the IOR string index (i.e. 3)
    }

  // Find the object key
  const ACE_CString::size_type objkey_index =
    ior.find (this->object_key_delimiter (), ior_index);

  if (objkey_index == 0 || objkey_index == ACE_CString::npos)
    {
      ACE_THROW_RETURN (CORBA::INV_OBJREF (), -1);
      // Failure: No endpoints specified or no object key specified.
    }

  const char endpoint_delimiter = ',';
  // The delimiter used to seperate inidividual addresses.

  // Count the number of endpoints in the IOR.  This will be the number
  // of entries in the MProfile.

  CORBA::ULong profile_count = 1;
  // Number of endpoints in the IOR  (initialized to 1).

  // Only check for endpoints after the protocol specification and
  // before the object key.
  for (ACE_CString::size_type i = ior_index; i < objkey_index; ++i)
    {
      if (ior[i] == endpoint_delimiter)
        ++profile_count;
    }

  // Tell the MProfile object how many Profiles it should hold.
  // MProfile::set(size) returns the number profiles it can hold.
  if (mprofile.set (profile_count) != static_cast<int> (profile_count))
    {
      ACE_THROW_RETURN (CORBA::INV_OBJREF (
                          CORBA::SystemException::_tao_minor_code (
                            TAO_MPROFILE_CREATION_ERROR,
                            0),
                          CORBA::COMPLETED_NO),
                        -1);
      // Error while setting the MProfile size!
    }

  // The idea behind the following loop is to split the IOR into several
  // strings that can be parsed by each profile.
  // For example,
  //    `1.3@moo,shu,1.1@chicken/arf'
  // will be parsed into:
  //    `1.3@moo/arf'
  //    `shu/arf'
  //    `1.1@chicken/arf'

  ACE_CString::size_type begin = 0;
  ACE_CString::size_type end = ior_index - 1;
  // Initialize the end of the endpoint index

  for (CORBA::ULong j = 0; j < profile_count; ++j)
    {
      begin = end + 1;

      if (j < profile_count - 1)
        {
          end = ior.find (endpoint_delimiter, begin);
        }
      else
        {
          end = objkey_index;  // Handle last endpoint differently
        }

      if (end < ior.length () && end != ior.npos)
        {
          ACE_CString endpoint = ior.substring (begin, end - begin);

          // Add the object key to the string.
          endpoint += ior.substring (objkey_index);

          // The endpoint should now be of the form:
          //    `N.n@endpoint/object_key'
          // or
          //    `endpoint/object_key'

          TAO_Profile *profile =
            this->make_profile (ACE_ENV_SINGLE_ARG_PARAMETER);
          ACE_CHECK_RETURN (-1);
          // Failure:  Problem during profile creation

          // Initialize a Profile using the individual endpoint
          // string.
          // @@ Not exception safe!  We need a TAO_Profile_var!
          profile->parse_string (endpoint.c_str ()
                                 ACE_ENV_ARG_PARAMETER);
          ACE_CHECK_RETURN (-1);

          // Give up ownership of the profile.
          if (mprofile.give_profile (profile) == -1)
            {
              profile->_decr_refcnt ();

              ACE_THROW_RETURN (CORBA::INV_OBJREF (
                                  CORBA::SystemException::_tao_minor_code (
                                     TAO_MPROFILE_CREATION_ERROR,
                                     0),
                                  CORBA::COMPLETED_NO),
                                -1);
              // Failure presumably only occurs when MProfile is full!
              // This should never happen.
            }
        }
      else
        {
          ACE_THROW_RETURN (CORBA::INV_OBJREF (), -1);
          // Unable to seperate endpoints
        }
    }

  return 0;  // Success
}

int
TAO_Connector::supports_parallel_connects(void) const
{
  return 0; // by default, we don't support parallel connection attempts;
}

TAO_Transport*
TAO_Connector::make_parallel_connection (TAO::Profile_Transport_Resolver *,
                                         TAO_Transport_Descriptor_Interface &,
                                         ACE_Time_Value *)
{
  return 0;
}


TAO_Transport*
TAO_Connector::parallel_connect (TAO::Profile_Transport_Resolver *r,
                                 TAO_Transport_Descriptor_Interface *desc,
                                 ACE_Time_Value *timeout
                                 ACE_ENV_ARG_DECL_NOT_USED)
{
  if (this->supports_parallel_connects() == 0)
    {
      errno = ENOTSUP;
      return 0;
    }

  errno = 0; // need to clear errno to ensure a stale enotsup is not set
  if (desc == 0)
    return 0;
  unsigned int endpoint_count = 0;
  TAO_Endpoint *root_ep = desc->endpoint();
  for (TAO_Endpoint *ep = root_ep->next_filtered (this->orb_core(),0);
       ep != 0;
       ep = ep->next_filtered(this->orb_core(),root_ep))
    if (this->set_validate_endpoint (ep) == 0)
      ++endpoint_count;
  if (endpoint_count == 0)
    return 0;

  TAO_Transport *base_transport = 0;

  TAO::Transport_Cache_Manager &tcm =
    this->orb_core ()->lane_resources ().transport_cache ();

  // Iterate through the endpoints. Since find_transport takes a
  // Transport Descriptor rather than an endpoint, we must create a
  // local TDI for each endpoint. The first one found will be used.
  for (TAO_Endpoint *ep = root_ep->next_filtered (this->orb_core(),0);
       ep != 0;
       ep = ep->next_filtered(this->orb_core(),root_ep))
    {
      TAO_Base_Transport_Property desc2(ep,0);
      if (tcm.find_transport (&desc2,
                              base_transport) == 0)
        {
          if (TAO_debug_level)
            {
              ACE_DEBUG ((LM_DEBUG,
                          ACE_TEXT ("TAO (%P|%t) TAO_Connector::parallel_connect: ")
                          ACE_TEXT ("found a transport [%d]\n"),
                          base_transport->id ()));
            }
          return base_transport;
        }
    }

  // Now we have searched the cache on all endpoints and come up
  // empty. We need to initiate connections on each of the
  // endpoints. Presumably only one will have a route and will succeed,
  // and the rest will fail. This requires the use of asynch
  // connection establishment. Maybe a custom wait strategy is needed
  // at this point to register several potential transports so that
  // when one succeeds the rest are cancelled or closed.

  return this->make_parallel_connection (r,*desc,timeout);
}

TAO_Transport*
TAO_Connector::connect (TAO::Profile_Transport_Resolver *r,
                        TAO_Transport_Descriptor_Interface *desc,
                        ACE_Time_Value *timeout
                        ACE_ENV_ARG_DECL)
{
  if (desc == 0 ||
      (this->set_validate_endpoint (desc->endpoint ()) == -1))
    return 0;

  TAO_Transport *base_transport = 0;

  TAO::Transport_Cache_Manager &tcm =
    this->orb_core ()->lane_resources ().transport_cache ();

  // Check the Cache first for connections
  // If transport found, reference count is incremented on assignment
  // @@todo: We need to send the timeout value to the cache registry
  // too. That should be the next step!
  if (tcm.find_transport (desc,
                          base_transport) != 0)
    {
      // @@TODO: This is not the right place for this!
      // Purge connections (if necessary)
      tcm.purge ();

      TAO_Transport* t = this->make_connection (r,
                                                *desc,
                                                timeout);

      if (t == 0)
        return t;

      t->opened_as (TAO::TAO_CLIENT_ROLE);

      if (TAO_debug_level > 4)
        {
          ACE_DEBUG ((LM_DEBUG,
                      ACE_TEXT("TAO (%P|%t) Transport_Connector::connect, ")
                      ACE_TEXT("opening Transport[%d] in TAO_CLIENT_ROLE\n"),
                      t->id ()));
        }

      // Call post connect hook. If the post_connect_hook () returns
      // false, just purge the entry.
      if (!t->post_connect_hook ())
        {
          (void) t->purge_entry ();

          // Call connect again
          return this->connect (r,
                                desc,
                                timeout
                                ACE_ENV_ARG_PARAMETER);
        }

      return t;
    }

  if (TAO_debug_level > 4)
    {
      TAO::Connection_Role cr =
        base_transport->opened_as ();

      ACE_DEBUG ((LM_DEBUG,
                  "TAO (%P|%t) - Transport_Connector::connect, "
                  "got an existing %s Transport[%d] in role %s\n",
                  base_transport->is_connected () ? "connected" : "unconnected",
                  base_transport->id (),
                  cr == TAO::TAO_SERVER_ROLE ? "TAO_SERVER_ROLE" :
                  cr == TAO::TAO_CLIENT_ROLE ? "TAO_CLIENT_ROLE" :
                  "TAO_UNSPECIFIED_ROLE" ));
    }

  // If connected return.
  if (base_transport->is_connected ())
    return base_transport;

  // It it possible to get a transport from the cache that is not
  // connected? If not, then the following code is bogus. We cannot
  // wait for a connection to complete on a transport in the cache.
  //
  // (mesnier_p@ociweb.com) It is indeed possible to reach this point.
  // The AMI_Buffering test does. When using non-blocking connects and
  // the first request(s) are asynch and may be queued, the connection
  // establishment may not be completed by the time the invocation is
  // done with it. In that case it is up to a subsequent invocation to
  // handle the connection completion.

  TransportCleanupGuard tg(base_transport);
  if (!this->wait_for_connection_completion (r,
                                             base_transport,
                                             timeout))
    {
      tg.awake();

      if (TAO_debug_level > 2)
        {
          ACE_ERROR ((LM_ERROR,
                      "TAO (%P|%t) - Transport_Connector::"
                      "connect, "
                      "wait for completion failed\n"));
        }
      return 0;
    }

  if (base_transport->is_connected () &&
      base_transport->wait_strategy ()->register_handler () == -1)
    {
      // Registration failures.
      tg.awake();

      if (TAO_debug_level > 0)
        {
          ACE_ERROR ((LM_ERROR,
                      "TAO (%P|%t) - Transport_Connector [%d]::connect, "
                      "could not register the transport "
                      "in the reactor.\n",
                      base_transport->id ()));
        }
      return 0;
    }

  return base_transport;
}

bool
TAO_Connector::wait_for_connection_completion (
    TAO::Profile_Transport_Resolver *r,
    TAO_Transport *&transport,
    ACE_Time_Value *timeout)
{
  int result = -1;
  if (!r->blocked_connect ())
    {
      if (transport->connection_handler ()->is_open ())
        {
          result = 0;
        }
      else if (transport->connection_handler ()->is_timeout ())
        {
          if (TAO_debug_level > 2)
            {
              ACE_DEBUG ((LM_DEBUG,
                          "TAO (%P|%t) - Transport_Connector::"
                          "wait_for_connection_completion, "
                          "transport [%d], Connection timed out.\n",
                          transport->id ()));
            }
          result = -1;
          errno = ETIME;
        }
      else if (transport->connection_handler ()->is_closed ())
        {
          if (TAO_debug_level > 2)
            {
              ACE_DEBUG ((LM_DEBUG,
              "TAO (%P|%t) - Transport_Connector::"
              "wait_for_connection_completion, "
              "transport [%d], Connection failed. (%d) %p\n",
              transport->id (), errno, ""));
            }
          result = -1;
        }
      else
        {
          if (TAO_debug_level > 2)
            {
              ACE_DEBUG ((LM_DEBUG,
                          "TAO (%P|%t) - Transport_Connector::"
                          "wait_for_connection_completion, "
                          "transport [%d], Connection not complete.\n",
                          transport->id ()));
            }
          transport->connection_handler ()->
            reset_state (TAO_LF_Event::LFS_CONNECTION_WAIT);
          result = 0;
        }
    }
  else
    {
      if (TAO_debug_level > 2)
        {
            ACE_DEBUG ((LM_DEBUG,
                        "TAO (%P|%t) - Transport_Connector::"
                        "wait_for_connection_completion, "
                        "going to wait for connection completion on transport"
                        "[%d]\n",
                        transport->id ()));
        }
      result = this->active_connect_strategy_->wait (transport, timeout);

      if (TAO_debug_level > 2)
        {
          ACE_DEBUG ((LM_DEBUG,
                      "TAO (%P|%t) - Transport_Connector::"
                      "wait_for_connection_completion, "
                      "transport [%d], wait done result = %d\n",
                      transport->id (), result));
        }
    // There are three possibilities when wait() returns: (a)
    // connection succeeded; (b) connection failed; (c) wait()
    // failed because of some other error.  It is easy to deal with
    // (a) and (b).  (c) is tricky since the connection is still
    // pending and may get completed by some other thread.  The
    // following code deals with (c).

    if (result == -1)
      {
        if (errno == ETIME)
          {
            if (TAO_debug_level > 2)
              {
                ACE_DEBUG ((LM_DEBUG,
                            "TAO (%P|%t) - Transport_Connector::"
                            "wait_for_connection_completion, "
                            "transport [%d], Connection timed out.\n",
                            transport->id ()));
              }
          }
        else
          {
            // The wait failed for some other reason.
            // Report that making the connection failed, don't print errno
            // because we touched the reactor and errno could be changed
            if (TAO_debug_level > 2)
              {
                ACE_ERROR ((LM_ERROR,
                            "TAO (%P|%t) - Transport_Connector::"
                            "wait_for_connection_completion, "
                  "transport [%d], wait for completion failed (%d) %p\n",
                  transport->id (), errno, ""));
              }
            TAO_Connection_Handler *con = transport->connection_handler ();
            result = this->check_connection_closure (con);
          }
      }
    }

  if (result == -1)
    {
      // Set transport to zero, it is not usable, and the reference
      // count we added above was decremented by the base connector
      // handling the connection failure.
      transport = 0;
      return false;
    }
  // Connection not ready yet but we can use this transport, if
  // we need a connected one we will block later to make sure
  // it is connected
  return true;
}

bool
TAO_Connector::wait_for_connection_completion (
    TAO::Profile_Transport_Resolver *r,
    TAO_Transport *&the_winner,
    TAO_Transport **transport,
    unsigned int count,
    TAO_LF_Multi_Event *mev,
    ACE_Time_Value *timeout)
{
  if (TAO_debug_level > 2)
    {
      ACE_DEBUG ((LM_DEBUG,
                  ACE_TEXT("TAO (%P|%t) Transport_Connector::")
                  ACE_TEXT("wait_for_connection_completion, ")
                  ACE_TEXT("waiting for connection completion on ")
                  ACE_TEXT("%d transports, ["),
                  count));
      for (unsigned int i = 0; i < count; i++)
        ACE_DEBUG ((LM_DEBUG,
                    ACE_TEXT("%d%s"),transport[i]->id (),
                    (i < (count -1) ? ", " : "]\n")));
    }

  int result = -1;
  if (r->blocked_connect ())
    {
      result = this->active_connect_strategy_->wait (mev, timeout);
      the_winner = 0;
    }
  else
    {
      errno = ETIME;
    }

  if (result != -1)
    {
      the_winner = mev->winner()->transport();
      if (TAO_debug_level > 2)
        {
          ACE_DEBUG ((LM_DEBUG,
                      ACE_TEXT ("TAO (%P|%t) Transport_Connector::")
                      ACE_TEXT ("wait_for_connection_completion, ")
                      ACE_TEXT ("transport [%d]\n"),
                      the_winner->id ()));
        }
    }
  else if (errno == ETIME)
    {
      // this is the most difficult case. In this situation, there is no
      // nominated by the Multi_Event. The best we can do is pick one of
      // the pending connections.
      // Of course, this shouldn't happen in any case, since the wait
      // strategy is called with a timeout value of 0.
      for (unsigned int i = 0; i < count; i++)
        if (!transport[i]->connection_handler()->is_closed())
          {
            the_winner = transport[i];
            break;
          }
    }

  // It is possible that we have more than one connection that happened
  // to complete, or that none completed. Therefore we need to traverse
  // the list and ensure that all of the losers are closed.
  for (unsigned int i = 0; i < count; i++)
    {
      if (transport[i] != the_winner)
        this->check_connection_closure (transport[i]->connection_handler());
      // since we are doing this on may connections, the result isn't
      // particularly important.
    }

  // In case of errors.
  if (the_winner == 0)
    {
      // Report that making the connection failed, don't print errno
      // because we touched the reactor and errno could be changed
      if (TAO_debug_level > 2)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT ("TAO (%P|%t) Transport_Connector::")
                      ACE_TEXT ("wait_for_connection_completion, failed\n")
                      ));
        }

      return false;
    }

  // Fix for a subtle problem. What happens if we are supposed to do
  // blocked connect but the transport is NOT connected? Force close
  // the connections
  if (r->blocked_connect () && !the_winner->is_connected ())
    {
      if (TAO_debug_level > 2)
        {
          ACE_DEBUG ((LM_DEBUG,
                      "TAO (%P|%t) - Transport_Connector::"
                      "wait_for_connection_completion, "
                      "no connected transport for a blocked connection, "
                      "cancelling connections and reverting things \n"));
        }

      // Forget the return value. We are busted anyway. Try our best
      // here.
      (void) this->cancel_svc_handler (the_winner->connection_handler ());
      the_winner = 0;
      return false;
    }

  // Connection may not ready for SYNC_NONE and SYNC_DELAYED_BUFFERING cases
  // but we can use this transport, if we need a connected one we will poll
  // later to make sure it is connected
  return true;
}

int
TAO_Connector::create_connect_strategy (void)
{
  if (this->active_connect_strategy_ == 0)
    {
      this->active_connect_strategy_ =
        this->orb_core_->client_factory ()->create_connect_strategy (
          this->orb_core_);
    }

  if (this->active_connect_strategy_ == 0)
    {
      return -1;
    }

  return 0;
}

int
TAO_Connector::check_connection_closure (
  TAO_Connection_Handler *connection_handler)
{
  int result = -1;

  // Check if the handler has been closed.
  bool closed =
    connection_handler->is_closed ();

  // In case of failures and close() has not be called.
  if (!closed)
    {
      // First, cancel from connector.
      if (this->cancel_svc_handler (connection_handler) == -1)
        return -1;

      // Double check to make sure the handler has not been closed
      // yet.  This double check is required to ensure that the
      // connection handler was not closed yet by some other
      // thread since it was still registered with the connector.
      // Once connector.cancel() has been processed, we are
      // assured that the connector will no longer open/close this
      // handler.
      closed = connection_handler->is_closed ();


      // If closed, there is nothing to do here.  If not closed,
      // it was either opened or is still pending.
      if (!closed)
        {
          // Check if the handler has been opened.
          const bool open = connection_handler->is_open ();

          // Some other thread was able to open the handler even
          // though wait failed for this thread.
          if (open)
            {
              // Set the result to 0, we have an open connection
              result = 0;
            }
          else
            {
              // Assert that it is still connecting.
              ACE_ASSERT (connection_handler->is_connecting ());

              // Force close the handler now.
              connection_handler->close_handler ();
            }
        }
    }

  return result;
}

//@@ TAO_CONNECTOR_SPL_METHODS_ADD_HOOK

TAO_END_VERSIONED_NAMESPACE_DECL

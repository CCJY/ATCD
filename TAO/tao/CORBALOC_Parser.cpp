#include "CORBALOC_Parser.h"
#include "ORB_Core.h"
#include "Stub.h"
#include "MProfile.h"
#include "Connector_Registry.h"
#include "Transport_Connector.h"
#include "Protocol_Factory.h"
#include "tao/debug.h"
#include "ace/Vector_T.h"
#include "ace/INET_Addr.h"

#if !defined(__ACE_INLINE__)
#include "CORBALOC_Parser.i"
#endif /* __ACE_INLINE__ */

ACE_RCSID (TAO,
           CORBALOC_Parser,
           "$Id$")

TAO_CORBALOC_Parser::~TAO_CORBALOC_Parser (void)
{
}

static const char prefix[] = "corbaloc:";
static const size_t prefix_len = sizeof prefix - 1;
static const char rir_token[] = "rir:/"; // includes key separator
static const size_t rir_token_len = sizeof rir_token - 1;
static const char iiop_token[] = "iiop:";
static const char iiop_token_len = sizeof iiop_token - 1;

int
TAO_CORBALOC_Parser::match_prefix (const char *ior_string) const
{
  // Check if the prefix is 'corbaloc:' and return the result.
  return (ACE_OS::strncmp (ior_string,
                           prefix,
                           prefix_len) == 0);
}

CORBA::Object_ptr
TAO_CORBALOC_Parser::make_stub_from_mprofile (CORBA::ORB_ptr orb,
                                              TAO_MProfile &mprofile
                                              ACE_ENV_ARG_DECL)
  ACE_THROW_SPEC ((CORBA::SystemException))
{
  // Create a TAO_Stub.
  TAO_Stub *data = orb->orb_core ()->create_stub ((const char *) 0,
                                                  mprofile
                                                  ACE_ENV_ARG_PARAMETER);
  ACE_CHECK_RETURN (CORBA::Object::_nil ());

  TAO_Stub_Auto_Ptr safe_data (data);

  CORBA::Object_var obj = orb->orb_core ()->create_object (data);

  if (!CORBA::is_nil (obj.in ()))
    {
      /// All is well, so release the stub object from its
      /// auto_ptr.
      (void) safe_data.release ();

      /// Return the object reference to the application.
      return obj._retn ();
    }

  /// Shouldnt come here: if so, return nil reference.
  return CORBA::Object::_nil ();
}

CORBA::Object_ptr
TAO_CORBALOC_Parser::parse_string_rir_helper (const char * ior,
                                              CORBA::ORB_ptr orb
                                              ACE_ENV_ARG_DECL)
  ACE_THROW_SPEC ((CORBA::SystemException))
{
  // Pass the key string as an argument to resolve_initial_references.
  // NameService is the default if an empty key string is supplied.
  const char *objkey = ior + rir_token_len;

  CORBA::Object_var rir_obj =
    orb->resolve_initial_references (*objkey == '\0' ? "NameService" :
                                     objkey
                                     ACE_ENV_ARG_PARAMETER);
  ACE_CHECK_RETURN (CORBA::Object::_nil ());

  return rir_obj._retn ();
}

CORBA::Object_ptr
TAO_CORBALOC_Parser::parse_string (const char * ior,
                                   CORBA::ORB_ptr orb
                                   ACE_ENV_ARG_DECL)
  ACE_THROW_SPEC ((CORBA::SystemException))
{
  // The decomposition of a corbaloc string is in Section 13.6.10.
  //
  // following the "corbaloc:"
  //  a comma separated list of <prot_addr> strings
  //    for each,
  // Separate out the key, delimited by '/'
  // Split out the various parts of our corbaloc string, comma-delimited
  // For each part
  //   Determine the protocol
  //     If rir, defer to another function and return the object
  //     If iiop, make the profile with <endpoint>:<port>/<key>
  //     If another protocol, use <remainder>/<key>
  //   Search through the collection of protocols for the correct one
  //     If not found, throw exception
  //     If found, make our_connector from it.
  //     our_connector->make_mprofile_unchecked (...);
  //     object = this->make_stub_from_mprofile (...);
  // Return the object

  // Skip the prefix.  We know it is there because this method is only
  // called if match_prefix() returns 1.
  ior += ACE_OS::strlen(prefix);

  //  First check for rir
  if (ACE_OS::strncmp (ior,rir_token,rir_token_len) == 0)
    return this->parse_string_rir_helper (ior,orb
                                          ACE_ENV_ARG_PARAMETER);

  // set up space for parsed endpoints. there will be at least 1, and
  // most likely commas will separate endpoints, although they could be
  // part of an endpoint address for some protocols.
  size_t max_endpoint_count = 1;
  for (const char *comma = ACE_OS::strchr (ior,',');
       comma;
       comma = ACE_OS::strchr (comma+1,','))
    max_endpoint_count++;

  ACE_Array<parsed_endpoint> endpoints(max_endpoint_count);
  endpoints.size (0);

  // Get the Connector Registry from the ORB.
  TAO_Connector_Registry *conn_reg =
    orb->orb_core ()->connector_registry(ACE_ENV_SINGLE_ARG_PARAMETER);

  while (1) { // will loop on comma only.
    size_t len = 0;
    size_t ndx = endpoints.size();
    endpoints.size(ndx+1);
    int uiop_compatible = 0;
    TAO_ConnectorSetIterator conn_iter = 0;
    for (conn_iter = conn_reg->begin();
         conn_iter != conn_reg->end() &&
           endpoints[ndx].profile_ == 0;
         conn_iter ++)
      {
        endpoints[ndx].profile_ =
          (*conn_iter)->corbaloc_scan(ior,len
                                      ACE_ENV_ARG_PARAMETER);
        ACE_CHECK_RETURN (CORBA::Object::_nil ());

        if (endpoints[ndx].profile_)
          {
            endpoints[ndx].obj_key_sep_ =
              (*conn_iter)->object_key_delimiter();
            uiop_compatible = (endpoints[ndx].obj_key_sep_ == '|');
            this->make_canonical (ior,len,endpoints[ndx].prot_addr_
                                   ACE_ENV_ARG_PARAMETER);
            ior += len;
            break;
          }
      }

    if (endpoints[ndx].profile_ == 0)
      {
        if (TAO_debug_level)
          ACE_ERROR ((LM_ERROR,
                      "(%P|%t) TAO_CORBALOC_Parser::parse_string "
                      "could not parse from %s",ior));
        ACE_THROW_RETURN (CORBA::BAD_PARAM (CORBA::OMGVMCID | 10,
                                            CORBA::COMPLETED_NO),
                          CORBA::Object::_nil ());
      }
    if (*ior == ',') // more endpoints follow
      {
        ior++;
        continue;
      }

    if (*ior == '/') // found key separator
      {
        ior ++;
        break;
      }

    if (uiop_compatible && *(ior - 1) == '|')
      // Assume this is an old uiop style corbaloc. No need to warn here,
      // the UIOP_Connector::corbaloc_scan already did.
      break;

    // anything else is a violation.
    if (TAO_debug_level)
      ACE_ERROR ((LM_ERROR,
                  "(%P|%t) TAO_CORBALOC_Parser::parse_string "
                  "could not parse from %s",ior));
    ACE_THROW_RETURN (CORBA::BAD_PARAM (CORBA::OMGVMCID | 10,
                                        CORBA::COMPLETED_NO),
                      CORBA::Object::_nil ());
  } // end of while

  // At this point, ior points at the start of the object key
  ACE_CString obj_key (*ior ? ior : (const char *)"NameService");

  // now take the collection of endpoints along with the decoded key and
  // mix them together to get the mprofile.
  TAO_MProfile mprofile (endpoints.size());

  for (size_t i = 0; i < endpoints.size(); i++)
    {
      ACE_CString full_ep = endpoints[i].prot_addr_ +
        endpoints[i].obj_key_sep_ +
        obj_key;
      const char * str = full_ep.c_str();
      endpoints[i].profile_->parse_string (str
                                                  ACE_ENV_ARG_PARAMETER);
      if (mprofile.add_profile(endpoints[i].profile_) != -1)
        endpoints[i].profile_ = 0;
    }

  CORBA::Object_ptr object = CORBA::Object::_nil ();
  // Get an object stub out.
  object = this->make_stub_from_mprofile (orb,
                                          mprofile
                                          ACE_ENV_ARG_PARAMETER);
  ACE_CHECK_RETURN (CORBA::Object::_nil ());

  return object;
}

void
TAO_CORBALOC_Parser::make_canonical (const char *ior,
                                     size_t prot_addr_len,
                                     ACE_CString &canonical_endpoint
                                     ACE_ENV_ARG_DECL)
  ACE_THROW_SPEC ((CORBA::SystemException))
{
  const char *separator = ACE_OS::strchr (ior, ':');

  // A special case for handling iiop
  if (ior[0] != ':' && ACE_OS::strncmp (ior,iiop_token,iiop_token_len) != 0)
    {
      canonical_endpoint.set (separator+1,
                              prot_addr_len - (separator - ior) - 1,1);
      return;
    }

  const char *addr_base = separator+1;
  const char *addr_tail = ior + prot_addr_len;
  // skip past version, if any
  separator = ACE_OS::strchr (addr_base,'@');
  if (separator != 0 && separator < addr_tail)
    {
      canonical_endpoint.set (addr_base,(separator - addr_base)+1,1);
      addr_base = separator + 1;
    }
  else
    canonical_endpoint.clear ();

  ACE_CString raw_host;
  ACE_CString raw_port;
  separator = ACE_OS::strchr (addr_base,':');
  if (separator != 0 && separator < addr_tail)
    {
      // we have a port number
      if (separator - addr_base > 0)
        raw_host.set (addr_base, (separator - addr_base),1);
      raw_port.set (separator, (addr_tail - separator), 1);
    }
  else
    {
      // we must default port #
      if (addr_base < addr_tail)
        raw_host.set (addr_base, (addr_tail - addr_base),1);
      raw_port.set (":2809");
    }

  if (raw_host.length() == 0)
    {
      ACE_INET_Addr host_addr;

      char tmp_host [MAXHOSTNAMELEN + 1];

      // If no host is specified: assign the default host, i.e. the
      // local host.
      if (host_addr.get_host_name (tmp_host,
                                   sizeof (tmp_host)) != 0)
        {
          // Can't get the IP address since the INET_Addr wasn't
          // initialized.  Just throw an exception.

          if (TAO_debug_level > 0)
            ACE_DEBUG ((LM_DEBUG,
                        ACE_TEXT ("TAO (%P|%t) ")
                        ACE_TEXT ("cannot determine hostname.\n")));

          ACE_THROW (CORBA::INV_OBJREF
                     (CORBA::SystemException::_tao_minor_code
                      (0, EINVAL),
                      CORBA::COMPLETED_NO));
        }
      else
        {
          canonical_endpoint += tmp_host;
        }
    }
  else
    {
      canonical_endpoint += raw_host;
    }

  canonical_endpoint += raw_port;
}

ACE_STATIC_SVC_DEFINE (TAO_CORBALOC_Parser,
                       ACE_TEXT ("CORBALOC_Parser"),
                       ACE_SVC_OBJ_T,
                       &ACE_SVC_NAME (TAO_CORBALOC_Parser),
                       ACE_Service_Type::DELETE_THIS |
                       ACE_Service_Type::DELETE_OBJ,
                       0)

ACE_FACTORY_DEFINE (TAO, TAO_CORBALOC_Parser)

#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)
template class ACE_Array_Base<TAO_CORBALOC_Parser::parsed_endpoint>;
#elif defined (ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)
#pragma instantiate ACE_Array_Base<TAO_CORBALOC_Parser::parsed_endpoint>
#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */

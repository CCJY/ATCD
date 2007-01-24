// "$Id$"

#include "tao/ORB.h"

ACE_RCSID (tao,
           ORB,
           "$Id$")

#include "tao/ORB_Table.h"
#include "tao/Connector_Registry.h"
#include "tao/IOR_Parser.h"
#include "tao/Stub.h"
#include "tao/ORB_Core.h"
#include "tao/ORB_Core_TSS_Resources.h"
#include "tao/TAO_Internal.h"
#include "tao/Dynamic_Adapter.h"
#include "tao/Profile.h"
#include "tao/default_ports.h"
#include "tao/ORBInitializer_Registry_Adapter.h"
#include "tao/PolicyFactory_Registry_Adapter.h"
#include "tao/NVList_Adapter.h"
#include "tao/TAO_Singleton_Manager.h"
#include "tao/Policy_Current.h"
#include "tao/Policy_Manager.h"
#include "tao/Valuetype_Adapter.h"
#include "tao/IFR_Client_Adapter.h"
#include "tao/TypeCodeFactory_Adapter.h"
#include "tao/debug.h"
#include "tao/CDR.h"
#include "tao/SystemException.h"
#include "tao/default_environment.h"
#include "tao/ObjectIdListC.h"

#if !defined (__ACE_INLINE__)
# include "tao/ORB.inl"
#endif /* ! __ACE_INLINE__ */

#include "ace/Dynamic_Service.h"
#include "ace/Service_Config.h"
#include "ace/Arg_Shifter.h"
#include "ace/Reactor.h"
#include "ace/Argv_Type_Converter.h"
#include "ace/ACE.h"
#include "ace/Static_Object_Lock.h"
#include "ace/OS_NS_strings.h"
#include "ace/OS_NS_string.h"
#include "ace/os_include/os_ctype.h"

void TAO_unexpected_exception_handler (void)
{
  throw ::CORBA::UNKNOWN ();
}

static const char ior_prefix[] = "IOR:";

// = Static initialization.

namespace
{
  // Count of the number of ORBs.
  int orb_init_count = 0;
}

// ****************************************************************

TAO_BEGIN_VERSIONED_NAMESPACE_DECL

CORBA::ORB::InvalidName::InvalidName (void)
  : CORBA::UserException ("IDL:omg.org/CORBA/ORB/InvalidName:1.0",
                          "InvalidName")
{
}

// Destructor - all members are of self managing types.
CORBA::ORB::InvalidName::~InvalidName (void)
{
}

// Copy constructor.
CORBA::ORB::InvalidName::InvalidName (const CORBA::ORB::InvalidName &exc)
  : CORBA::UserException (exc._rep_id (),
                          exc._name ())
{
}

// Assignment operator.
CORBA::ORB::InvalidName&
CORBA::ORB::InvalidName::operator= (const ::CORBA::ORB::InvalidName &_tao_excp)
{
  this->UserException::operator= (_tao_excp);
  return *this;
}

CORBA::ORB::InvalidName *
CORBA::ORB::InvalidName::_downcast (CORBA::Exception *exc)
{
  return dynamic_cast<InvalidName *> (exc);
}

CORBA::Exception *CORBA::ORB::InvalidName::_alloc (void)
{
  CORBA::Exception *retval = 0;
  ACE_NEW_RETURN (retval, ::CORBA::ORB::InvalidName, 0);
  return retval;
}

CORBA::Exception *
CORBA::ORB::InvalidName::_tao_duplicate (void) const
{
  CORBA::Exception *result = 0;
  ACE_NEW_RETURN (
      result,
      ::CORBA::ORB::InvalidName (*this),
      0);
  return result;
}

void CORBA::ORB::InvalidName::_raise (void) const
{
  TAO_RAISE (*this);
}

void CORBA::ORB::InvalidName::_tao_encode (
    TAO_OutputCDR &) const
{
  throw ::CORBA::MARSHAL ();
}

void CORBA::ORB::InvalidName::_tao_decode (
    TAO_InputCDR &)
{
  throw ::CORBA::MARSHAL ();
}

// ****************************************************************

CORBA::ORB::ORB (TAO_ORB_Core *orb_core)
  : refcount_ (1)
  , orb_core_ (orb_core)
  , use_omg_ior_format_ (1)
  , timeout_ (0)
{
}

CORBA::ORB::~ORB (void)
{
  // This destructor is only invoked when the last ORB reference (not
  // instance) is being destroyed.
}

void
CORBA::ORB::shutdown (CORBA::Boolean wait_for_completion)
{
  // We cannot lock the exceptions here. We need to propogate
  // BAD_INV_ORDER  exceptions if needed to the caller. Locking
  // exceptions down would render us non-compliant with the spec.
  this->check_shutdown ();

  this->orb_core ()->shutdown (wait_for_completion);
}

void
CORBA::ORB::destroy (void)
{
  if (this->orb_core () == 0)
    {
      // If the ORB_Core pointer is zero, assume that the ORB_Core has
      // been destroyed.

      // As defined by the CORBA 2.3 specification, throw a
      // CORBA::OBJECT_NOT_EXIST exception if the ORB has been
      // destroyed by the time an ORB function is called.

      throw ( ::CORBA::OBJECT_NOT_EXIST (0,
                                          CORBA::COMPLETED_NO));
    }

  if (TAO_debug_level > 2)
    {
      ACE_DEBUG ((LM_DEBUG,
                  ACE_TEXT ("CORBA::ORB::destroy() called on ORB <%s>.\n"),
                  ACE_TEXT_CHAR_TO_TCHAR (this->orb_core ()->orbid ())));
    }

  this->orb_core ()->destroy ();

  // Now invalidate the pointer to the ORB_Core that created this
  // ORB.
  this->orb_core_ = 0;
}

void
CORBA::ORB::run (void)
{
  this->run (0);
}

void
CORBA::ORB::run (ACE_Time_Value &tv)
{
  this->run (&tv);
}

void
CORBA::ORB::run (ACE_Time_Value *tv)
{
  this->check_shutdown ();

  this->orb_core ()->run (tv, 0);
}

void
CORBA::ORB::perform_work (void)
{
  this->perform_work (0);
}

void
CORBA::ORB::perform_work (ACE_Time_Value &tv)
{
  this->perform_work (&tv);
}

void
CORBA::ORB::perform_work (ACE_Time_Value *tv)
{
  // This method should not be called if the ORB has been shutdown.
  this->check_shutdown ();

  this->orb_core ()->run (tv, 1);
}

CORBA::Boolean
CORBA::ORB::work_pending (ACE_Time_Value &tv)
{
  // This method should not be called if the ORB has been shutdown.
  this->check_shutdown ();

  int const result = this->orb_core_->reactor ()->work_pending (tv);
  if (result == 0 || (result == -1 && errno == ETIME))
    return 0;

  if (result == -1)
    ACE_THROW_RETURN (CORBA::INTERNAL (), 0);

  return 1;
}

CORBA::Boolean
CORBA::ORB::work_pending (void)
{
  // This method should not be called if the ORB has been shutdown.
  this->check_shutdown ();

  const int result = this->orb_core_->reactor ()->work_pending ();
  if (result == 0)
    return 0;

  if (result == -1)
    ACE_THROW_RETURN (CORBA::INTERNAL (), 0);

  return 1;
}

#if (TAO_HAS_MINIMUM_CORBA == 0) && !defined (CORBA_E_COMPACT) && !defined (CORBA_E_MICRO)

void
CORBA::ORB::create_list (CORBA::Long count, CORBA::NVList_ptr &new_list)
{
  TAO_NVList_Adapter *adapter =
    ACE_Dynamic_Service<TAO_NVList_Adapter>::instance ("TAO_NVList_Adapter");

  if (adapter == 0)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT ("(%P|%t) %p\n"),
                  ACE_TEXT ("ORB unable to find the ")
                  ACE_TEXT ("NVList Adapter instance")));
      throw ::CORBA::INTERNAL ();
    }

  adapter->create_list (count, new_list);
}

void
CORBA::ORB::create_exception_list (CORBA::ExceptionList_ptr &list)
{
  TAO_Dynamic_Adapter *dynamic_adapter =
    ACE_Dynamic_Service<TAO_Dynamic_Adapter>::instance (
        TAO_ORB_Core::dynamic_adapter_name ());

  dynamic_adapter->create_exception_list (list);
}

void
CORBA::ORB::create_operation_list (CORBA::OperationDef_ptr opDef,
                                   CORBA::NVList_ptr &result)
{
  TAO_IFR_Client_Adapter *adapter =
    ACE_Dynamic_Service<TAO_IFR_Client_Adapter>::instance (
        TAO_ORB_Core::ifr_client_adapter_name ());

  if (adapter == 0)
    {
      throw ( ::CORBA::INTF_REPOS ());
    }

  adapter->create_operation_list (this,
                                  opDef,
                                  result);
}


void
CORBA::ORB::create_environment (CORBA::Environment_ptr &environment)
{
  ACE_NEW_THROW_EX (environment,
                    CORBA::Environment (),
                    CORBA::NO_MEMORY (
                      CORBA::SystemException::_tao_minor_code (
                        0,
                        ENOMEM),
                      CORBA::COMPLETED_NO));
}

void
CORBA::ORB::create_named_value (CORBA::NamedValue_ptr &nv)
{
  TAO_NVList_Adapter *adapter =
    ACE_Dynamic_Service<TAO_NVList_Adapter>::instance (
        "TAO_NVList_Adapter");

  if (adapter == 0)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT ("(%P|%t) %p\n"),
                  ACE_TEXT ("ORB unable to find the ")
                  ACE_TEXT ("NVList Adapter instance")));

      throw ::CORBA::INTERNAL ();
    }

  adapter->create_named_value (nv);
}

// The following functions are not implemented - they just throw
// CORBA::NO_IMPLEMENT.

CORBA::Boolean
CORBA::ORB::get_service_information (
  CORBA::ServiceType /* service_type */,
  CORBA::ServiceInformation_out /* service_information */)
{
  ACE_THROW_RETURN (CORBA::NO_IMPLEMENT (
                      CORBA::SystemException::_tao_minor_code (
                        0,
                        ENOTSUP),
                      CORBA::COMPLETED_NO),
                    0);
}

void
CORBA::ORB::create_context_list (CORBA::ContextList_ptr &)
{
  throw ( ::CORBA::NO_IMPLEMENT (
               CORBA::SystemException::_tao_minor_code (
                 0,
                 ENOTSUP),
               CORBA::COMPLETED_NO));
}

void
CORBA::ORB::get_default_context (CORBA::Context_ptr &)
{
  throw ( ::CORBA::NO_IMPLEMENT (
               CORBA::SystemException::_tao_minor_code (
                 0,
                 ENOTSUP),
               CORBA::COMPLETED_NO));
}

void
CORBA::ORB::send_multiple_requests_oneway (const CORBA::RequestSeq &)
{
  throw ( ::CORBA::NO_IMPLEMENT (
               CORBA::SystemException::_tao_minor_code (
                 0,
                 ENOTSUP),
               CORBA::COMPLETED_NO));
}

void
CORBA::ORB::send_multiple_requests_deferred (const CORBA::RequestSeq &)
{
  throw ( ::CORBA::NO_IMPLEMENT (
               CORBA::SystemException::_tao_minor_code (
                 0,
                 ENOTSUP),
               CORBA::COMPLETED_NO));
}

void
CORBA::ORB::get_next_response (CORBA::Request_ptr &)
{
  throw ( ::CORBA::NO_IMPLEMENT (
               CORBA::SystemException::_tao_minor_code (
                 0,
                 ENOTSUP),
               CORBA::COMPLETED_NO));
}

CORBA::Boolean
CORBA::ORB::poll_next_response (void)
{
  ACE_THROW_RETURN (CORBA::NO_IMPLEMENT (
                      CORBA::SystemException::_tao_minor_code (
                        0,
                        ENOTSUP),
                      CORBA::COMPLETED_NO),
                    0);
}

// ****************************************************************

CORBA::TypeCode_ptr
CORBA::ORB::create_struct_tc (
    const char *id,
    const char *name,
    const CORBA::StructMemberSeq & members)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_struct_tc (id,
                                    name,
                                    members);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_union_tc (
    const char *id,
    const char *name,
    CORBA::TypeCode_ptr discriminator_type,
    const CORBA::UnionMemberSeq & members)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_union_tc (id,
                                   name,
                                   discriminator_type,
                                   members);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_enum_tc (
    const char *id,
    const char *name,
    const CORBA::EnumMemberSeq & members)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_enum_tc (id,
                                  name,
                                  members);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_alias_tc (
    const char *id,
    const char *name,
    CORBA::TypeCode_ptr original_type)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_alias_tc (id,
                                   name,
                                   original_type);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_exception_tc (
    const char *id,
    const char *name,
    const CORBA::StructMemberSeq & members)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_exception_tc (id,
                                       name,
                                       members);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_interface_tc (
    const char *id,
    const char *name)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_interface_tc (id,
                                       name);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_string_tc (
    CORBA::ULong bound)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_string_tc (bound);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_wstring_tc (
    CORBA::ULong bound)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_wstring_tc (bound);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_fixed_tc (
    CORBA::UShort digits,
    CORBA::UShort scale)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_fixed_tc (digits,
                                   scale);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_sequence_tc (
    CORBA::ULong bound,
    CORBA::TypeCode_ptr element_type)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_sequence_tc (bound,
                                      element_type);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_array_tc (
    CORBA::ULong length,
    CORBA::TypeCode_ptr element_type)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_array_tc (length,
                                   element_type);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_value_tc (
    const char *id,
    const char *name,
    CORBA::ValueModifier type_modifier,
    CORBA::TypeCode_ptr concrete_base,
    const CORBA::ValueMemberSeq & members)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_value_tc (id,
                                   name,
                                   type_modifier,
                                   concrete_base,
                                   members);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_value_box_tc (
    const char *id,
    const char *name,
    CORBA::TypeCode_ptr boxed_type)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_value_box_tc (id,
                                       name,
                                       boxed_type);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_native_tc (
    const char *id,
    const char *name)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_native_tc (id,
                                    name);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_recursive_tc (
    const char *id)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_recursive_tc (id);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_abstract_interface_tc (
    const char *id,
    const char *name)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_abstract_interface_tc (id,
                                                name);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_local_interface_tc (
    const char *id,
    const char *name)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_local_interface_tc (id,
                                             name);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_component_tc (
    const char *id,
    const char *name)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_component_tc (id,
                                       name);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_home_tc (
    const char *id,
    const char *name)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_home_tc (id,
                                  name);
}

CORBA::TypeCode_ptr
CORBA::ORB::create_event_tc (
    const char *id,
    const char *name,
    CORBA::ValueModifier type_modifier,
    CORBA::TypeCode_ptr concrete_base,
    const CORBA::ValueMemberSeq &members)
{
  TAO_TypeCodeFactory_Adapter *adapter =
    ACE_Dynamic_Service<TAO_TypeCodeFactory_Adapter>::instance (
        TAO_ORB_Core::typecodefactory_adapter_name ());

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        0);
    }

  return adapter->create_event_tc (id,
                                   name,
                                   type_modifier,
                                   concrete_base,
                                   members);
}

// ****************************************************************

#endif /* TAO_HAS_MINIMUM_CORBA */

CORBA::Object_ptr
CORBA::ORB::resolve_policy_manager (void)
{
#if (TAO_HAS_CORBA_MESSAGING == 1)

  TAO_Policy_Manager *policy_manager =
    this->orb_core_->policy_manager ();

  if (policy_manager == 0)
    {
      return CORBA::Object::_nil ();
    }

  return CORBA::Object::_duplicate (policy_manager);

#else

  return CORBA::Object::_nil ();

#endif /* TAO_HAS_CORBA_MESSAGING == 1 */
}

CORBA::Object_ptr
CORBA::ORB::resolve_policy_current (void)
{

#if (TAO_HAS_CORBA_MESSAGING == 1)

  TAO_Policy_Current &policy_current = this->orb_core_->policy_current ();
  return CORBA::Object::_duplicate (&policy_current);

#else

  return CORBA::Object::_nil ();

#endif /* TAO_HAS_CORBA_MESSAGING == 1 */
}

void
CORBA::ORB::resolve_service (TAO::MCAST_SERVICEID mcast_service_id)
{
  static char const * const env_service_port[] =
  {
    "NameServicePort",
    "TradingServicePort",
    "ImplRepoServicePort",
    "InterfaceRepoServicePort"
  };

  static unsigned short const default_service_port[] =
  {
    TAO_DEFAULT_NAME_SERVER_REQUEST_PORT,
    TAO_DEFAULT_TRADING_SERVER_REQUEST_PORT,
    TAO_DEFAULT_IMPLREPO_SERVER_REQUEST_PORT,
    TAO_DEFAULT_INTERFACEREPO_SERVER_REQUEST_PORT
  };

  // By now, the table filled in with -ORBInitRef arguments has been
  // checked.  We only get here if the table didn't contain an initial
  // reference for the requested Service.

  CORBA::String_var default_init_ref =
        this->orb_core_->orb_params ()->default_init_ref ();

  static char const mcast_prefix[] = "mcast://:::";

  if ((ACE_OS::strncmp (default_init_ref.in (),
                        mcast_prefix,
                        sizeof (mcast_prefix) - 1) == 0))
    {
      // First, determine if the port was supplied on the command line
      unsigned short port =
        this->orb_core_->orb_params ()->service_port (mcast_service_id);

      if (port == 0)
        {
          // Look for the port among our environment variables.
          char const * const port_number =
            ACE_OS::getenv (env_service_port[mcast_service_id]);

          if (port_number != 0)
            port = static_cast<unsigned short> (ACE_OS::atoi (port_number));
          else
            port = default_service_port[mcast_service_id];
        }

      // Set the port value in ORB_Params: modify the default mcast
      // value.
      static char const mcast_fmt[] = "mcast://:%d::";
      static size_t const PORT_BUF_SIZE = 256;

      char def_init_ref[PORT_BUF_SIZE] = { 0 }; // snprintf() doesn't
                                                // null terminate.
                                                // Make sure we do.

      ACE_OS::snprintf (def_init_ref,
                        PORT_BUF_SIZE - 1, // Account for null
                                           // terminator.
                        mcast_fmt,
                        port);

      this->orb_core_->orb_params ()->default_init_ref (def_init_ref);
    }
}

CORBA::Object_ptr
CORBA::ORB::resolve_initial_references (const char *name)
{
  return this->resolve_initial_references (name, 0);
}

CORBA::Object_ptr
CORBA::ORB::resolve_initial_references (const char *name,
                                        ACE_Time_Value *timeout)
{
  // This method should not be called if the ORB has been shutdown.
  this->check_shutdown ();

  CORBA::Object_var result;

  if (ACE_OS::strcmp (name, TAO_OBJID_ROOTPOA) == 0)
    {
      result = this->orb_core ()->root_poa ();
    }
  else if (ACE_OS::strcmp (name, TAO_OBJID_POACURRENT) == 0)
    {
      result = this->orb_core ()->resolve_poa_current
        ();
    }
  else if (ACE_OS::strcmp (name, TAO_OBJID_POLICYMANAGER) == 0)
    {
      result = this->resolve_policy_manager ();
    }
  else if (ACE_OS::strcmp (name, TAO_OBJID_POLICYCURRENT) == 0)
    {
      result = this->resolve_policy_current ();
    }
  else if (ACE_OS::strcmp (name, TAO_OBJID_IORMANIPULATION) == 0)
    {
      result = this->orb_core ()->resolve_ior_manipulation
        ();
    }
  else if (ACE_OS::strcmp (name, TAO_OBJID_IORTABLE) == 0)
    {
      result = this->orb_core ()->resolve_ior_table
        ();
    }
  else if (ACE_OS::strcmp (name, TAO_OBJID_DYNANYFACTORY) == 0)
    {
      result = this->orb_core ()->resolve_dynanyfactory
        ();
    }
  else if (ACE_OS::strcmp (name, TAO_OBJID_TYPECODEFACTORY) == 0)
    {
      result = this->orb_core ()->resolve_typecodefactory
        ();
    }
  else if (ACE_OS::strcmp (name, TAO_OBJID_CODECFACTORY) == 0)
    {
      result = this->orb_core ()->resolve_codecfactory
        ();
    }
  else if (ACE_OS::strcmp (name, TAO_OBJID_COMPRESSIONMANAGER) == 0)
    {
      result = this->orb_core ()->resolve_compression_manager
        ();
    }
#if TAO_HAS_INTERCEPTORS == 1
  else if (ACE_OS::strcmp (name, TAO_OBJID_PICurrent) == 0)
    {
      result = this->orb_core ()->resolve_picurrent
        ();
    }
#endif

  // -----------------------------------------------------------------

  if (CORBA::is_nil (result.in ()))
    {
      // Search the object reference table.  This search must occur before
      // the InitRef table search, since it may contain local objects.
      result =
        this->orb_core ()->object_ref_table ().resolve_initial_reference (
          name);
    }

  if (!CORBA::is_nil (result.in ()))
    return result._retn ();

  // -----------------------------------------------------------------

  // Check ORBInitRef options.

  // @@ There appears to be long standing (i.e. back when the map was
  //    an ACE_Hash_Map_Manager) race condition here since the map
  //    access is not synchronized.

  // Is the service name in the IOR Table.
  TAO_ORB_Core::InitRefMap::iterator ior =
    this->orb_core_->init_ref_map ()->find (ACE_CString (name));

  if (ior != this->orb_core_->init_ref_map ()->end ())
    return this->string_to_object ((*ior).second.c_str ());

  // Look for an environment variable called "<name>IOR".
  //
  CORBA::String_var ior_env_var_name =
    CORBA::string_alloc (static_cast<CORBA::ULong> (ACE_OS::strlen (name) + 3));

  ACE_OS::strcpy (ior_env_var_name.inout (),
                  name);

  ACE_OS::strcat (ior_env_var_name.inout (),
                  "IOR");

  ACE_CString service_ior = ACE_OS::getenv (ior_env_var_name.in ());

  if (ACE_OS::strcmp (service_ior.c_str (), "") != 0)
    {
      result = this->string_to_object (service_ior.c_str());

      return result._retn ();
    }

  // May be trying the explicitly specified services and the well
  // known services should be tried first before falling on to default
  // services.

  // Set the timeout value.
  this->set_timeout (timeout);

  if (ACE_OS::strcmp (name, TAO_OBJID_NAMESERVICE) == 0)
    {
      this->resolve_service (TAO::MCAST_NAMESERVICE);
    }
  else if (ACE_OS::strcmp (name, TAO_OBJID_TRADINGSERVICE) == 0)
    {
      this->resolve_service (TAO::MCAST_TRADINGSERVICE);
    }
  else if (ACE_OS::strcmp (name, TAO_OBJID_IMPLREPOSERVICE) == 0)
    {
      this->resolve_service (TAO::MCAST_IMPLREPOSERVICE);
    }
  else if (ACE_OS::strcmp (name, TAO_OBJID_INTERFACEREP) == 0)
    {
      this->resolve_service (TAO::MCAST_INTERFACEREPOSERVICE);
    }

  // Is not one of the well known services, try to find it in the
  // InitRef table....check the defaultinitref values also.

  result = this->orb_core ()->resolve_rir (name);

  if (!CORBA::is_nil (result.in ()))
    return result._retn ();
  // -----------------------------------------------------------------

  ACE_THROW_RETURN (CORBA::ORB::InvalidName (), CORBA::Object::_nil ());
}

#if !defined(CORBA_E_MICRO)
void
CORBA::ORB::register_initial_reference (const char * id,
                                        CORBA::Object_ptr obj)
{
  if (id == 0 || ACE_OS::strlen (id) == 0)
    throw ( ::CORBA::ORB::InvalidName ());

  if (CORBA::is_nil (obj))
    throw ( ::CORBA::BAD_PARAM (CORBA::OMGVMCID | 27,
                                 CORBA::COMPLETED_NO));

  TAO_Object_Ref_Table &table = this->orb_core_->object_ref_table ();

  if (table.register_initial_reference (id, obj) == -1)
    throw ( ::CORBA::ORB::InvalidName ());
}
#endif

CORBA::ORB::ObjectIdList_ptr
CORBA::ORB::list_initial_services (void)
{
  this->check_shutdown ();

  return
    this->orb_core ()->list_initial_references ();
}

void
CORBA::ORB::check_shutdown (void)
{
  if (this->orb_core () != 0)
    {
      this->orb_core ()->check_shutdown ();
    }
  else
    {
      // If the ORB_Core pointer is zero, assume that the ORB_Core has
      // been destroyed.

      // As defined by the CORBA 2.3 specification, throw a
      // CORBA::OBJECT_NOT_EXIST exception if the ORB has been
      // destroyed by the time an ORB function is called.

      throw ( ::CORBA::OBJECT_NOT_EXIST (0,
                                          CORBA::COMPLETED_NO));
    }
}

// ****************************************************************

void
TAO::ORB::init_orb_globals (void)
{
  // This method should be invoked atomically.  It is the caller's
  // responsibility to ensure that this condition is satisfied.

  // Prevent multiple initializations.
  if (orb_init_count != 0)
    {
      return;
    }
  else
    {
      ++orb_init_count;
    }

  // This must be done after the system TypeCodes and Exceptions have
  // been initialized.  An unexpected exception will cause TAO's
  // unexpected exception handler to be called.  That handler
  // transforms all unexpected exceptions to CORBA::UNKNOWN, which of
  // course requires the TypeCode constants and system exceptions to
  // have been initialized.
  /**
   * @note The new TypeCode implementation obviates the above
   *       comment.  Standard CORBA exception TypeCodes are now
   *       available before CORBA::ORB_init() is ever called.
   */
  TAO_Singleton_Manager::instance ()->_set_unexpected (
    ::TAO_unexpected_exception_handler);
}

CORBA::ORB_ptr
CORBA::ORB::_tao_make_ORB (TAO_ORB_Core * orb_core)
{
  CORBA::ORB_ptr orb = CORBA::ORB_ptr ();

  ACE_NEW_RETURN (orb,
                  CORBA::ORB (orb_core),
                  CORBA::ORB::_nil ());

  return orb;
}

// ****************************************************************

// ORB initialization, per OMG document 98-12-01.

CORBA::ORB_ptr
CORBA::ORB_init (int &argc,
                 char *argv[],
                 const char *orb_name)
{
  CORBA::Environment env;
  return CORBA::ORB_init (argc,
                          argv,
                          orb_name,
                          env /* unused */);
}

CORBA::ORB_ptr
CORBA::ORB_init (int &argc,
                 char *argv[],
                 const char *orbid,
                 CORBA::Environment &try_ENV)
{
  // This ugly macro will go away once we've merged the two ORB_init's.
  ACE_UNUSED_ARG(try_ENV); // FUZZ: ignore check_for_ace_check

  // Use this string variable to hold the orbid
  ACE_CString orbid_string (orbid);

  // Copy command line parameter not to use original.
  ACE_Argv_Type_Converter command_line(argc, argv);

  {
    // Using ACE_Static_Object_Lock::instance() precludes ORB_init()
    // from being called within a static object CTOR.
    ACE_MT (ACE_GUARD_RETURN (TAO_SYNCH_RECURSIVE_MUTEX,
                              guard,
                              *ACE_Static_Object_Lock::instance (),
                              CORBA::ORB::_nil ()));

    // Make sure TAO's singleton manager is initialized.
    // We need to initialize before TAO_default_environment() is called
    // since that call instantiates a TAO_TSS_Singleton.
    if (TAO_Singleton_Manager::instance ()->init () == -1)
      {
        return CORBA::ORB::_nil ();
      }

    TAO::ORB::init_orb_globals ();
  }

  // Make sure the following is done after the global ORB
  // initialization since we need to have exceptions initialized.

  // It doesn't make sense for argc to be zero and argv to be
  // non-empty/zero, or for argc to be greater than zero and argv be
  // zero.
  const size_t argv0_len =
    (command_line.get_TCHAR_argv ()
     ? (*command_line.get_TCHAR_argv ()
        ? ACE_OS::strlen (*command_line.get_TCHAR_argv ())
        : 0)
     : 0);

  if ((command_line.get_argc () == 0 && argv0_len != 0)
      || (command_line.get_argc () != 0
          && (command_line.get_TCHAR_argv () == 0
              || command_line.get_TCHAR_argv ()[0] == 0)))
    {
      ACE_THROW_RETURN (CORBA::BAD_PARAM (
                          CORBA::SystemException::_tao_minor_code (
                            0,
                            EINVAL),
                          CORBA::COMPLETED_NO),
                        CORBA::ORB::_nil ());
    }



  if (orbid_string.length () == 0)
    {
      ACE_Arg_Shifter arg_shifter (command_line.get_argc (),
                                   command_line.get_TCHAR_argv ());

      while (arg_shifter.is_anything_left ())
        {
          const ACE_TCHAR *current_arg = arg_shifter.get_current ();

          static const ACE_TCHAR orbid_opt[] = ACE_TEXT ("-ORBid");
          size_t orbid_len = ACE_OS::strlen (orbid_opt);
          if (ACE_OS::strcasecmp (current_arg,
                                  orbid_opt) == 0)
            {
              arg_shifter.consume_arg ();
              if (arg_shifter.is_parameter_next ())
                {
                  orbid_string =
                    ACE_TEXT_ALWAYS_CHAR (arg_shifter.get_current ());
                  arg_shifter.consume_arg ();
                }
            }
          else if (ACE_OS::strncasecmp (current_arg, orbid_opt,
                                        orbid_len) == 0)
            {
              arg_shifter.consume_arg ();
              // The rest of the argument is the ORB id...
              // but we should skip an optional space...
              if (current_arg[orbid_len] == ' ')
                orbid_string =
                  ACE_TEXT_ALWAYS_CHAR (current_arg + orbid_len + 1);
              else
                orbid_string = ACE_TEXT_ALWAYS_CHAR (current_arg + orbid_len);
            }
          else
            arg_shifter.ignore_arg ();
        }
    }

  // Get ORB Core
  TAO_ORB_Core_Auto_Ptr oc (
    TAO::ORB_Table::instance ()->find (orbid_string.c_str ()));

  // The ORB was already initialized.  Just return that one.
  if (oc.get () != 0)
    {
      return CORBA::ORB::_duplicate (oc->orb ());
    }
  else
    {
      // An ORB corresponding to the desired ORBid doesn't exist so create
      // a new one.
      TAO_ORB_Core * tmp = 0;
      ACE_NEW_THROW_EX (tmp,
                        TAO_ORB_Core (orbid_string.c_str ()),
                        CORBA::NO_MEMORY (
                          CORBA::SystemException::_tao_minor_code (0,
                                                                   ENOMEM),
                          CORBA::COMPLETED_NO));

      // The ORB table increases the reference count on the ORB Core
      // so do not release it here.  Allow the TAO_ORB_Core_Auto_Ptr
      // to decrease the reference count on the ORB Core when it goes
      // out of scope.
      oc.reset (tmp);
    }


  // Having the ORB's default static services be shared among all ORBs
  // is tempting from the point of view of reducing the dynamic
  // footprint. However, if the ORB in a DLL and the rest of that
  // application most likely neither cares, nor wishes to know about
  // them. Furthermore, if the ORB DLL gets unloaded, the static
  // services it had registered globaly will no longer be accesible,
  // which will have disastrous consequences at the process
  // shutdown. Hence, the ACE_Service_Config_Guard ensures that for
  // the current thread, any references to the global
  // ACE_Service_Config will be forwarded to the ORB's.

  // Making sure the initialization process in the current thread uses
  // the correct service repository (ours), instead of the global one.
  ACE_Service_Config_Guard scg (oc->configuration ());

  /*
   * Currently I choose to make the ORB an owner of its configuration,
   * which in general is not quite correct because it is very common ORBs to
   * need to share the same configuration.
   */

  // Initialize the Service Configurator.  This must occur before the
  // ORBInitializer::pre_init() method is invoked on each registered
  // ORB initializer.
  int result = TAO::ORB::open_services (oc->configuration (),
                                        command_line.get_argc (),
                                        command_line.get_TCHAR_argv ());

  // Check for errors returned from <TAO_Internal::open_services>.
  if (result != 0 && errno != ENOENT)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT ("(%P|%t) %p\n"),
                  ACE_TEXT ("Unable to initialize the ")
                  ACE_TEXT ("Service Configurator")));
      ACE_THROW_RETURN (CORBA::INITIALIZE (
                          CORBA::SystemException::_tao_minor_code (
                            TAO_ORB_CORE_INIT_LOCATION_CODE,
                            0),
                          CORBA::COMPLETED_NO),
                        CORBA::ORB::_nil ());
    }

  TAO::ORBInitializer_Registry_Adapter * orbinitializer_registry =
    oc.get ()->orbinitializer_registry ();

  PortableInterceptor::SlotId slotid = 0;
  size_t pre_init_count = 0;

  if (orbinitializer_registry != 0)
    {
      pre_init_count =
        orbinitializer_registry->pre_init (oc.get (),
                                           command_line.get_argc(),
                                           command_line.get_ASCII_argv(),
                                           slotid);
    }

  // Initialize the ORB Core instance.
  result = oc->init (command_line.get_argc(),
                     command_line.get_ASCII_argv());

  if (orbinitializer_registry != 0)
    {

      orbinitializer_registry->post_init (pre_init_count,
                                          oc.get (),
                                          command_line.get_argc(),
                                          command_line.get_ASCII_argv(),
                                          slotid);
    }

  if (TAO_debug_level > 2)
    {
      ACE_DEBUG ((LM_DEBUG,
                  ACE_TEXT("TAO (%P|%t) created new ORB <%s>\n"),
                  ACE_TEXT_CHAR_TO_TCHAR (orbid_string.c_str ())));
    }

  // Before returning remember to store the ORB into the table...
  if (TAO::ORB_Table::instance ()->bind (orbid_string.c_str (),
                                         oc.get ()) != 0)
    ACE_THROW_RETURN (CORBA::INTERNAL (0,
                                       CORBA::COMPLETED_NO),
                      CORBA::ORB::_nil ());

  // Return a duplicate since the ORB_Core should release the last
  // reference to the ORB.
  return CORBA::ORB::_duplicate (oc->orb ());
}

// ****************************************************************

// Object reference stringification.
char *
CORBA::ORB::object_to_string (CORBA::Object_ptr obj)
{
  // This method should not be called if the ORB has been shutdown.
  this->check_shutdown ();

  if (!CORBA::is_nil (obj))
    {
      if (!obj->can_convert_to_ior ())
        ACE_THROW_RETURN (CORBA::MARSHAL (CORBA::OMGVMCID | 4,
                                          CORBA::COMPLETED_NO),
                          0);

      // Allow a user to provide custom object stringification
      char* user_string =
        obj->convert_to_ior (this->use_omg_ior_format_,
                             ior_prefix);
      if (user_string != 0)
        return user_string;
    }

  // Application writer controls what kind of objref strings they get,
  // maybe along with other things, by how they initialize the ORB.

  if (use_omg_ior_format_)
    {
      // By default, orbs use IOR strings; these are ugly (and error
      // prone) but specified by CORBA.
      //
      // XXX there should be a simple way to reuse this code in other
      // ORB implementations ...

#if defined (ACE_INITIALIZE_MEMORY_BEFORE_USE)
      char buf [ACE_CDR::DEFAULT_BUFSIZE] = { 0 };
#else
      // Avoid the initialization overhead if not compiling with
      // support for a memory profiler.  There is no need to actually perform
      // initialization otherwise.
      char buf [ACE_CDR::DEFAULT_BUFSIZE];
#endif /* ACE_INITIALIZE_MEMORY_BEFORE_USE */

      TAO_OutputCDR cdr (buf,  sizeof buf,
                         TAO_ENCAP_BYTE_ORDER,
                         this->orb_core_->output_cdr_buffer_allocator (),
                         this->orb_core_->output_cdr_dblock_allocator (),
                         this->orb_core_->output_cdr_msgblock_allocator (),
                         this->orb_core_->orb_params ()->cdr_memcpy_tradeoff (),
                         TAO_DEF_GIOP_MAJOR,
                         TAO_DEF_GIOP_MINOR);

      // There is no translator currently available for stringifying an object
      // reference, since there is no transport with which to choose an NCS/TCS
      // pair.

      // support limited oref ACE_OS::strcmp.
      (void) ACE_OS::memset (buf, 0, sizeof (buf));

      // Marshal the objref into an encapsulation bytestream.
      (void) cdr.write_octet (TAO_ENCAP_BYTE_ORDER);
      if ((cdr << obj) == 0)
        ACE_THROW_RETURN (CORBA::MARSHAL (), 0);

      // Now hexify the encapsulated CDR data into a string, and
      // return that string.

      const size_t total_len = cdr.total_length ();

      char *cp = 0;
      ACE_ALLOCATOR_RETURN (cp,
                            CORBA::string_alloc (
                              sizeof ior_prefix
                              + 2
                              * static_cast<CORBA::ULong> (total_len)),
                            0);

      CORBA::String_var string = cp;

      ACE_OS::strcpy (cp, ior_prefix);
      cp += sizeof (ior_prefix) - 1;

      for (const ACE_Message_Block *i = cdr.begin ();
           i != 0;
           i = i->cont ())
        {
          const char *bytes = i->rd_ptr ();
          size_t len = i->length ();

          while (len--)
            {
              *cp++ = ACE::nibble2hex ((*bytes) >> 4);
              *cp++ = ACE::nibble2hex (*bytes);
              ++bytes;
            }
        }
      // Null terminate the string..
      *cp = 0;

      return string._retn ();
    }
  else
    {
      // It is perfectly valid to marshal a nil object reference.
      // However, it is not possible to convert a nil object reference
      // to a URL IOR, so throw an exception.
      if (CORBA::is_nil (obj) || obj->_stubobj () == 0)
        {
          if (TAO_debug_level > 0)
            ACE_ERROR ((LM_ERROR,
                        ACE_TEXT ("Nil object reference or TAO_Stub ")
                        ACE_TEXT ("pointer is zero when converting\n")
                        ACE_TEXT ("object reference to URL IOR.\n")));

          ACE_THROW_RETURN (CORBA::MARSHAL (
                              CORBA::SystemException::_tao_minor_code (
                                0,
                                EINVAL),
                              CORBA::COMPLETED_NO),
                            0);
        }

      TAO_MProfile &mp = obj->_stubobj ()->base_profiles ();

      if (mp.profile_count () == 0)
        {
          if (TAO_debug_level > 0)
            ACE_ERROR ((LM_ERROR,
                        ACE_TEXT ("(%P|%t) Cannot stringify given ")
                        ACE_TEXT ("object.  No profiles.\n")));


          ACE_THROW_RETURN (CORBA::MARSHAL (
                              CORBA::SystemException::_tao_minor_code (
                                0,
                                EINVAL),
                              CORBA::COMPLETED_NO),
                            0);
        }

      // For now we just use the first profile.
      TAO_Profile *profile = mp.get_profile (0);

      return profile->to_string ();
    }
}

// Destringify arbitrary objrefs.  This method is called from
// <resolve_name_service> with an IOR <multicast_to_service>.

CORBA::Object_ptr
CORBA::ORB::string_to_object (const char *str)
{
  // This method should not be called if the ORB has been shutdown.
  this->check_shutdown ();

  // Check for NULL pointer
  if (str == 0)
    ACE_THROW_RETURN (CORBA::INV_OBJREF (
                        CORBA::SystemException::_tao_minor_code (
                          0,
                          EINVAL),
                        CORBA::COMPLETED_NO),
                      CORBA::Object::_nil ());

  TAO_IOR_Parser *ior_parser =
    this->orb_core_->parser_registry ()->match_parser (str);

  if (ior_parser != 0)
    {
      return ior_parser->parse_string (str,
                                       this);
    }

  if (ACE_OS::strncmp (str,
                       ior_prefix,
                       sizeof ior_prefix - 1) == 0)
    return this->ior_string_to_object (str + sizeof ior_prefix - 1);
  else
    return this->url_ior_string_to_object (str);
}

// ****************************************************************

char *
CORBA::ORB::id (void)
{
  return CORBA::string_dup (this->orb_core_->orbid ());
}

// ****************************************************************

#if !defined(CORBA_E_MICRO)
CORBA::Policy_ptr
CORBA::ORB::create_policy (CORBA::PolicyType type,
                           const CORBA::Any& val)
{
  this->check_shutdown ();

  TAO::PolicyFactory_Registry_Adapter *adapter =
    this->orb_core_->policy_factory_registry ();

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        CORBA::Policy::_nil ());
    }

  // Attempt to obtain the policy from the policy factory registry.
  return adapter->create_policy (type, val);
}
#endif

#if !defined(CORBA_E_MICRO)
CORBA::Policy_ptr
CORBA::ORB::_create_policy (CORBA::PolicyType type)
{
  this->check_shutdown ();

  TAO::PolicyFactory_Registry_Adapter *adapter =
    this->orb_core_->policy_factory_registry ();

  if (adapter == 0)
    {
      ACE_THROW_RETURN (CORBA::INTERNAL (),
                        CORBA::Policy::_nil ());
    }

  // Attempt to obtain the policy from the policy factory registry.
  return adapter->_create_policy (type);
}
#endif

// Destringify OMG-specified "IOR" string.
//
// XXX there should be a simple way to reuse this code in other ORB
// implementations ...

CORBA::Object_ptr
CORBA::ORB::ior_string_to_object (const char *str)
{
  // Unhex the bytes, and make a CDR deencapsulation stream from the
  // resulting data.
  ACE_Message_Block mb (ACE_OS::strlen (str) / 2 + 1
                        + ACE_CDR::MAX_ALIGNMENT + 1);

  ACE_CDR::mb_align (&mb);

  char *buffer = mb.rd_ptr ();
  const char *tmp = str;
  size_t len = 0;

  while (tmp [0] && tmp [1])
    {
      // Some platforms define 'byte' as a macro, solve the problem
      // here.
#undef byte
      unsigned char byte;

      if (!(isxdigit (tmp [0]) && isxdigit (tmp [1])))
        break;

      byte = (u_char) (ACE::hex2byte (tmp [0]) << 4);
      byte |= ACE::hex2byte (tmp [1]);

      buffer [len++] = byte;
      tmp += 2;
    }

  if (tmp [0] && !isspace (tmp [0]))
    {
      ACE_THROW_RETURN (CORBA::BAD_PARAM (),
                        CORBA::Object::_nil ());
    }

  // Create deencapsulation stream ... then unmarshal objref from that
  // stream.

  int byte_order = *(mb.rd_ptr ());
  mb.rd_ptr (1);
  mb.wr_ptr (len);
  TAO_InputCDR stream (&mb,
                       byte_order,
                       TAO_DEF_GIOP_MAJOR,
                       TAO_DEF_GIOP_MINOR,
                       this->orb_core_);

  CORBA::Object_ptr objref = CORBA::Object::_nil ();
  stream >> objref;
  return objref;
}

// ****************************************************************

// Convert an URL style IOR in an object

CORBA::Object_ptr
CORBA::ORB::url_ior_string_to_object (const char* str)
{
  TAO_MProfile mprofile;
  // It is safe to declare this on the stack since the contents of
  // mprofile get copied.  No memory is allocated for profile storage
  // here.  The Connector Registry will determine the exact number
  // of profiles and tell the MProfile object to allocate enough memory
  // to hold them all.

  TAO_Connector_Registry *conn_reg =
    this->orb_core_->connector_registry ();

  int const retv =
    conn_reg->make_mprofile (str,
                             mprofile);
  // Return nil.

  if (retv != 0)
    {
      ACE_THROW_RETURN (CORBA::INV_OBJREF (
                          CORBA::SystemException::_tao_minor_code (
                            0,
                            EINVAL),
                          CORBA::COMPLETED_NO),
                        CORBA::Object::_nil ());
    }

  // Now make the TAO_Stub.
  TAO_Stub *data = this->orb_core_->create_stub ((char *) 0,
                                                 mprofile);

  TAO_Stub_Auto_Ptr safe_objdata (data);

  // Figure out if the servant is collocated.
  CORBA::Object_ptr obj =
    this->orb_core_->create_object (safe_objdata.get ());
  if (CORBA::is_nil (obj))
    return CORBA::Object::_nil ();

  // Transfer ownership to the CORBA::Object
  (void) safe_objdata.release ();

  return obj;
}

// ****************************************************************

ACE_Time_Value *
CORBA::ORB::get_timeout (void)
{
  return this->timeout_;
}

void
CORBA::ORB::set_timeout (ACE_Time_Value *timeout)
{
  this->timeout_ = timeout;
}

// *************************************************************
// Valuetype factory operations
// *************************************************************

#if !defined(CORBA_E_MICRO)
CORBA::ValueFactory
CORBA::ORB::register_value_factory (const char *repository_id,
                                    CORBA::ValueFactory factory)
{
  TAO_Valuetype_Adapter *vta = this->orb_core ()->valuetype_adapter ();

  if (vta)
    {
      int const result = vta->vf_map_rebind (repository_id,
                                             factory);

      if (result == 0)              // No previous factory found
        {
          return 0;
        }

      if (result == -1)
        {
          // Error on bind.
          ACE_THROW_RETURN (CORBA::MARSHAL (),
                            0);
        }
    }

  return factory;    // previous factory was found
}
#endif

#if !defined(CORBA_E_MICRO)
void
CORBA::ORB::unregister_value_factory (const char *repository_id)
{
  TAO_Valuetype_Adapter *vta = this->orb_core ()->valuetype_adapter ();

  if (vta)
    {
      // Dont care whther it was successful or not!
      (void) vta->vf_map_unbind (repository_id);
    }
}
#endif

#if !defined(CORBA_E_MICRO)
CORBA::ValueFactory
CORBA::ORB::lookup_value_factory (const char *repository_id)
{
  TAO_Valuetype_Adapter *vta = this->orb_core ()->valuetype_adapter ();

  if (vta)
    {
      return vta->vf_map_find (repository_id);
    }

  return 0;
}
#endif

TAO_END_VERSIONED_NAMESPACE_DECL

// $Id$
//
// ****              Code generated by the                 ****
// ****  Component Integrated ACE ORB (CIAO) CIDL Compiler ****
// CIAO has been developed by:
//       Center for Distributed Object Computing
//       Washington University
//       St. Louis, MO
//       USA
//       http://www.cs.wustl.edu/~schmidt/doc-center.html
// CIDL Compiler has been developed by:
//       Institute for Software Integrated Systems
//       Vanderbilt University
//       Nashville, TN
//       USA
//       http://www.isis.vanderbilt.edu/
//
// Information about CIAO is available at:
//    http://www.dre.vanderbilt.edu/CIAO

#ifndef CIAO_SIMPLECONSUMER_EXEC_H
#define CIAO_SIMPLECONSUMER_EXEC_H

#include /**/ "ace/pre.h"

#include "SimpleConsumerEC.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
# pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

#include "SimpleConsumer_exec_export.h"
#include "tao/LocalObject.h"

namespace CIAO_Simple_SimpleConsumer_Impl
{
  class SIMPLECONSUMER_EXEC_Export SimpleConsumer_exec_i
    : public virtual SimpleConsumer_Exec,
      public virtual ::CORBA::LocalObject
  {
    public:
    SimpleConsumer_exec_i (void);
    virtual ~SimpleConsumer_exec_i (void);

    // Supported or inherited operations.

    // Attribute operations.

    // Port operations.

    virtual void
    push_hello_ (
      ::Simple::Hello *ev);

    // Operations from Components::SessionComponent

    virtual void
    set_session_context (
      ::Components::SessionContext_ptr ctx);

    virtual void configuration_complete ();

    virtual void ccm_activate ();

    virtual void ccm_passivate ();

    virtual void ccm_remove ();

    private:
    ::Simple::CCM_SimpleConsumer_Context_var context_;
  };

  class SIMPLECONSUMER_EXEC_Export SimpleConsumerHome_exec_i
    : public virtual SimpleConsumerHome_Exec,
      public virtual ::CORBA::LocalObject
  {
    public:
    SimpleConsumerHome_exec_i (void);
    virtual ~SimpleConsumerHome_exec_i (void);

    // Supported or inherited operations.

    // Home operations.

    // Factory and finder operations.

    // Attribute operations.

    // Implicit operations.

    virtual ::Components::EnterpriseComponent_ptr
    create ();
  };

  extern "C" SIMPLECONSUMER_EXEC_Export ::Components::HomeExecutorBase_ptr
  create_Simple_SimpleConsumerHome_Impl (void);
}

#include /**/ "ace/post.h"

#endif /* CIAO_SIMPLECONSUMER_EXEC_H */


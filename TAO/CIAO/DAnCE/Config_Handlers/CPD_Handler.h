//================================================
/**
 *  @file  CPD_Handler.h
 *
 *  $Id$
 *
 *  @author Jules White <jules@dre.vanderbilt.edu>
 */
//================================================

#ifndef CIAO_CONFIG_HANDLERS_CPD_HANDLER_H
#define CIAO_CONFIG_HANDLERS_CPD_HANDLER_H

#include /**/ "ace/pre.h"

#include "Config_Handlers/Config_Handlers_Export.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
# pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

namespace Deployment
{
  struct ComponentPortDescription;
}

namespace CIAO
{
  namespace Config_Handlers
  {
    class ComponentPortDescription;
    /*
     * @class CPD_Handler
     *
     * @brief Handler class for <CCMComponentPortDescription> types.
     *
     * This class defines handler methods to map values from
     * XSC objects, parsed from the descriptor files, to the
     * corresponding CORBA IDL type for the schema element.
     *
     */

    class Config_Handlers_Export CPD_Handler
    {

    public:
      /// Maps the values from the XSC object
      /// <ComponentInterfaceDescription> to the CORBA IDL type
      /// <Deployment::ComponentInterfaceDescription>.
     static void component_port_description (
         const ComponentPortDescription &desc,
         ::Deployment::ComponentPortDescription &toconfig);
    };

 }
}

#include /**/ "ace/post.h"
#endif /* CIAO_CONFIG_HANDLERS_CPD_HANDLER_H*/

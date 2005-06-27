// -*- C++ -*-
//
// $Id$

// ****  Code generated by the The ACE ORB (TAO) IDL Compiler ****
// TAO and the TAO IDL Compiler have been developed by:
//       Center for Distributed Object Computing
//       Washington University
//       St. Louis, MO
//       USA
//       http://www.cs.wustl.edu/~schmidt/doc-center.html
// and
//       Distributed Object Computing Laboratory
//       University of California at Irvine
//       Irvine, CA
//       USA
//       http://doc.ece.uci.edu/
// and
//       Institute for Software Integrated Systems
//       Vanderbilt University
//       Nashville, TN
//       USA
//       http://www.isis.vanderbilt.edu/
//
// Information about TAO is available at:
//     http://www.cs.wustl.edu/~schmidt/TAO.html

#include "BoundsC.h"
#include "tao/Null_RefCount_Policy.h"
#include "tao/TypeCode_Constants.h"
#include "tao/Alias_TypeCode_Static.h"
#include "tao/Struct_TypeCode_Static.h"
#include "tao/TypeCode_Struct_Field.h"
#include "tao/CDR.h"
#include "tao/Any.h"
#include "tao/Any_Dual_Impl_T.h"

// TAO_IDL - Generated from
// be\be_visitor_typecode/struct_typecode.cpp:87

static TAO::TypeCode::Struct_Field<char const *, CORBA::TypeCode_ptr const *> const * const _tao_fields_CORBA_Bounds = 0;
static TAO::TypeCode::Struct<char const *,
                      CORBA::TypeCode_ptr const *,
                      TAO::TypeCode::Struct_Field<char const *, CORBA::TypeCode_ptr const *> const *,
                      TAO::Null_RefCount_Policy>
  _tao_tc_CORBA_Bounds (
    CORBA::tk_except,
    "IDL:omg.org/CORBA/Bounds:1.0",
    "Bounds",
    _tao_fields_CORBA_Bounds,
    0);
  
namespace CORBA
{
  ::CORBA::TypeCode_ptr const _tc_Bounds =
    &_tao_tc_CORBA_Bounds;
}



// TAO_IDL - Generated from 
// be\be_visitor_exception/any_op_cs.cpp:50

namespace TAO
{
  template<>
  CORBA::Boolean
  Any_Dual_Impl_T<CORBA::Bounds>::demarshal_value (
      TAO_InputCDR & cdr
    )
  {
    CORBA::String_var id;
    
    if (!(cdr >> id.out ()))
      {
        return false;
      }
    
    ACE_TRY_NEW_ENV
      {
        this->value_->_tao_decode (cdr ACE_ENV_ARG_PARAMETER);
        ACE_TRY_CHECK;
      }
    ACE_CATCHANY
      {
        return false;
      }
    ACE_ENDTRY;
    
    return true;
  }
}

// Copying insertion.
void operator<<= (
    CORBA::Any &_tao_any,
    const CORBA::Bounds &_tao_elem
  )
{
  TAO::Any_Dual_Impl_T<CORBA::Bounds>::insert_copy (
      _tao_any,
      CORBA::Bounds::_tao_any_destructor,
      CORBA::_tc_Bounds,
      _tao_elem
    );
}

// Non-copying insertion.
void operator<<= (
    CORBA::Any &_tao_any,
    CORBA::Bounds *_tao_elem
  )
{
  TAO::Any_Dual_Impl_T<CORBA::Bounds>::insert (
      _tao_any,
      CORBA::Bounds::_tao_any_destructor,
      CORBA::_tc_Bounds,
      _tao_elem
    );
}

// Extraction to non-const pointer (deprecated).
CORBA::Boolean operator>>= (
    const CORBA::Any &_tao_any,
    CORBA::Bounds *&_tao_elem
  )
{
  return _tao_any >>= const_cast<
      const CORBA::Bounds *&> (
      _tao_elem
    );
}

// Extraction to const pointer.
CORBA::Boolean operator>>= (
    const CORBA::Any &_tao_any,
    const CORBA::Bounds *&_tao_elem
  )
{
  return
    TAO::Any_Dual_Impl_T<CORBA::Bounds>::extract (
        _tao_any,
        CORBA::Bounds::_tao_any_destructor,
        CORBA::_tc_Bounds,
        _tao_elem
      );
}

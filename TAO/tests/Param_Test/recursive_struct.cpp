// $Id$

// ============================================================================
//
// = LIBRARY
//    TAO/tests/Param_Test
//
// = FILENAME
//    recursive_struct.cpp
//
// = DESCRIPTION
//    test structure that contains a sequence of itself
//
// = AUTHORS
//    Aniruddha Gokhale, Jeff Parsons
//
// ============================================================================

#include "recursive_struct.h"

ACE_RCSID(Param_Test, recursive_struct, "$Id$")

const CORBA::ULong MAX_DEPTH = 5;
const CORBA::ULong MAX_SEQ_LENGTH = 3;

// ************************************************************************
//               Test_Recursive_Struct
// ************************************************************************

Test_Recursive_Struct::Test_Recursive_Struct (void)
  : opname_ (CORBA::string_dup ("test_recursive_struct"))
{
}

Test_Recursive_Struct::~Test_Recursive_Struct (void)
{
  CORBA::string_free (this->opname_);
  this->opname_ = 0;
  // the other data members will be freed as they are "_var"s and objects
  // (rather than pointers to objects)
}

const char *
Test_Recursive_Struct::opname (void) const
{
  return this->opname_;
}

void
Test_Recursive_Struct::dii_req_invoke (CORBA::Request *req,
                                       CORBA::Environment &ACE_TRY_ENV)
{
  req->invoke (ACE_TRY_ENV);
}

int
Test_Recursive_Struct::init_parameters (Param_Test_ptr ,
                                        CORBA::Environment &)
{
  // The client calls init_parameters() before the first
  // call and reset_parameters() after each call. For this
  // test, we want the same thing to happen each time.
  return this->reset_parameters ();
}

int
Test_Recursive_Struct::reset_parameters (void)
{
  // Since these are _vars, we do this the first call and
  // every call thereafter (if any).
  this->inout_ = new Param_Test::Recursive_Struct;
  this->out_ = new Param_Test::Recursive_Struct;
  this->ret_ = new Param_Test::Recursive_Struct;

   // value generator
  Generator *gen = GENERATOR::instance ();

  // Set the depth of recursion.
  CORBA::ULong depth = (CORBA::ULong) (gen->gen_long () % MAX_DEPTH) + 1;

  // No recursion for inout_ until after the call.
  this->inout_->children.length (0);

  // Keeps Purify happy.
  this->inout_->x = 0;

  // Call the recursive helper function.
  this->deep_init (this->in_,
                   gen,
                   depth);

  return 0;
}

int
Test_Recursive_Struct::run_sii_test (Param_Test_ptr objref,
                                     CORBA::Environment &ACE_TRY_ENV)
{
  ACE_TRY
    {
      Param_Test::Recursive_Struct_out out (this->out_.out ());

      this->ret_ = objref->test_recursive_struct (this->in_,
                                                  this->inout_.inout (),
                                                  out,
                                                  ACE_TRY_ENV);
      ACE_TRY_CHECK;

      return 0;
    }
  ACE_CATCHANY
    {
      ACE_PRINT_EXCEPTION (ACE_ANY_EXCEPTION,
                           "Test_Recursive_Struct::run_sii_test\n");

    }
  ACE_ENDTRY;
  return -1;
}

int
Test_Recursive_Struct::add_args (CORBA::NVList_ptr param_list,
			                           CORBA::NVList_ptr retval,
			                           CORBA::Environment &ACE_TRY_ENV)
{
  ACE_TRY
    {
      CORBA::Any in_arg (Param_Test::_tc_Recursive_Struct,
                         &this->in_,
                         0);

      CORBA::Any inout_arg (Param_Test::_tc_Recursive_Struct,
                            &this->inout_.inout (), // .out () causes crash
                            0);

      CORBA::Any out_arg (Param_Test::_tc_Recursive_Struct,
                          &this->out_.inout (),
                          0);

      // add parameters
      param_list->add_value ("rs1",
                             in_arg,
                             CORBA::ARG_IN,
                             ACE_TRY_ENV);
      ACE_TRY_CHECK;

      param_list->add_value ("rs2",
                             inout_arg,
                             CORBA::ARG_INOUT,
                             ACE_TRY_ENV);
      ACE_TRY_CHECK;

      param_list->add_value ("rs3",
                             out_arg,
                             CORBA::ARG_OUT,
                             ACE_TRY_ENV);
      ACE_TRY_CHECK;

      // add return value
      CORBA::NamedValue *item = retval->item (0,
                                              ACE_TRY_ENV);
      ACE_TRY_CHECK;

      item->value ()->replace (Param_Test::_tc_Recursive_Struct,
                               &this->ret_.inout (), // see above
                               0, // does not own
                               ACE_TRY_ENV);
      ACE_TRY_CHECK;

      return 0;
    }
  ACE_CATCHANY
    {
      ACE_PRINT_EXCEPTION (ACE_ANY_EXCEPTION,
                           "Test_Recursive_Struct::add_args\n");

    }
  ACE_ENDTRY;
  return -1;
}


CORBA::Boolean
Test_Recursive_Struct::check_validity (void)
{
  // Pair in_ with each of the returned values and call the
  // helper function with that pair.

  if (this->deep_check (this->in_, this->inout_.in ()) == 0)
    {
      ACE_DEBUG ((LM_DEBUG,
                  "mismatch of inout arg\n"));

      return 0;
    }

  if (this->deep_check (this->in_, this->out_.in ()) == 0)
    {
      ACE_DEBUG ((LM_DEBUG,
                  "mismatch of out arg\n"));

      return 0;
    }

  if (this->deep_check (this->in_, this->ret_.in ()) == 0)
    {
      ACE_DEBUG ((LM_DEBUG,
                  "mismatch of ret value\n"));

      return 0;
    }

  // If we get this far, all is correct.
  return 1;
}

CORBA::Boolean
Test_Recursive_Struct::check_validity (CORBA::Request_ptr)
{
  return this->check_validity ();
}

void
Test_Recursive_Struct::print_values (void)
{
}

// Private helper function to recursively initialize the struct.
void
Test_Recursive_Struct::deep_init (Param_Test::Recursive_Struct &rs,
                                  Generator *gen,
                                  CORBA::ULong level)
{
  rs.x = gen->gen_long ();

  if (level == 1) 
  // No more recursion.
    {
      rs.children.length (0);

      return;
    }
  else
    {
      // Generate a sequence length.
      CORBA::ULong len = (CORBA::ULong) (gen->gen_long () % MAX_SEQ_LENGTH) + 1;

      rs.children.length (len);

      // We recurse for each element of the member sequence.
      for (CORBA::ULong i = 0; i < len; i++)
        {
          this->deep_init (rs.children[i],
                           gen,
                           level - 1);
        }
    }
}

// Private helper function for check_validity (so we can recurse).
CORBA::Boolean
Test_Recursive_Struct::deep_check (const Param_Test::Recursive_Struct &in_struct,
                                   const Param_Test::Recursive_Struct &test_struct)
{
  // Do the CORBA::Long members match? 
  if (in_struct.x != test_struct.x)
    {
      ACE_DEBUG ((LM_DEBUG,
                  "mismatch of CORBA::Long struct members\n"));

      return 0;
    }

  // Do the sequence lengths match?
  if (in_struct.children.length () != test_struct.children.length ())
    {
      ACE_DEBUG ((LM_DEBUG,
                  "mismatch of member sequence lengths\n"));

      return 0;
    }

  // At the bottom level, the length is 0 and we skip this part.
  // Otherwise recurse.
  for (CORBA::ULong i = 0; i < in_struct.children.length (); i++)
    {
      if (!this->deep_check (in_struct.children[i], 
                             test_struct.children[i]))
        {
          ACE_DEBUG ((LM_DEBUG,
                      "mismatch of contained structs\n"));

          return 0;
        }
    }

  return 1;
}

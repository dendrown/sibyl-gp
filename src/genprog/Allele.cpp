/***************************************************************************/
/**
 * MODULE: Allele.cpp
 *
 * @author Dennis Drown
 * @date   16 Aug 2011
 *
 * @copyright 2011 Dennis Drown and Osterich Ideas
 */
/***************************************************************************/

#include <cstdlib>

#include "genprog/Allele.hpp"
#include "genprog/ConstAllele.hpp"
#include "genprog/FuncAllele.hpp"
#include "genprog/LookupAllele.hpp"

namespace oi { namespace genprog {


/***************************************************************************/
/* STATIC DATA                                                             */
/***************************************************************************/
const FactoryPtr Allele::FactoryLib[] = { ConstAllele::newAllele,
                                          FuncAllele::newAllele,
                                          LookupAllele::newAllele
                                        };


/***************************************************************************/
/* PUBLIC CLASS METHODS                                                    */
/***************************************************************************/

// --------------------------------------------------------------------------
// CONSTRUCTOR:
// --------------------------------------------------------------------------
/**
 * Creates an empty Allele
 *
 * @param parent    Pointer to the parent node, if known   (DEFAULT: NULL)
 * @param func      Pointer to the function node, if known (DEFAULT: NULL)
 */
// --------------------------------------------------------------------------
Allele::Allele(Allele *parent) :    nodeCnt_ (1     ),
                                    parent_  (parent)

{ }


// --------------------------------------------------------------------------
// CONSTRUCTOR:
// --------------------------------------------------------------------------
/**
 * Creates an Allele by copying another...except that the Allele member data
 * in this, the base class, is distinct, so we're really just creating a new
 * Allele.
 *
 * @param that  The Allele to copy
 */
// --------------------------------------------------------------------------
Allele::Allele(const Allele& that) :    nodeCnt_ (1   ),    // Recalculated in subclass
                                        parent_  (NULL)
{ }


// --------------------------------------------------------------------------
// DESTRUCTOR:
// --------------------------------------------------------------------------
/**
 * Destroys the object and frees up associated memory
 */
// --------------------------------------------------------------------------
Allele::~Allele()
{
    // Don't delete the node pointer because we don't own it,
    // but NULL it out in case we try to access it after deletion
    parent_ = NULL;
}


// --------------------------------------------------------------------------
// calcNodeCount:
// --------------------------------------------------------------------------
/**
 * Returns the sum of all the nodes in this Allele.
 *
 * @note    This method is meant for debugging. @ref getNodeCount() is the
 *          preferred method for obtaining the node sum.
 *
 * @return  The count of nodes in this Allele and all sub-alleles.
 */
// --------------------------------------------------------------------------
u_int Allele::__calcNodeCount() const
{
    return 1;
}


} } // ns{ oi::genprog }

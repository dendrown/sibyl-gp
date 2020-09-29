/***************************************************************************/
/**
 * MODULE: ConstAllele.cpp
 *
 * @author Dennis Drown
 * @date   19 Aug 2011
 *
 * @copyright 2011 Dennis Drown and Osterich Ideas
 */
/***************************************************************************/

#include <cstdlib>
#include <iostream>

#if defined(MEMPOOLS)
#include <boost/pool/singleton_pool.hpp>
#endif

#include "genprog/ConstAllele.hpp"

namespace oi { namespace genprog {

using namespace std;

/***************************************************************************/
/* CONSTANTS                                                               */
/***************************************************************************/
#define CFG_CHANCE_POSITIVE 0.50    ///< INI! Chance a sliding random value will be positive
#define CFG_CHANCE_WHOLE    0.75    ///< INI! Chance a sliding random value will be a whole number
#define CFG_PRINT_PRECISION 3       ///< INI! Decimal places to show when printing/logging


/***************************************************************************/
/* TYPE DEFINITIONS                                                        */
/***************************************************************************/

#if defined(MEMPOOLS)

struct ConstAlleleMemPoolTag { };
typedef boost::singleton_pool<ConstAlleleMemPoolTag, sizeof(ConstAllele), boost::default_user_allocator_malloc_free> ConstAlleleMemPool;

#endif // MEMPOOLS



/***************************************************************************/
/* PUBLIC CLASS METHODS                                                    */
/***************************************************************************/

// --------------------------------------------------------------------------
// CONSTRUCTOR:
// --------------------------------------------------------------------------
/**
 * Creates a ConstAllele
 *
 * @param val   The value this constant allele will represent
 */
// --------------------------------------------------------------------------
ConstAllele::ConstAllele(Real val) : Value(val)
{ }


// --------------------------------------------------------------------------
// CONSTRUCTOR:
// --------------------------------------------------------------------------
/**
 * Creates a ConstAllele
 *
 * @param that  The object we're creating a copy of
 */
// --------------------------------------------------------------------------
ConstAllele::ConstAllele(const ConstAllele& that) : Value(that.Value)
{ }



#if defined(MEMPOOLS)
// ---------------------------------------------------------------- STATIC --
// operator new:
// --------------------------------------------------------------------------
/**
 * Allocates memory for a new ConstAllele
 *
 * @param size  Number of bytes requested, should equal sizeof(ConstAllele)
 *
 * @return      Raw allocated memory ready for constructor
 */
// --------------------------------------------------------------------------
void * ConstAllele::operator new(size_t size)
{
    // Check size to make sure we should handle this
    if(size != sizeof(ConstAllele))
    {
        return ::operator new(size);
    }

    // Loop to handle out-of-memory conditions
    while(true)
    {
        void *p = ConstAlleleMemPool::malloc();

        if(p)
        {
            return p;
        }

        // Handle out of memory conditions
        new_handler globalHandler = set_new_handler(0);

        set_new_handler(globalHandler);

        if(globalHandler)   (*globalHandler)();
        else                throw bad_alloc();
    }
}


// ---------------------------------------------------------------- STATIC --
// operator delete:
// --------------------------------------------------------------------------
/**
 * Deallocates memory for a used up ConstAllele.  Well, we don't actually
 * return the memory to the heap (until the program completes), we keep it
 * in an always-allocated memory pool for fast new/delete operations.
 *
 * @param size  Number of bytes to be freed, should equal sizeof(ConstAllele)
 *
 * @return      Raw allocated memory ready for constructor
 */
// --------------------------------------------------------------------------
void ConstAllele::operator delete(void *p, size_t size)
{
    if(p)
    {
        if(size != sizeof(ConstAllele)) ::operator delete(p);           // Global handler for weird stuff
        else                            ConstAlleleMemPool::free(p);    // Normal pooled object deallocation
    }
}

#endif // MEMPOOLS


/***************************************************************************/
/* PUBLIC FRIENDS                                                          */
/***************************************************************************/

// --------------------------------------------------------------------------
// operator <<:
// --------------------------------------------------------------------------
/**
 * Output stream operator for a ConstAllele
 *
 * @param   out     The output stream (cout maybe)
 * @param   allele  The allele to show
 *
 * @return          The same output stream for chaining
 */
// --------------------------------------------------------------------------
ostream& operator <<(ostream& out, const ConstAllele& allele)
{
    if(out.good())
    {
        //ios_base::fmtflags origFlags     = out.flags(ios::fixed | ios::showpoint);
        //streamsize         origPrecision = out.precision(CFG_PRINT_PRECISION);

        out << allele.Value;

        // Clean up
        //out.flags(origFlags);
        //out.precision(origPrecision);
    }

    return out;
}


/***************************************************************************/
/* PRIVATE CLASS METHODS                                                   */
/***************************************************************************/

// --------------------------------------------------------------------------
// getRandFunc:
// --------------------------------------------------------------------------
/**
 * Returns a random GP function from the library.
 *
 * @return      The random GP function
 */
// --------------------------------------------------------------------------
Real ConstAllele::getSlidingReal()
{
    double isPos   = drand48();
    double isWhole = drand48();
    double howBig  = drand48();
    int    wholePart;

    // Get the integer part...sliding scale on how big it is
    wholePart = lrand48();
    if     (howBig < 0.50000000)    wholePart %= 10;            // Half chance of 0..10
    else if(howBig < 0.75000000)    wholePart %= 100;           // 3/4  chance of 0..100
    else if(howBig < 0.87500000)    wholePart %= 1000;          // 7/8  chance
    else if(howBig < 0.93750000)    wholePart %= 10000;         // 15/16
    else if(howBig < 0.96875000)    wholePart %= 100000;        // 31/32
    else if(howBig < 0.98437500)    wholePart %= 1000000;       // 63/64
    else if(howBig < 0.99218750)    wholePart %= 10000000;      // 127/128
    else if(howBig < 0.99609375)    wholePart %= 100000000;     // 255/256
    else                            wholePart %= 1000000000;

    if(isPos < CFG_CHANCE_POSITIVE) wholePart *= -1;

    return (isWhole < CFG_CHANCE_WHOLE) ? (Real) wholePart                  // Real whole number
                                        : (Real) wholePart + drand48();     // Real decimal number
}



} } // ns{ oi::genprog }

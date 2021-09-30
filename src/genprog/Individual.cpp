/***************************************************************************/
/**
 * MODULE: Individual.cpp
 *
 * @author Dennis Drown
 * @date   30 Aug 2011
 *
 * @copyright 2011 Dennis Drown and Ostrich Ideas
 */
/***************************************************************************/

#include <cassert>
#include <cstdlib>
#include <iostream>

#include <boost/pool/singleton_pool.hpp>

#include "genprog/Individual.hpp"
#include "genprog/World.hpp"


namespace oi { namespace genprog {

using namespace std;


/***************************************************************************/
/* CONSTANTS                                                               */
/***************************************************************************/


/***************************************************************************/
/* TYPE DEFINITIONS                                                        */
/***************************************************************************/
#if 0   //defined(MEMPOOL_INDIVIDUAL)

struct MemPoolTag { };
typedef boost::singleton_pool<MemPoolTag, sizeof(Individual), boost::default_user_allocator_malloc_free> MemPool;

#endif // MEMPOOL_INDIVIDUAL


/***************************************************************************/
/* PUBLIC CLASS METHODS                                                    */
/***************************************************************************/

// --------------------------------------------------------------------------
// CONSTRUCTOR:
// --------------------------------------------------------------------------
/**
 * Creates a new Individual with a no-op genetic program.  He's really just
 * a zombie, and you shouldn't use this constructor if you can help it.
 */
// --------------------------------------------------------------------------
Individual::Individual() :  chromosome_ (),
                            isDead_     (false),
                            isSick_     (true),
                            fitness_    (FITNESS_UNFIT)
{ }

// --------------------------------------------------------------------------
// CONSTRUCTOR:
// --------------------------------------------------------------------------
/**
 * Creates a new Individual by copying another
 *
 * @param that  The object we're copying
 */
// --------------------------------------------------------------------------
Individual::Individual(const Individual& that)
:    chromosome_ (that.chromosome_),
     isDead_     (that.isDead_),
     isSick_     (that.isSick_),
     fitness_    (FITNESS_UNFIT)
{

}

// --------------------------------------------------------------------------
// CONSTRUCTOR:
// --------------------------------------------------------------------------
/**
 * Creates a new Individual
 *
 * @param world     The GP world this individual will be part of
 */
// --------------------------------------------------------------------------
Individual::Individual(const World& world)
:    chromosome_ (world),
     isDead_     (false),
     isSick_     (false),
     fitness_    (FITNESS_UNFIT)
{ }


// --------------------------------------------------------------------------
// CONSTRUCTOR:
// --------------------------------------------------------------------------
/**
 * Creates a new Individual
 *
 * @param world     The GP world this individual will be part of
 */
// --------------------------------------------------------------------------
Individual::Individual(const World& world, const string& func)
:    chromosome_ (world, func),
     isDead_     (false),
     isSick_     (false),
     fitness_    (FITNESS_UNFIT)
{ }



#if 0   //defined(MEMPOOL_INDIVIDUAL)
// ---------------------------------------------------------------- STATIC --
// operator new:
// --------------------------------------------------------------------------
/**
 * Allocates memory for a new individual
 *
 * @param size  Number of bytes requested, should equal sizeof(Individual)
 *
 * @return      Raw allocated memory ready for constructor
 */
// --------------------------------------------------------------------------
void * Individual::operator new(size_t size)
{
    // Check size to make sure we should handle this
    if(size != sizeof(Individual))
    {
        return ::operator new(size);
    }

    // Loop to handle out-of-memory conditions
    while(true)
    {
        void *p = MemPool::malloc();

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
 * Deallocates memory for a used up Individual.  Well, we don't actually
 * return the memory to the heap (until the program completes), we keep it
 * in an always-allocated memory pool for fast new/delete operations.
 *
 * @param size  Number of bytes to be freed, should equal sizeof(Individual)
 *
 * @return      Raw allocated memory ready for constructor
 */
// --------------------------------------------------------------------------
void Individual::operator delete(void *p, size_t size)
{
    if(p)
    {
        if(size != sizeof(Individual))  ::operator delete(p);    // Global handler for weird stuff
        else                            MemPool::free(p);           // Normal pooled object deallocation
    }
}

#endif // MEMPOOL_INDIVIDUAL


// --------------------------------------------------------------------------
// mate:
// --------------------------------------------------------------------------
/**
 * Creates a two new Individuals using crossover.
 *
 * @param he        The individual we're mating with (We assume this is "she".)
 * @param crib      Vector where we'll put the offspring
 * @param babyNdx1  Index for the first child
 * @param babyNdx2  Index for the second child
 *
 * @return  true if we could produce two offspring, false otherwise
 */
// --------------------------------------------------------------------------
bool Individual::mate(const Individual_p& he,
                      Individual_Vp&      crib,
                      u_int               babyNdx1,
                      u_int               babyNdx2,
                      Real                mutationRate) const
{
    bool gaveBirth = false;

    assert(canReproduce());
    assert(he->canReproduce());

    /*~ DEBUG ~*
    string xxChromo = toString();
    cout << "MOM: " << xxChromo      << endl;
    cout << "DAD: " << he.toString() << endl;

    if(xxChromo == "(SUB 3.85202 (INV close[16]))")
    {
        cout << "*** THIS ONE PUKES..." << endl;
    }
    **/

    // Start by cloning the parents
    Individual_p baby1 = make_unique<Individual>(*this);

    if(baby1)
    {
        Individual_p  baby2 = make_unique<Individual>(*he);
        if(baby2)
        {
            FuncAllele::crossover(baby1->chromosome_, baby2->chromosome_);

            /*~ DEBUG ~*
            cout << "baby1: " << baby1->toString() << endl;
            cout << "baby2: " << baby2->toString() << endl;
            **/

            if(drand48() < mutationRate)
            {
                baby1->mutate();
            }
            crib[babyNdx1] = move(baby1);

            // Make sure two slots are available in the crib
            if(babyNdx1 != babyNdx2)
            {
                // Good! Room for two babies
                if(drand48() < mutationRate)
                {
                    baby2->mutate();
                }
                crib[babyNdx2] = move(baby2);
            }
            else
            {
                // Oh well, throw 2nd baby in the dumpster!
                //
                // NOTE: I've tried to do some enhancements to FuncAllele::crossover() and Splice::splice() to
                //       not require a second baby if we're going to dump it (use const dad's chromosomes), or
                //       to not do the actual gene splice on baby2.  It creates a spaghetti bowl of issues, so
                //       I've put things back and we're creating the second baby and immediately deleting it.
                //
                //       We probably only want to try to tame this if profiling shows that something is really
                //       expensive here.  Remember that we only encounter this scenario once per generation and it
                //       can be avoided altogether if the Tournament size divides evenly into the Population size.
                //
                // Alas, with the baby being a unique_ptr, we now no longer need to delete it, but I want to
                // keep my favourite comment in all my history of programming...for the time being.
                //
                // [C++14] delete baby2;
            }
        }
        else
        {
            cerr << __FUNCTION__ << ": Out of memory";
        }
    }
    else cerr << __FUNCTION__ << ": Out of memory";


//    u_int   mySplitNodeCnt  = rand() % getGeneNodeCnt();
//    u_int   hisSplitNodeCnt = rand() % he.getGeneNodeCnt();

    return gaveBirth;
}

/***************************************************************************/
/* PUBLIC FRIENDS                                                          */
/***************************************************************************/

// --------------------------------------------------------------------------
// operator <<:
// --------------------------------------------------------------------------
/**
 * Output stream operator for an Individual
 *
 * @param   out     The output stream (cout maybe)
 * @param   allele  The allele to show
 *
 * @return          The same output stream for chaining
 */
// --------------------------------------------------------------------------
ostream& operator <<(ostream& out, const Individual& rhs)
{

    if(out.good())
    {
        out << rhs.toString();
    }

    return out;
}


/***************************************************************************/
/* PRIVATE METHODS                                                         */
/***************************************************************************/




} } // ns{ oi::genprog }

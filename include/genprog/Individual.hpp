/***************************************************************************/
/**
 * MODULE: Individual.hpp
 *
 * @author Dennis Drown
 * @date   28 Aug 2011
 *
 * @copyright 2011 Dennis Drown and  Osterich Ideas
 */
/***************************************************************************/
#ifndef INDIVIDUAL_HPP
#define	INDIVIDUAL_HPP

#include <limits>
#include <new>
#include <vector>

#include "genprog/genprog.hpp"
#include "genprog/FuncAllele.hpp"


namespace oi { namespace genprog {

#if defined(MEMPOOLS)
#define MEMPOOL_INDIVIDUAL
#endif

class Individual;
using Individual_p  = std::unique_ptr<Individual>;
using Individual_P  = std::shared_ptr<Individual>;

using Individual_Vp = std::vector<Individual_p>;
using Individual_VP = std::vector<Individual_P>;

// --------------------------------------------------------------------------
// Individual:
// --------------------------------------------------------------------------
/**
 * An indexed lookup system of a stream of data values
 */
// --------------------------------------------------------------------------
class Individual
{
public:

    Individual();
    Individual(const Individual& that);
    Individual(const World& world);
    Individual(const World& world, const std::string& func);
    ~Individual() { };

    friend std::ostream& operator <<(std::ostream& out, const Individual& rhs);

#if 0   //defined(MEMPOOL_INDIVIDUAL)
    static void * operator new(size_t size);
    static void   operator delete(void *p, size_t size);
#endif

    const std::string toString()                                    const;
    const FuncAllele& getChromosome()                               const;

    Real            getFitness()                                    const;
    u_int           getChromoNodeCnt()                              const;
    Real            getChromoValue(const AttrWindow& win)           const;
    GPFuncResult    execChromosome(const AttrWindow& win)           const;
    bool            isDead()                                        const;
    bool            isSick()                                        const;
    bool            canReproduce()                                  const;

    bool            mate(const Individual_p& he,
                         Individual_Vp&      crib,
                         u_int               babyNdx1,
                         u_int               babyNdx2,
                         Real                mutationRate = 0.0)    const;
    void            mutate();
    void            setFitness(Real fitness);
    void            setIsDead(bool yesNo);
    void            setIsSick(bool yesNo);


private:
    Individual & operator=(const Individual& rhs);      ///< DISABLED!

    FuncAllele  chromosome_;    ///< GP function representing the tree of guy's genes
    bool        isDead_;        ///< Will be removed from population (no reproduction either)
    bool        isSick_;        ///< Cannot take part in reproduction this round
    Real        fitness_;       ///< Fitness Score, once calculated

};


// --------------------------------------------------------------------------
// toString:
// --------------------------------------------------------------------------
/**
 * String representation of the Allele
 *
 * @return      The node count for this Allele
 */
// --------------------------------------------------------------------------
inline const std::string Individual::toString() const
{
    return chromosome_.toString();
}


// --------------------------------------------------------------------------
// getFitness:
// --------------------------------------------------------------------------
/**
 * Returns the fitness value for this Individual, once calculated.  If the
 * fitness has not yet been calculated, or if the Individual evaluates to
 * dead or sick, then this method returns @ref FITNESS_UNFIT.
 *
 * @return      The individual's fitness, or FITNESS_UNFIT if no fitness
 *              score is available.
 */
// --------------------------------------------------------------------------
inline Real Individual::getFitness() const
{
    return fitness_;
}


// --------------------------------------------------------------------------
// getChromosome:
// --------------------------------------------------------------------------
/**
 * Returns the GP Function Allele that is this Individual's genes
 *
 * @return      The gene (top) FuncAllele
 */
// --------------------------------------------------------------------------
inline const FuncAllele& Individual::getChromosome() const
{
    return chromosome_;
}



// --------------------------------------------------------------------------
// getGeneNodeCnt:
// --------------------------------------------------------------------------
/**
 * Returns the count of the total nodes in this individual's genes
 *
 * @return      The gene node count for this individual
 */
// --------------------------------------------------------------------------
inline u_int Individual::getChromoNodeCnt() const
{
    return chromosome_.getNodeCnt();
}


// --------------------------------------------------------------------------
// getChromoValue:
// --------------------------------------------------------------------------
/**
 * Returns the value of the chromosome, which is the value of the GP function
 * this individual represents.
 *
 * @param win   Attribute window (thread specific)
 *
 * @return      The function value of this individual
 */
// --------------------------------------------------------------------------
inline Real Individual::getChromoValue(const AttrWindow& win) const
{
    return chromosome_.getValue(win);
}


// --------------------------------------------------------------------------
// execChromosome:
// --------------------------------------------------------------------------
/**
 * Returns the value of the chromosome, which is the value of the GP function
 * this individual represents.
 *
 * @param win   Attribute window (thread specific)
 *
 * @return      The function value of this individual
 */
// --------------------------------------------------------------------------
inline GPFuncResult Individual::execChromosome(const AttrWindow& win) const
{
    return chromosome_.exec(win);
}


// --------------------------------------------------------------------------
// isSick:
// --------------------------------------------------------------------------
/**
 * Returns true if the individual is considered sick
 *
 * @return      True, if this guy is not well
 */
// --------------------------------------------------------------------------
inline bool Individual::isSick() const
{
    return isSick_;
}


// --------------------------------------------------------------------------
// isDead:
// --------------------------------------------------------------------------
/**
 * Returns true if the individual has died (usually because his function
 * exploded).
 *
 * @return      True, if this guy is no more
 */
// --------------------------------------------------------------------------
inline bool Individual::isDead() const
{
    return isDead_;
}


// --------------------------------------------------------------------------
// canReproduce:
// --------------------------------------------------------------------------
/**
 * Returns true if the individual is healthy and can reproduce
 *
 * @return      True, if this guy is ready to boogy
 */
// --------------------------------------------------------------------------
inline bool Individual::canReproduce() const
{
    return !(isDead_ || isSick_);
}


// --------------------------------------------------------------------------
// mutate:
// --------------------------------------------------------------------------
/**
 * Mutates a gene in this individual's chromosome.
 */
// --------------------------------------------------------------------------
inline void Individual::mutate()
{
    chromosome_.mutate();
}


// --------------------------------------------------------------------------
// setFitness:
// --------------------------------------------------------------------------
/**
 * Sets a fitness value for this individual.
 *
 * @param fitness   The individual's fitness rating (usually as calculated
 *                  by the World object which owns this Individual)
 */
// --------------------------------------------------------------------------
inline void Individual::setFitness(Real fitness)
{
    fitness_ = fitness;
}


// --------------------------------------------------------------------------
// setIsDead:
// --------------------------------------------------------------------------
/**
 * Set indicator that this individual has died
 *
 * @param yesNo Set to true if he died; false if he lives
 */
// --------------------------------------------------------------------------
inline void Individual::setIsDead(bool yesNo)
{
    isDead_ = yesNo;
}


// --------------------------------------------------------------------------
// setIsSick:
// --------------------------------------------------------------------------
/**
 * Set indicator that this individual is alive, but not well
 *
 * @param yesNo Set to true if he is unwell; false if he is healthy
 */
// --------------------------------------------------------------------------
inline void Individual::setIsSick(bool yesNo)
{
    isSick_ = yesNo;
}



} } // ns{ oi::genprog }


#endif	/* INDIVIDUAL_HPP */


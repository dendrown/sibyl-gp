/*\***********************************************************************\*//**
 * MODULE: ostrich.hpp
 *
 * @author Dennis Drown
 * @date   21 Aug 2011
 *
 * @copyright 2012-2017 Dennis Drown and Ostrich Ideas
 *//**********************************************************************\*///
#ifndef OSTRICH_HPP
#define	OSTRICH_HPP

#include <sys/types.h>

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

// Placate non C++11 compilers where possible
#if __cplusplus <= 199711L
#   define constexpr
#endif

namespace oi {

// --------------------------------------------------------------------------
// PROJECTS:
// --------------------------------------------------------------------------
#if defined(OI_SIBYL)
#   define OI_XTN_GENPROG
#   define OI_XTN_MARKET
#endif

// --------------------------------------------------------------------------
// TYPE DEFINITIONS:
// --------------------------------------------------------------------------
#define GP_REAL_PRECISION_DOUBLE    ///< Always use double. Don't undefine this line.
#ifndef GP_REAL_PRECISION_DOUBLE
#   define  GP_REAL_PRECISION_FLOAT ///< Use float, unless double specifically requested
#endif

#if defined(GP_REAL_PRECISION_DOUBLE)
    typedef double Real;
#else
    typedef float Real;                 ///< Never use float, MAXFLOAT isn't high enough!
#endif


/*/- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- **\*/
// ⣏⡉ ⣏⡱ ⢎⡑ ⡇ ⡇  ⡎⢱ ⡷⣸
// ⠧⠤ ⠇  ⠢⠜ ⠇ ⠧⠤ ⠣⠜ ⠇⠹
/*/- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- *//**
 * Wiggle room constant for comparisons between Real (float|double) numbers.
 *///-------------------------------------------------------------------------
constexpr Real EPSILON = static_cast<Real>(6.2e-12);


#ifndef foreach_
#define foreach_        BOOST_FOREACH
#endif

#ifndef foreach_r_
#define foreach_r_      BOOST_REVERSE_FOREACH
#endif


/*/- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- **\*/
//  ⠄ ⢀⣀ ⢇⢸ ⢀⡀ ⢀⣀
//  ⠇ ⠭⠕  ⠇ ⠣⠭ ⠭⠕
/*/- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- *//**
 * Basic str2bool function with a catchier name.  Parses a string to determine
 * if represents a true value { 1, Y, YES, TRUE } without regard to case.
 *
 * @return  true if we have an affirmative word, false otherwise 
 *///-------------------------------------------------------------------------
inline bool isYes(const std::string& s)
{
    return (s == "1")                   ||
           boost::iequals(s, "1")       ||
           boost::iequals(s, "Y")       ||
           boost::iequals(s, "T")       ||
           boost::iequals(s, "YES")     ||
           boost::iequals(s, "TRUE");
}


/*/- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- **\*/
// ⢀⡀ ⢀⡀ ⣰⡀ ⣏⡱ ⢀⣀ ⣀⡀ ⡇⡠ ⡎⠑ ⢀⡀ ⢀⣸ ⢀⡀
// ⣑⡺ ⠣⠭ ⠘⠤ ⠇⠱ ⠣⠼ ⠇⠸ ⠏⠢ ⠣⠔ ⠣⠜ ⠣⠼ ⠣⠭
/*/- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- *//**
 * Returns a one character code for ranking. Basically it's a base62
 * character.  Intended for MPI machine rank, but can be used for anything.
 *
 * @param rank  Rank integer to encode
 *
 * @return      Char code for the specified rank, or '#' if the rank is >= 62
 *///-------------------------------------------------------------------------
inline char getRankCode(int rank)
{
    int code = '#';

    if     (rank < 10) code = '0' + rank;
    else if(rank < 36) code = 'A' + (rank - 10);
    else if(rank < 62) code = 'a' + (rank - 36);

    return code;
}

} // ns{ oi }

#endif	/* OSTRICH_HPP */


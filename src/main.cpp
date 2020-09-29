/************************************************************************//**
 * MODULE: main.cpp
 *
 * @author Dennis Drown
 * @date   16 Aug 2011
 *
 * @copyright 2011-2016 Dennis Drown and Ostrich Ideas
 *//************************************************************************/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <queue>
#include <unistd.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <boost/algorithm/string/split.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/thread.hpp>

#include "sibyl.hpp"
#include "oi-conf.hpp"
#include "oi-cluster.hpp"
#include "oi-string.hpp"
#include "genprog/test.hpp"
#include "market/Delphi.hpp"
#include "market/PriceDataPack.hpp"
#include "market/PriceWorld.hpp"
#include "market/Prognosticator.hpp"
#include "market/SecurityPack.hpp"
#include "util/Logger.hpp"
#include "util/HumanClock.hpp"
#include "util/CPUClock.hpp"

#if ENABLE_CUDA
#include "gpgpu-nv.hpp"
#endif

#ifndef SYSCONFDIR
#  define SYSCONFDIR "."
#endif
#define CFGDEF_CFG_FILEPATH     SYSCONFDIR "/sibyl.conf"

namespace B   = boost;
namespace dts = boost::gregorian;

using namespace std;
using namespace oi;
using namespace oi::genprog;
using namespace oi::market;
using namespace oi::util;

#define MPI_TAG_JOB_REQ     1001
#define MPI_TAG_JOB_RSP     1002

// --------------------------------------------------------------------------
// module Data Structures:
// --------------------------------------------------------------------------
/**
 * The jobs that need to be done. The MPI root will serve these out as nodes
 * become available.
 *
 * @note    Getting to be time to promote this to a class.
 */
struct ToDoQueue
{
    /**
     * Job numbers are non-negative and starting from zero.
     */
    enum JobCode
    {
        ERROR    = -1,
        INIT     = -1000,
        SYNC     = -1080,
        DONE     = -1090
    };

    // MEMBER DATA
    queue<int> taskQ_;
    bool       allDone_;
    B::mutex   lock_;

    /**
     * Gives text names to our job codes
     */
    static const char * jobCodeText(int code)
    {
        switch(code)
        {
            case ERROR: return "ERROR";
            case INIT:  return "INIT";
            case SYNC:  return "SYNC";
            case DONE:  return "DONE";
            default:    return "!UNKNOWN!";
        }
    }


    /**
     * Constructor for something new TO-DO
     */
    ToDoQueue() : allDone_(false)
    {}


    /**
     * Common start-up stuff
     */
    void init()
    {
        B::lock_guard<B::mutex> guard(lock_);

        // The queue should really be empty, but just in case...
        while(!taskQ_.empty())
        {
            taskQ_.pop();
        }
        allDone_ = false;
    }


    /**
     * Make-it-stop stuff
     */
    void shutdown()
    {
        B::lock_guard<B::mutex> guard(lock_);
        allDone_ = true;
    }


    /**
     * Returns the next task on the to-do list, or codeOnEmpty if the work is done.
     */
    int getTask(JobCode codeOnEmpty = JobCode::DONE)
    {
        int task = JobCode::ERROR;
        B::lock_guard<B::mutex> guard(lock_);

        if (!allDone_)
        {
            if(!taskQ_.empty())
            {
                task = taskQ_.front();
                taskQ_.pop();
            }
            else
            {
                allDone_ = true;
                task     = codeOnEmpty;
            }
        }
        else task = codeOnEmpty;

        return task;
    }

    /**
     * Returns how many tasks are left TO-DO.
     */
    inline size_t size()
    {
        return taskQ_.size();
    }

};  // ToDoQueue


// --------------------------------------------------------------------------
// Module Global Data:
// --------------------------------------------------------------------------
static const char *APP_VERSION = PACKAGE_VERSION;       ///< Sibyl program version

static string CFG_CFG_FILEPATH(CFGDEF_CFG_FILEPATH);    ///< CLI: Location of config file
static string CFG_LOG_FILESTUB("");                     ///< CLI: /path/to/log/stub
                                                        ///<      ("" means stdout)
static string CFG_INSERT_BEST_GENS("");                 ///< CLI: CSV of generations when
                                                        ///<      we should insert previous
                                                        ///<      best models
static string CFG_SECURITIES("*");                      ///< CLI: CSV of securities (stock
                                                        ///<      symbols) to run
static string CFG_SEEDS_48("");                         ///< CLI: CSV of three unsigned
                                                        ///<      shorts for RNG, or ""
                                                        ///<      "" for random seeds
static int    CFG_RUN_TEST      = -1;                   ///< CLI: Run this test number and
                                                        ///<      exit (-1 means none)
static int    CFG_MASTER_NODE   = -1;                   ///< CLI: Node that directs the
                                                        ///<      others. -1 means last one
                                                        ///<      in machines file
static int    CFG_MAX_ERRORS    = 1;                    ///< CLI: Maximum errors before
                                                        ///<      throwing in the towel
static int    CFG_DAYS_TO_PULL  = (8 * 30);             ///< CLI: Number of days (records)
                                                        ///<      to pull from DB per run
static int    CFG_DAYS_IN_WIN   = 90;                   ///< CLI: Number of day in the
                                                        ///<      sliding compute window
                                                        ///<      (for creating model)
static bool   CFG_USE_XSEC_DIA  = false;                ///< CLI: Whether to use DIA
                                                        ///<      (Dow Jones) ETF as an
                                                        ///<      extra security attribute
static bool   CFG_USE_XSEC_GLD  = false;                ///< CLI: Whether to use GLD (gold)
                                                        ///<      ETF as an extra attribute
#if ENABLE_CUDA
static bool   CFG_MIRROR_GPU    = false;                ///< CLI: Whether to use the GPU to
                                                        ///<      mirror CPU work to check
                                                        ///<      that both get the same
                                                        ///<      results.
#endif
static bool   IsLoner           = false;                ///< Are we the only node running?

static ToDoQueue *ToDos = NULL;         ///< List of jobs to process. Only used on MPI root

/**
 * Delphi model version:
 *  - A0: Original text <--> code
 *  - A1: Remove adjClose attribute
 *  - A2: Add price/earnings
*/
static const string MODEL_CODE("A1");


// --------------------------------------------------------------------------
// Project Global Data:
// --------------------------------------------------------------------------
namespace oi {

const string OK_("!OK!");
const string ERROR_("!ERR!");

string CFG_DB_HOST("localhost");                ///< INI: Database server running delphi
string CFG_DB_USER("pythia");                   ///< INI: Database user with rights to delphi

} // ns{ oi }


// --------------------------------------------------------------------------
// MPI Declarations:
// --------------------------------------------------------------------------
BOOST_IS_BITWISE_SERIALIZABLE(char);
BOOST_IS_BITWISE_SERIALIZABLE(int);
BOOST_IS_BITWISE_SERIALIZABLE(long);
BOOST_IS_BITWISE_SERIALIZABLE(double);


// --------------------------------------------------------------------------
// initRand48:
// --------------------------------------------------------------------------
/**
 * Seeds the rand48 RNG family with the value specified in the configuration,
 * or with random values, based on system entropy, if no seed array was
 * specified.
 *
 * @warning     Any threads other than the main thread which use rand48
 *              functions should use reentrant versions.  Basically Sibyl
 *              isn't currently doing and random operations on her work
 *              threads, but this changes, we'll have to address RNGs
 *              again.
 *
 * @param out   Output stream for logging
 */
// --------------------------------------------------------------------------
static void initRand48(ostream& out)
{
    using namespace boost;

    unsigned short seeds[3];
    vector<string> cfgSeeds;
    int            numCfgSeeds = 0;

    // Specify any seeds in the configuration??
    if(!CFG_SEEDS_48.empty())
    {
        cfgSeeds.reserve(3);
        split(cfgSeeds, CFG_SEEDS_48, is_any_of(", "), token_compress_on);
        numCfgSeeds = cfgSeeds.size();
    }

    // First choice is config
    if(numCfgSeeds >= 3)
    {
        seeds[0] = lexical_cast<unsigned short>(cfgSeeds[0]);
        seeds[1] = lexical_cast<unsigned short>(cfgSeeds[1]);
        seeds[2] = lexical_cast<unsigned short>(cfgSeeds[2]);
    }
    else
    {
        // Second choice: system entropy
        int sysRNG = open("/dev/urandom", O_RDONLY);

        if(sysRNG >= 0)
        {
            ssize_t rd_rc;
            rd_rc = read(sysRNG, &seeds[0], sizeof(unsigned short));    assert(rd_rc > 0);
            rd_rc = read(sysRNG, &seeds[1], sizeof(unsigned short));    assert(rd_rc > 0);
            rd_rc = read(sysRNG, &seeds[2], sizeof(unsigned short));    assert(rd_rc > 0);
            close(sysRNG);                                              ((void)rd_rc);
        }
        else
        {
            // Third choice: random function (still better than rand)
            srandom(time(NULL));
            seeds[0] = (unsigned short) random();
            seeds[1] = (unsigned short) random();
            seeds[2] = (unsigned short)(random() + time(NULL));
        }
    }

    // Now seed that dude!
    seed48(seeds);
    out << LOG_NOTICE << "RNG { " << seeds[0] << "," << seeds[1] << "," << seeds[2] << " }" << endl;

    // An afterthought...
    //
    // HumanClock still uses random() for non-critical timing
    srandom(time(NULL));
}


/*/- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- **\*/
// ⢀⣸ ⢀⡀ ⡎⠑ ⡇  ⡇ ⡎⢱ ⣀⡀ ⣰⡀ ⠄ ⢀⡀ ⣀⡀ ⢀⣀
// ⠣⠼ ⠣⠜ ⠣⠔ ⠧⠤ ⠇ ⠣⠜ ⡧⠜ ⠘⠤ ⠇ ⠣⠜ ⠇⠸ ⠭⠕
/*/- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- *//**
 * Sets up command line configuration options, collects them, and processes
 * them.
 *
 * @note    CLI options should be processed (with this function) before
 *          config file options so that they take precedence.
 *
 * @warning This function may not return (on --help or --version)
 *
 * @param argc  Command line argument count
 * @param argv  Command line arguments
 * @param descr Command line Options descriptions (output, we fill it in here)
 * @param cfg   Configuration var-map with the CLI options in it (output)
 *
 *///-------------------------------------------------------------------------
static void doCLIOptions(int argc, char **argv, ConfInfo& descr, ConfMap& cfg)
{
    using namespace ini;

    descr.add_options()
            ("config,c",         value<string>(), "Use this configuration file (default: " CFGDEF_CFG_FILEPATH ")")
            ("days-to-pull",     value<int>(),    "Days (records) to pull from the delphi DB per run")
            ("days-in-win",      value<int>(),    "Days in sliding GP compute window")
            ("db-host",          value<string>(), "Database server with Delphi")
            ("db-user",          value<string>(), "Database user for Delphi")
            ("discrete,d",                        "Do not save generated models in delphi")
            ("eager,E",                           "Skip cool-down break between partial runs")
            ("help,?",                            "Display this handy help text")
            ("insert-best-gens", value<string>(), "CSV list of generations when Sibyl should insert previous"
                                                  " \"best\" models")
            ("log-stub",         value<string>(), "Logs output to /path/log-stub.YYYYMMDD.log")
            ("master-node",      value<int>(),    "MPI node number responsible for managing the others (does"
                                                  " no real work)."
                                                  " Defaults to -1, which means the last node")
            ("max-errors",       value<int>(),    "Maximum number of errors before throwing in the towel")
#if ENABLE_CUDA
            ("mirror-gpu",                        "Do work on CPU and GPU to compare (debug option)")
#endif
            ("once,1",                            "Run through securities once and quit")
            ("prog-jobs",        value<string>(), "Prognosticator job list: low:30,high:30,close:30")
            ("moving-avg",       value<string>(), "Moving average(s) for an attribute: close:50,200")
            ("seeds48",          value<string>(), "Seed array for rand48 RNG, example: \"1,2,3\"")
            ("securities",       value<string>(), "CSV list of stock symbols to run, or * for all active"
                                                  " securities")
            ("system",                            "Show info about footprint on this machine")
            ("test",             value<int>(),    "Run test number")
            ("use-xsec-dia",                      "Use closing prices for DIA (Dow Jones) ETF as an extra"
                                                  " attribute [IGNORED]")
            ("use-xsec-gld",                      "Use closing prices for GLD (gold) ETF as an extra attribute")
            ("version",                           "Show program version and exit");

    store(parse_command_line(argc, argv, descr), cfg);
    notify(cfg);

    if(cfg.count("help"))
    {
        cout << descr << endl;
        exit(EXIT_SUCCESS);
    }
    if(cfg.count("version"))
    {
        cout << "Sibyl v" << APP_VERSION << endl;
        exit(EXIT_SUCCESS);
    }

    // CLI-only config values:
    if(cfg.count("config"))     CFG_CFG_FILEPATH = cfg["config"].as<string>();
}


// --------------------------------------------------------------------------
// doCfgOptions:
// --------------------------------------------------------------------------
/**
 * Processes the configuration file options
 *
 * @param descr Command line Options descriptions (output, we fill it in here)
 * @param cfg   Configuration var-map with the CLI options in it (output)
 *
 */
// --------------------------------------------------------------------------
static void doCfgOptions(ini::options_description& descr,
                         ini::variables_map&       cfg)
{
    using namespace ini;

    const bool allowUnregistered = true;

    // Handle config file options
    ifstream in(CFG_CFG_FILEPATH.c_str());

    if(in.good())
    {
        // Collect options descriptions from classes that want them
        descr.add(HumanClock::getOptionsDescr());
        descr.add(PriceWorld::getOptionsDescr());

        store(parse_config_file(in, descr, allowUnregistered), cfg);
        notify(cfg);

        // Now let those same classes know their options
        PriceWorld::setOptions(cfg);

        in.close();
    }
    else
    {
       cerr << __func__ << ": No configuration file '" << CFG_CFG_FILEPATH << "'\n";
    }
}

// --------------------------------------------------------------------------
// showSystem:
// --------------------------------------------------------------------------
/**
 * Prints information about the machine Sibyl is running on and her footprint
 * on it.
 *
 * @param out   Output stream for logging
 */
// --------------------------------------------------------------------------
static void showSystem(ostream& out)
{
    using namespace boost;

    out << LOG_INFO << "CPU cores      : " << thread::hardware_concurrency() << endl
        << LOG_INFO << "g++ (GCC)      : " << __VERSION__                    << endl
        << LOG_INFO << "Boost          : " << BOOST_LIB_VERSION              << endl
        << LOG_INFO << "INT_MAX        : " << INT_MAX                        << endl
        << LOG_INFO << "lrand48 Max    : " << (1L << 32)                     << endl
#if ENABLE_CUDA
        << LOG_INFO << "Cuda cores     : " << nv_getSpecs()                  << endl
#endif
        << LOG_INFO << "Real size      : " << sizeof(Real)                   << endl
        << LOG_INFO << "World size     : " << sizeof(World)                  << endl
        << LOG_INFO << "Individual size: " << sizeof(Individual)             << endl
        << LOG_INFO << "Attribute size : " << sizeof(Attribute)              << endl
        << LOG_INFO << "Allele size    : " << sizeof(Allele)                 << endl;
}

// --------------------------------------------------------------------------
// runTest:
// --------------------------------------------------------------------------
/**
 * Runs a pre-packaged test
 *
 * @param testNum   Test number to run: 0 = test000(), 100 = test100(), etc.
 * @param out       Output stream for logging
 *
 * @return          Return code from test
 */
// --------------------------------------------------------------------------
static int runTest(int testNum, ostream& out)
{
    int rc = EXIT_FAILURE;

    switch(testNum)
    {
                                                // System checkout
        case   0:   rc = test000(out); break;      // Create individual
        case   1:   rc = test001(out); break;      // Check crossover op
//      case   2:   rc = test002(out); break;      // Fitness func [World::scoreFitness() must be public]
        case   3:   rc = test003(out); break;      // Check mutation op
        case  10:   rc = test010(out); break;      // Re-instantiation

                                                // Equation solving
        case 100:   rc = test100(out); break;      // y = x
        case 101:   rc = test101(out); break;      // y = -x^2
        case 102:   rc = test102(out); break;      // y = -x^(1/3)
        case 110:   rc = test110(out); break;      // y = sin x

                                                // Market analysis
        case 200:   rc = test200(out); break;      // stock
        case 201:   rc = test201(out); break;      // stock+gold
        case 210:   rc = test210(out); break;      // stock: 10 days
        case 230:   rc = test230(out); break;      // stock: 30 days

        default:
            cerr << "Unknown test code " << testNum << endl;
            break;
    }

    return rc;
}


// --------------------------------------------------------------------------
// serveJobs:
// --------------------------------------------------------------------------
/**
 * Receives MPI requests for jobs and responds by handing them out, one by
 * one, and then sends back ToDoQueue::DONE when there is no more work.
 *
 * If the MPI request contains a negative number as the "last job completed",
 * the thread exits. This can represent normal logic flow when the root node
 * sends a ToDoQueue::DONE as its last job, or it can represent an error
 * condition.
 *
 * @param mpiComm   MPI communicator
 * @param out       logging, if provided
 */
// --------------------------------------------------------------------------
static void serveJobs(MPI_Communicator& mpiComm, ToDoQueue::JobCode endCode, ostream& out)
{
#if ENABLE_MPI
// FIXME: DELETE COMMENTS WHEN STABLE
//        This is now working for -np=2, but not 3
//        mpi 0 now works, but
//
//        RECEIVED FROM 2
//        SENDING JOB #1 to 2
//        RECEIVED FROM 1
//        SENDING ERROR -1 to JobCode
//        ^Cmpirun: killing job...
//
    using namespace mpi;
    using namespace boost::posix_time;
    assert(ToDos);

    // Prepare a checklist for who has finished all their assignments
    int   numNodes    = mpiComm.size();
    int   workingNode = 1;                  // First node to check if still working (skip root)
    bool  allDone     = false;
    bool *nodesDone   = (bool *) alloca(numNodes);

    memset(nodesDone, false, numNodes * sizeof(bool));

    // Prepare end-of-work text code
    const char *endCodeText = ToDoQueue::jobCodeText(endCode);
    out << LOG_DEBUG << "Serving " << ToDos->size() << " tasks: end[" << endCodeText << ']' << endl;

    // Check to see if this is still the case and convert to non-blocking
    while(!allDone)
    {
        int     job     = ToDoQueue::ERROR;
        request req     = mpiComm.irecv(any_source, MPI_TAG_JOB_REQ, job);
        bool    waiting = true;

        // We loop ourselves and poll a non-blocking request because the
        // MPI blocking receive uses near 100% of one core while its waiting
        // for something to come in.
        while(waiting)
        {
            if(B::optional<status> stat = req.test())
            {
                int source = stat->source();

                // It's in!
                waiting = false;
                out << LOG_DEBUG << "Received task request from MPI-" << source << endl;

                // They should sent their last job, which we don't care about,
                // except to know that it's not an exit/error code.
                //
                // FIXME: Figure out if we should test for error if status::test() returns,
                //        and either fix or get rid of error processing accordingly
                if(true) //!stat->error())
                {
                    // We can block when sending back. The node should be waiting
                    job = ToDos->getTask(endCode);
                    if(job >= 0)
                    {
                        out << LOG_INFO << "Assigning job #" << job << " to MPI-" << source << endl;
                    }
                    else if (endCode == job)
                    {
                        out << LOG_INFO << "Returning " << endCodeText << " to MPI-" << source << endl;
                        nodesDone[source] = true;
                    }
                    else
                    {
                        string errMsg = "ToDo error";
                        out << errMsg << ": " << job << endl;
                        throw Exception(errMsg, job);
                    }
                    mpiComm.send(stat->source(), MPI_TAG_JOB_RSP, job);
                }
                else
                {
                    // Not sure if it's a good idea to try to send back an error
                    // if we ran into problems with MPI to begin with.
                    job = ToDoQueue::ERROR;
                    out << LOG_ERR << "Sernding ERROR " << job
                                   << "(" << stat->error() << ") to MPI-" << source
                                   << endl;
                    mpiComm.send(stat->source(), MPI_TAG_JOB_RSP, job);
                    ToDos->shutdown();
                }
            }
            else B::this_thread::sleep(milliseconds(250));
        }

        // Have we finished assigning tasks?
        if(ToDos->allDone_)
        {
            // Check all-done status for worker nodes...skip our own node [ 0 ]
            for(int i        = workingNode,
                    lastNode = numNodes-1;  i < numNodes; ++i)
            {
                if(nodesDone[i])
                {
                    ++workingNode;
                    if(i == lastNode)
                    {
                        allDone = true;
                        out << LOG_DEBUG << "Work complete for all MPI nodes" << endl;
                    }
                }
                else
                {
                    out << LOG_DEBUG << "MPI-" << i << " is still computing" << endl;
                    break;
                }
            }
        }
        else out << LOG_DEBUG << "Continuing job service" << endl;
    }
#endif // ENABLE_MPI
}


// --------------------------------------------------------------------------
// initJobList:
// --------------------------------------------------------------------------
/**
 * Sets up the node ToDo list on the MPI root node. Acts as a NOOP on all the
 * other nodes.
 *
 * @param isMaster  Indicates this node is responsible for delegating work
 * @param numJobs   Number of jobs on the ToDo list {0 .. (n-1)}
 * @param out       Logging if provided
 *
 * @return          ToDoQueue::INIT to indicate things are set up, but no work
 *                  has been assigned
 */
// --------------------------------------------------------------------------
static inline ToDoQueue::JobCode initJobList(bool     isMaster,
                                             int      numJobs,
                                             ostream& out)
{
    // The MPI root keeps the master list
    if(isMaster || IsLoner)
    {
        // ToDo allocation occurs ONCE
        if (NULL == ToDos)
        {
            out << LOG_INFO << "Allocating job ToDo list." << endl;
            ToDos = new ToDoQueue;
        }

        out << LOG_INFO << "Initializing job ToDo list: "
                           "cnt[" << numJobs << "]"
                        << endl;

        // Load 'er up...
        ToDos->init();
        for(int j = 0; j < numJobs; ++j)
        {
            // NOTE: No mutex locking during setup
            ToDos->taskQ_.push(j);
        }
    }
    return ToDoQueue::INIT;
}


// --------------------------------------------------------------------------
// shutdownJobList:
// --------------------------------------------------------------------------
/**
 * Sets up the node ToDo list on the MPI root node. Acts as a NOOP on all the
 * other nodes.
 *
 * @param isMaster  Indicates this node is responsible for delegating work
 * @param out       Logging if provided
 */
// --------------------------------------------------------------------------
static inline void shutdownJobList(bool     isMaster,
                                   ostream& out)
{
    if(isMaster)
    {
        out << LOG_INFO << "Shutting down job ToDo list" << endl;

        assert(ToDos);
        ToDos->shutdown();     
    }
    else out << LOG_INFO << "Job list complete" << endl;
}


// --------------------------------------------------------------------------
// getNextJob:
// --------------------------------------------------------------------------
/**
 * Request the next job off the MPI ToDo list 
 * other nodes.
 *
 * @param mpiComm   MPI communicator
 * @param lastJob   The last job this MPI node worked on
 * @param out       Logging if provided
 *
 * @return          The next job number, or -1 if there is no more work
 */
// --------------------------------------------------------------------------
static int getNextJob(MPI_Communicator& mpiComm, int lastJob, ostream& out)
{
#if ENABLE_MPI
    using namespace mpi;
#endif

    int  nextJob = ToDoQueue::ERROR;

    // A single-node "MPI" process consults locally
    if(IsLoner)
    {
        assert(ToDos);
        nextJob = ToDos->getTask();
    }
    else
    {
#if ENABLE_MPI
        bool gotJob  = false;
        do
        {
            // Tell the root we need more work.
            mpiComm.send(0, MPI_TAG_JOB_REQ, lastJob);

            // Get back the next job
            status stat = mpiComm.recv(0, MPI_TAG_JOB_RSP, nextJob);
            if(!stat.error())
            {
                // If the other guys aren't done, we may have to idle here a bit
                if(nextJob >= 0)
                {
                    gotJob = true;
                }
                else switch(nextJob)
                {
                    case ToDoQueue::SYNC:
                        out << LOG_INFO << "Waiting for other MPI nodes to complete" << endl;
                        sleep(2 * 60);
                        break;

                    case ToDoQueue::DONE:
                        out << LOG_INFO << "All jobs complete" << endl;
                        gotJob = true;
                        break;

                    default:
                        out << LOG_INFO << "Received job error code "
                                        << ToDoQueue::jobCodeText(nextJob) 
                                        << '[' << nextJob << ']' << endl;
                        throw Exception("Job assignment error", nextJob);
                }
            }
            else throw Exception("MPI error. Cannot getNextJob", stat.error());
            
        } while(!gotJob);
#endif //ENABLE_MPI
    }

    return nextJob;
}


// --------------------------------------------------------------------------
// pullTravellers:
// --------------------------------------------------------------------------
/**
 * Pulls the best travellers (individuals that go from one GP World to another)
 * stored in the database from previous runs for insertion into a current run.
 *
 * Selects the best models to use as Genetic Programming travelling Individuals.
 * The models are selected by fitness according to the specified criteria.
 *
 * @param db          Delphi database object
 * @param symbol      Model works for this Stock symbol
 * @param fnName      The functional (short) name for the model's target attribute,
 *                    (usually "c" for closing price)
 * @param days        Number of days ahead the model prophesies
 * @param travellers  Output vector for the best, previous models from the DB
 * @param out         Output stream for logging
 *
 * @return            The number of travellers retrieved from the database
 */
// --------------------------------------------------------------------------
static int pullTravellers(Delphi&                     db,
                          const string&               symbol,
                          const string&               fnName,
                          size_t                      days,
                          vector<genprog::Traveller>& travellers,
                          ostream&                    out)
{
    using namespace boost;

    int numPulled = 0;

    // Do they want travellers??
    if(!CFG_INSERT_BEST_GENS.empty())
    {
        vector<string> insertGens;

        // See how many generations the configuration says
        split(insertGens, CFG_INSERT_BEST_GENS, is_any_of(", "), token_compress_on);

        // Try to get that many models
        numPulled = db.pullBestTravellers(symbol,
                                          fnName,
                                          days,
                                          MODEL_CODE,
                                          CFG_USE_XSEC_GLD,
                                          CFG_DAYS_TO_PULL,
                                          insertGens.size(),
                                          travellers, out);

        // Put it all together
        for(int i = 0; i < numPulled; ++i)
        {
            // Add the requested insertion generations to the travellers
            travellers[i].generation_ = lexical_cast<u_int>(insertGens[i]);
        }
    }

    return numPulled;
}


/*/- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- **\*/
// ⡀⣀ ⡀⢀ ⣀⡀
// ⠏  ⠣⠼ ⠇⠸
/*/- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- -=- *//**
 * Main run function for Sibyl MPI processing nodes. Note that if there is 
 * more than one node, rank 0 acts as master and simply orchestrates the 
 * other node(s), doing no real work itself.  The intent is that this node-0
 * be the head node on a Beowulf system.
 *
 * @param cfg       Sibyl configuration (INI and CLI options)
 * @param mpiComm   MPI Communicator object (MPI COMM World)
 * @param out       Output stream for logging
 *
 * @return          Return code from run
 *///-------------------------------------------------------------------------
static int run(ConfMap& cfg, MPI_Communicator& mpiComm, std::ostream& out)
{
    int  rc             = EXIT_FAILURE;
    int  errCnt         = 0;

    HumanClock        scheduler(&cfg);
    Delphi            db(CFG_DB_HOST, CFG_DB_USER);
    PriceDataPack     priceData(out);
    vector<Traveller> travellers;
    vector<ProgJob>   jobs;
    int               numJobs    = 0;
    bool              isMaster   = (!IsLoner && (0 == mpiComm.rank()));
    bool              isDiscrete = isConfigured(cfg, "discrete");
    bool              isEager    = isConfigured(cfg, "eager");

    // Pull job list from the INI file
    if(cfg.count("prog-jobs"))
    {
        numJobs = ProgJob::fill(cfg["prog-jobs"].as<string>(), jobs, true);

        out << LOG_INFO << "Processing "
                        << numJobs << " job" << ((1 == numJobs) ? "" : "s")
                        << " per security" << endl;
    }

    // Any extras on the base attributes?
    if(cfg.count("moving-avg"))
    {
        priceData.addMovingAvg(cfg["moving-avg"].as<string>());
    }

    // Are we using any extra attributes?
    if(CFG_USE_XSEC_DIA)    priceData.addExtra(PriceData::XSEC_DIA);
    if(CFG_USE_XSEC_GLD)    priceData.addExtra(PriceData::XSEC_GLD);
#if ENABLE_CUDA
    if(CFG_MIRROR_GPU)      priceData.mirrorGPU();                  // <= Debug setting
#endif

    // Loop once per day FOREVER unless we start having problems
    while(errCnt < CFG_MAX_ERRORS) try
    {
        out << LOG_NOTICE << "Sibyl ACTIVE for delphi @ " << scheduler.getStartInfo() << endl;

        // Run through today's securities
        const SecurityPack& secPack =  db.getSecurities(true, CFG_SECURITIES);

        for(auto sec = secPack.begin(); sec != secPack.end(); ++sec)
        {
            // Every time, assume the worst...
            rc = EXIT_FAILURE;

            int numRecs = priceData.loadAndShare(mpiComm, db, sec->symbol_,
                                                 CFG_DAYS_TO_PULL);

            // Enough data to build a model??
            if(numRecs > CFG_DAYS_IN_WIN)
            {
                out << LOG_DEBUG
                    << "GP World[" << sec->symbol_
                    << "] with "   << numRecs << " days of data!" << endl;

                // Run all (my) jobs for this security's data...
                int job = initJobList(isMaster, numJobs, out);

                if(isMaster)
                {
                    // No real work, just delegate
                    serveJobs(mpiComm, ToDoQueue::SYNC, out);   // Tell 'em all what to do
                    serveJobs(mpiComm, ToDoQueue::DONE, out);   // Tell 'em all it's done!
                }
                else while((job = getNextJob(mpiComm, job, out)) >= 0)
                {
                    string daysText  = boost::lexical_cast<string>(jobs[job].days_);
                    string name      = sec->symbol_    + "." +
                                       jobs[job].name_ + "." +
                                       daysText;
                    string tradeDate = dts::to_iso_string(sec->lastUpdate_);

                    // Main job type is arbitrary, but CLOSE is a logical choice
                    bool isMainJob = (numJobs == 1) ||
                                     (jobs[job].name_ == ProgJob::CLOSE);

                    // Take a nap before we do anything if we need to:
                    out << LOG_NOTICE << "JOB #" << job
                                       <<   " ["  << name
                                       <<  "] @ ";
                    if(isEager)
                        out << "NOW" << endl;
                    else
                    {
                        out << scheduler << endl;
                        usleep(scheduler++);
                    }

                    // Set up for evolution
                    auto  world      = make_unique<PriceWorld>(name, tradeDate, out);
                    auto& targetCode = world->configureJob(jobs[job], priceData, CFG_DAYS_IN_WIN);

                    world->setMPICommunicator(&mpiComm);
                    
                    if(pullTravellers(db,
                                      sec->symbol_,
                                      targetCode,
                                      jobs[job].days_,
                                      travellers,
                                      out) > 0)
                    {
                        world->readyTravellers(travellers);
                    }

                    // go, Go, GO...!!
                    CPUClock cpuClock;
                    world->createPopulation();
                    world->evolve();

                    // Guess the future, but write full JSON data only when working a CLOSE
                    world->prognosticate(isMainJob);

                    // Shall we save a copy of the winner to the database
                    if (!isDiscrete)    db.addModel(sec->symbol_,
                                                    targetCode,
                                                    jobs[job].days_,
                                                    MODEL_CODE,
                                                    tradeDate,
                                                    world->getBestFitness(),
                                                    world->prophesy(true),
                                                    world->getBestSolution(),
                                                    world->getCPUTime(),
                                                    world->getWallSecs(),
                                                    CFG_USE_XSEC_GLD,
                                                    out);

                    out << LOG_INFO << "  TIME:  "  << cpuClock << endl;

                    // Give the CPU a break before we ask for more work
                    if(!isEager)
                    {
                        out << LOG_INFO << "End of job cool down...(whew!)" << endl;
                        sleep(2 * 60);
                    }

                    // If this is the last one, it was good!
                    rc = EXIT_SUCCESS;

                } //else-while(job)

                // Indicate we're done
                shutdownJobList(isMaster, out);
            }
            else out << LOG_ERR << __func__
                                << ": Not enough records for "  << sec->symbol_
                                <<                 ": need["    << CFG_DAYS_IN_WIN
                                <<               "] pulled["    << numRecs
                                <<                       "]"    << endl;

        } //rof


        // All done until tomorrow!
        db.deactivate();
        scheduler.endDay();

        // Did they request NO-LOOP mode??
        if(isConfigured(cfg, "once"))
        {
            out << LOG_NOTICE << "Sibyl run COMPLETE." << endl;
            break;
        }
        else // WAIT FOR RE-LOOP
        {
            out << LOG_NOTICE << "Sibyl DEACTIVATED. Will wake @ " << scheduler << endl;
            sleep(scheduler.getSecsUntilNextTask());
        }

        // Warm up the DB...
        db.activate();
    }
    catch(exception& e)
    {
        cerr << __func__ << ": " << e.what() << endl;
        ++errCnt;
    }
    catch(...)
    {
        cerr << __func__ << ": Runtime error";
        ++errCnt;
    }

    return rc;
}


// --------------------------------------------------------------------------
// main:
// --------------------------------------------------------------------------
/**
 * Where all things Sibyl begin...
 *
 * @param argc  Count of command line parameters
 * @param argv  Array of command line parameters
 *
 * @return      Final return code for the program
 */
// --------------------------------------------------------------------------
int main(int argc, char** argv)
{
    int     rc = EXIT_SUCCESS;
    Logger *logPtr = NULL;

    try
    {
        // Setup for MPI parallel processing
        MPI_Environment  mpiEnv(argc, argv);
        MPI_Communicator mpiWorld;

        // Who are we in this big MPI world?
        u_int rank     = mpiWorld.rank();
        u_int numNodes = mpiWorld.size();
        char  hostName[64];

        gethostname(hostName, sizeof(hostName));
        hostName[sizeof(hostName)-1] = '\0';

        IsLoner  = (1 == numNodes);

        // Handle CLI and INI config options...
        ini::variables_map       cfg;
        ini::options_description cfgDescr("Sibyl options");

        doCLIOptions(argc, argv, cfgDescr, cfg);
        doCfgOptions(cfgDescr, cfg);

        configure<string>(cfg, "log-stub",         CFG_LOG_FILESTUB);
        configure<string>(cfg, "db-host",          CFG_DB_HOST);
        configure<string>(cfg, "db-user",          CFG_DB_USER);
        configure<string>(cfg, "insert-best-gens", CFG_INSERT_BEST_GENS);
        configure<string>(cfg, "securities",       CFG_SECURITIES);
        configure<string>(cfg, "seeds48",          CFG_SEEDS_48);

        configure<int>(cfg, "test",         CFG_RUN_TEST);
        configure<int>(cfg, "master-node",  CFG_MASTER_NODE);
        configure<int>(cfg, "max-errors",   CFG_MAX_ERRORS);
        configure<int>(cfg, "days-to-pull", CFG_DAYS_TO_PULL);
        configure<int>(cfg, "days-in-win",  CFG_DAYS_IN_WIN);

        CFG_USE_XSEC_DIA = cfg.count("use-xsec-dia");
        CFG_USE_XSEC_GLD = cfg.count("use-xsec-gld");
#if ENABLE_CUDA
        CFG_MIRROR_GPU   = cfg.count("mirror-gpu");
#endif

        // Prepare a log file if they have requested one
        if(!CFG_LOG_FILESTUB.empty())
        {
            logPtr = new Logger(CFG_LOG_FILESTUB + "." +
                                B::lexical_cast<std::string>(rank));
        }

        // Logging works from here on:
        ostream& log = (logPtr) ? *logPtr
                                :  cout;
        log.setf(ios::fixed);
        log.setf(ios::showpoint);
        log.precision(4);
        log << LOG_NOTICE << "Welcome to Sibyl: v"  << APP_VERSION                                  << endl
            << LOG_NOTICE << "CFG { " << CFG_CFG_FILEPATH                                   << " }" << endl
            << LOG_NOTICE << "MPI { " << rank << ":" << mpiWorld.size() << ":" << hostName  << " }" << endl;
        if(!IsLoner && (0 == rank))
        {
            log << LOG_NOTICE << "Node acting as MPI master" << endl;
        }
        
        // Genetic Programming needs lots of randomness
        initRand48(log);

        // Need hardware Info??
        if(cfg.count("system"))
        {
            showSystem(log);
        }

        // MAIN BIT!
        //
        // go, Go, GO...!!
        if(CFG_RUN_TEST >= 0)  rc = runTest(CFG_RUN_TEST, log);         // Pre-defined TEST
        else                   rc = run(cfg, mpiWorld, log);            // The REAL THING
    }
    catch(exception& e)
    {
        cerr << e.what() << endl;
        rc = EXIT_FAILURE;
    }
    catch(...)
    {
        cerr << "Internal Error" << endl;
        rc = EXIT_FAILURE;
    }

    // Cleanup and go!
    delete logPtr;
    exit(rc);
}


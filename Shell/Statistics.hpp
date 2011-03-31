/**
 * @file Statistics.hpp
 * Defines proof-search statistics
 *
 * @since 02/01/2008 Manchester
 */

#ifndef __Statistics__
#define __Statistics__

#include <ostream>

/**
 * Identifier of the Vampire version
 */
#define VERSION_STRING "Vampire 0.6 (revision 904)"

namespace Kernel {
  class Unit;
}

namespace Shell {

using namespace std;

/**
 * Class Statistics
 * @since 02/01/2008 Manchester
 */
class Statistics
{
public:
  Statistics();

  void print(ostream& out);

  // Input
  /** number of input clauses */
  unsigned inputClauses;
  /** number of input formulas */
  unsigned inputFormulas;

  // Preprocessing
  /** number of formula names introduced during preprocessing */
  unsigned formulaNames;
  /** number of initial clauses */
  unsigned initialClauses;
  /** number of inequality splittings performed */
  unsigned splittedInequalities;
  /** number of pure predicates */
  unsigned purePredicates;
  /** number of unused predicate definitions */
  unsigned unusedPredicateDefinitions;
  /** number of eliminated function definitions */
  unsigned functionDefinitions;
  /** number of formulas selected by SInE selector */
  unsigned selectedBySine;
  /** number of iterations before SInE reached fixpoint */
  unsigned sineIterations;

  //Generating inferences
  /** number of clauses generated by factoring*/
  unsigned factoring;
  /** number of clauses generated by binary resolution*/
  unsigned resolution;
  /** number of clauses generated by unit resulting resolution*/
  unsigned urResolution;
  /** number of clauses generated by forward superposition*/
  unsigned forwardSuperposition;
  /** number of clauses generated by backward superposition*/
  unsigned backwardSuperposition;
  /** number of clauses generated by self superposition*/
  unsigned selfSuperposition;
  /** number of clauses generated by equality factoring*/
  unsigned equalityFactoring;
  /** number of clauses generated by equality resolution*/
  unsigned equalityResolution;

  // Simplifying inferences
  /** number of duplicate literals deleted */
  unsigned duplicateLiterals;
  /** number of literals s |= s deleted */
  unsigned trivialInequalities;
  /** number of forward subsumption resolutions */
  unsigned forwardSubsumptionResolution;
  /** number of backward subsumption resolutions */
  unsigned backwardSubsumptionResolution;
  /** number of forward demodulations */
  unsigned forwardDemodulations;
  /** number of forward demodulations into equational tautologies */
  unsigned forwardDemodulationsToEqTaut;
  /** number of backward demodulations */
  unsigned backwardDemodulations;
  /** number of backward demodulations into equational tautologies */
  unsigned backwardDemodulationsToEqTaut;
  /** number of forward literal rewrites */
  unsigned forwardLiteralRewrites;
  /** number of condensations */
  unsigned condensations;
  /** number of global subsumptions */
  unsigned globalSubsumption;
  /** number of evaluations */
  unsigned evaluations;
  /** number of interpreted simplifications */
  unsigned interpretedSimplifications;

  // Deletion inferences
  /** number of tautologies A \/ ~A */
  unsigned simpleTautologies;
  /** number of equational tautologies s=s */
  unsigned equationalTautologies;
  /** number of forward subsumed clauses */
  unsigned forwardSubsumed;
  /** number of backward subsumed clauses */
  unsigned backwardSubsumed;
  /** number of subsumed empty clauses */
  unsigned subsumedEmptyClauses;
  /** number of empty clause subsumptions */
  unsigned emptyClauseSubsumptions;
  /** number of empty clause subsumptions by BDD marking*/
  unsigned subsumedByMarking;

  // Saturation
  /** all clauses ever occurring in the unprocessed queue */
  unsigned generatedClauses;
  /** all passive clauses */
  unsigned passiveClauses;
  /** all active clauses */
  unsigned activeClauses;

  unsigned discardedNonRedundantClauses;

  unsigned inferencesSkippedDueToColors;

  /** passive clauses at the end of the saturation algorithm run */
  unsigned finalPassiveClauses;
  /** active clauses at the end of the saturation algorithm run */
  unsigned finalActiveClauses;

  /** number of clause reactivations */
  unsigned reactivatedClauses;

  unsigned splitClauses;
  unsigned splitComponents;
  unsigned uniqueComponents;
  /** Number of introduced name predicates for splitting without backtracking */
  unsigned splittingNamesIntroduced;
  /** Derived clauses with empty non-propositional and non-empty propositional part */
  unsigned bddPropClauses;
  /** Number of clauses generated for the SAT solver */
  unsigned satClauses;
  /** Number of unit clauses generated for the SAT solver */
  unsigned unitSatClauses;
  /** Number of binary clauses generated for the SAT solver */
  unsigned binarySatClauses;
  /** Number of clauses learned by the SAT solver */
  unsigned learntSatClauses;
  /** Number of literals in clauses learned by the SAT solver */
  unsigned learntSatLiterals;
  /** Memory used by BDDs */
  size_t bddMemoryUsage;

  unsigned backtrackingSplits;
  unsigned backtrackingSplitsRefuted;
  unsigned backtrackingSplitsRefutedZeroLevel;

  unsigned instGenGeneratedClauses;
  unsigned instGenRedundantClauses;
  unsigned instGenKeptClauses;
  unsigned instGenIterations;

  /** Number of pure variables eliminated by SAT solver */
  unsigned satPureVarsEliminated;

  /** termination reason */
  enum TerminationReason {
    /** refutation found */
    REFUTATION,
    /** satisfiability detected (saturated set built) */
    SATISFIABLE,
    /** saturation terminated but an incomplete strategy was used */
    REFUTATION_NOT_FOUND,
    /** unknown termination reason */
    UNKNOWN,
    /** time limit reached */
    TIME_LIMIT,
    /** memory limit reached */
    MEMORY_LIMIT
  };
  /** termination reason */
  TerminationReason terminationReason;
  /** refutation, if any */
  Kernel::Unit* refutation;

  enum ExecutionPhase {
    /** Whatever happens before we start parsing the problem */
    INITIALIZATION,
    PARSING,
    /** Scanning for properties to be passed to preprocessing */
    PROPERTY_SCANNING,
    NORMALIZATION,
    SINE_SELECTION,
    INCLUDING_THEORY_AXIOMS,
    PREPROCESS_1,
    UNUSED_PREDICATE_DEFINITION_REMOVAL,
    PREPROCESS_2,
    NAMING,
    PREPROCESS_3,
    CLAUSIFICATION,
    FUNCTION_DEFINITION_ELIMINATION,
    INEQUALITY_SPLITTING,
    EQUALITY_RESOLUTION_WITH_DELETION,
    EQUALITY_PROXY,
    GENERAL_SPLITTING,
    /** The actual run of the saturation algorithm */
    SATURATION,
    /** Whatever happens after the saturation algorithm finishes */
    FINALIZATION,
    UNKNOWN_PHASE
  };

  ExecutionPhase phase;

private:
  static const char* phaseToString(ExecutionPhase p);
}; // class Statistics

}

#endif

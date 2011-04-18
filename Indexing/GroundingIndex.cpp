/**
 * @file GroundingIndex.cpp
 * Implements class GroundingIndex.
 */

#include "GroundingIndex.hpp"

#include "Lib/SharedSet.hpp"

#include "Kernel/Grounder.hpp"
#include "Kernel/Inference.hpp"

#include "SAT/TWLSolver.hpp"

#include "Saturation/SaturationAlgorithm.hpp"

namespace Indexing
{

GroundingIndex::GroundingIndex(Grounder* gnd)
 : _grounder(gnd)
{
  CALL("GroundingIndex::GroundingIndex");

  _solver = new TWLSolver();
}

void GroundingIndex::handleClause(Clause* c, bool adding)
{
  CALL("GroundingIndex::handleClause");

  //We are adding clauses into the index when performing the subsumption check
}

}
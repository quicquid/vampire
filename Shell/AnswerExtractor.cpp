/**
 * @file AnswerExtractor.cpp
 * Implements class AnswerExtractor.
 */

#include "Lib/DArray.hpp"
#include "Lib/Environment.hpp"
#include "Lib/Stack.hpp"

#include "Kernel/BDD.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Inference.hpp"
#include "Kernel/MainLoop.hpp"
#include "Kernel/RobSubstitution.hpp"

#include "Tabulation/TabulationAlgorithm.hpp"

#include "Shell/Flattening.hpp"
#include "Shell/Options.hpp"

#include "AnswerExtractor.hpp"

#undef LOGGING
#define LOGGING 0


namespace Shell
{

void AnswerExtractor::tryOutputAnswer(Clause* refutation)
{
  CALL("AnswerExtractor::tryOutputAnswer");

  Stack<TermList> answer;

  if(!AnswerLiteralManager::getInstance()->tryGetAnswer(refutation, answer)) {
    ConjunctionGoalAnswerExractor cge;
    if(!cge.tryGetAnswer(refutation, answer)) {
      return;
    }
  }
  env.beginOutput();
  env.out() << "% SZS answers Tuple [[";
  Stack<TermList>::BottomFirstIterator ait(answer);
  while(ait.hasNext()) {
    TermList aLit = ait.next();
    env.out() << aLit.toString();
    if(ait.hasNext()) {
      env.out() << ',';
    }
  }
  env.out() << "]|_] for " << env.options->problemName() << endl;
  env.endOutput();
}


void AnswerExtractor::getNeededUnits(Clause* refutation, ClauseStack& premiseClauses, Stack<Unit*>& conjectures)
{
  CALL("AnswerExtractor::getNeededUnits");

  InferenceStore& is = *InferenceStore::instance();

  DHSet<UnitSpec> seen;
  Stack<UnitSpec> toDo;
  toDo.push(UnitSpec(refutation, false));

  while(toDo.isNonEmpty()) {
    UnitSpec curr = toDo.pop();
    if(!seen.insert(curr)) {
      continue;
    }
    Inference::Rule infRule;
    UnitSpecIterator parents = is.getParents(curr, infRule);
    if(infRule==Inference::NEGATED_CONJECTURE) {
      ASS(curr.withoutProp());
      conjectures.push(curr.unit());
    }
    if(infRule==Inference::CLAUSIFY ||
	(curr.isClause() && (infRule==Inference::INPUT || infRule==Inference::NEGATED_CONJECTURE )) ){
      ASS(curr.withoutProp());
      ASS(curr.isClause());
      premiseClauses.push(static_cast<Clause*>(curr.unit()));
    }
    while(parents.hasNext()) {
      UnitSpec premise = parents.next();
      toDo.push(premise);
    }
  }
}


class ConjunctionGoalAnswerExractor::SubstBuilder
{
public:
  SubstBuilder(LiteralStack& goalLits, LiteralIndexingStructure& lemmas, RobSubstitution& subst)
   : _goalLits(goalLits), _lemmas(lemmas), _subst(subst),
     _goalCnt(goalLits.size()), _btData(_goalCnt), _unifIts(_goalCnt), _triedEqUnif(_goalCnt)
  {}
  ~SubstBuilder()
  {
    CALL("ConjunctionGoalAnswerExractor::SubstBuilder::~SubstBuilder");
    for(unsigned i=0; i<_goalCnt; i++) {
      _btData[i].drop();
    }
  }

  bool run()
  {
    CALL("ConjunctionGoalAnswerExractor::SubstBuilder::run");

    _depth = 0;
    enterGoal();
    for(;;) {
      if(nextGoalUnif()) {
	_depth++;
	if(_depth==_goalCnt) {
	  break;
	}
	enterGoal();
      }
      else {
	leaveGoal();
	if(_depth==0) {
	  return false;
	}
	_depth--;
      }
    }
    ASS_EQ(_depth, _goalCnt);
    //pop the recording data
    for(unsigned i=0; i<_depth; i++) {
      _subst.bdDone();
    }
    return true;
  }

  void enterGoal()
  {
    CALL("ConjunctionGoalAnswerExractor::SubstBuilder::enterGoal");

    _unifIts[_depth] = _lemmas.getUnifications(_goalLits[_depth], false, false);
    _triedEqUnif[_depth] = false;
    _subst.bdRecord(_btData[_depth]);
  }
  void leaveGoal()
  {
    CALL("ConjunctionGoalAnswerExractor::SubstBuilder::leaveGoal");

    _subst.bdDone();
    _btData[_depth].backtrack();
  }
  bool nextGoalUnif()
  {
    CALL("ConjunctionGoalAnswerExractor::SubstBuilder::nextGoalUnif");

    Literal* goalLit = _goalLits[_depth];

    LOGV(goalLit->toString());
    while(_unifIts[_depth].hasNext()) {
      SLQueryResult qres = _unifIts[_depth].next();
      ASS_EQ(goalLit->header(), qres.literal->header());
      LOGV(qres.literal->toString());
      if(_subst.unifyArgs(goalLit, 0, qres.literal, 1)) {
	return true;
      }
    }
    if(!_triedEqUnif[_depth] && goalLit->isEquality() && goalLit->isPositive()) {
      _triedEqUnif[_depth] = true;
      if(_subst.unify(*goalLit->nthArgument(0), 0, *goalLit->nthArgument(1), 0)) {
	return true;
      }
    }
    return false;
  }

private:
  LiteralStack& _goalLits;
  LiteralIndexingStructure& _lemmas;
  RobSubstitution& _subst;

  unsigned _goalCnt;
  DArray<BacktrackData> _btData;
  DArray<SLQueryResultIterator> _unifIts;
  DArray<bool> _triedEqUnif;

  unsigned _depth;
};

bool ConjunctionGoalAnswerExractor::tryGetAnswer(Clause* refutation, Stack<TermList>& answer)
{
  CALL("ConjunctionGoalAnswerExractor::tryGetAnswer");

  ClauseStack premiseClauses;
  Stack<Unit*> conjectures;
  getNeededUnits(refutation, premiseClauses, conjectures);

  if(conjectures.size()!=1 || conjectures[0]->isClause()) {
    return false;
  }
  Formula* form = static_cast<FormulaUnit*>(conjectures[0])->formula();

  form = Flattening::flatten(form);

  if(form->connective()!=NOT) {
    return false;
  }
  form = form->uarg();
  if(form->connective()!=EXISTS) {
    return false;
  }
  Formula::VarList* answerVariables = form->vars();
  form = form->qarg();

  LiteralStack goalLits;
  if(form->connective()==LITERAL) {
    goalLits.push(form->literal());
  }
  else if(form->connective()==AND) {
    FormulaList::Iterator git(form->args());
    while(git.hasNext()) {
      Formula* gf = git.next();
      if(gf->connective()!=LITERAL) {
        return false;
      }
      goalLits.push(gf->literal());
    }
  }
  else {
    return false;
  }

  Tabulation::TabulationAlgorithm talg;
  talg.addInputClauses(pvi( ClauseStack::Iterator(premiseClauses) ));
  MainLoopResult res = talg.run();

  LiteralIndexingStructure& lemmas = talg.getLemmaIndex();

  RobSubstitution subst;

  SLQueryResultIterator alit = lemmas.getAll();
  while(alit.hasNext()) {
    SLQueryResult aqr = alit.next();
    LOGV(aqr.literal->toString());
  }

  if(!SubstBuilder(goalLits, lemmas, subst).run()) {
    return false;
  }

  Formula::VarList::Iterator vit(answerVariables);
  while(vit.hasNext()) {
    int var = vit.next();
    TermList varTerm(var, false);
    TermList asgn = subst.apply(varTerm, 0); //goal variables have index 0
    answer.push(asgn);
  }

  return true;
}


///////////////////////
// AnswerLiteralManager
//

AnswerLiteralManager* AnswerLiteralManager::getInstance()
{
  CALL("AnswerLiteralManager::getInstance");

  static AnswerLiteralManager instance;

  return &instance;
}

bool AnswerLiteralManager::tryGetAnswer(Clause* refutation, Stack<TermList>& answer)
{
  CALL("AnswerLiteralManager::tryGetAnswer");

  RCClauseStack::Iterator cit(_answers);
  while(cit.hasNext()) {
    Clause* ansCl = cit.next();
    if(ansCl->length()!=1) {
      continue;
    }
    Literal* lit = (*ansCl)[0];
    unsigned arity = lit->arity();
    for(unsigned i=0; i<arity; i++) {
      answer.push(*lit->nthArgument(i));
    }
    return true;
  }
  return false;
}

Literal* AnswerLiteralManager::getAnswerLiteral(Formula::VarList* vars)
{
  CALL("AnswerLiteralManager::getAnswerLiteral");

  static Stack<TermList> litArgs;
  litArgs.reset();
  Formula::VarList::Iterator vit(vars);
  while(vit.hasNext()) {
    unsigned var = vit.next();
    litArgs.push(TermList(var, false));
  }

  unsigned vcnt = litArgs.size();
  unsigned pred = env.signature->addNamePredicate(vcnt, "ans");
  Signature::Symbol* predSym = env.signature->getPredicate(pred);
  predSym->markAnswerPredicate();
  return Literal::create(pred, vcnt, true, false, litArgs.begin());
}

Unit* AnswerLiteralManager::tryAddingAnswerLiteral(Unit* unit)
{
  CALL("AnswerLiteralManager::tryAddingAnswerLiteral");

  if(unit->isClause() || unit->inputType()!=Unit::CONJECTURE) {
    return unit;
  }

  FormulaUnit* fu = static_cast<FormulaUnit*>(unit);
  Formula* form = fu->formula();

  if(form->connective()!=NOT || form->uarg()->connective()!=EXISTS) {
    return unit;
  }

  Formula* quant =form->uarg();
  Formula::VarList* vars = quant->vars();
  ASS(vars);

  FormulaList* conjArgs = 0;
  FormulaList::push(quant->qarg(), conjArgs);
  Literal* ansLit = getAnswerLiteral(vars);
  FormulaList::push(new AtomicFormula(ansLit), conjArgs);

  Formula* conj = new JunctionFormula(AND, conjArgs);
  Formula* newQuant = new QuantifiedFormula(EXISTS, vars, conj);
  Formula* newForm = new NegatedFormula(newQuant);

  newForm = Flattening::flatten(newForm);

  Inference* inf = new Inference1(Inference::ANSWER_LITERAL, unit);
  Unit* res = new FormulaUnit(newForm, inf, unit->inputType());

  return res;
}

void AnswerLiteralManager::addAnswerLiterals(UnitList*& units)
{
  CALL("AnswerLiteralManager::addAnswerLiterals");

  UnitList::DelIterator uit(units);
  while(uit.hasNext()) {
    Unit* u = uit.next();
    Unit* newU = tryAddingAnswerLiteral(u);
    if(u!=newU) {
      uit.replace(newU);
    }
  }
}

bool AnswerLiteralManager::isAnswerLiteral(Literal* lit)
{
  CALL("AnswerLiteralManager::isAnswerLiteral");

  unsigned pred = lit->functor();
  Signature::Symbol* sym = env.signature->getPredicate(pred);
  return sym->answerPredicate();
}

void AnswerLiteralManager::onNewClause(Clause* cl)
{
  CALL("AnswerLiteralManager::onNewClause");

  if(!cl->noProp() || !cl->noSplits()) {
    return;
  }

  unsigned clen = cl->length();
  for(unsigned i=0; i<clen; i++) {
    if(!isAnswerLiteral((*cl)[i])) {
      return;
    }
  }

  _answers.push(cl);

  Clause* refutation = getRefutation(cl);

  throw MainLoop::RefutationFoundException(refutation);

//  env.beginOutput();
//  env.out()<<cl->toString()<<endl;
//  env.endOutput();
}

Clause* AnswerLiteralManager::getResolverClause(unsigned pred)
{
  CALL("AnswerLiteralManager::getResolverClause");

  Clause* res;
  if(_resolverClauses.find(pred, res)) {
    return res;
  }

  static Stack<TermList> args;
  args.reset();

  Signature::Symbol* predSym = env.signature->getPredicate(pred);
  ASS(predSym->answerPredicate());
  unsigned arity = predSym->arity();

  for(unsigned i=0; i<arity; i++) {
    args.push(TermList(i, false));
  }
  Literal* lit = Literal::create(pred, arity, true, false, args.begin());
  res = Clause::fromIterator(getSingletonIterator(lit), Unit::AXIOM,
      new Inference(Inference::ANSWER_LITERAL));

  _resolverClauses.insert(pred, res);
  return res;
}

Clause* AnswerLiteralManager::getRefutation(Clause* answer)
{
  CALL("AnswerLiteralManager::getRefutation");

  unsigned clen = answer->length();
  UnitList* premises = 0;
  UnitList::push(answer, premises);

  for(unsigned i=0; i<clen; i++) {
    Clause* resolvingPrem = getResolverClause((*answer)[i]->functor());
    UnitList::push(resolvingPrem, premises);
  }

  Inference* inf = new InferenceMany(Inference::UNIT_RESULTING_RESOLUTION, premises);
  Clause* refutation = Clause::fromIterator(LiteralIterator::getEmpty(), answer->inputType(), inf);
  return refutation;
}

}











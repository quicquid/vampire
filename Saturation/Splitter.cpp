/**
 * @file Splitter.cpp
 * Implements class Splitter.
 */

#include "../Lib/DHMap.hpp"
#include "../Lib/Environment.hpp"
#include "../Lib/IntUnionFind.hpp"
#include "../Kernel/BDD.hpp"
#include "../Kernel/Clause.hpp"
#include "../Kernel/Inference.hpp"
#include "../Kernel/InferenceStore.hpp"
#include "../Kernel/Term.hpp"
#include "../Shell/Statistics.hpp"

#include "Splitter.hpp"

namespace Saturation
{

using namespace Lib;
using namespace Kernel;

#define REPORT_SPLITS 0

void Splitter::doSplitting(Clause* cl, ClauseIterator& newComponents,
	ClauseIterator& modifiedComponents)
{
  CALL("Splitter::doSplitting");

  BDD* bdd=BDD::instance();

  int clen=cl->length();

  if(clen<=1) {
    handleNoSplit(cl, newComponents, modifiedComponents);
    return;
  }

  //Master literal of an variable is the literal
  //with lowest index, in which it appears.
  static DHMap<unsigned, int, IdentityHash> varMasters;
  varMasters.reset();
  IntUnionFind components(clen);

  if(clen>1) {
    for(int i=0;i<clen;i++) {
      Term::VariableIterator vit((*cl)[i]);
      while(vit.hasNext()) {
	int master=varMasters.findOrInsert(vit.next().var(), i);
	if(master!=i) {
	  components.doUnion(master, i);
	}
      }
    }
  }
  components.finish();

  int compCnt=components.getComponentCount();

  if(compCnt==1) {
    handleNoSplit(cl, newComponents, modifiedComponents);
    return;
  }

  env.statistics->splittedClauses++;
  env.statistics->splittedComponents+=compCnt;

  static Stack<Clause*> masterPremises;
  masterPremises.reset();

#if REPORT_SPLITS
  cout<<"####Split####\n";
  cout<<(*cl)<<endl;
  cout<<"vvv Into vvv\n";
#endif

  static Stack<Clause*> unnamedComponentStack(16);
  static Stack<Clause*> newComponentStack(16);
  unnamedComponentStack.reset();
  newComponentStack.reset();

  BDDNode* newMasterProp=cl->prop();
  masterPremises.push(cl);

  static Stack<Literal*> lits(16);

  int remainingComps=compCnt;
  {
    //first we handle propositional components
    IntUnionFind::ComponentIterator cit0(components);
    while(cit0.hasNext()) {

      IntUnionFind::ElementIterator elit=cit0.next();

      ALWAYS(elit.hasNext());
      Literal* lit=(*cl)[elit.next()];
      if(elit.hasNext()) {
	continue;
      }

      if(lit->arity()!=0) {
	continue;
      }

      remainingComps--;

      int name;
      Clause* premise;
      bool newPremise;

      getPropPredName(lit, name, premise, newPremise);
      newMasterProp=bdd->disjunction(newMasterProp, bdd->getAtomic(name, lit->isPositive()));
      masterPremises.push(premise);

      //As long as we're sure that all occurences of the propositional
      //predicate get replaced, there is no need to add the premise
      //among new clauses.

#if REPORT_SPLITS
      cout<<'P'<<name<<": "<<(*premise)<<endl;
#endif
    }
  }


  IntUnionFind::ComponentIterator cit(components);
  Clause* masterComp=0;
  ASS(cit.hasNext());
  while(cit.hasNext()) {
    IntUnionFind::ElementIterator elit=cit.next();

    lits.reset();

    while(elit.hasNext()) {
      int litIndex=elit.next();
      lits.push((*cl)[litIndex]);
    }
    int compLen=lits.size();

    if(compLen==1 && lits.top()->arity()==0) {
      //we have already handled propositional components
      continue;
    }

    remainingComps--;

    Clause* comp;
    ClauseIterator variants=_variantIndex.retrieveVariants(lits.begin(), compLen);
    if(variants.hasNext()) {
      comp=variants.next();

      ASS(!variants.hasNext());
      int compName;
      if(_clauseNames.find(comp, compName)) {
	if(!remainingComps && newComponentStack.isEmpty() && unnamedComponentStack.isEmpty()) {
	  masterComp=comp;
	} else {
	  newMasterProp=bdd->disjunction(newMasterProp, bdd->getAtomic(compName, true));
#if REPORT_SPLITS
	  cout<<compName<<": "<<(*comp)<<endl;
#endif
	  if(bdd->isTrue(newMasterProp)) {
	    //the propositional part of cl is true, so there is no point in adding it
	    newComponents=ClauseIterator::getEmpty();
	    modifiedComponents=ClauseIterator::getEmpty();
	    return;
	  }
	  masterPremises.push(comp);
	}
      } else {
	unnamedComponentStack.push(comp);
      }
    } else {
      env.statistics->uniqueComponents++;
      Inference* inf=new Inference(Inference::TAUTOLOGY_INTRODUCTION);
      Unit::InputType inpType = cl->inputType();
      comp = new(compLen) Clause(compLen, inpType, inf);
      for(int i=0;i<compLen;i++) {
	(*comp)[i]=lits.pop();
      }

      _variantIndex.insert(comp);

      comp->setProp(bdd->getTrue());
      InferenceStore::instance()->recordNonPropInference(comp);


      newComponentStack.push(comp);
    }
  }

  bool masterNew=false;
  if(newComponentStack.isNonEmpty()) {
    ASS(!masterComp);
    masterNew=true;
    masterComp=newComponentStack.pop();
  } else if(unnamedComponentStack.isNonEmpty()) {
    ASS(!masterComp);
    masterComp=unnamedComponentStack.pop();
  } else {
    if(!masterComp) {
      //this should happen just if the clause being splitted consist
      //of only propositional literals
      Clause* emptyCl=new(0) Clause(0,Clause::AXIOM, new Inference(Inference::TAUTOLOGY_INTRODUCTION));
      emptyCl->setProp(bdd->getTrue());

      bool aux1, aux2;
      masterComp=insertIntoIndex(emptyCl, aux1, aux2);
      masterNew = masterComp==emptyCl;
    }
  }


  ClauseIterator unnamedIt=pvi( getConcatenatedIterator(
	  Stack<Clause*>::Iterator(newComponentStack),
	  Stack<Clause*>::Iterator(unnamedComponentStack)) );
  while(unnamedIt.hasNext()) {
    Clause* comp=unnamedIt.next();
    if(comp==masterComp) {
      //The same component can appear multiple times, here
      //we catch cases when the master component is unnamed
      //and appears more than once.
      continue;
    }
    int compName=bdd->getNewVar();

    if(_clauseNames.insert(comp, compName)) {
      //The same component can appear multiple times.
      //We detect the harmful cases here as attempts to name
      //a component multiple times.
      BDDNode* oldCompProp=comp->prop();
      BDDNode* newCompProp=bdd->conjunction(oldCompProp, bdd->getAtomic(compName, false));
      if(newCompProp!=oldCompProp) {
	comp->setProp(newCompProp);
	InferenceStore::instance()->recordPropAlter(comp, oldCompProp, newCompProp, Inference::CLAUSE_NAMING);
      }
      newMasterProp=bdd->disjunction(newMasterProp, bdd->getAtomic(compName, true));
      masterPremises.push(comp);
#if REPORT_SPLITS
      cout<<'n'<<compName<<": "<<(*comp)<<endl;
#endif
    }
  }

  ASS(!bdd->isTrue(newMasterProp));

  BDDNode* oldProp=masterComp->prop();
  masterComp->setProp( bdd->conjunction(oldProp, newMasterProp) );
  InferenceStore::instance()->recordSplitting(masterComp, oldProp, masterComp->prop(), masterPremises.size(),
	  masterPremises.begin());

  ASS(!bdd->isTrue(masterComp->prop()));

#if REPORT_SPLITS
  cout<<(*masterComp)<<endl;
#endif

  if(!masterNew) {
    if(oldProp!=masterComp->prop()) {
      modifiedComponents=getPersistentIterator(getConcatenatedIterator(
  		  getSingletonIterator(masterComp),
  		  Stack<Clause*>::Iterator(unnamedComponentStack)) );
    } else {
      modifiedComponents=getPersistentIterator(
  		  Stack<Clause*>::Iterator(unnamedComponentStack) );
    }
    newComponents=getPersistentIterator(Stack<Clause*>::Iterator(newComponentStack));
  } else {
    modifiedComponents=getPersistentIterator(
		  Stack<Clause*>::Iterator(unnamedComponentStack) );
    newComponents=getPersistentIterator(getConcatenatedIterator(
		  getSingletonIterator(masterComp),
		  Stack<Clause*>::Iterator(newComponentStack)) );
  }

#if REPORT_SPLITS
  cout<<"^^^^^^^^^^^^\n";
#endif

}

void Splitter::getPropPredName(Literal* lit, int& name, Clause*& premise, bool& newPremise)
{
  unsigned pred=lit->functor();
  int* pname;
  if(_propPredNames.getValuePtr(pred, pname)) {
    *pname=BDD::instance()->getNewVar();
  }
  name=*pname;

  Clause** ppremise;
  if(lit->isPositive()) {
    _propPredPosNamePremises.getValuePtr(pred, ppremise, 0);
  } else {
    _propPredNegNamePremises.getValuePtr(pred, ppremise, 0);
  }
  newPremise=*ppremise;
  if(!*ppremise) {
    Clause* cl=new(1) Clause(1,Clause::AXIOM,new Inference(Inference::CLAUSE_NAMING));
    (*cl)[0]=lit;
    cl->setProp( BDD::instance()->getAtomic(name, lit->isNegative()) );
    InferenceStore::instance()->recordNonPropInference(cl);
    *ppremise=cl;
  }
  premise=*ppremise;
}

Clause* Splitter::insertIntoIndex(Clause* cl, bool& newInserted, bool& modified)
{
  CALL("Splitter::insertIntoIndex");

  BDD* bdd=BDD::instance();

  newInserted=false;
  modified=false;

  ClauseIterator variants=_variantIndex.retrieveVariants(cl->literals(), cl->length());
  if(variants.hasNext()) {
    Clause* comp=variants.next();

    ASS(!variants.hasNext());

    BDDNode* oldCompProp=comp->prop();
    BDDNode* oldClProp=cl->prop();
    BDDNode* newCompProp=bdd->conjunction(oldCompProp, oldClProp);

    if(oldCompProp==newCompProp) {
      return comp;
    }

#if REPORT_SPLITS
    cout<<"----Merging----\n";
    cout<<"Clause "<<(*cl)<<" caused\n";
    cout<<(*comp)<<" to get prop. part\n";
    cout<<bdd->toString(newCompProp)<<endl;
    cout<<"^^^^^^^^^^^^^^^\n";
#endif
    comp->setProp( newCompProp );
    InferenceStore::instance()->recordMerge(comp, oldCompProp, cl, newCompProp);

    modified=true;
    return comp;

  } else {
    env.statistics->uniqueComponents++;
    _variantIndex.insert(cl);

    newInserted=true;
    return cl;
  }
}

void Splitter::handleNoSplit(Clause* cl, ClauseIterator& newComponents,
	ClauseIterator& modifiedComponents)
{
  CALL("Splitter::handleNoSplit");

  newComponents=ClauseIterator::getEmpty();
  modifiedComponents=ClauseIterator::getEmpty();

  if(cl->length()==1 && (*cl)[0]->arity()==0) {
    Literal* lit=(*cl)[0];
    int name;
    Clause* premise;
    bool newPremise;

    getPropPredName(lit, name, premise, newPremise);

    Clause* newCl=new(0) Clause(0,cl->inputType(), new Inference2(Inference::SPLITTING, cl, premise));
    newCl->setProp( BDD::instance()->getAtomic(name, lit->isPositive()) );
    InferenceStore::instance()->recordNonPropInference(newCl);

    //As long as we're sure that all occurences of the propositional
    //predicate get replaced, there is no need to add the premise
    //among new clauses.

#if REPORT_SPLITS
    cout<<"####PSplit####\n";
    cout<<(*cl)<<endl;
    cout<<"vvv Into vvv\n";
    cout<<'P'<<name<<": "<<(*premise)<<endl;
    cout<<(*newCl)<<endl;
    cout<<"^^^^^^^^^^^^^^^\n";
#endif
    cl=newCl;
  }

  bool newInserted;
  bool modified;
  Clause* insCl=insertIntoIndex(cl, newInserted, modified);

  if(newInserted) {
    ASS_EQ(insCl, cl);
    newComponents=pvi( getSingletonIterator(insCl) );
  } else if(modified) {
    ASS_NEQ(insCl, cl);
    if( insCl->isEmpty() && BDD::instance()->isFalse(insCl->prop()) ) {
      BDDNode* oldClProp=cl->prop();
      cl->setProp(insCl->prop());
      InferenceStore::instance()->recordMerge(cl, oldClProp, insCl, cl->prop());

      //we want the refutation to be a new clause that is put on the unprocessed stack.
      newComponents=pvi( getSingletonIterator(cl) );
    } else {
      modifiedComponents=pvi( getSingletonIterator(insCl) );
    }
  }
}


}

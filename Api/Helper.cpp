  /**
 * @file Helper.cpp
 * Implements class Helper.
 */

#include "Helper_Internal.hpp"

namespace Api
{

using namespace Kernel;
using namespace Shell;

///////////////////////
// DefaultHelperCore
//


DefaultHelperCore* DefaultHelperCore::instance()
{
  static DefaultHelperCore inst;

  return &inst;
}

string DefaultHelperCore::getVarName(unsigned v) const
{
  CALL("DefaultHelperCore::getVarName");

  return "X"+Int::toString(v);
}

string DefaultHelperCore::toString(Kernel::TermList t) const
{
  CALL("DefaultHelperCore::toString(TermList)");

  if(t.isOrdinaryVar()) {
    return getVarName(t.var());
  }
  ASS(t.isTerm());
  return toString(t.term());
}

string DefaultHelperCore::toString(const Kernel::Term* t0) const
{
  CALL("DefaultHelperCore::toString(const Kernel::Term*)");

  string res;
  if(t0->isLiteral()) {
    const Literal* l=static_cast<const Literal*>(t0);
    if(l->isEquality()) {
      res=toString(*l->nthArgument(0));
      res+= l->isPositive() ? " = " : " != ";
      res+=toString(*l->nthArgument(1));
      return res;
    }
    res=(l->isPositive() ? "" : "~") + l->predicateName();
  }
  else {
    res=t0->functionName();
  }
  if(t0->arity()==0) {
    return res;
  }

  res+='(';

  static Stack<int> remArg; //how many arguments remain to be printed out for a term at each level
  remArg.reset();
  remArg.push(t0->arity());
  SubtermIterator sti(t0);
  ASS(sti.hasNext());

  while(sti.hasNext()) {
    TermList t=sti.next();
    remArg.top()--;
    ASS_GE(remArg.top(),0);
    bool separated=false;
    if(t.isOrdinaryVar()) {
      res+=getVarName(t.var());
    }
    else {
      Kernel::Term* trm=t.term();
      res+=trm->functionName();
      if(trm->arity()) {
	res+='(';
	remArg.push(trm->arity());
	separated=true;
      }
    }
    if(!separated) {
      while(!remArg.top()) {
	res+=')';
	remArg.pop();
	if(remArg.isEmpty()) {
	  goto fin;
	}
      }
      ASS(remArg.top());
      res+=',';
    }
  }
  ASSERTION_VIOLATION;

  fin:
  ASS(remArg.isEmpty());
  return res;
}

string DefaultHelperCore::toString(const Kernel::Formula* f)
{
  CALL("DefaultHelperCore::toString(const Kernel::Formula*)");

  static string names [] =
  { "", " & ", " | ", " => ", " <=> ", " <~> ",
      "~", "!", "?", "$false", "$true"};
  Connective c = f->connective();
  string con = names[(int)c];
  switch (c) {
  case LITERAL:
    return toString(f->literal());
  case AND:
  case OR:
  {
    const Kernel::FormulaList* fs = f->args();
    string result = "(" + toString(fs->head());
    fs = fs->tail();
    while (! fs->isEmpty()) {
      result += con + toString(fs->head());
      fs = fs->tail();
    }
    return result + ")";
  }

  case IMP:
  case IFF:
  case XOR:
    return string("(") + toString(f->left()) +
	con + toString(f->right()) + ")";

  case NOT:
    return string("(") + con + toString(f->uarg()) + ")";

  case FORALL:
  case EXISTS:
  {
    string result = string("(") + con + "[";
    Kernel::Formula::VarList::Iterator vit(f->vars());
    ASS(vit.hasNext());
    while (vit.hasNext()) {
      result += getVarName(vit.next());
      if(vit.hasNext()) {
	result += ',';
      }
    }
    return result + "] : (" + toString(f->qarg()) + ") )";
  }
  case FALSE:
  case TRUE:
    return con;
  }
  ASSERTION_VIOLATION;
  return "formula";
}

string DefaultHelperCore::toString(const Kernel::Clause* clause)
{
  CALL("DefaultHelperCore::toString(const Kernel::Clause*)");

  string res;
  Kernel::Clause::Iterator lits(*const_cast<Kernel::Clause*>(clause));
  while(lits.hasNext()) {
    res+=toString(lits.next());
    if(lits.hasNext()) {
      res+=" | ";
    }
  }

  if(clause->prop() && !BDD::instance()->isFalse(clause->prop())) {
    if(res!="") {
      res+=" | ";
    }
    res+=BDD::instance()->toTPTPString(clause->prop());
  }
  return res;
}


/**
 * Output unit in TPTP format
 *
 * If the unit is a formula of type @b CONJECTURE, output the
 * negation of Vampire's internal representation with the
 * TPTP role conjecture. If it is a clause, just output it as
 * is, with the role negated_conjecture.
 */
string DefaultHelperCore::toString (const Kernel::Unit* unit)
{
  CALL("DefaultHelperCore::toString(const Kernel::Unit*)");

  string prefix;
  string main = "";

  bool negate_formula = false;
  string kind;
  switch (unit->inputType()) {
  case Unit::ASSUMPTION:
    kind = "hypothesis";
    break;

  case Unit::CONJECTURE:
    if(unit->isClause()) {
      kind = "negated_conjecture";
    }
    else {
      negate_formula = true;
      kind = "conjecture";
    }
    break;

  default:
    kind = "axiom";
    break;
  }

  if (unit->isClause()) {
    prefix = "cnf";
    main = toString(static_cast<const Kernel::Clause*>(unit));
  }
  else {
    prefix = "fof";
    const Kernel::Formula* f = static_cast<const Kernel::FormulaUnit*>(unit)->formula();
    if(negate_formula) {
      Kernel::Formula* quant=Kernel::Formula::quantify(const_cast<Kernel::Formula*>(f));
      if(quant->connective()==NOT) {
	ASS_EQ(quant,f);
	main = toString(quant->uarg());
      }
      else {
	Kernel::Formula* neg=new Kernel::NegatedFormula(quant);
	main = toString(neg);
	neg->destroy();
      }
      if(quant!=f) {
	ASS_EQ(quant->connective(),FORALL);
	static_cast<Kernel::QuantifiedFormula*>(quant)->vars()->destroy();
	quant->destroy();
      }
    }
    else {
      main = toString(f);
    }
  }

  string unitName;
  if(!Parser::findAxiomName(unit, unitName)) {
    unitName="u" + Int::toString(unit->number());
  }


  return prefix + "(" + unitName + "," + kind + ",\n"
      + "    " + main + ").\n";
}

struct DefaultHelperCore::Var2NameMapper
{
  Var2NameMapper(DefaultHelperCore& a) : aux(a) {}
  DECL_RETURN_TYPE(string);
  string operator()(unsigned v)
  {
    return aux.getVarName(v);
  }
  DefaultHelperCore& aux;
};

StringIterator DefaultHelperCore::getVarNames(VarList* l)
{
  CALL("DefaultHelperCore::getVarNames");

  VirtualIterator<string> res=pvi( getPersistentIterator(
      getMappingIterator(
	  VarList::DestructiveIterator(l),
	  Var2NameMapper(*this))
  ) );

  return StringIterator(res);
}



///////////////////////
// FBHelperCore
//


/** build a term f(*args) with specified @b arity */
Term FBHelperCore::term(const Function& f,const Term* args, unsigned arity)
{
  CALL("FBHelperCore::term");

  if(f>=static_cast<unsigned>(env.signature->functions())) {
    throw FormulaBuilderException("Function does not exist");
  }
  if(arity!=env.signature->functionArity(f)) {
    throw FormulaBuilderException("Invalid function arity: "+env.signature->functionName(f));
  }

  DArray<TermList> argArr;
  argArr.initFromArray(arity, args);

  Term res(Kernel::TermList(Kernel::Term::create(f,arity,argArr.array())));
  res._aux=this; //assign the correct helper object
  return res;
}

/** build a predicate p(*args) with specified @b arity */
Formula FBHelperCore::atom(const Predicate& p, bool positive, const Term* args, unsigned arity)
{
  CALL("FBHelperCore::atom");

  if(p>=static_cast<unsigned>(env.signature->predicates())) {
    throw FormulaBuilderException("Predicate does not exist");
  }
  if(arity!=env.signature->predicateArity(p)) {
    throw FormulaBuilderException("Invalid predicate arity: "+env.signature->predicateName(p));
  }

  DArray<TermList> argArr;
  argArr.initFromArray(arity, args);

  Kernel::Literal* lit=Kernel::Literal::create(p, arity, positive, false, argArr.array());

  Formula res(new Kernel::AtomicFormula(lit));
  res._aux=this; //assign the correct helper object
  return res;
}

string FBHelperCore::getVarName(unsigned v) const
{
  CALL("FBHelperCore::getVarName");

  string res;
  if(varNames.find(v,res)) {
    return res;
  }
  else {
    Map<Var,string>::Iterator it(varNames);
    while(it.hasNext()) {
      string v;
      unsigned k;
      it.next(k,v);
      cout<<k<<" "<<v<<endl;
    }
    throw FormulaBuilderException("Var object was used in FormulaBuilder object which did not create it");
  }
}

unsigned FBHelperCore::getVar(string varName)
{
  if(_checkNames) {
    if(!isupper(varName[0])) {
      throw InvalidTPTPNameException("Variable name must start with an uppercase character", varName);
    }
    //TODO: add further checks
  }

  unsigned res=vars.insert(varName, nextVar);
  if(res==nextVar) {
    nextVar++;
    varNames.insert(res, varName);
  }
  ASS_L(res, nextVar);
  return res;
}

/**
 * Return an alias variable for variable number @b var
 */
unsigned FBHelperCore::FBVarFactory::getVarAlias(unsigned var)
{
  CALL("FBHelperCore::FBVarFactory::getVarAlias");

  string origName=_parent.getVarName(var);
  int i=0;
  string name;
  do {
    i++;
    name=origName+"_"+Int::toString(i);
  } while(_parent.vars.find(name));

  return _parent.getVar(name);
}

/**
 * Return name of variable number @b var
 */
string FBHelperCore::FBVarFactory::getVarName(unsigned var)
{
  CALL("FBHelperCore::FBVarFactory::getVarName");

  return _parent.getVarName(var);
}

///////////////////////
// ApiHelper
//


ApiHelper::ApiHelper() : _obj(0) {}

ApiHelper::~ApiHelper()
{
  CALL("ApiHelper::~ApiHelper");

  updRef(false);
}

ApiHelper::ApiHelper(const ApiHelper& h)
{
  CALL("ApiHelper::ApiHelper(ApiHelper&)");

  _obj=h._obj;
  updRef(true);
}

ApiHelper& ApiHelper::operator=(const ApiHelper& h)
{
  const_cast<ApiHelper&>(h).updRef(true);
  updRef(false);
  _obj=h._obj;
  return *this;
}

ApiHelper& ApiHelper::operator=(FBHelperCore* hc)
{
  hc->incRef();
  updRef(false);
  _obj=hc;
  return *this;
}

void ApiHelper::updRef(bool inc)
{
  CALL("ApiHelper::updRef");

  if(_obj) {
    if(inc) {
      _obj->incRef();
    }
    else {
      _obj->decRef();
    }
  }
}

bool ApiHelper::operator==(const ApiHelper& h) const
{
  CALL("ApiHelper::operator==");

  return _obj==h._obj;
}

bool ApiHelper::operator!=(const ApiHelper& h) const
{
  CALL("ApiHelper::operator!=");

  return _obj!=h._obj;
}

DefaultHelperCore* ApiHelper::operator->() const
{
  CALL("ApiHelper::operator->");

  if(_obj) {
    return _obj;
  }
  else {
    return DefaultHelperCore::instance();
  }
}

///////////////////////
// FBHelper
//


FBHelper::FBHelper()
{
  CALL("FBHelper::FBHelper");

  _obj=new FBHelperCore;
  updRef(true);
}

FBHelperCore* FBHelper::operator->() const
{
  CALL("FBHelper::operator->");

  ASS(_obj);
  return _obj;
}


}
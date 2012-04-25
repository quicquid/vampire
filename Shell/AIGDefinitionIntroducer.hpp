/**
 * @file AIGDefinitionIntroducer.hpp
 * Defines class AIGDefinitionIntroducer.
 */

#ifndef __AIGDefinitionIntroducer__
#define __AIGDefinitionIntroducer__

#include "Forwards.hpp"

#include "Lib/DHMap.hpp"
#include "Lib/DHSet.hpp"
#include "Lib/Stack.hpp"

#include "AIG.hpp"
#include "AIGRewriter.hpp"


namespace Shell {

class AIGDefinitionIntroducer : public ScanAndApplyFormulaUnitTransformer
{
  typedef SharedSet<unsigned> VarSet;

  struct NodeInfo {

    //these members are filled-in in the doFirstRefAIGPass() function

    /** True if contains quantifiers with {negative,positive} polarity */
    bool _hasQuant[2];

    bool _hasName;
    AIGRef _name;

    VarSet* _freeVars;
    /** Color of self or parents. If should be invalid, exception is thrown. */
    Color _clr;

    /** Number of AIG nodes that refer to this node */
    unsigned _directRefCnt;

    //these members are filled-in in the doSecondRefAIGPass() function

    /** True if occurs in toplevel AIGs with {negative,positive} polarity */
    bool _inPol[2];
    /** True if occurs in quantifier AIG nodes with {negative,positive} polarity */
    bool _inQuant[2];

    /**
     * How many times will an AIG node appear in formulas
     * after conversion of AIG to formulas
     *
     * Equals to 1 if node has a name
     */
    unsigned _formRefCnt;


    //these members are used in the third pass

    /** Before the third pass may be zero */
    FormulaUnit* _namingUnit;
  };

  //options
  bool _eprPreserving;
  unsigned _namingRefCntThreshold;
  unsigned _mergeEquivDefs;

  mutable AIGFormulaSharer _fsh;
  AIGRewriter _arwr;

  typedef Stack<AIGRef> TopLevelStack;
  TopLevelStack _toplevelAIGs;

  /**
   * All positive AIG refs used in the problem, ordered topologically,
   * so that references go only toward the bottom of the stack.
   */
  AIGStack _refAIGs;
  /** stack of infos corresponding to nodes at corresponding _refAIGs positions */
  Stack<NodeInfo> _refAIGInfos;
  /** Indexes of AIG refs in the _refAIGs stack */
  DHMap<AIGRef,size_t> _aigIndexes;

  /** Definitions that were already present in the problem before this rule.
   * Key is a defined AIG and value is its atom name. */
  DHMap<AIGRef,AIGRef> _existingDefs;
  DHMap<AIGRef,FormulaUnit*> _existingDefUnits;

  /**
   * _defs saturated on the relevand AIGs
   */
  AIGRewriter::RefMap _defsSaturated;

  /** Newly introduced definitions */
  Stack<FormulaUnit*> _newDefs;



  //these aren't used by the algorithm but are exposed by the interface
  /** Newly introduced predicates */
  DHSet<unsigned> _introducedPreds;
  /** Maps introduced atoms (positive) to original AIGs */
  DHMap<Literal*,AIGRef> _introducedAtoms;


  AIGRef getPreNamingAig(unsigned aigStackIdx);

  bool shouldIntroduceName(unsigned aigStackIdx);
  Literal* getNameLiteral(unsigned aigStackIdx);
  void introduceName(unsigned aigStackIdx);

  bool scanDefinition(FormulaUnit* def);

  //functions called from scan()
  void collectTopLevelAIGsAndDefs(UnitList* units);
  void processTopLevelAIGs();

  //functions called from processTopLevelAIGs()
  void doFirstRefAIGPass();
  void doSecondRefAIGPass();
  void doThirdRefAIGPass();

  VarSet* getAtomVars(Literal* l);
  NodeInfo& getNodeInfo(AIGRef r);

  FormulaUnit* createNameUnit(AIGRef rhs, AIGRef atomName);

  Inference* getInferenceFromPremIndexes(Unit* orig, AIGRewriter::PremiseSet* premIndexes);

public:
  AIGDefinitionIntroducer(unsigned threshold=4);

  virtual void scan(UnitList* units);

  using ScanAndApplyFormulaUnitTransformer::apply;
  virtual bool apply(FormulaUnit* unit, Unit*& res);

  virtual UnitList* getIntroducedFormulas();

  /** Name predicates introduced by the algorithm */
  const DHSet<unsigned>& introducedPreds() const { return _introducedPreds; }
  /** Positive name atoms introduced by the algorithm */
  Formula* getNamedFormula(Literal* nameAtom, Unit*& premise) const;
protected:
  virtual void updateModifiedProblem(Problem& prb);
};

}

#endif // __AIGDefinitionIntroducer__
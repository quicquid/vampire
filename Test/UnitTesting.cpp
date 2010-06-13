/**
 * @file RuntimeStatistics.cpp
 * Implements class RuntimeStatistics.
 */

#include "Debug/Tracer.hpp"

#include "Lib/Comparison.hpp"
#include "Lib/Int.hpp"
#include "Lib/Sort.hpp"
#include "Lib/Stack.hpp"

#include "UnitTesting.hpp"

namespace Test
{

using namespace Lib;

TestUnit::TestUnit(const char* id)
: _id(id), _tests(0)
{
  CALL("TestUnit::TestUnit");

  UnitTesting::instance()->add(this);
}


/**
 * Return iterator over the tests in this unit
 *
 * All elements of the iterator must be retrieved, or
 * a memory leak will occur
 */
TestUnit::Iterator TestUnit::getTests()
{
  CALL("TestUnit::getTests");

  TestList* lst=_tests->copy()->reverse();
  return TestList::DestructiveIterator(lst);
}


UnitTesting* UnitTesting::instance()
{
  static UnitTesting inst;

  return &inst;
}

UnitTesting::UnitTesting()
: _units(0)
{
}

UnitTesting::~UnitTesting()
{
  _units->destroy();
}

TestUnit* UnitTesting::get(const char* unitId)
{
  CALL("UnitTesting::get");

  TestUnitList::Iterator it(_units);
  while(it.hasNext()) {
    TestUnit* u=it.next();
    if(!strcmp(u->id(), unitId)) {
      return u;
    }
  }
  return 0;
}

bool UnitTesting::runTest(const char* unitId, ostream& out)
{
  TestUnit* unit=get(unitId);
  if(!unit) {
    return false;
  }
  runTest(unit, out);
  return true;
}

void UnitTesting::runTest(TestUnit* unit, ostream& out)
{
  out<<"Testing unit "<<unit->id()<<":"<<endl;

  TestUnit::Iterator uit=unit->getTests();
  if(!uit.hasNext()) {
    out<<"No tests in this unit"<<endl;
  }
  while(uit.hasNext()) {
    TestUnit::Test t=uit.next();
    out<<"Test "<<t.name<<"... ";
    out.flush();
    {
      CALL(t.name);
      t.proc();
    }
    out<<"OK"<<endl;
  }
}

void UnitTesting::runAllTests(ostream& out)
{
  TestUnitList::Iterator tuit(_units);
  while(tuit.hasNext()) {
    runTest(tuit.next(), out);
    if(tuit.hasNext()) {
      out<<endl;
    }
  }
}

void UnitTesting::printTestNames(ostream& out)
{
  CALL("UnitTesting::printTestNames");

  TestUnitList::Iterator tuit(_units);
  while(tuit.hasNext()) {
    out<<tuit.next()->id()<<endl;
  }
}


}

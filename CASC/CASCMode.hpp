/**
 * @file CASCMode.hpp
 * Defines class CASCMode.
 */

#ifndef __CASCMode__
#define __CASCMode__

#include "Forwards.hpp"

#include "Lib/Portability.hpp"
#include "Lib/ScopedPtr.hpp"
#include "Lib/Set.hpp"
#include "Lib/Stack.hpp"

#include "Lib/VString.hpp"

#include "Shell/Property.hpp"

#include "Schedules.hpp"

namespace CASC
{

using namespace std;
using namespace Lib;
using namespace Shell;

class CASCMode {
public:
  virtual ~CASCMode() {}
  static bool perform(int argc,char* argv []);
protected:
  static unsigned getSliceTime(vstring sliceCode,vstring& chopped);

  static void getSchedules(Property& prop, Schedule& quick, Schedule& fallback);
  static void getSchedulesSat(Property& prop, Schedule& quick, Schedule& fallback);

  /**
   * Run a slice corresponding to the options.
   * Return true iff the proof or satisfiability was found
   */
  virtual bool runSlice(Options& opt) = 0;

  void handleSIGINT() __attribute__((noreturn));

  /** The problem property, computed once in the parent process */
  Property* _property;

private:
  typedef Set<vstring> StrategySet;
  bool perform();
  bool runSchedule(Schedule&,unsigned ds,StrategySet& remember,bool fallback);
  bool runSlice(vstring sliceCode, unsigned ds);
};

}

#endif // __CASCMode__

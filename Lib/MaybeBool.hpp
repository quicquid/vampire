/**
 * @file MaybeBool.hpp
 * Defines class MaybeBool.
 */

#ifndef __MaybeBool__
#define __MaybeBool__

#include "Forwards.hpp"

#include "Debug/Assertion.hpp"
#include "Debug/Tracer.hpp"


namespace Lib {

class MaybeBool
{
public:
  enum Value {
    FALSE = 0,
    TRUE = 1,
    UNKNOWN = 2
  };

  MaybeBool() : _value(UNKNOWN) {}
  MaybeBool(bool val) : _value(val ? TRUE : FALSE) {}

  bool known() const { return _value!=UNKNOWN; }
  bool isTrue() const { return _value==TRUE; }
  bool isFalse() const { return _value==FALSE; }

  bool operator==(const MaybeBool& o) const { return _value==o._value; }
  bool operator==(MaybeBool::Value o) const { return _value==o; }

  bool value() const {
    CALL("MaybeBool::value");
    ASS(known());
    return _value==TRUE;
  }

  void makeUnknown() { _value = UNKNOWN; }
  void mightBecameFalse() { if(isTrue()) { makeUnknown(); } }
  void mightBecameTrue() { if(isFalse()) { makeUnknown(); } }

private:
  Value _value;
};



}

#endif // __MaybeBool__
/**
 * @file ClauseSharing.cpp
 * Implements class ClauseSharing.
 */

#include "../Lib/Hash.hpp"

#include "ClauseSharing.hpp"

namespace SAT
{

SATClause* ClauseSharing::insert(SATClause* c)
{
  SATClause* res=_storage.insert(c);
  if(res!=c) {
    c->destroy();
  }
  return res;
}

void ClauseSharing::wipe()
{
  ClauseSet::Iterator it(_storage);
  while(it.hasNext()) {
    SATClause* cl=it.next();
    if(!cl->kept()) {
      cl->destroy();
    }
  }
  _storage.~ClauseSet();
  ::new(&_storage) ClauseSet();
}


ClauseSharing* ClauseSharing::getInstance()
{
  static ClauseSharing* inst=0;
  if(!inst) {
    inst=new ClauseSharing();
  }
  return inst;
}


unsigned ClauseSharing::Hasher::hash(SATClause* c)
{
  return Hash::hash(reinterpret_cast<const unsigned char*>(c->literals()),
	  c->length()*sizeof(SATLiteral));
}

bool ClauseSharing::Hasher::equals(SATClause* c1, SATClause* c2)
{
  if(c1->length()!=c2->length()) {
    return false;
  }
  for(int i=c1->length();i>=0;i--) {
    if((*c1)[i]!=(*c2)[i]) {
      return false;
    }
  }
  return true;
}


}

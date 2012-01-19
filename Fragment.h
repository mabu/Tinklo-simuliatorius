#ifndef FRAGMENT_H
#define FRAGMENT_H

#include "types.h"
#include <map>

class Fragment
{
  private:
    map<unsigned, unsigned> mMissing;
    Byte*                   mData;
    unsigned                mDataLength;

  public:
    Fragment(unsigned dataLength, Byte* data, unsigned dataOffset,
            unsigned fragmentLength);
    ~Fragment();

    void  add(unsigned dataLength, Byte* data, unsigned dataOffset,
            unsigned fragmentLength);
    bool  isComplete();
    Byte* data();

  private:
    void recreate(unsigned dataLength, Byte* data, unsigned dataOffset,
                  unsigned fragmentLength);
};

#endif

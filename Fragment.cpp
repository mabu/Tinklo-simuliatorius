#include "Fragment.h"
#include <cstring>

Fragment::Fragment(unsigned dataLength, Byte* data, unsigned dataOffset,
                   unsigned fragmentLength)
{
  mDataLength = dataLength;
  if (0 != dataOffset) mMissing.insert(make_pair(0, dataOffset));
  if (dataLength != dataOffset + fragmentLength)
  {
    mMissing.insert(make_pair(dataOffset + fragmentLength, dataLength));
  }
  mData = new Byte[dataLength];
  memcpy(mData + dataOffset, data, fragmentLength);
}

Fragment::~Fragment()
{
  delete[] mData;
}

void Fragment::add(unsigned dataLength, Byte* data, unsigned dataOffset,
                   unsigned fragmentLength)
{
  if (dataLength != mDataLength || dataOffset + fragmentLength > mDataLength)
  {
    recreate(dataLength, data, dataOffset, fragmentLength);
  }
  else
  {
    unsigned i = dataOffset;
    auto it = mMissing.upper_bound(i);
    if (it != mMissing.begin()) --it;
    if (it != mMissing.end() && it->second <= i) ++it;
    for (; it != mMissing.end() && i - dataOffset < fragmentLength;
         mMissing.erase(it++))
    {
      while (i < it->first && i - dataOffset < fragmentLength)
      {
        if (data[i - dataOffset] != mData[i])
        {
          recreate(dataLength, data, dataOffset, fragmentLength);
          return;
        }
        ++i;
      }
      while (i < it->second && i - dataOffset < fragmentLength)
      {
        mData[i] = data[i - dataOffset];
        ++i;
      }
      if (i - dataOffset >= fragmentLength)
      {
        if (i > it->first && i < it->second)
        {
          mMissing.insert(make_pair(i, it->second));
        }
      }
    }
    while (i - dataOffset < fragmentLength)
    {
      if (data[i - dataOffset] != mData[i])
      {
        recreate(dataLength, data, dataOffset, fragmentLength);
        return;
      }
      i++;
    }
  }
}

void Fragment::recreate(unsigned dataLength, Byte* data, unsigned dataOffset,
                        unsigned fragmentLength)
{
  // Gautas kito turinio to paties ilgio paketas nuo to paties gavėjo tuo pačiu ID
  if (dataLength > mDataLength)
  {
    delete[] mData;
    mData = new Byte[dataLength];
  }
  mDataLength = dataLength;
  mMissing.clear();
  mMissing.insert(make_pair(0, dataOffset));
  mMissing.insert(make_pair(dataOffset + fragmentLength, dataLength));
  memcpy(mData + dataOffset, data, fragmentLength);
}

bool Fragment::isComplete()
{
  return mMissing.empty();
}

Byte* Fragment::data()
{
  return mData;
}

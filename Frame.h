#ifndef FRAME_H
#define FRAME_H

#include "types.h"
#include <cstring>

struct Frame
{
  Byte*       data;
  FrameLength length;

  Frame(FrameLength frameLength):
    length(frameLength)
  {
    if (frameLength > 0) data = new Byte[frameLength];
  }

  ~Frame()
  {
    delete[] data;
  }

  Frame(const Frame& rFrame)
  {
    length = rFrame.length;
    data = new Byte[length];
    memcpy(data, rFrame.data, sizeof(Byte) * length);
  }
};

#endif

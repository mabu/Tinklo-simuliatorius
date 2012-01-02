#ifndef FRAME_H
#define FRAME_H
#include "types.h"
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
};

#endif

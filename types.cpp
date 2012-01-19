#include "types.h"

void int_to_bytes(Byte* bytes, unsigned num)
{
  for (unsigned i = 0; i < sizeof(unsigned); i++)
  {
    bytes[i] = (num >> ((sizeof(unsigned) - i - 1) * 8)) & 0xff;
  }
}

unsigned bytes_to_int(Byte* bytes)
{
  unsigned num = 0;
  for (unsigned i = 0; i < sizeof(unsigned); i++)
  {
    num |= bytes[i] << ((sizeof(unsigned) - i - 1) * 8);
  }
  return num;
}

unsigned short bytes_to_short(Byte* bytes)
{
  return (bytes[0] << 8) + bytes[1];
}

void short_to_bytes(Byte* bytes, unsigned short num)
{
  bytes[0] = (num & 0xff00) >> 8;
  bytes[1] =  num & 0x00ff;
}

bool operator < (const timespec& a, const timespec& b)
{
  if (a.tv_sec == b.tv_sec) return a.tv_nsec < b.tv_nsec;
  else return a.tv_sec < b.tv_sec;
}

timespec operator - (const timespec& a, const timespec& b)
{
  timespec result = a;
  result.tv_sec -= b.tv_sec;
  result.tv_nsec -= b.tv_nsec;
  if (result.tv_nsec < 0)
  {
    result.tv_nsec += 1000 * MILLION;
    --result.tv_sec;
  }
  return result;
}

void add_milliseconds(timespec& rTime, int milliseconds)
{
  rTime.tv_sec  += milliseconds / 1000;
  rTime.tv_nsec += milliseconds % 1000 * MILLION;
  rTime.tv_sec += rTime.tv_nsec / (1000 * MILLION);
  rTime.tv_nsec %= 1000 * MILLION;
}

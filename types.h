#ifndef TYPES_H
#define TYPES_H

#include <ctime>

#define BROADCAST_MAC 0xffffffffffff
#define BROADCAST_IP  0xffffffff
#define MILLION 1000000

using namespace std;

typedef unsigned       IpAddress;
typedef long long      MacAddress;
typedef unsigned char  Byte;
typedef unsigned short FrameLength;

void int_to_bytes(Byte* bytes, unsigned num);
unsigned bytes_to_int(Byte* bytes);
unsigned short bytes_to_short(Byte* bytes);
void short_to_bytes(Byte* bytes, unsigned short num);
bool operator < (const timespec& a, const timespec& b);
timespec operator - (const timespec& a, const timespec& b);
void add_milliseconds(timespec& rTime, int milliseconds);

#endif

#include "Node.h"
#include "LinkLayer.h"
#include <cstdio>
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MILLION 1000000

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

Node::Node(int wireSocket, int appSocket, MacAddress macAddress,
           IpAddress ipAddress):
  mWireSocket(wireSocket),
  mAppSocket(appSocket),
  mMacAddress(macAddress),
  mIpAddress(ipAddress),
  mNetworkLayer(this),
  mTransportLayer(this)
{
  FD_ZERO(&mFdSet);
  FD_SET(mWireSocket, &mFdSet);
  FD_SET(mAppSocket,  &mFdSet);
}

Node::~Node()
{
  for (auto it = mSocketToMacSublayer.begin();
       it != mSocketToMacSublayer.end(); it++)
  {
    if (-1 == close(it->first)) perror("Klaida atsijungiant nuo laido");
  }
  for (auto it = mSocketToApp.begin();
       it != mSocketToApp.end(); it++)
  {
    if (-1 == close(it->first)) perror("Klaida atsijungiant nuo programos");
  }
}

void Node::layerMessage(const char* layerName, const char* format, va_list vl)
{
  timespec current;
  clock_gettime(CLOCK_REALTIME, &current);
  printf("[%ld.%09ld] %s: ", current.tv_sec, current.tv_nsec, layerName);
  vprintf(format, vl);
}

void Node::startTimer(Layer* layer, int milliseconds, long long id)
{
  timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  time.tv_sec  += milliseconds / 1000;
  time.tv_nsec += milliseconds % 1000 * MILLION;
  time.tv_sec += time.tv_nsec / (1000 * MILLION);
  time.tv_nsec %= 1000 * MILLION;
  mTimers.insert(make_pair(time, make_pair(layer, id)));
}

IpAddress Node::ipAddress()
{
  return mIpAddress;
}

MacAddress Node::macAddress()
{
  return mMacAddress;
}

void Node::run()
{
  while (1)
  {
    int moreThanMaxSocket = max(mWireSocket, mAppSocket) + 1;
    if (false == mSocketToMacSublayer.empty())
    {
      moreThanMaxSocket = max(moreThanMaxSocket,
                              mSocketToMacSublayer.rbegin()->first + 1);
    }
    if (false == mSocketToApp.empty())
    {
      moreThanMaxSocket = max(moreThanMaxSocket,
                              mSocketToApp.rbegin()->first + 1);
    }
    fd_set tempFdSet = mFdSet;
    timespec timeout;
    if (!mTimers.empty())
    {
      clock_gettime(CLOCK_MONOTONIC, &timeout);
      if (timeout < mTimers.begin()->first)
      {
        timeout = mTimers.begin()->first - timeout;
      }
      else
      {
        timeout.tv_sec = 0;
        timeout.tv_nsec = 0;
      }
    }
    if (pselect(moreThanMaxSocket, &tempFdSet, NULL, NULL,
                (mTimers.empty() ? NULL : &timeout), NULL) < 0)
    {
      perror("select");
      break;
    }

    for (auto it = mSocketToMacSublayer.begin();
         it != mSocketToMacSublayer.end();)
    {
      if (FD_ISSET(it->first, &tempFdSet))
      {
        char voltage;
        int bytesReceived = recv(it->first, &voltage, 1, 0);
        if (bytesReceived == 1) (it++)->second->fromPhysicalLayer(voltage);
        else if (bytesReceived == 0)
        {
          auto old = it++;
          removeLink(old->first, old->second);
        }
        else
        {
          perror("Klaida priimant signalą iš laido");
          break;
        }
      }
      else ++it;
    }

    if (FD_ISSET(mWireSocket, &tempFdSet))
    { // prisijungė laidas
      printf("Prisijungė laidas.\n");
      int wireSocket = accept(mWireSocket, NULL, NULL);
      if (-1 == wireSocket)
      {
        perror("Klaida prijungiant laidą");
        break;
      }
      MacSublayer* pMacSublayer = new MacSublayer(this);
      LinkLayer*   pLinkLayer   = new LinkLayer(this, pMacSublayer);
      mMacToLink.insert(make_pair(pMacSublayer, pLinkLayer));
      mSocketToMacSublayer.insert(make_pair(wireSocket, pMacSublayer));
      mMacSublayerToSocket.insert(make_pair(pMacSublayer, wireSocket));
      mNetworkLayer.addLink(pLinkLayer);
      FD_SET(wireSocket,  &mFdSet);
      //sendRandomFrames(pMacSublayer);
      sendRandomPackets(pLinkLayer);
    }
//    if (FD_ISSET(mAppSocket,  &tempFdSet))

    while (!mTimers.empty())
    {
      timespec current;
      clock_gettime(CLOCK_MONOTONIC, &current);
      if (current < mTimers.begin()->first) break;
      pair<Layer*, long long> timer = mTimers.begin()->second;
      mTimers.erase(mTimers.begin());
      timer.first->timer(timer.second);
    }
  }
}

bool Node::toPhysicalLayer(MacSublayer* pMacSublayer, char voltage)
{
  //printf("Siunčia signalą %hhd\n", voltage);
  auto it = mMacSublayerToSocket.find(pMacSublayer);
  if (it != mMacSublayerToSocket.end())
  {
    if (1 != send(it->second, &voltage, 1, MSG_NOSIGNAL))
    {
      perror("Nepavyko išsiųsti signalo");
      removeLink(it->second, pMacSublayer);
    }
    else return true;
  }
  printf("Laidas atsijungė prieš išsiunčiant signalą.\n");
  return false;
}

void Node::toLinkLayer(MacSublayer* pMacSublayer, MacAddress source,
                       Frame& rFrame)
{
  mMacToLink.find(pMacSublayer)->second->fromMacSublayer(source, rFrame);
}

void Node::toNetworkLayer(MacAddress source, Byte* packet,
                          FrameLength packetLength)
{
  printf("Tinklo lygiui keliaus %hu ilgio paketas nuo %llx.\n", packetLength,
         source);
  // TODO
}

void Node::removeLink(int wireSocket, MacSublayer* pMacSublayer)
{
  printf("Atsijungė laidas.\n");
  if (-1 == close(wireSocket)) perror("Klaida atsijungiant nuo laido");
  auto it = mMacToLink.find(pMacSublayer);
  if (it == mMacToLink.end())
  {
    printf("Jau buvo atsijungta.\n");
    return;
  }
  mNetworkLayer.removeLink(it->second);
  mMacSublayerToSocket.erase(pMacSublayer);
  mSocketToMacSublayer.erase(wireSocket);
  pMacSublayer->selfDestruct();
  it->second->selfDestruct();
  mMacToLink.erase(it);
  FD_CLR(wireSocket,  &mFdSet);
}

void Node::sendRandomFrames(MacSublayer* pMacSublayer)
{
  Frame fr0(0);
  Frame fr2(2);
  Frame fr3(3);
  pMacSublayer->fromLinkLayer(BROADCAST_MAC, &fr0);
  pMacSublayer->fromLinkLayer(0xaa004499bb32, &fr3);
  pMacSublayer->fromLinkLayer(0x003344221122, &fr2);
}

void Node::sendRandomPackets(LinkLayer* pLinkLayer)
{
  Byte foo[2] = {3, 14};
  pLinkLayer->fromNetworkLayer(BROADCAST_MAC, NULL, 0);
  pLinkLayer->fromNetworkLayer(0xaa004499bb32, foo, 2);
  pLinkLayer->fromNetworkLayer(0x003344221122, foo, 1);
}

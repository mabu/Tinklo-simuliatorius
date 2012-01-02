#include "Node.h"
#include "LinkLayer.h"
#include <cstdio>
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MILLION 1000000

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

void Node::layerMessage(const char* format, va_list vl)
{
  vprintf(format, vl);
}

void Node::layerMessage(char* message)
{
  printf("%s", message);
}

void Node::startTimer(Layer* layer, int milliseconds, long long id)
{
  mTimers.push_back(make_pair(clock() + milliseconds * (CLOCKS_PER_SEC / 1000),
                              make_pair(layer, id)));
  push_heap(mTimers.begin(), mTimers.end());
  //printf("Pradėtas skaičiuoti %dms laikas. Dabar eilėje %lu.\n", milliseconds,
  //       mTimers.size());
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
    timeval tv;
    if (!mTimers.empty())
    {
      clock_t current = clock();
      if (current > mTimers.front().first)
      {
        clock_t microsecondsLeft = (mTimers.front().first - current)
                                   / (CLOCKS_PER_SEC / (double) MILLION);
        tv.tv_sec = microsecondsLeft / MILLION;
        tv.tv_usec = microsecondsLeft % MILLION;
      }
      else
      {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
      }
    }
    if (select(moreThanMaxSocket, &tempFdSet, NULL, NULL, (mTimers.empty() ?
                                                           NULL : &tv)) < 0)
    {
      perror("select");
      break;
    }

    for (auto wire : mSocketToMacSublayer)
    {
      if (FD_ISSET(wire.first, &tempFdSet))
      {
        char voltage;
        int bytesReceived = recv(wire.first, &voltage, 1, 0);
        if (bytesReceived == 1) wire.second->fromPhysicalLayer(voltage);
        else if (bytesReceived == 0) removeLink(wire.first, wire.second);
        else
        {
          perror("Klaida priimant signalą iš laido");
          break;
        }
      }
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
    }
//    if (FD_ISSET(mAppSocket,  &tempFdSet))

    while (!mTimers.empty() && mTimers.front().first <= clock())
    {
      pair<Layer*, long long>& timer = mTimers.front().second;
      timer.first->timer(timer.second);
      pop_heap(mTimers.begin(), mTimers.end());
      mTimers.pop_back();
    }
  }
}

void Node::toPhysicalLayer(MacSublayer* pMacSublayer, char voltage)
{
  //printf("Siunčia signalą %hhd\n", voltage);
  int wireSocket = mMacSublayerToSocket.find(pMacSublayer)->second;
  if (1 != send(wireSocket, &voltage, 1, MSG_NOSIGNAL))
  {
    perror("Nepavyko išsiųsti signalo");
    removeLink(wireSocket, pMacSublayer);
  }
}

void Node::toLinkLayer(MacSublayer* pMacSublayer, MacAddress source,
                       Frame& frame)
{
  mMacToLink.find(pMacSublayer)->second->fromMacSublayer(source, frame);
}

void Node::removeLink(int wireSocket, MacSublayer* pMacSublayer)
{
  printf("Atsijungė laidas.\n");
  if (-1 == close(wireSocket)) perror("Klaida atsijungiant nuo laido");
  auto it = mMacToLink.find(pMacSublayer);
  mNetworkLayer.removeLink(it->second);
  mMacSublayerToSocket.erase(pMacSublayer);
  mSocketToMacSublayer.erase(wireSocket);
  pMacSublayer->selfDestruct();
  delete it->second;
  mMacToLink.erase(it);
  FD_CLR(wireSocket,  &mFdSet);
}

void Node::sendRandomFrames(MacSublayer* pMacSublayer)
{
  Frame fr0(0);
  Frame fr2(2);
  Frame fr3(3);
  pMacSublayer->fromLinkLayer(BROADCAST_MAC, fr0);
  pMacSublayer->fromLinkLayer(0xaa004499bb32, fr3);
  pMacSublayer->fromLinkLayer(0x003344221122, fr2);
}

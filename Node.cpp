#include "Node.h"
#include "LinkLayer.h"
#include <cstdio>
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

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
    int moreThanMaxSocket = std::max(mWireSocket, mAppSocket) + 1;
    if (false == mSocketToMacSublayer.empty())
    {
      moreThanMaxSocket = std::max(moreThanMaxSocket,
                              mSocketToMacSublayer.rbegin()->first + 1);
    }
    if (false == mSocketToApp.empty())
    {
      moreThanMaxSocket = std::max(moreThanMaxSocket,
                              mSocketToApp.rbegin()->first + 1);
    }
    fd_set tempFdSet = mFdSet;
    if (select(moreThanMaxSocket, &tempFdSet, NULL, NULL, NULL) <= 0)
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
      mMacToLink.insert(std::make_pair(pMacSublayer, pLinkLayer));
      mSocketToMacSublayer.insert(std::make_pair(wireSocket, pMacSublayer));
      mMacSublayerToSocket.insert(std::make_pair(pMacSublayer, wireSocket));
      mNetworkLayer.addLink(pLinkLayer);
      FD_SET(wireSocket,  &mFdSet);
    }
//    if (FD_ISSET(mAppSocket,  &tempFdSet))
  }
}

void Node::toPhysicalLayer(MacSublayer* pMacSublayer, char voltage)
{
  printf("Siunčia signalą %hhd\n", voltage);
  int wireSocket = mMacSublayerToSocket.find(pMacSublayer)->second;
  if (1 != send(wireSocket, &voltage, 1, MSG_NOSIGNAL))
  {
    perror("Nepavyko išsiųsti signalo");
    removeLink(wireSocket, pMacSublayer);
  }
}

void Node::removeLink(int wireSocket, MacSublayer* pMacSublayer)
{
  printf("Atsijungė laidas.\n");
  if (-1 == close(wireSocket)) perror("Klaida atsijungiant nuo laido");
  auto it = mMacToLink.find(pMacSublayer);
  mNetworkLayer.removeLink(it->second);
  mMacSublayerToSocket.erase(pMacSublayer);
  mSocketToMacSublayer.erase(wireSocket);
  delete pMacSublayer;
  delete it->second;
  mMacToLink.erase(it);
  FD_CLR(wireSocket,  &mFdSet);
}

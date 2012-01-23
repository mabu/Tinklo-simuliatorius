#include "Node.h"
#include "LinkLayer.h"
#include <cstdio>
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <arpa/inet.h> // inet_pton

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
  FD_SET(0,           &mFdSet);
}

Node::~Node()
{
  for (auto it = mSocketToMacSublayer.begin();
       it != mSocketToMacSublayer.end(); it++)
  {
    if (-1 == close(it->first)) perror("Klaida atsijungiant nuo laido");
  }
  for (auto it = mAppSockets.begin(); it != mAppSockets.end(); it++)
  {
    if (-1 == close(*it)) perror("Klaida atsijungiant nuo programos");
  }
}

void Node::layerMessage(const char* layerName, const char* format, va_list vl)
{
  if (strcmp(layerName, "Tinklo lygis")) return;
  timespec current;
  clock_gettime(CLOCK_REALTIME, &current);
  printf("[%ld.%09ld] %s: ", current.tv_sec, current.tv_nsec, layerName);
  vprintf(format, vl);
  fflush(stdout);
}

void Node::startTimer(Layer* layer, int milliseconds, long long id)
{
  timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  add_milliseconds(time, milliseconds);
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
    if (false == mAppSockets.empty())
    {
      moreThanMaxSocket = max(moreThanMaxSocket,
                              *mAppSockets.rbegin() + 1);
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
        if (1 == bytesReceived) (it++)->second->fromPhysicalLayer(voltage);
        else if (0 == bytesReceived)
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

    for (auto it = mAppSockets.begin(); it != mAppSockets.end();)
    {
      if (FD_ISSET(*it, &tempFdSet))
      {
        unsigned char action;
        int bytesReceived = recv(*it, &action, 1, 0);
        if (1 == bytesReceived) mTransportLayer.appAction(*(it++), action);
        else if (0 == bytesReceived) removeApp(*(it++));
        else
        {
          perror("Klaida priimant signalą iš programos");
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
      LinkLayer*   pLinkLayer   = new LinkLayer(this, pMacSublayer,
                                                &mNetworkLayer);
      mMacToLink.insert(make_pair(pMacSublayer, pLinkLayer));
      mSocketToMacSublayer.insert(make_pair(wireSocket, pMacSublayer));
      mMacSublayerToSocket.insert(make_pair(pMacSublayer, wireSocket));
      mNetworkLayer.addLink(pLinkLayer);
      FD_SET(wireSocket, &mFdSet);
    }

    if (FD_ISSET(mAppSocket,  &tempFdSet))
    { // prisijungė programa
      printf("Prisijungė programa.\n");
      int appSocket = accept(mAppSocket, NULL, NULL);
      if (-1 == appSocket)
      {
        perror("Klaida prijungiant programą");
        break;
      }
      mTransportLayer.addApp(appSocket);
      mAppSockets.insert(appSocket);
      FD_SET(appSocket, &mFdSet);
    }

    if (FD_ISSET(0, &tempFdSet))
    {
      char ipStr[4 * 4 + 1];
      if (NULL != fgets(ipStr, sizeof(ipStr), stdin))
      {
        IpAddress ip;
        ipStr[strlen(ipStr) - 1] = '\0'; // nuima \n
        if (1 != inet_pton(AF_INET, ipStr, &ip))
        {
          perror("Netaisyklingas IP adresas");
          printf("%s\n", ipStr);
        }
        else
        {
          printf("Siunčiama į tinklo lygį.\n");
          Byte uninit[128];
          mNetworkLayer.fromTransportLayer(ntohl(ip), uninit, 128);
        }
      }
    }

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

bool Node::isWireIdle(MacSublayer* pMacSublayer)
{
  auto it = mMacSublayerToSocket.find(pMacSublayer);
  if (it == mMacSublayerToSocket.end())
  {
    printf("Laidas atsijungė prieš patikrinant jo aktyvumą.\n");
    return false;
  }
  pollfd pollFd;
  pollFd.fd = it->second;
  pollFd.events = POLLIN | POLLPRI | POLLRDHUP;
  int result = poll(&pollFd, 1, 1);
  if (result == 0) return true;
  else if (result == -1) perror("Klaida tikrinant laido aktyvumą");
  return false;
}

void Node::toLinkLayer(MacSublayer* pMacSublayer, MacAddress source,
                       Frame& rFrame)
{
  mMacToLink.find(pMacSublayer)->second->fromMacSublayer(source, rFrame);
}

void Node::toNetworkLayer(IpAddress destination, Byte* tpdu, unsigned length)
{
  mNetworkLayer.fromTransportLayer(destination, tpdu, length);
}

void Node::toTransportLayer(IpAddress source, Byte* tpdu, unsigned length)
{
  mTransportLayer.fromNetworkLayer(source, tpdu, length);
}

void Node::removeApp(int appSocket)
{
  printf("Atsijungė programa (%d).\n", appSocket);
  if (-1 == close(appSocket)) perror("Klaida atsijungiant nuo programos");
  if (1 == mAppSockets.erase(appSocket))
  {
    mTransportLayer.removeApp(appSocket);
    FD_CLR(appSocket,  &mFdSet);
  }
  else printf("Jau buvo atsijungta nuo programos.\n");
}

void Node::removeLink(int wireSocket, MacSublayer* pMacSublayer)
{
  printf("Atsijungė laidas (%d).\n", wireSocket);
  if (-1 == close(wireSocket)) perror("Klaida atsijungiant nuo laido");
  auto it = mMacToLink.find(pMacSublayer);
  if (it != mMacToLink.end())
  {
    mNetworkLayer.removeLink(it->second);
    mMacSublayerToSocket.erase(pMacSublayer);
    mSocketToMacSublayer.erase(wireSocket);
    pMacSublayer->selfDestruct();
    it->second->selfDestruct();
    mMacToLink.erase(it);
    FD_CLR(wireSocket,  &mFdSet);
  }
  else printf("Jau buvo atsijungta nuo laido.\n");
}

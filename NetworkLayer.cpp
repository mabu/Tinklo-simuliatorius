#include "NetworkLayer.h"
#include "LinkLayer.h"
#include "Node.h"
#include <cstdlib>
#include <ctime>

#define ARP_LENGTH sizeof(Header) + 1 + sizeof(timespec)

NetworkLayer::NetworkLayer(Node* pNode):
  Layer(pNode),
  mLastTimerId(0)
{ }

void NetworkLayer::timer(long long id)
{
  auto it = mTimers.find(id);
  if (it == mTimers.end()
      || mLinks.find(it->second.second) == mLinks.end()) return; // atjungtas
  if (it->second.first == TimerType::SEND_ARP)
  {
    Header header;
    header.protocol = 0;
    header.ttl = 0;
    header.id = 0;
    header.length = ARP_LENGTH;
    header.offset = 0;
    header.source = mpNode->ipAddress();
    header.destination = BROADCAST_IP;
    Byte packet[header.length];
    header.toBytes(packet);
    packet[sizeof(Header)] = 0;
    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    memcpy(packet + sizeof(Header) + 1, &time, sizeof(timespec));
    it->second.second->fromNetworkLayer(BROADCAST_MAC, packet, header.length);
    info("Išsiuntė ARP užklausą.\n");
    startTimer(LS_DELTA, TimerType::SEND_LS, it->second.second);
  }
  else if (it->second.first == TimerType::SEND_LS)
  {
    startTimer(max(0, ARP_PERIOD - LS_DELTA), TimerType::SEND_ARP,
               it->second.second);
  }
  mTimers.erase(it);
}

void NetworkLayer::addLink(LinkLayer* pLinkLayer)
{
  mLinks.insert(pLinkLayer);
  startTimer(rand() % ARP_STARTED, TimerType::SEND_ARP, pLinkLayer);
}

void NetworkLayer::removeLink(LinkLayer* pLinkLayer)
{
  mLinks.erase(pLinkLayer);
  for (auto it = mTimers.begin(); it != mTimers.end();)
  {
    if (it->second.second == pLinkLayer) mTimers.erase(it++);
    else it++;
  }
}

void NetworkLayer::fromLinkLayer(LinkLayer* pLinkLayer, MacAddress source,
                                 Byte* packet, FrameLength packetLength)
{
  Header header(packet);
  if (header.protocol == 0)
  {
    switch (packet[sizeof(Header)])
    {
      case 0:
        info("Gavo ARP užklausą nuo %llx.\n", source);
        packet[sizeof(Header)] = 1;
        header.destination = header.source;
        header.source = mpNode->ipAddress();
        header.toBytes(packet);
        pLinkLayer->fromNetworkLayer(source, packet, packetLength);
        break;
      case 1:
        if (ARP_LENGTH != packetLength)
        {
          info("Blogas ARP atsakymo ilgis %d.\n", packetLength);
          break;
        }
        timespec time;
        clock_gettime(CLOCK_MONOTONIC, &time);
        time = time - *((timespec*)(packet + sizeof(Header) + 1));
        info("Gavo ARP atsakymą nuo %llx praėjus %ld.%09ld.\n", source,
             time.tv_sec, time.tv_nsec);
        break;
      default:
        info("Gautas pektas transporto lygiui.\n");
        // mpNode->toTransportLayer();
    }
  }
}

void NetworkLayer::startTimer(int timeout, TimerType timerType,
                              LinkLayer* pLinkLayer)
{
  mpNode->startTimer(this, timeout, ++mLastTimerId);
  mTimers.insert(make_pair(mLastTimerId, make_pair(timerType, pLinkLayer)));
}

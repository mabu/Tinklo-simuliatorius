#include "NetworkLayer.h"
#include "LinkLayer.h"
#include "Node.h"
#include <cstdlib>
#include <ctime>
#include <algorithm>

#define ARP_LENGTH 1 + sizeof(timespec)

NetworkLayer::NetworkLayer(Node* pNode):
  Layer(pNode),
  mLastTimerId(0)
{ }

void NetworkLayer::timer(long long id)
{
  auto timerIt = mTimers.find(id);
  if (timerIt == mTimers.end()
      || mLinks.find(timerIt->second.second) == mLinks.end()) return; // atjungtas
  if (timerIt->second.first == TimerType::SEND_ARP)
  {
    Header header;
    header.protocol = ARP_PROTOCOL;
    header.ttl = 0;
    header.id = 0;
    header.length = ARP_LENGTH;
    header.offset = 0;
    header.source = mpNode->ipAddress();
    header.destination = BROADCAST_IP;
    Byte packet[sizeof(Header) + header.length];
    header.toBytes(packet);
    packet[sizeof(Header)] = 0;
    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    memcpy(packet + sizeof(Header) + 1, &time, sizeof(timespec));
    timerIt->second.second->fromNetworkLayer(BROADCAST_MAC, packet,
                                             sizeof(Header) + header.length);
    info("Išsiuntė ARP užklausą.\n");
    startTimer(LS_DELTA, TimerType::SEND_LS, timerIt->second.second);
  }
  else if (timerIt->second.first == TimerType::SEND_LS)
  {
    info("Siųs LS.\n");
    timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);
    auto it = mArpCache.begin();
    while (it != mArpCache.end())
    {
      if (it->second.timeout < current) mArpCache.erase(it++);
      else ++it;
    }
    unsigned packetLength = sizeof(Header) + 4 + 8 * mArpCache.size();
    Byte packet[packetLength];
    Header header;
    header.protocol    = LS_PROTOCOL;
    header.ttl         = 255;
    header.id          = 0;
    header.length      = packetLength - sizeof(Header);
    header.offset      = 0;
    header.source      = mpNode->ipAddress();
    header.destination = BROADCAST_IP;
    header.toBytes(packet);
    int_to_bytes(packet + sizeof(Header), current.tv_sec);
    it = mArpCache.begin();
    for (int i = 0; it != mArpCache.end(); it++, ++i)
    {
      int_to_bytes(packet + sizeof(Header) + 4 + 8 * i, it->first);
      int_to_bytes(packet + sizeof(Header) + 8 + 8 * i, it->second.responseTime);
    }
    mNodes[mpNode->ipAddress()].update(packet + sizeof(Header),
                                       packetLength - sizeof(Header));
    kruskal();
    for (auto destinationIp : mSpanningTree)
    {
      it = mArpCache.find(destinationIp);
      it->second.pLinkLayer->fromNetworkLayer(it->second.macAddress, packet,
                                              packetLength);
      info("Išsiųstas LS į %llx.\n", it->second.macAddress);
    }
    startTimer(max(0, ARP_PERIOD - LS_DELTA), TimerType::SEND_ARP,
               timerIt->second.second);
  }
  mTimers.erase(timerIt);
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
  if (header.protocol == ARP_PROTOCOL)
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
        if (sizeof(Header) + ARP_LENGTH != packetLength)
        {
          info("Blogas ARP atsakymo ilgis %d.\n", packetLength);
          break;
        }
        timespec time, responseTime;
        clock_gettime(CLOCK_MONOTONIC, &time);
        responseTime = time - *((timespec*)(packet + sizeof(Header) + 1));
        if (responseTime.tv_sec < 0 || responseTime.tv_nsec < 0)
        {
          info("Blogas ARP atsakymo turinys.\n");
          break;
        }
        info("Gavo ARP atsakymą nuo %llx praėjus %ld.%09ld.\n", source,
             responseTime.tv_sec, responseTime.tv_nsec);
        mArpCache[header.source].update(source, responseTime, time, pLinkLayer);
        break;
      default:
        info("Gautas paketas transporto lygiui.\n");
        // mpNode->toTransportLayer();
    }
    return;
  }
  else if (header.destination == BROADCAST_IP)
  {
    info("Gautas visiems skirtas paketas nuo %x.\n", header.source);
    if (header.ttl > 0)
    {
      --header.ttl;
      header.toBytes(packet);
      for (auto ip : mSpanningTree)
      {
        auto it = mArpCache.find(ip);
        if (it == mArpCache.end())
        {
          info("Nerastas kaimyno %x MAC adresas.\n", ip);
          return;
        }
        if (it->second.macAddress != source)
        {
          it->second.pLinkLayer->fromNetworkLayer(it->second.macAddress, packet,
                                                  packetLength);
          info("Visiems skirtas paketas persiųstas į %x.\n", ip);
        }
      }
      ++header.ttl;
      header.toBytes(packet);
    }
  }
  if (header.protocol == LS_PROTOCOL)
  {
    if (mNodes[header.source].update(packet + sizeof(Header), packetLength
                                                          - sizeof(Header)))
    {
      info("Atnaujinti mazgo %x duomenys.\n", header.source);
      kruskal();
    }
    else
    {
      info("Gauti seni mazgo %x duomenys.\n", header.source);
    }
  }
  else info("Protokolas: %hhu\n", header.protocol);
}

void NetworkLayer::startTimer(int timeout, TimerType timerType,
                              LinkLayer* pLinkLayer)
{
  mpNode->startTimer(this, timeout, ++mLastTimerId);
  mTimers.insert(make_pair(mLastTimerId, make_pair(timerType, pLinkLayer)));
}

void NetworkLayer::kruskal()
{
  mSpanningTree.clear();
  vector<pair<unsigned, pair<unsigned, unsigned> > > edges;
  for (auto& node : mNodes)
  {
    node.second.kruskalSet = node.first;
    for (auto& edge : node.second.neighbours)
    {
      edges.push_back(make_pair(edge.second, make_pair(node.first, edge.first)));
    }
  }
  sort(edges.begin(), edges.end());
  for (auto& edge : edges)
  {
    unsigned setA = kruskalSetOf(edge.second.first);
    unsigned setB = kruskalSetOf(edge.second.second);
    if (setA != setB)
    {
      if (setA < setB) mNodes[edge.second.second].kruskalSet = setA;
      else mNodes[edge.second.first].kruskalSet = setB;

      if (edge.second.second == mpNode->ipAddress())
      {
        mSpanningTree.insert(edge.second.first);
      }
      else if (edge.second.first == mpNode->ipAddress())
      {
        mSpanningTree.insert(edge.second.second);
      }
    }
  }
  info("Atnaujintas minimalus jungiamasis medis.\n");
}

unsigned NetworkLayer::kruskalSetOf(unsigned node)
{
  for (unsigned set = mNodes[node].kruskalSet;
       set != node && set != BROADCAST_IP;
       node = set, set = mNodes[node].kruskalSet);
  return node;
}

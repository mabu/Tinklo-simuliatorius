#include "NetworkLayer.h"
#include "LinkLayer.h"
#include "Node.h"
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <map>

#define ARP_LENGTH 1 + sizeof(timespec)

NetworkLayer::NetworkLayer(Node* pNode):
  Layer(pNode),
  mLastTimerId(0),
  mLastBroadcastId(0)
{
  startTimer(LS_PERIOD, TimerType::SEND_LS, NULL);
}

void NetworkLayer::timer(long long id)
{
  auto timerIt = mTimers.find(id);
  if (timerIt == mTimers.end()) return; // atjungtas
  if (timerIt->second.first == TimerType::SEND_LS)
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
    header.ttl         = BROADCAST_TTL;
    header.id          = ++mLastBroadcastId;
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
      if (it != mArpCache.end())
      {
        if (it->second.pLinkLayer->fromNetworkLayer(it->second.macAddress,
                                                    packet, packetLength))
        {
          info("Išsiųstas LS į %llx.\n", it->second.macAddress);
        }
        else info("Nepavyko išsiųsti LS į %llx.\n", it->second.macAddress);
      }
      else
      {
        info("Nepersiųsta į %x, nes podėlyje nerastas MAC adresas.\n",
             destinationIp);
      }
    }
    startTimer(LS_PERIOD, TimerType::SEND_LS, NULL);
  }
  else if (mLinks.find(timerIt->second.second) == mLinks.end()) return;

  if (timerIt->second.first == TimerType::SEND_ARP)
  {
    Header header;
    header.protocol = ARP_PROTOCOL;
    header.ttl = 0;
    header.id = ++mLastBroadcastId;
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
    if (timerIt->second.second->fromNetworkLayer(BROADCAST_MAC, packet,
                                                 sizeof(Header)
                                                 + header.length))
    {
      info("Išsiuntė ARP užklausą.\n");
    }
    else info("Nepavyko išsiųsti ARP užklausos.\n");
    startTimer(ARP_PERIOD, TimerType::SEND_ARP, timerIt->second.second);
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
        packet[sizeof(Header)] = 1;
        header.destination = header.source;
        header.source = mpNode->ipAddress();
        header.toBytes(packet);
        if (pLinkLayer->fromNetworkLayer(source, packet, packetLength))
        {
          info("Gavo ARP užklausą nuo %llx. Išsiuntė atsakymą.\n", source);
        }
        else
        {
          info("Gavo ARP užklausą nuo %llx, bet atsakymo išsiųsti nepavyko.\n",
               source);
        }
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
        info("Klaida: blogas pirmas ARP baitas.");
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
        }
        else if (it->second.macAddress != source)
        {
          if (it->second.pLinkLayer->fromNetworkLayer(it->second.macAddress,
                                                      packet, packetLength))
          {
            info("Visiems skirtas paketas persiųstas į %x.\n", ip);
          }
          else info("Visiems skirto paketo persiųsti į %x nepavyko.\n", ip);
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
      dijkstras();
    }
    else
    {
      info("Gauti seni mazgo %x duomenys.\n", header.source);
    }
  }
  else if (header.destination == mpNode->ipAddress()
           || header.destination == BROADCAST_IP)
  {
    if (header.length + sizeof(Header) == packetLength)
    {
      mpNode->toTransportLayer(header.source, packet + sizeof(Header),
                               header.length);
    }
    else
    {
      info("Gautas %hu ilgio paketo fragmentas [%hu; %hu).\n", header.length,
           header.offset, header.offset + packetLength - sizeof(Header));
      auto it = mFragments.find(make_pair(header.source, header.id));
      if (it == mFragments.end())
      {
        mFragments.insert(make_pair(make_pair(header.source, header.id),
                                    new Fragment(header.length,
                                             packet + sizeof(Header),
                                             header.offset,
                                             packetLength - sizeof(Header))));
      }
      else
      {
        it->second->add(header.length, packet + sizeof(Header), header.offset,
                        packetLength - sizeof(Header));
        if (it->second->isComplete())
        {
          mpNode->toTransportLayer(header.source, it->second->data(),
                                   header.length);
          delete it->second;
          mFragments.erase(it);
        }
      }
    }
  }
  else
  {
    info("Paketas skirtas kitam mazgui.\n");
    --header.ttl;
    route(header, packet, packetLength);
  }
}

bool NetworkLayer::fromTransportLayer(IpAddress destination, Byte* tpdu,
                                      unsigned length)
{
  info("Gautas %u ilgio paketas iš transporto lygio, adresuotas %x.\n",
       length, destination);
  if (destination == mpNode->ipAddress())
  {
    info("Gavėjas lygus siuntėjui, nesiunčiama.\n");
    return false;
  }
  if (mNodes.find(destination) == mNodes.end() && destination != BROADCAST_IP)
  {
    info("Nežinomas adresatas.\n");
    return false;
  }
  Byte packet[sizeof(Header) + length];
  Byte* pPacket = packet;
  memcpy(packet + sizeof(Header), tpdu, length);
  Header header;
  header.protocol = TRANSPORT_PROTOCOL;
  header.length = length;
  header.offset = 0;
  header.source = mpNode->ipAddress();
  header.destination = destination;
  if (destination == BROADCAST_IP)
  {
    info("Siunčia visiems.\n");
    header.ttl = BROADCAST_TTL;
    header.id = ++mLastBroadcastId;
    header.toBytes(packet);
    bool sent = false;
    do
    {
      info("offset = %hu\n", header.offset);
      unsigned currentLength = min(MAX_PACKET_SIZE,
                                   length + unsigned(sizeof(Header)));
      header.toBytes(pPacket);
      for (auto ip : mSpanningTree)
      {
        auto it = mArpCache.find(ip);
        if (it == mArpCache.end()) info("Nerastas kaimyno %x MAC adresas.\n", ip);
        else
        {
          if (it->second.pLinkLayer->fromNetworkLayer(it->second.macAddress,
                                                      pPacket,
                                                      currentLength))
          {
            info("Visiems skirtas paketas išsiųstas į %x.\n", ip);
            sent = true;
          }
          else info("Nepavyko išsiųsti į %x.\n", ip);
        }
      }
      pPacket += currentLength - sizeof(Header);
      header.offset += currentLength - sizeof(Header);
      length -= currentLength - sizeof(Header);
    }
    while (length);
    return sent;
  }
  else
  {
    auto nodeIt = mNodes.find(destination);
    if (nodeIt != mNodes.end())
    {
      header.id = ++(nodeIt->second.lastId);
      return route(header, packet, sizeof(Header) + length);
    }
    info("Adresato mazgas nerastas, nesiunčiama.\n");
    return false;
  }
}

void NetworkLayer::startTimer(int timeout, TimerType timerType,
                              LinkLayer* pLinkLayer)
{
  mpNode->startTimer(this, timeout, ++mLastTimerId);
  mTimers.insert(make_pair(mLastTimerId, make_pair(timerType, pLinkLayer)));
}

void NetworkLayer::kruskal()
{
  timespec current;
  clock_gettime(CLOCK_MONOTONIC, &current);
  mSpanningTree.clear();
  vector<pair<IpAddress, pair<IpAddress, unsigned> > > edges;
  for (auto it = mNodes.begin(); it != mNodes.end();)
  {
    if (it->second.timeout < current)
    {
      mNodes.erase(it++);
      continue;
    }
    it->second.kruskalSet = it->first;
    for (auto& edge : it->second.neighbours)
    {
      edges.push_back(make_pair(edge.second, make_pair(it->first, edge.first)));
    }
    ++it;
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
  info("Atnaujintas minimalus jungiamasis medis (%u).\n", mSpanningTree.size());
}

unsigned NetworkLayer::kruskalSetOf(IpAddress node)
{
  for (unsigned kruskalSet = mNodes[node].kruskalSet;
       kruskalSet != node && kruskalSet != BROADCAST_IP;
       node = kruskalSet, kruskalSet = mNodes[node].kruskalSet);
  return node;
}

void NetworkLayer::dijkstras()
{
  for (auto& arpCache : mArpCache)
  {
    dijkstra(arpCache.first, arpCache.second.distances);
  }
}

void NetworkLayer::dijkstra(IpAddress root, DistanceMap& rDistances)
{
  rDistances.clear();
  NodeInfo& rRootNode = mNodes[root];
  multimap<Distance, IpAddress> priorityQueue;
  for (auto& rNeighbour : rRootNode.neighbours)
  {
    if (rNeighbour.first == mpNode->ipAddress()
        || mArpCache.find(rNeighbour.first) != mArpCache.end()) continue;
    Distance distance;
    distance.delay = rNeighbour.second + CONSTANT_WEIGTH;
    distance.hops = 1;
    rDistances.insert(make_pair(rNeighbour.first, distance));
    priorityQueue.insert(make_pair(distance, rNeighbour.first));
  }
  for (; !priorityQueue.empty(); priorityQueue.erase(priorityQueue.begin()))
  {
    Distance distance = rDistances.find(priorityQueue.begin()->second)->second;
    if (distance < priorityQueue.begin()->first) continue;
    auto& rNeighbours = mNodes[priorityQueue.begin()->second].neighbours;
    for (auto& rNeighbour : rNeighbours)
    {
      if (rNeighbour.first == mpNode->ipAddress()
          || mArpCache.find(rNeighbour.first) != mArpCache.end()) continue;
      DistanceMap::iterator it = rDistances.find(rNeighbour.first);
      if (it == rDistances.end())
      {
        rDistances.insert(make_pair(rNeighbour.first,
                                    distance + rNeighbour.second));
      }
      else if (distance + rNeighbour.second < it->second)
      {
        it->second = distance + rNeighbour.second;
        priorityQueue.insert(make_pair(it->second, rNeighbour.first));
      }
    }
  }
}

bool NetworkLayer::route(Header& rHeader, Byte* packet, unsigned length)
{
  vector<pair<IpAddress, Distance> > ipDistance;
  vector<double> weights;
  double totalWeight = 0;
  auto it = mArpCache.find(rHeader.destination);
  if (it == mArpCache.end())
  {
    if (mNodes.find(rHeader.destination) == mNodes.end())
    {
      info("Nerastas kelias į adresato mazgą, paketas neišsiųstas.\n");
      return false;
    }
    unsigned long long maxDistance = 0;
    for (auto& arpCache : mArpCache)
    {
      auto distanceIt = arpCache.second.distances.find(rHeader.destination);
      if (distanceIt != arpCache.second.distances.end())
      {
        Distance distance = distanceIt->second + arpCache.second.responseTime;
        ipDistance.push_back(make_pair(arpCache.first, distance));
        if (maxDistance < distance.delay) maxDistance = distance.delay;
      }
    }
    if (ipDistance.empty())
    {
      info("Atrodo, nebėra kelio į adresato mazgą, paketas neišsiųstas.\n");
      return false;
    }
    if (maxDistance == 0)
    {
      info("Klaida: didžiausias atstumas yra 0.\n");
      return false;
    }
    for (auto& ipDist : ipDistance)
    {
      totalWeight += ipDist.second.delay / double(maxDistance);
      weights.push_back(totalWeight);
    }
  }
  else
  {
    info("Siunčia tiesiogiai.\n");
    rHeader.ttl = 0;
  }
  do
  {
    info("offset = %hu\n", rHeader.offset);
    unsigned currentLength = min(MAX_PACKET_SIZE, length);
    rHeader.toBytes(packet);
    if (it != mArpCache.end())
    {
      if (!it->second.pLinkLayer->fromNetworkLayer(it->second.macAddress,
                                                   packet,
                                                   currentLength))
      {
        info("Išsiųsti nepavyko.\n");
        return false;
      }
    }
    else
    {
      unsigned selected = lower_bound(weights.begin(), weights.end(),
                                      totalWeight * (double(rand()) / RAND_MAX))
                          - weights.begin();
      if (selected >= ipDistance.size())
      {
        info("Klaida pasirenkant maršrutą.\n");
        return false;
      }
      info("Pasirinko %x (#%u iš %u galimų).\n", ipDistance[selected].first,
                                                 selected, weights.size());
      rHeader.ttl = ipDistance[selected].second.hops * 3 / 2;
      rHeader.toBytes(packet);
      ArpCache& rArpCache = (mArpCache.find(ipDistance[selected].first))->second;
      if (!rArpCache.pLinkLayer->fromNetworkLayer(rArpCache.macAddress,
                                                  packet, length))
      {
        info("Išsiųsti nepavyko.\n");
        return false;
      }
    }
    packet += currentLength - sizeof(Header);
    rHeader.offset += currentLength - sizeof(Header);
    length -= currentLength - sizeof(Header);
  }
  while (length > sizeof(Header));
  return true;
}

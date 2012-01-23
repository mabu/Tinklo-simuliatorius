#include "TransportLayer.h"
#include "Node.h"
#include <cstdio> // perror
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

TransportLayer::TransportLayer(Node* pNode):
  Layer(pNode),
  mLastPort(0),
  mLastTimer(0)
{ }

void TransportLayer::timer(long long id)
{
  auto it = mTimers.find(id);
  if (it == mTimers.end()) info("Klaida: nerastas laikmatis %lld.\n", id);
  else if (it->second->lastTimer == id)
  {
    if (NULL == it->second->pSelfDestructMap)
    {
      it->second->lastTimer = 0;
      send(it->second);
    }
    else
    {
      it->second->pSelfDestructMap->erase(it->second->selfDestructIt);
      delete it->second;
    }
  }
  mTimers.erase(it);
}

void TransportLayer::fromNetworkLayer(IpAddress source, Byte* tpdu,
                                      unsigned length)
{
  if (0 == checksum(tpdu, length))
  {
    Header header(tpdu);
    info("Gautas %u ilgio %hhu tipo segmentas iš %x:%hu į prievadą %hu.\n",
        length, header.type, source, header.sourcePort, header.destinationPort);
    auto portToAppIt = mPortToApp.find(header.destinationPort);
    if (mPortToApp.end() == portToAppIt)
    {
      info("Jokia programa nenaudoja prievado, kuriam adresuota.");
      reject(source, header);
    }
    else
    {
      auto appIt = mApps.find(portToAppIt->second);
      if (mApps.end() == appIt)
      {
        info("Klaida: nerasti programos duomenys.\n");
        reject(source, header);
      }
      else
      {
        App& rApp = appIt->second;
        switch (header.type)
        {
          case 1:
          { // prisijungia
            auto listenIt = rApp.listen.find(header.destinationPort);
            if (rApp.listen.end() == listenIt)
            {
              info("Niekas nesiklauso.\n");
              reject(source, header);
            }
            else if (listenIt->second.size() >= MAX_LISTEN_QUEUE)
            {
              info("Prisijungimų eilė pilna.\n");
              reject(source, header);
            }
            else if (!newSocket(rApp))
            {
              info("Klaida: visos jungtys užimtos.\n");
              reject(source, header);
            }
            else
            {
              rApp.portToSocket[header.destinationPort].insert(make_pair(
                        make_pair(source, header.sourcePort), rApp.lastSocket));
              rApp.socketToConnection.insert(make_pair(rApp.lastSocket,
                                   new Connection(this, header.destinationPort,
                                                  source, header.sourcePort, 2,
                                                  header.syn + 1)));
              if (ACCEPT == rApp.blocked
                  && rApp.blockedSocketOrPort == header.destinationPort)
              {
                info("Atblokavom.\n");
                rApp.blocked = NONE;
                sendInt(appIt->first, rApp.lastSocket);
              }
              else
              {
                listenIt->second.push(rApp.lastSocket);
                info("Laukia eilėje (#%u).\n", listenIt->second.size());
              }
            }
            break;
          }
          case 2:
          { // patvirtina prisijungimą
            if (CONNECT != rApp.blocked)
            {
              info("Gautas nelauktas prisijungimo patvirtinimas.\n");
            }
            else
            {
              auto socketIt = rApp.portToSocket[header.destinationPort]
                                .find(make_pair(source, header.sourcePort));
              if (rApp.portToSocket[header.destinationPort].end() == socketIt)
              {
                info("Prisijungimo patvirtinimas iš nekomunikuoto taško.\n");
              }
              else if (socketIt->second != rApp.blockedSocketOrPort)
              {
                info("Gautas prisijungimo patvirtinimas ne iš laukto taško.\n");
              }
              else
              {
                rApp.blocked = NONE;
                sendInt(appIt->first, rApp.blockedSocketOrPort);
                auto it = rApp.socketToConnection
                                            .find(rApp.blockedSocketOrPort);
                if (rApp.socketToConnection.end() == it)
                {
                  info("Klaida: nerastas blockedSocketOrPort prisijungimas.\n");
                }
                else
                {
                  Connection& rConnection = *it->second;
                  rConnection.header.ack = header.syn + 1;
                  rConnection.header.syn++;
                  rConnection.header.type = 0;
                  startTimer(&rConnection, SEGMENT_ACK_TIMEOUT);
                }
              }
            }
            break;
          }
          case 3:
          { // atmeta prisijungimą
            if (CONNECT != rApp.blocked)
            {
              info("Gautas nelauktas prisijungimo atmetimas.\n");
            }
            else
            {
              auto socketIt = rApp.portToSocket[header.destinationPort]
                                .find(make_pair(source, header.sourcePort));
              if (rApp.portToSocket[header.destinationPort].end() == socketIt)
              {
                info("Prisijungimo atmetimas iš nekomunikuoto taško.\n");
              }
              else if (socketIt->second != rApp.blockedSocketOrPort)
              {
                info("Gautas prisijungimo atmetimas ne iš laukto taško.\n");
              }
              else
              {
                auto connIt = rApp.socketToConnection.find(socketIt->second);
                if (rApp.socketToConnection.end() == connIt)
                {
                  info("Klaida: nerastas blockedSocketOrPort prisijungimas.\n");
                }
                else if (connIt->second->header.syn + 1 == header.ack)
                {
                  rApp.blocked = NONE;
                  sendInt(appIt->first, -1);
                  rApp.portToSocket[header.destinationPort].erase(socketIt);
                  connIt->second->selfDestruct(&rApp.socketToConnection,
                                               connIt);
                }
                else
                {
                  info("Neatmesta, nes netinkamas ACK (%u vietoj %u).\n",
                       header.ack, connIt->second->header.syn + 1);
                }
              }
            }
            break;
          }
          case 4:
          { // atsijungia
            auto socketIt = rApp.portToSocket[header.destinationPort]
                              .find(make_pair(source, header.sourcePort));
            if (rApp.portToSocket[header.destinationPort].end() == socketIt)
            {
              info("Nori atsijungti, nors nėra prijungtas.\n");
            }
            else
            {
              auto connIt = rApp.socketToConnection.find(socketIt->second);
              if (rApp.socketToConnection.end() == connIt)
              {
                info("Klaida: nerastas prisijungimas pagal jungtį.\n");
              }
              else
              {
                connIt->second->remoteDisconnected = true;
                connIt->second->header.ack = header.syn + 1;
                rApp.portToSocket[header.destinationPort].erase(socketIt);
              }
            }
            break;
          }
        }
      }
    }
  }
  else
  {
    info("Bloga kontrolinė suma %u ilgio segmento iš %x.\n", length, source);
  }
}

void TransportLayer::addApp(int appSocket)
{
  if (mApps[appSocket].lastSocket)
  {
    info("Klaida: programa %d jau buvo prijungta.\n", appSocket);
  }
}

void TransportLayer::removeApp(int appSocket)
{
  if (1 != mApps.erase(appSocket))
  {
    info("Klaida: programa %d jau buvo atjungta.\n", appSocket);
  }
}

void TransportLayer::appAction(int appSocket, unsigned char action)
{
  auto appIt = mApps.find(appSocket);
  if (appIt == mApps.end())
  {
    info("Klaida appAction – programa %d nerasta.\n", appSocket);
    mpNode->removeApp(appSocket);
    return;
  }
  auto& rApp = appIt->second;
  switch (action)
  {
    case 1: // listen
    {
      Port port;
      if (recvPort(appSocket, &port))
      {
        info("[listen] Progama %d, prievadas %hu.\n", appSocket, port);
        if (mPortToApp.insert(make_pair(port, appSocket)).second == true)
        {
          rApp.listen[port];
          sendByte(appSocket, true);
        }
        else
        {
          info("[listen] Šiuo prievadu jau klausomasi.\n");
          sendByte(appSocket, false);
        }
      }
      break;
    }
    case 2: // accept
    {
      Port port;
      if (recvPort(appSocket, &port))
      {
        const int length = sizeof(int) + sizeof(IpAddress) + sizeof(Port);
        info("[accept] Progama %d, prievadas %hu.\n", appSocket, port);
        auto listenIt = rApp.listen.find(port);
        if (listenIt == rApp.listen.end())
        {
          info("[accept] Šiuo prievadu nesiklauso.\n");
          sendBigFalse(appSocket, length);
        }
        else
        {
          auto& rQueue = listenIt->second;
          if (rQueue.empty())
          {
            info("[accept] Eilė tuščia. Programa blokuojama.\n");
            rApp.blocked = ACCEPT;
            rApp.blockedSocketOrPort = port;
          }
          else
          {
            auto it = rApp.socketToConnection.find(rQueue.front());
            if (rApp.socketToConnection.end() == it)
            {
              info("[accept] Klaida – nerastas susijungimas.\n");
              sendBigFalse(appSocket, length);
              break;
            }
            Connection& rConnection = *it->second;
            info("[accept] Prisijungimas %d, IP %u, lizdas %hu.\n",
                 rQueue.front(), rConnection.remoteIp,
                 rConnection.header.destinationPort);
            Byte buffer[length];
            int_to_bytes(buffer, rQueue.front());
            int_to_bytes(buffer + sizeof(int), rConnection.remoteIp);
            short_to_bytes(buffer + sizeof(int) + sizeof(IpAddress),
                           rConnection.header.destinationPort);
            rQueue.pop();
            if (length != ::send(appSocket, buffer, length, 0))
            {
              perror("[accept] Nepavyko nusiųsti duomenų");
              removeApp(appSocket);
            }
          }
        }
      }
      break;
    }
    case 3: // connect
    {
      const int length = sizeof(IpAddress) + sizeof(Port);
      Byte buffer[length];
      if (length != recv(appSocket, buffer, length, 0))
      {
        perror("[connect] Nepavyko gauti duomenų");
        mpNode->removeApp(appSocket);
        break;
      }
      Port oldLastPort = mLastPort;
      do
      {
        ++mLastPort;
      } while (mPortToApp.end() != mPortToApp.find(mLastPort)
               && oldLastPort != mLastPort);
      if (oldLastPort == mLastPort)
      {
        info("[connect] Klaida: visi prievadai užimti.\n");
        sendInt(appSocket, -1);
        return;
      }
      if (!newSocket(rApp))
      {
        info("[connect] Klaida: visos jungtys užimtos.\n");
        sendInt(appSocket, -1);
        return;
      }
      IpAddress remoteIp = bytes_to_int(buffer);
      Port remotePort = bytes_to_short(buffer + 4);
      rApp.portToSocket[mLastPort].insert(make_pair(
                            make_pair(remoteIp, remotePort), rApp.lastSocket));
      rApp.socketToConnection.insert(make_pair(rApp.lastSocket,
                               new Connection(this, mLastPort, remoteIp,
                                              remotePort, 1)));
      rApp.blocked = CONNECT;
      rApp.blockedSocketOrPort = rApp.lastSocket;
      break;
    }
    case 4: // send
    {
      int socket;
      unsigned short length;
      if (recvSocketAndLength(appSocket, &socket, &length))
      {
        info("[send] Programa %d, jungtis %d, ilgis %hu.\n", appSocket, socket,
             length);
        auto connIt = rApp.socketToConnection.find(socket);
        if (rApp.socketToConnection.end() == connIt)
        {
          info("[send] Nurodyta neegzistuojanti jungtis.\n");
          sendInt(appSocket, -1);
          break;
        }
        Connection& rConnection = *connIt->second;
        if (rConnection.header.type == 4)
        {
          info("[send] Norima siųsti uždaryta jungtimis.\n");
          sendInt(appSocket, -1);
        }
        else if (rConnection.header.type != 0)
        {
          info("[send] Klaida: netinkamas tipas %hhu.\n",
               rConnection.header.type);
        }
        else
        {
          Byte buffer[length];
          if (recvAll(appSocket, buffer, length))
          {
            bool wasEmpty = rConnection.sndQueue.empty();
            if (length >= SND_BUFFER - rConnection.sndQueue.size())
            { // užsiblokuos; TODO: atblokavimas
              length = SND_BUFFER - rConnection.sndQueue.size();
              rApp.blocked = SEND;
              rApp.blockedSocketOrPort = socket;
              rApp.blockedRet = length;
            }
            else sendInt(appSocket, length);
            rConnection.sndQueue.insert(rConnection.sndQueue.end(), buffer,
                                        buffer + length);
            if (wasEmpty) send(&rConnection);
          }
        }
      }
      break;
    }
    case 5: // recv
    {
      int socket;
      unsigned short length;
      if (recvSocketAndLength(appSocket, &socket, &length))
      {
        info("[recv] Programa %d, jungtis %d, ilgis %hu.\n", appSocket, socket,
             length);
        auto connIt = rApp.socketToConnection.find(socket);
        if (rApp.socketToConnection.end() == connIt)
        {
          info("[recv] Nurodyta neegzistuojanti jungtis.\n");
          sendInt(appSocket, -1);
          break;
        }
        Connection& rConnection = *connIt->second;
        if (rConnection.header.type != 0 && rConnection.header.type != 4)
        {
          info("[recv] Klaida: netinkamas tipas %hhu.\n",
               rConnection.header.type);
        }
        else
        {
          if (rConnection.rcvQueue.empty())
          {
            rApp.blocked = RECV;
            rApp.blockedSocketOrPort = socket;
            rApp.blockedRet = length;
          }
          else
          {
            if (rConnection.rcvQueue.size() < length)
            {
              length = rConnection.rcvQueue.size();
            }
            sendInt(appSocket, length);
            Byte buffer[length];
            for (int i = 0; i < length; i++)
            {
              buffer[i] = rConnection.rcvQueue[i];
            }
            if (!sendAll(appSocket, buffer, length)) break;
            rConnection.rcvQueue.erase(rConnection.rcvQueue.begin(),
                                       rConnection.rcvQueue.begin() + length);
          }
        }
      }
      break;
    }
    case 6:
    {
      int socket;
      if (sizeof(socket) == recv(appSocket, &socket, sizeof(socket), 0))
      {
        socket = ntohl(socket);
        auto connIt = rApp.socketToConnection.find(socket);
        if (rApp.socketToConnection.end() == connIt)
        {
          info("[close] Programa %d nebuvo prisijungusi prie jungties %d.\n",
               appSocket, socket);
          sendByte(appSocket, false);
        }
        else if (connIt->second->remoteDisconnected)
        {
          info("[close] Programa %d nutraukė jungtį %d.\n", appSocket, socket);
          connIt->second->selfDestruct(&rApp.socketToConnection, connIt);
          sendByte(appSocket, true);
        }
        else
        {
          info("[close] Programa %d, jungtis %d.\n", appSocket, socket);
          connIt->second->header.type = 4;
          sendByte(appSocket, true);
        }
      }
      else
      {
        info("[close] Nepavyko gauti duomenų.\n");
        mpNode->removeApp(appSocket);
      }
      break;
    }
    default:
      info("Klaida: netinkamas %d programos veiksmas %hhu.\n", appSocket,
           action);
  }
}

void TransportLayer::sendByte(int appSocket, Byte value)
{
  if (sizeof(value) != ::send(appSocket, &value, sizeof(value), 0))
  {
    perror("Nepavyko išsiųsti duomenų");
    mpNode->removeApp(appSocket);
  }
}

bool TransportLayer::recvPort(int appSocket, Port* pPort)
{
  if (2 == recv(appSocket, pPort, 2, 0))
  {
    *pPort = ntohs(*pPort);
    return true;
  }
  else
  {
    perror("Nepavyko gauti prievado");
    mpNode->removeApp(appSocket);
    return false;
  }
}

bool TransportLayer::recvSocketAndLength(int appSocket, int* pSocket,
                                         unsigned short* pLength)
{
  const int length = sizeof(int) + sizeof(unsigned short);
  Byte buffer[length];
  if (length == recv(appSocket, buffer, length, 0))
  {
    *pSocket = bytes_to_int(buffer);
    *pLength = bytes_to_short(buffer + sizeof(int));
    return true;
  }
  else
  {
    perror("Nepavyko gauti jungties ir ilgio");
    mpNode->removeApp(appSocket);
    return false;
  }
}

bool TransportLayer::recvAll(int appSocket, Byte* buffer, unsigned short length)
{
  int bytesReceived = 0;
  while (bytesReceived < length)
  {
    int curBytesReceived = recv(appSocket, buffer + bytesReceived,
                                length - bytesReceived, 0);
    if (0 > curBytesReceived)
    {
      perror("[recvAll] Klaida priimant duomenis");
      mpNode->removeApp(appSocket);
      return false;
    }
    else if (0 == curBytesReceived)
    {
      info("[recvAll] Programa atsijungė.\n");
      mpNode->removeApp(appSocket);
      return false;
    }
    bytesReceived += curBytesReceived;
  }
  if (bytesReceived != length)
  {
    info("[recvAll] Klaida: neatitinka gautų baitų kiekis (%d ir %hu).\n",
         bytesReceived, length);
    return false;
  }
  return true;
}

bool TransportLayer::sendAll(int appSocket, Byte* buffer, unsigned short length)
{
  int bytesSent = 0;
  while (bytesSent < length)
  {
    int curBytesSent = ::send(appSocket, buffer + bytesSent, length - bytesSent,
                              0);
    if (0 >= curBytesSent)
    {
      perror("[sendAll] Klaida siunčiant duomenis");
      mpNode->removeApp(appSocket);
      return false;
    }
    bytesSent += curBytesSent;
  }
  if (bytesSent != length)
  {
    info("[sendAll] Klaida: neatitinka išsiųstų baitų kiekis (%d ir %hu).\n",
         bytesSent, length);
    return false;
  }
  return true;
}

void TransportLayer::sendInt(int appSocket, int socket)
{
  socket = htonl(socket);
  if (sizeof(socket) != ::send(appSocket, &socket, sizeof(socket), 0))
  {
    perror("Nepavyko išsiųsti duomenų");
    mpNode->removeApp(appSocket);
  }
}

void TransportLayer::sendBigFalse(int appSocket, int length)
{
  Byte buffer[length];
  memset(buffer, 0xff, length);
  if (length != ::send(appSocket, buffer, length, 0))
  {
    perror("[accept] Nepavyko nusiųsti duomenų");
    mpNode->removeApp(appSocket);
  }
}

bool TransportLayer::newSocket(App& rApp)
{
  auto oldLastSocket = rApp.lastSocket; 
  do
  {
    ++rApp.lastSocket;
  }
  while (rApp.socketToConnection.end()
         != rApp.socketToConnection.find(rApp.lastSocket)
         && oldLastSocket != rApp.lastSocket);
  return oldLastSocket != rApp.lastSocket;
}

void TransportLayer::reject(IpAddress destination, Header header)
{
  swap(header.destinationPort, header.sourcePort);
  header.ack = header.syn + 1;
  header.syn = 0;
  header.type = 3;
  Byte buffer[sizeof(Header)];
  header.toBytes(buffer);
  mpNode->toNetworkLayer(destination, buffer, sizeof(Header));
}

Byte TransportLayer::checksum(Byte* data, int length)
{
  unsigned long long sum = 0;
  for (int i = 0; i < length; i++) sum += data[i];
  while (sum > 0xff)
  {
    sum = sum % 0x100 + sum / 0x100;
  }
  return ~Byte(sum);
}

void TransportLayer::send(Connection* pConnection)
{
  if (0 == pConnection->congestionWindow)
  {
    // TODO: prarastas/neužmezgamas ryšys
  }
  Byte type = pConnection->header.type;
  if (pConnection->header.type == 4
      && pConnection->sndQueue.size() > min(pConnection->congestionWindow,
                                            pConnection->window))
  {
    pConnection->header.type = 0;
  }
  pConnection->header.window = RCV_BUFFER - pConnection->rcvQueue.size();
  auto length = min((unsigned long)(min(pConnection->congestionWindow,
                                        pConnection->window)),
                    pConnection->sndQueue.size());
  Byte buffer[sizeof(Header) + length];
  for (unsigned long i = 0; i < length; i++)
  {
    buffer[sizeof(Header) + i] = pConnection->sndQueue[i];
  }
  pConnection->header.checksum = 0;
  pConnection->header.toBytes(buffer);
  pConnection->header.checksum = checksum(buffer, sizeof(Header) + length);
  pConnection->header.toBytes(buffer);
  mpNode->toNetworkLayer(pConnection->remoteIp, buffer,
                         sizeof(Header) + length);
  pConnection->header.type = type;
  startTimer(pConnection, RTO);
}

void TransportLayer::startTimer(Connection* pConnection, int timeout)
{
  mTimers.insert(make_pair(++mLastTimer, pConnection));
  pConnection->lastTimer = mLastTimer;
  mpNode->startTimer(this, timeout, mLastTimer);
}

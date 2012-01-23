#ifndef TRANSPORTLAYER_H
#define TRANSPORTLAYER_H

#include "Layer.h"
#include <unordered_map>
#include <queue>
#include <vector>
#include <algorithm>
#include "hashes.h"

#define MAX_SEGMENT_SIZE 80
#define RTO 100000 // persiuntimo laikmatis milisekundėmis; TODO – RFC2988
#define SND_BUFFER ((1 << 16) - 1)
#define RCV_BUFFER ((1 << 16) - 1)
#define MAX_LISTEN_QUEUE 10 // kiek daugiau klientų gali eilėje laukti accept()
#define SEGMENT_ACK_TIMEOUT 1000 // kiek milisekundžių laukia iki ACK
                                 // išsiuntimo, kai siuntimo eilė tuščia

class Node;

/**
 * Transporto lygis.
 *
 * Transporto serviso prieigos taškas bus vadinamas prievadu (angl. port) ir
 * bus skaičius nuo 0 iki 2^16 - 1.
 * Primityvai transporto servisui: transport_service.h
 *
 * Segmento antraštė:
 * * siuntėjo prievadas (2 baitai)
 * * gavėjo prievadas   (2 baitai)
 * * SYN laukas         (4 baitai)
 * * ACK laukas         (4 baitai)
 * * lango dydis        (2 baitai)
 * * segmento tipas     (1 baitas)
 * * kontrolinė suma    (1 baitas)
 *
 * Pirmi penki laukai pagal prasmę atitinka analogiškus laukus TCP protokole.
 * Segmento tipų reikšmės:
 * 0 – siunčiami duomenys anksčiau buvusiu susijungimu;
 * 1 – norima pradėti naują prisijungimą; ACK laukas turi būti ignoruojamas;
 * 2 – susijungimo patvirtinimas, ACK ir SYN laukai galiojantys;
 * 3 – prisijungimas nepriimtas (niekas nesiklauso nurodytu prievadu);
 * 4 – atsijungimas (vienpusis – siuntėjas duomenų nebesiųs).
 * 
 * Kontrolinė suma skaičiuojama kaip ir TCP protokole, tačiau ne 2 baitų
 * žodžiais, o po 1 baitą bei neįtraukiant IP pseudoantraštės.
 *
 * Persipildymo valdymas – koks aprašytas A. S. Tannenbaum „Computer Networks“
 * 4 leidimo 6.5.9 skyriaus pabaigoje.
 */
class TransportLayer: public Layer
{
  private:
    typedef unsigned short                  Port;
    struct Connection;
    typedef unordered_map<int, Connection*> ConnectionMap;
    typedef unordered_map<Port, unordered_map<pair<IpAddress, unsigned short>,
                                              int> > PortMap;

    enum BlockType { NONE, ACCEPT, CONNECT, RECV, SEND };

    struct Header
    {
      Port           sourcePort;
      Port           destinationPort;
      unsigned       syn;
      unsigned       ack;
      unsigned short window;
      unsigned char  type;
      unsigned char  checksum;

      Header() { }
      Header(Byte* bytes)
      {
        sourcePort      = bytes_to_short(bytes);
        destinationPort = bytes_to_short(bytes + 2);
        syn             = bytes_to_int(bytes + 4);
        ack             = bytes_to_int(bytes + 8);
        window          = bytes_to_short(bytes + 12);
        type            = bytes[14];
        checksum        = bytes[15];
      }

      void toBytes(Byte* bytes)
      {
        short_to_bytes(bytes, sourcePort);
        short_to_bytes(bytes + 2, destinationPort);
        int_to_bytes(bytes + 4, syn);
        int_to_bytes(bytes + 8, ack);
        short_to_bytes(bytes + 12, window);
        bytes[14] = type;
        bytes[15] = checksum;
      }
    };

    struct Connection
    {
      IpAddress               remoteIp;
      unsigned short          threshold;
      unsigned short          congestionWindow;
      unsigned short          window; // gautas siuntimo langas
      vector<Byte>            sndQueue;
      vector<Byte>            rcvQueue;
      long long               lastTimer;
      bool                    remoteDisconnected : 1;
      Header                  header; // paskutinio išsiųsto segmento antraštė
      ConnectionMap*          pSelfDestructMap;
      ConnectionMap::iterator selfDestructIt;
      

      Connection(TransportLayer* pTransportLayer, Port myPort, IpAddress ip,
                 Port theirPort, unsigned char type, unsigned char ack = 0):
        remoteIp(ip),
        threshold(0),
        congestionWindow(MAX_SEGMENT_SIZE - sizeof(Header)),
        window(0),
        lastTimer(0),
        remoteDisconnected(false),
        pSelfDestructMap(NULL)
      {
        header.sourcePort      = myPort;
        header.destinationPort = theirPort;
        header.type            = type;
        header.ack             = ack;
        timespec current;
        clock_gettime(CLOCK_MONOTONIC, &current);
        header.syn = current.tv_sec * 1000 + current.tv_nsec / MILLION;
        // FIXME: po nulūžimo gali pasirinkti blogą SYN
        pTransportLayer->send(this);
      }

      void selfDestruct(ConnectionMap* pMap, ConnectionMap::iterator it)
      {
        if (lastTimer)
        {
          pSelfDestructMap = pMap;
          selfDestructIt = it;
        }
        pMap->erase(it);
        delete this;
      }
    };

    struct App
    {
      int                              lastSocket;
      ConnectionMap                    socketToConnection;
      unordered_map<Port, queue<int> > listen; // jungčių eilės
      PortMap                          portToSocket;
      BlockType                        blocked;
      int                              blockedSocketOrPort;
      unsigned short                   blockedRet; // ką grąžins send() arba
                                                   // koks buvo recv() length

      App():
        lastSocket(0),
        blocked(NONE)
      { }
    };

  private:
    unordered_map<int, App>               mApps;
    unordered_map<Port, int>              mPortToApp;
    Port                                  mLastPort;
    unordered_map<long long, Connection*> mTimers;
    long long                             mLastTimer;

  public:
    TransportLayer(Node* pNode);
    void timer(long long id); // žr. Layer.h
    void fromNetworkLayer(IpAddress source, Byte* tpdu, unsigned length);
    void addApp(int appSocket);
    void removeApp(int appSocket);
    void appAction(int appSocket, unsigned char action);
    void send(Connection* pConnection);

  protected:
    const char* layerName()
      { return "Transporto lygis"; }

  private:
    void sendByte(int appSocket, Byte value);
    bool recvPort(int appSocket, Port* pPort);
    bool recvSocketAndLength(int appSocket, int* pSocket,
                             unsigned short* pLength);
    bool recvAll(int appSocket, Byte* buffer, unsigned short length);
    bool sendAll(int appSocket, Byte* buffer, unsigned short length);
    void sendInt(int appSocket, int socket);
    void sendBigFalse(int appSocket, int length); // siunčia length 0xff baitų
    
    /**
     * Randa neužimtą jungtį, nustato ją rApp.lastSocket.
     *
     * @param rApp programa, kurios neužimtos jungtįes ieškoti
     * @return true, jei rasti pavyko; false priešingu atveju
     */
    bool newSocket(App& rApp);

    void reject(IpAddress destination, Header header);
    void startTimer(Connection* pConnection, int timeout);

    static Byte checksum(Byte* data, int length);
};

#endif

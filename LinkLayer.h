#ifndef LINKLAYER_H
#define LINKLAYER_H
#include "types.h"
#include "Layer.h"
#include "Frame.h"
#include <unordered_map>
#include <deque>

#define ACK_TIMEOUT           100
#define MIN_FRAME_TIMEOUT    1000LL
#define MAX_FRAME_TIMEOUT   10000LL
#define FRAME_TIMEOUT_DECR     50LL
#define MAX_FRAME_QUEUE_SIZE   10
#define MAX_RETRIES            10

class Node;
class MacSublayer;

/**
 * Kanalinis lygis.
 *
 * Protokolas.
 * Pirmame baite saugoma tarnybinė informacija. Dviejuose vyriausiuose
 * bituose – kadro tipas, tolesniuose trijuose – Seq laukas, likusiuose – Ack.
 * Ryšio užmezgimas.
 * Nusiunčiamas vieno baito, lygaus 0, kadras. Tokį kadrą gavus atgal
 * siunčiamas vieno baito, lygaus 1, kadras. Gavus tokį kadrą galima pradėti
 * duomenų perdavimą.
 * Kai ryšys užmegztas, kadro tipo lauko reikšmė turi būti 1.
 * Seq naudojamas siunčiamų kadrų numeracijai, Ack – gautų kadrų patvirtinimui.
 * Adresu X kadras siunčiamas tik tada, kai jo Seq numeris lygus vėliausiai iš
 * adreso X gautai Ack reikšmei. Po duomenų gavimo praėjus ne daugiau nei
 * ACK_TIMEOUT milisekundžių turi būti išsiunčiamas kadras, kurio Ack laukas 
 * vienetu didesnis už gautąjį Seq. Jei išsiuntus duomenis per nustatytą laiką
 * negaunamas kadras su Ack lauku, vienetu didesniu už siųstą Seq, kadro
 * siuntimas kartojamas. Pirmu kadro siuntimo mėginimu laukimo laikas lygus
 * max(X - FRAME_TIMEOUT_DECR, MIN_FRAME_TIMEOUT), kitais atvejais –
 * atsitiktinis iš intervalo [X, 3X), bet nedidesnis už MAX_FRAME_TIMEOUT;
 * čia X yra paskutinio tam pačiam adresatui siųsto kadro laukimo laikas. Jei
 * per MAX_RETRIES bandymų patvirtinimo nesulaukiama, ryšys nutraukiamas ir
 * visi eilėje buvę kadrai išmetami.
 * Po ryšio užmezgimo pirmo siunčiamo paketo Seq laukas lygus 1, vėlesnių didėja
 * po vieną. Kai Seq viršija MAX_SEQ, jis tampa 0.
 *
 * Siunčiant visiems (į BROADCAST_MAC) ryšys neužmezgamas, paketų gavimas
 * nepatvirtinamas, tarnybinio baito reikšmė neapibrėžta ir nenaudojama.
 */

class LinkLayer: public Layer
{
  private:
    typedef deque<Frame*>   FramePtrQueue;

    struct ControlByte
    {
      unsigned char type : 2; // 0 – užmezgimas, 1 – vienam, 2 – visiems
      unsigned char seq  : 3;
      unsigned char ack  : 3;

      ControlByte(Byte byte):
        type((byte >> 6) & 3),
         seq((byte >> 3) & 7),
         ack(byte & 7)
      { }
      ControlByte():
        type(0),
         seq(0),
         ack(0)
      { }
      ControlByte& operator=(Byte byte)
      {
        type = (byte >> 6) & 3;
        seq  = (byte >> 3) & 7;
        ack  = byte & 7;
        return *this;
      }
      operator Byte()
      {
        return (((Byte)type) << 6) + (((Byte)seq) << 3) + ack;
      }
    };

    /**
     * Nusako ryšio būseną pagal paskutinį išsiųstą nepatvirtintą kadrą.
     * Prieš siunčiant į pirmą kadro baitą įrašomas controlByte.
     */
    struct Connection
    {
      ControlByte   controlByte;
      FramePtrQueue framePtrQueue; // nepristatyti kadrai
      int           timer;         // identifikatorius laikmačio, į kurį reikėtų
                                   // reaguoti pakartotinai išsiunčiant kadrą
      int           timeouts;      // kiek kartų eilės priekyje esantis kadras
                                   // buvo išsiųstas
      int           lastDuration;  // paskiausia laukimo trukmė
      
      Connection():
        controlByte(0),
        timer(0),
        timeouts(0),
        lastDuration(MIN_FRAME_TIMEOUT)
      {
        framePtrQueue.push_back(new Frame(1)); // VALGRIND
        framePtrQueue.back()->data[0] = ControlByte();
      }

      ~Connection()
      {
        while (!framePtrQueue.empty())
        {
          delete framePtrQueue.back();
          framePtrQueue.pop_back();
        }
      }
    };

  private:
    MacSublayer*                                             mpMacSublayer;
    unordered_map<MacAddress, Connection>                    mConnections;
    unsigned long long                                       mTimersStarted;
    unsigned long long                                       mTimersFinished;
    unordered_map<long long, pair<MacAddress, Connection*> > mTimerToConnection;
    MacAddress                                               mLastDestination;
    ControlByte                                              mLastControlByte;
    bool                                                     mIsZombie : 1;

  public:
    LinkLayer(Node* pNode, MacSublayer* pMacSublayer);
    void timer(long long id); // žr. Layer.h
    bool fromNetworkLayer(MacAddress destination, Byte* packet,
                          FrameLength packetLength);
    void fromMacSublayer(MacAddress source, Frame& rFrame);
    void selfDestruct();

  protected:
    const char* layerName()
      { return "Kanalinis lygis"; }

  private:
    void toMacSublayer(MacAddress destination, Connection* pConnection);
    void startTimer(MacAddress destination, Connection* pConnection,
                    bool ack = false);
    void needsAck(MacAddress destination, Connection* pConnection);
    void gotAck(Connection& rConnection);
};

#endif

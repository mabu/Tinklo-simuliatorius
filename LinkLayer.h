#ifndef LINKLAYER_H
#define LINKLAYER_H
#include "types.h"
#include "Layer.h"
#include "Frame.h"
#include <unordered_map>
#include <queue>

#define ACK_TIMEOUT           100
#define FRAME_TIMEOUT         500
#define MAX_FRAME_QUEUE_SIZE   10

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
 * siuntimas kartojamas. Šis laukimo laikas priklauso nuo to, kelintą kartą
 * siunčiamas tas pats kadras, ir yra atsitiktinis iš intervalo
 * [2^x * FRAME_TIMEOUT, 2^(x + 1) * FRAME_TIMEOUT) milisekundėmis
 * (čia x = min(10, kiek kartų buvo siųstas šis kadras anksčiau). Jei per 16
 * siuntimų patvirtinimo nesulaukiama, ryšys nutraukiamas ir visi eilėje buvę
 * kadrai išmetami.
 * Po ryšio užmezgimo pirmo siunčiamo paketo Seq laukas lygus 1, vėlesnių didėja
 * po vieną. Kai Seq viršija MAX_SEQ, jis tampa 0.
 *
 * Siunčiant visiems (į BROADCAST_MAC) ryšys neužmezgamas, paketų gavimas
 * nepatvirtinamas, tarnybinio baito reikšmė neapibrėžta ir nenaudojama.
 */

class LinkLayer: public Layer
{
  private:
    typedef queue<Frame>   FrameQueue;

    struct ControlByte
    {
      bool type : 2; // 0 – užmezgimas, 1 – duomenys, 2 – duomenys visiems
      bool seq  : 3;
      bool ack  : 3;
      ControlByte(Byte byte):
        type(byte & 0xc0),
         seq(byte & 0x38),
         ack(byte & 0x07)
      { }
      ControlByte():
        type(0),
         seq(0),
         ack(0)
      { }
      ControlByte& operator=(Byte byte)
      {
        type = byte & 0xc0;
        seq  = byte & 0x38;
        ack  = byte & 0x07;
        return *this;
      }
      operator Byte()
      {
        return (((Byte)type) << 6) + (((Byte)seq) << 3) + ack;
      }
    };

    /**
     * Nusako ryšio būseną pagal paskutinį išsiųstą nepatvirtintą kadrą.
     * Jei kadro nėra, galima siųsti naują – jį įrašome, pasiėmę controlByte.
     */
    struct Connection
    {
      ControlByte controlByte;
      FrameQueue  frameQueue;  // nepristatyti kadrai
      int         timer;       // identifikatorius laikmačio, į kurį reikėtų
                               // reaguoti pakartotinai išsiunčiant kadrą
      int         timeouts;    // kiek kartų paskutinis kadras buvo pakartinai
                               // išsiųstas
    };

  private:
    MacSublayer*                                mpMacSublayer;
    unordered_map<MacAddress, ControlByte>      mConnections;

  public:
    LinkLayer(Node* pNode, MacSublayer* pMacSublayer);
    void timer(long long id); // žr. Layer.h
    bool fromNetworkLayer(MacAddress destination, Byte* packet,
                          int packetLength);
    void fromMacSublayer(MacAddress source, Frame& frame);

  protected:
    const char* layerName()
      { return "Kanalinis lygis"; }
};

#endif

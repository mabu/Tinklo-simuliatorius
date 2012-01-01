#ifndef MACSUBLAYER_H
#define MACSUBLAYER_H

#include <vector>
#include "types.h"
#include "Layer.h"

#define MAX_DATA_LENGTH      1500 // didžiausias kadro duomenų dalies ilgis
#define MIN_DATA_LENGTH (FrameLength)46 // mažiausias kadro duomenų dalies ilgis
#define MAC_ADDRESS_LENGTH      6 // baitais
#define POSITIVE_VOLTAGE        5
#define NEGATIVE_VOLTAGE       -3
#define CRC_POLYNOMIAL 0x04c11db7 // (1) 0000 0100 1100 0001 0001 1101 1011 0111
#define CHECKSUM_LENGTH        32 // bitais
#define SIGNAL_TIMEOUT      10000 // jei tiek milisekundžių negauna signalo,
                                  // laikoma, kad gavimas nutrūko (leidžiama
                                  // siųsti)
// nuo kelinto bito prasideda kadras:
#define FRAME_START (2 * MAC_ADDRESS_LENGTH + sizeof(FrameLength)) * 8

class Node;

/**
 * MAC polygis.
 *
 * Protokolas.
 * Siunčiami duomenys prasideda baitu 01111110, toliau esantiems duomenims
 * naudojamas bitų įterpimas. Pradžioje eina gavėjo MAC adresas, po to siuntėjo.
 * Toliau – kadro ilgis (2 baitai) ir siunčiamas kadras. Jei kadro ilgis L
 * trumpesnis už 46 baitus, po jo eina 46 - L nulinių baitų užpildas.
 * Pabaigoje 32 bitų CRC, sudarytas pagal visus siųstus duomenis, kuriems buvo
 * naudotas bitų įterpimas, tačiau jam nesant.
 */
class MacSublayer: public Layer
{
  private:
    typedef std::vector<bool> BitVector;

  private:
    BitVector   mOutputBuffer;
    BitVector   mInputBuffer;
    char        mConsequentOnes; // kiek vienetinių bitų išsiuntė/gavo iš eilės
    char        mLastVoltage;
    char        mPreambleBits;  // kiek iš 01111110 bitų sekos buvo paskutiniai
                                // gauti bitai
    long long   mTimersRunning; // jei teigiamas – siuntimas negalimas, nes eina
                                // dar mTimersRunning laikmačių; jei neigiamas –
                                // siuntimas galimas, tačiau dar eina
                                // -mTimersRunning laikmačių
    bool        mReceivingData : 1; // ar jau buvo užfiksuota kadro pradžia
    bool        mJustArrived   : 1; // ar ką tik buvo sėkmingai priimtas kadras
    bool        mIsZombie : 1;  // jei true, bus sunaikintas, kai mTimersRunning
                                // taps nuliu (kad nebūtų SIGSEGV)
    FrameLength mLength;        // priimamų duomenų ilgis

  public:
    MacSublayer(Node* pNode);
    void fromPhysicalLayer(char voltage);

    /**
     * Siunčia kadrą.
     *
     * @param destination gavėjo MAC adresas
     * @param frame       rodyklė į kadro baitus
     * @param length      kadro ilgis baitais
     * @return true, jei pavyko išsiųsti, false – priešingu atveju (pavyzdžiui,
     *         tuo metu laidas buvo naudojamas ir norėta išvengti kolizijos)
     */
    bool fromLinkLayer(MacAddress destination, Byte* frame, FrameLength length);

    /**
     * Bando pakartotinai siųsti vėliausiai supakuotą kadrą.
     * Tai yra efektyvesnis pakartotinas siuntimas, kurį galima naudoti vietoj
     * pakartotino fromLinkLayer() iškvietimo tam pačiam paketui siųsti.
     * @return true, jei pavyko išsiųsti, false – priešingu atveju (pavyzdžiui,
     *         tuo metu laidas buvo naudojamas ir norėta išvengti kolizijos)
     */
    bool sendBuffer();

    void timer(); // žr. Layer.h

    void selfDestruct();

  private:
    void bufferAddresss(MacAddress macAddress);
    void bufferByte(Byte byte);
    void bufferChecksum();
    void sendPreamble();
    void sendByte(Byte byte);
    void sendBit(bool bit);
    void calculateChecksum(BitVector& bufferWithChecksum);
    bool isInputValid();
    void receivedBit(bool bit);
};

#endif

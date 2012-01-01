#ifndef NODE_H
#define NODE_H

#include <cstdarg>
#include <ctime>
#include <vector>
#include <map>
#include <unordered_map>
#include <sys/select.h>
#include "types.h"
#include "MacSublayer.h"
#include "NetworkLayer.h"
#include "TransportLayer.h"

class LinkLayer;

class Node
{
  private:
    int                                          mWireSocket;
    int                                          mAppSocket;
    MacAddress                                   mMacAddress;
    IpAddress                                    mIpAddress;
    NetworkLayer                                 mNetworkLayer;
    TransportLayer                               mTransportLayer;
    std::map<int, MacSublayer*>                  mSocketToMacSublayer;
    std::unordered_map<MacSublayer*, int>        mMacSublayerToSocket;
    std::map<int, int>                           mSocketToApp;
    std::unordered_map<int, int>                 mAppToSocket;
    std::unordered_map<MacSublayer*, LinkLayer*> mMacToLink;
    fd_set                                       mFdSet;
    std::vector<std::pair<clock_t, Layer*> >     mTimers;

  public:
    Node(int wireSocket, int appSocket, MacAddress macAddress,
         IpAddress ipAddress);
    ~Node();

    /**
     * Apdoroja gautą informacinį pranešimą.
     *
     * @param format formatas, žr. man vprintf
     * @param vl     argumentai
     */
    void       layerMessage(const char* format, va_list vl);

    /**
     * Paleidžia laikmatį.
     * Praėjus milliseconds milisekundžių įvykdo layer->timer().
     *
     * @param layer        tinklo lygio esybė, kuriai taikomas laikmatis
     * @param milliseconds už kelių milisekundžių laikmatis turi baigtis
     */
    void       startTimer(Layer* layer, int milliseconds);

    IpAddress  ipAddress();
    MacAddress macAddress();

    /**
     * Pradeda mazgo simuliacija.
     * Įeina į amžiną ciklą. Nutraukiamas SIGINT arba SIGTERM pagalba.
     */
    void       run();

    /**
     * Signalo siuntimas į fizinį lygį.
     *
     * @param pMacSublayer rodyklė į MAC polygį, siunčiantį signalą
     * @param voltage      signalo įtampa
     */
    void toPhysicalLayer(MacSublayer* pMacSublayer, char voltage);

    void toLinkLayer(MacSublayer* pMacSublayer, MacAddress source,
                     Byte* frame, FrameLength length);

  private:
    /**
     * Atjungia nuo laido.
     *
     * @param wireSocket   lizdas, kuriuo buvo prijungta prie laido
     * @param pMacSublayer rodyklė į MAC polygį, susietą su laidu
     */
    void removeLink(int wireSocket, MacSublayer* pMacSublayer);

    void sendRandomFrames(MacSublayer* pMacSublayer); // testavimui
};

#endif

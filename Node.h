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
    int                                              mWireSocket;
    int                                              mAppSocket;
    MacAddress                                       mMacAddress;
    IpAddress                                        mIpAddress;
    NetworkLayer                                     mNetworkLayer;
    TransportLayer                                   mTransportLayer;
    map<int, MacSublayer*>                           mSocketToMacSublayer;
    unordered_map<MacSublayer*, int>                 mMacSublayerToSocket;
    map<int, int>                                    mSocketToApp;
    unordered_map<int, int>                          mAppToSocket;
    unordered_map<MacSublayer*, LinkLayer*>          mMacToLink;
    fd_set                                           mFdSet;
    vector<pair<clock_t, pair<Layer*, long long> > > mTimers;

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

    void       layerMessage(char* message);

    /**
     * Paleidžia laikmatį.
     * Praėjus milliseconds milisekundžių įvykdo layer->timer(id).
     *
     * @param layer        tinklo lygio esybė, kuriai taikomas laikmatis
     * @param milliseconds už kelių milisekundžių laikmatis turi baigtis
     * @param id           kokia reikšmė pasibaigus perduodama layer->timer
     */
    void       startTimer(Layer* layer, int milliseconds, long long id);

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
                     Frame& frame);

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

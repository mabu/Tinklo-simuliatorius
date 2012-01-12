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
    multimap<timespec, pair<Layer*, long long> > mTimers;
    int                                          mWireSocket;
    int                                          mAppSocket;
    MacAddress                                   mMacAddress;
    IpAddress                                    mIpAddress;
    NetworkLayer                                 mNetworkLayer;
    TransportLayer                               mTransportLayer;
    map<int, MacSublayer*>                       mSocketToMacSublayer;
    unordered_map<MacSublayer*, int>             mMacSublayerToSocket;
    map<int, int>                                mSocketToApp;
    unordered_map<int, int>                      mAppToSocket;
    unordered_map<MacSublayer*, LinkLayer*>      mMacToLink;
    fd_set                                       mFdSet;

  public:
    Node(int wireSocket, int appSocket, MacAddress macAddress,
         IpAddress ipAddress);
    ~Node();

    /**
     * Apdoroja gautą informacinį pranešimą.
     *
     * @param layerName lygio, kuris siunčia pranešimą, pavadinimas
     * @param format    formatas, žr. man vprintf
     * @param vl        argumentai
     */
    void       layerMessage(const char* layerName, const char* format,
                            va_list vl);

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
     * @return true, jei nusiųsti pavyko; false, jei atsijungė laidas
     */
    bool toPhysicalLayer(MacSublayer* pMacSublayer, char voltage);

    /**
     * Patikrina, ar laidu neateina duomenys.
     * Fizinio lygio teikiama paslauga MAC polygiui kolizijų prevencijai.
     *
     * @param pMacSublayer besikreipiantis MAC polygis
     * @return true, jei laidas prijungtas ir pasyvus; false, jei kinta laido
     *         įtampą arba laidas atjungtas
     */
    bool isWireIdle(MacSublayer* pMacSublayer);

    void toLinkLayer(MacSublayer* pMacSublayer, MacAddress source,
                     Frame& rFrame);

  private:
    /**
     * Atjungia nuo laido.
     *
     * @param wireSocket   lizdas, kuriuo buvo prijungta prie laido
     * @param pMacSublayer rodyklė į MAC polygį, susietą su laidu
     */
    void removeLink(int wireSocket, MacSublayer* pMacSublayer);
};

#endif

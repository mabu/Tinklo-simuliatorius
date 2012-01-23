#ifndef TRANSPORT_SERVICE_H
#define TRANSPORT_SERVICE_H

#include "types.h"

/**
 * Inicializuoja transporto servisą – prisijungia prie mazgo.
 *
 * @param nodeSocketName mazgo programų jungties pavadinimas
 * @return lizdas, naudojamas komunikacijai su servisu, arba -1, jei prisijungti
 *         nepavyko
 */
int transport_service(const char* nodeSocketName = NULL);

/**
 * Rezervuoja prievadą.
 *
 * @param port prievadas
 * @return true, jei rezervacija pavyko, false, jei šio prievado jau klausomasi
 */
bool listen(unsigned short port);

/**
 * Laukia prisijungimo prie prievado.
 * Prievadas prieš tai turi būti užrezervuotas (žr. listen()).
 *
 * @param port  prievadas
 * @param pIp   jei ne NULL, tuo adresu bus įrašytas kliento IP adresas
 * @param pPort jei ne NULL, tuo adresu bus įrašytas kliento prievadas
 * @return jungtis, per kurią prisijungė, arba -1, jei prievadas nerezervuotas
 */
int accept(unsigned short port, IpAddress* pIp, unsigned short* pPort);

/**
 * Prisijungia prie serverio ip per prievadą port.
 *
 * @param ip   serverio, prie kurio prisijungti, IP adresas
 * @param port serverio prievadas, prie kurio prisijungti
 * @return prisijungimo jungtis arba -1, jei prisijungti nepavyko
 */
int connect(IpAddress ip, unsigned short port);

/**
 * Siunčia duomenis jungtimi.
 *
 * @param socket jungtis
 * @param pData  rodyklė į duomenų pradžią
 * @param length kiek duomenų baitų siųsti
 * @return kiek duomenų baitų nuo pradžios buvo išsiųsta arba -1, jei nurodyta
 *         jungtimi duomenų siųsti negalima
 */
int send(int socket, void* pData, unsigned short length);

/**
 * Priima duomenis iš jungties.
 *
 * @param socket jungtis
 * @param pData  rodyklė į buferio pradžią, kur išsaugoti duomenis
 * @param length kiek duomenų baitų gauti
 * @return kiek duomenų baitų buvo gauta ir įrašyta į pData arba -1, jei
 *         blogai nurodyta jungtis, arba 0, jei kitas jungties galas atsijungė
 */
int recv(int socket, void* pData, unsigned short length);

/**
 * Atsijungia nuo jungties.
 * Jungtimi siųsti duomenų nebebus galima, tačiau bus galima gauti, kol kitas
 * galas neatsijungęs.
 *
 * @param socket jungtis
 * @return true, jei atsijungimas pavyko, false priešingu atveju
 */
bool close(int socket);

#endif

#ifndef NETWORKLAYER_H
#define NETWORKLAYER_H
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "types.h"
#include "Layer.h"

#define ARP_PROTOCOL        0
#define ARP_STARTED      5000
#define ARP_PERIOD      20000
#define ARP_TIMEOUT    100000
#define LS_PROTOCOL         1
#define LS_DELTA        10000 // turi būti mažesnis už ARP_PERIOD
#define LS_TIMEOUT     500000
#define PACKET_TIMEOUT 500000
#define MAX_FRAME_SIZE   1499

class Node;
class LinkLayer;

/**
 * Tinklo polygis.
 *
 * Paketo struktūra.
 * Pirmas baitas – protokolas: ARP_PROTOCOL – ARP, LS_PROTOCOL – LS, kiti –
 * perduodami į transporto lygį.
 * Antras baitas – TTL: kokiam kiekiui mazgų šis paketas dar gali būti
 * persiųstas (kaskart mažinama vienetu).
 * 3–4 baitai – paketo ID.
 * 9-12 baitai – siuntėjo tinklo adresas.
 * 13–16 baitai – gavėjo tinklo adresas.
 *
 * Tolesni paketo duomenys interpretuojami priklausomai nuo protokolo.
 * Jei nurodytas ARP protokolas, jo duomenis sudaro 1 ar daugiau baitų. Kai
 * mazgas gauna paketą su pirmu duomenų baitu lygiu 0, turi atgal siuntėjui
 * siųsti tokį patį paketą, tik pirmam baite įrašęs reikšmę 1. ARP paketams
 * TTL = 0.
 * Jei nurodytas LS protokolas, reiškia duomenis sudaro 4 baitų laiko žymė
 * (sekundės nuo UNIX eros pradžios) ir toliau einantys 8 baitų duomenų blokai:
 * 1–4 batai – tinklo adresas, 5–8 – delsa mikrosekundėmis kanale tarp paketo
 * siuntėjo ir mazgo su 1–4 baituose nurodytu tinklo adresu.
 *
 * Tarnybinių paketų siuntimas tinklo grafo sudarymui.
 * Mazgas LS paketo formavimui laiko kaimynų delsos sąrašą. Kas ARP_PERIOD
 * milisekundžių BROADCAST_IP adresu visiems kaimynams išsiunčia ARP užklausą
 * su periodo pradžios laiko momento identifikatoriumi. Gavęs ARP atsakymus su
 * teisingu identifikatoriumi, išsisaugo jų siuntimo ir gavimo laiką.
 * Po kiekvienos periodinės ARP užklausos praėjus LS_DELTA milisekundžių
 * kaimynai, iš kurių atsakymas gautas seniau nei prieš ARP_TIMEOUT
 * milisekundžių, ištrinami iš sąrašo, o iš likusių suformuojamas LS paketas
 * ir išsiunčiamas BROADCAST_IP adresu.
 * Periodinių ARP paketų siuntimas kiekvienu kanalu prasideda (ir vyksta)
 * skirtingu laiku. Pirmasis paketas išsiunčiamas praėjus atsitiktiniam
 * laikui iš intervalo [0; ARP_STARTED) milisekundėmis po laido prijungimo.
 *
 * Maršrutizavimas.
 * Pagal naujausius iš kiekvieno mazgo, bet ne senesnius nei LS_TIMEOUT
 * milisekundžių, LS paketus sudaromas svorinis orientuotas grafas. Prie
 * kiekvienos briaunos svorio dar pridedama CONSTANT_WEIGTH.
 * Iš kiekvieno kaimyninio mazgo naudojant Dijkstros algoritmą randamas
 * trumpiausias kelias iki kitų mazgų, neinantis per jokį kitą kaimyninį mazgą.
 * Kai reikia siųsti paketą mazgui X, per kurį kaimyninį mazgą jį
 * siųsti nusprendžiama pagal tai, kokie atstumai yra nuo jų iki mazgo X.
 * Sunumeruokime kaimynus, nuo kurių atstumai iki X (kaip buvo apskaičiuoti –
 * neinant per kitus kaimynus) nėra begaliniai, nuo 1 iki N. Tegu d_i yra
 * atstumas nuo i-ojo iš šių kaimynų iki X + atstumas iki šio kaimyno.
 * Tegu a_i = max(d_1, d_2, ..., d_N) / d_i. Tada tikimybė, kad siuntimui į X
 * bus pasirinktas kaimynas i lygi a_i / (a_1 + a_2 + ... + a_N).
 * Išimtis – siuntimas BROADCAST_IP adresu.
 * Gavus paketą, adresuotą BROADCAST_IP adresu, sudaromas minimalus jungiamasis
 * medis naudojant Kruskalo algoritmą (arba naudojamas anksčiau sudarytas, jei
 * grafas nepakito). Svarbu, jog gavus LS paketą pirmiau būtų perskaičiuojami
 * atstumai, ir tik po to formuojamas jungiamasis medis. Šis paketas, nepakeitus
 * gavėjo adreso, persiunčiamas visiems kaimynams, su kuriais yra jungiamojo
 * medžio briauna, išskyrus tą, nuo kurio paketas buvo gautas. Be to, jeigu
 * paketas nėra tarnybinio protokolo, jis perduodamas transporto lygiui.
 * Atstumų perskaičiavimas.
 * Pakitus briaunos svoriui iš X į Y (jei briauna susikuria, laikykime, kad
 * X lygus begalybei, o jei briauna išnyksta, kad Y lygus begalybei) Dijkstros
 * algoritmu atstumus reikės perskaičiuoti tik iki tų mazgų, iki kurių atstumas
 * buvo didesnis už min(X, Y).
 *
 * Persipildymo valdymas.
 * Apkrova paskirstoma tolygiai pagal pralaidumą.
 * Siunčiant paketą žingsnių skaitliukui suteikiama pradinė reikšmė nedidesnė,
 * nei pusantro karto tiek, koks atstumas turėtų būti pagal siunčiančiojo
 * mazgo numatymą.
 * ARP paketai kanaliniame lygyje su visais laukia bendroje eilėje, todėl
 * laukimas yra įtraukiamas skaičiuojant briaunos svorį.
 *
 * Fragmentavimas.
 * Didžiausias paketo ilgis – 2^16 - 1. Jis siunčiamas ne didesniais, nei
 * MAX_PACKET_SIZE fragmentais. Siunčiant fragmentą, lauke offset nurodoma,
 * kelintu baitu nuo paketo pradžios prasideda šio fragmento duomenys.
 * Fragmentų priklausymas tam pačiam paketui nustatomas pagal ID lauką. Jei po
 * paskutinio tam tikram paketui gauto fragmento praėjo
 * PACKET_TIMEOUT milisekundžių, paketas išmetamas.
 */
class NetworkLayer: public Layer
{
  private:
    enum class TimerType: unsigned char { SEND_ARP, SEND_LS };

    struct Header
    {
      unsigned char  protocol;
      unsigned char  ttl;
      unsigned short id;
      unsigned short length;
      unsigned short offset;
      unsigned int   source;
      unsigned int   destination;

      Header() { }
      Header(Byte* bytes)
      {
        protocol    = bytes[0];
        ttl         = bytes[1];
        id          = bytes_to_short(bytes + 2);
        length      = bytes_to_short(bytes + 4);
        offset      = bytes_to_short(bytes + 6);
        source      = bytes_to_int(bytes + 8);
        destination = bytes_to_int(bytes + 12);
      }

      void toBytes(Byte* bytes)
      {
        bytes[0] = protocol;
        bytes[1] = ttl;
        short_to_bytes(bytes + 2, id);
        short_to_bytes(bytes + 4, length);
        short_to_bytes(bytes + 6, offset);
        int_to_bytes(bytes + 8,  source);
        int_to_bytes(bytes + 12, destination);
      }
    };

    struct ArpCache
    {
      MacAddress macAddress;
      unsigned   responseTime;
      timespec   timeout;
      LinkLayer* pLinkLayer;

      void update(MacAddress m, timespec& r, timespec& t, LinkLayer* p)
      {
        macAddress = m;
        responseTime = r.tv_sec * 1000 + (r.tv_nsec + (MILLION / 2)) / MILLION;
        timeout = t;
        add_milliseconds(timeout, ARP_TIMEOUT);
        pLinkLayer = p;
      }
    };

    struct NodeInfo
    {
      unsigned                syn;        // LS paketo identifikatorius
      timespec                timeout;    // duomenų galiojimo laikas
      vector<pair<int, int> > neighbours; // kaimynų IP, atstumai iki jų
      unsigned                kruskalSet; // Kruskalo algoritmui

      NodeInfo(): syn(0), kruskalSet(BROADCAST_IP) { }

      bool update(Byte* data, int length)
      {
        if (length < 12 || (length - 4) % 8 != 0 || bytes_to_int(data) <= syn)
        {
          return false;
        }
        clock_gettime(CLOCK_MONOTONIC, &timeout);
        add_milliseconds(timeout, LS_TIMEOUT);
        neighbours.clear();
        for (int i = 4; i < length; i += 8)
        {
          neighbours.push_back(make_pair(bytes_to_int(data + i),
                                         bytes_to_int(data + i + 4)));
        }
        return true;
      }
    };

  private:
    unordered_set<LinkLayer*>                              mLinks;
    long long                                              mLastTimerId;
    unordered_map<long long, pair<TimerType, LinkLayer*> > mTimers;
    unordered_map<unsigned, ArpCache>                      mArpCache;
    unordered_set<unsigned>                                mSpanningTree;
    unordered_map<unsigned, NodeInfo>                      mNodes;

  public:
    NetworkLayer(Node* pNode);
    void timer(long long id); // žr. Layer.h
    void addLink(LinkLayer* pLinkLayer);
    void removeLink(LinkLayer* pLinkLayer);
    void fromLinkLayer(LinkLayer* pLinkLayer, MacAddress source, Byte* packet,
                       FrameLength packetLength);

  protected:
    const char* layerName()
      { return "Tinklo lygis"; }

  private:
    void     startTimer(int timeout, TimerType timerType, LinkLayer* pLinkLayer);
    void     kruskal();
    unsigned kruskalSetOf(unsigned node);
};

#endif

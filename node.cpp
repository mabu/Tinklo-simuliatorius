/**
 * Mazgas.
 * Mazgus galima sujungti laidais. Programos gali naudotis jų tinklo paslauga,
 * naudodamos biblioteką, kuri dar nerealizuota.
 *
 * Naudojimas: node [pavadinimas] mac ip
 * Jei pavadinimas nenurodomas, jis sutapatinamas su mac adresu.
 * mac – mazgo aparatinis adresas, susidedantis iš 12 šešioliktainių skaitmenų,
 *       galimai atskirtų minusais arba dvitaškiais;
 * ip  – mazgo tinklo adresas, pateiktas įprastu IPv4 formatu
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h> // inet_pton

#include "common.h"
#include "Node.h"

#define BACKLOG             10 // maksimalus prisijungimų prie lizdo eilės ilgis
#define HEXS_IN_MAC_ADDRESS 12 // šešioliktainių simbolių MAC adrese kiekis
#define USAGE_INFO "Naudojimas: node [pavadinimas] adresas\nJei pavadinimas nenurodomas, jis sutapatinamas su mac adresu.\n\
                    mac – mazgo aparatinis adresas, susidedantis iš 12\n\
                          šešioliktainių skaitmenų,\n\
                          galimai atskirtų minusais arba dvitaškiais;\n\
                    ip  – mazgo tinklo adresas, pateiktas įprastu IPv4\n\
                          formatu.\n"

using namespace std;

Node*    gpNode;
char     gWireSocketName[FILENAME_MAX]; // lizdo pavadinimas laidams
char     gAppSocketName[FILENAME_MAX];  // lizdo pavadinimas aplikacijų lygiui
int      gWireSocket;                   // lizdas laidams
int      gAppSocket;                    // lizdas programoms jungtis prie
                                        // transporto lygio

void destroy_and_exit(int sig = 0)
{
  printf("Išsijunginėja...\n");
  if (0 != gWireSocket)
  {
    if (-1 == close(gWireSocket)) perror("Klaida atjungiant laidų lizdą");
    unlink(gWireSocketName);
  }
  if (0 != gAppSocket)
  {
    if (-1 == close(gAppSocket)) perror("Klaida atjungiant programų lizdą");
    unlink(gAppSocketName);
  }
  if (gpNode != NULL) delete gpNode;
  exit(!sig);
}

/**
 * Sukuria besiklausantį Unix lizdą.
 *
 * @param name lizdo (failo) vardas
 * @return lizdo deskriptorius, arba -1, jei jo sukurti nepavyko
 */
int create_listening_socket(char* name)
{
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (-1 == sock) return -1;
  sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, name);
  if (0 != bind(sock, (sockaddr*)&addr,
                strlen(addr.sun_path) + sizeof(addr.sun_family)))
  {
    close(sock);
    return -1;
  }
  if (0 != listen(sock, BACKLOG)) return -1;
  return sock;
}

/**
 * Paverčia simbolių eilutę MAC adresu.
 *
 * @param macStr adresas, išreikštas 12 šešioliktainių skaitmenų, galimai
 *               skiriamų minusais, dvitaškiais
 * @return adreso skaitinė išraiška arba -1, jei duota eilutė netaisyklinga
 */
MacAddress parse_mac_address(char* macStr)
{
  MacAddress macAddress = 0;
  char symbolsFound = 0;
  for (char* p = macStr; *p != '\0'; p++)
  {
    if (*p >= '0' && *p <= '9')      macAddress += *p - '0';
    else if (*p >= 'a' && *p <= 'f') macAddress += *p - 'a' + 10;
    else if (*p >= 'A' && *p <= 'F') macAddress += *p - 'A' + 10;
    else if (*p == ':' || *p != '-') continue;
    else break;

    if (HEXS_IN_MAC_ADDRESS < ++symbolsFound) break;
    else macAddress <<= 4;
  }
  if (HEXS_IN_MAC_ADDRESS != symbolsFound) return -1;
  return macAddress;
}

int main(int argc, char* argv[])
{
  if (argc < 3 || argc > 4)
  {
    printf(USAGE_INFO);
    return 1;
  }
  if (strlen(argv[1]) + 4 >= FILENAME_MAX)
  {
    printf("Pavadinimas turi neviršyti %d simbolių ilgio.\n%s",
           FILENAME_MAX - 5, USAGE_INFO);
    return 1;
  }
  MacAddress macAddress = parse_mac_address(argv[argc - 2]);
  if (macAddress == -1)
  {
    printf("Netaisyklingas mazgo adresas.\n%s", USAGE_INFO);
    return 1;
  }
  IpAddress ipAddress;
  if (1 != inet_pton(AF_INET, argv[argc - 1], &ipAddress))
  {
    perror("Netaisyklingas IP adresas");
    printf(USAGE_INFO);
    return 1;
  }
  strcpy(gWireSocketName, argv[1]);
  strcpy(gAppSocketName,  argv[1]);
  strcat(gAppSocketName,  ".app");

  // gaudom signalus gražiam išsijungimui
  if (false == set_signals_handler(destroy_and_exit))
  {
    perror("Nepavyko nustatyti signalų apdorojimo funkcijos");
    return 1;
  }

  gWireSocket = create_listening_socket(gWireSocketName);
  if (gWireSocket == -1)
  {
    perror("Nepavyko sukurti lizdo laidams");
    return 1;
  }
  gAppSocket = create_listening_socket(gAppSocketName);
  if (gAppSocket == -1)
  {
    perror("Nepavyko sukurti lizdo programoms");
    return 1;
  }

  gpNode = new Node(gWireSocket, gAppSocket, macAddress, ipAddress);
  printf("Startuoja...\n");
  gpNode->run();

  return 0;
}

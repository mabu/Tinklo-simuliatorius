/**
 * Laidas.
 * Informacijos tarp mazgų perdavimo terpė. Ja gali naudotis 2 ar daugiau mazgų.
 * Laidą įkišti į mazgą galima interaktyviai arba per paleidimo argumentus
 * nurodant mazgų pavadinimus.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cerrno>
#include <map>
#include <unordered_map>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"

#define CHEAT "siųsk " // parašius po šito vieną simbolį, jį išsiunčia laidu
#define SEND_BUFFER_SIZE 10000000 // lizdų siuntimo buferių dydžiai baitais

using namespace std;

fd_set gFdSet; // aibė UNIX lizdų kiekvienam mazgui
map<int, string> gSocketToName;
unordered_map<string, int> gNameToSocket;

/**
 * Uždaro visus atidarytus lizdus ir nutraukia programos darbą.
 *
 * @param sig signalo numeris, jei jis įvyko
 */
void close_and_exit(int sig = 0)
{
  printf("Išsijunginėja...\n");
  for (auto it = gSocketToName.begin(); it != gSocketToName.end(); it++)
  {
    printf("Atjungia %s\n", (*it).second.c_str());
    if (-1 == close((*it).first)) perror("Klaida uždarant lizdą");
  }
  exit(!sig);
}

/**
 * Prijungia nurodytą mazgą.
 *
 * @param node mazgo pavadinimas, koks buvo nurodytas sukuriant mazgą
 * @return true, jei prijungimas pavyksta; false, priešingu atveju
 */
bool connect_node(char* node)
{
  int nodeSocket = socket(AF_UNIX, SOCK_STREAM, 0);
  unsigned size = SEND_BUFFER_SIZE;
  if (0 != setsockopt(nodeSocket, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)))
  {
    perror("setsockopt SO_SNDBUF");
  }
  if (nodeSocket == -1) return false;
  sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, node);
  if (connect(nodeSocket, (sockaddr*)&addr,
      strlen(addr.sun_path) + sizeof(addr.sun_family)) == -1)
  {
    close(nodeSocket);
    return false;
  }
  gSocketToName.insert(make_pair(nodeSocket, string(node)));
  gNameToSocket.insert(make_pair(string(node), nodeSocket));
  FD_SET(nodeSocket, &gFdSet);
  return true;
}

/**
 * Atjungia nurodytą mazgą.
 *
 * @param node mazgo pavadinimas, koks buvo nurodytas prijungiant mazgą
 * @return true, jei atjungimas pavyksta; false, priešingu atveju
 */
bool disconnect_node(const char* node)
{
  auto it = gNameToSocket.find(string(node));
  if (it == gNameToSocket.end()) return false;
  int nodeSocket = it->second;
  gSocketToName.erase(nodeSocket);
  gNameToSocket.erase(it);
  FD_CLR(nodeSocket, &gFdSet);
  return close(nodeSocket) == 0;
}

void send_signal(int sender, char valueToSend)
{
  //printf("Siunčiama reikšmė: %hhd\n", valueToSend);
  for (auto it = gSocketToName.begin(); it != gSocketToName.end();)
  {
    if (it->first != sender)
    {
      if (send(it->first, &valueToSend, 1, MSG_NOSIGNAL | MSG_DONTWAIT) != 1)
      {
        if (errno == ECONNRESET || errno == EPIPE)
        {
          printf("Besiunčiant atsijungė mazgas %s\n", it->second.c_str());
          if (!disconnect_node((it++)->second.c_str()))
          {
            perror("Klaida užbaigiant ryšį");
          }
          continue;
        } else {
          perror("Klaida siunčiant");
          printf("Nepavyko nusiųsti į mazgą %s\n", it->second.c_str());
        }
      }
      //else printf("Nusiųsta į %s\n", it->second.c_str());
    }
    ++it;
  }
}

int main(int argc, char* argv[])
{
  // gaudom signalus gražiam išsijungimui
  if (false == set_signals_handler(close_and_exit))
  {
    perror("Nepavyko nustatyti signalų apdorojimo funkcijos");
    return 1;
  }

  FD_ZERO(&gFdSet);
  FD_SET(0, &gFdSet); // stdin

  // prijungiam mazgus, perduotus parametrais
  for (int i = 1; i < argc; i++)
  {
    if (gNameToSocket.find(string(argv[i])) != gNameToSocket.end())
    {
      printf("Prie mazgo %s jau buvo prisijungta.\n", argv[i]);
      continue;
    }
    printf("Jungiamasi prie mazgo %s\n", argv[i]);
    if (false == connect_node(argv[i]))
    {
      perror("Nepavyko prisijungti prie mazgo");
    }
  }

  printf("Norėdami prijungti ar atjungti mazgą, įrašykite jo pavadinimą. Norėdami gauti prijungtų mazgų sąrašą, spauskite „Įvesti“.\n");

  while (1)
  {
    fd_set tempFdSet = gFdSet;
    int moreThanMaxSocket;
    if (gSocketToName.empty()) moreThanMaxSocket = 1;
    else moreThanMaxSocket = gSocketToName.rbegin()->first + 1;
    if (select(moreThanMaxSocket, &tempFdSet, NULL, NULL, NULL) <= 0)
    {
      perror("select");
      close_and_exit();
    }

    // paimame siunčiamus signalus
    char valueToSend = 0;
    int sender = 0;
    for (auto it = gSocketToName.begin(); it != gSocketToName.end();)
    {
      if (FD_ISSET(it->first, &tempFdSet))
      {
        if (sender == 0) sender = it->first;
        else sender = -1;
        char volts;
        if (0 == recv(it->first, &volts, 1, 0))
        {
          printf("Atsijungė mazgas %s\n", it->second.c_str());
          if (sender > 0) sender = 0;
          if (!disconnect_node((it++)->second.c_str()))
          {
            perror("Klaida užbaigiant ryšį");
          }
          continue;
        }
        else
        {
          valueToSend += volts;
        }
      }
      it++;
    }

    // siunčiame
    if (sender != 0)
    {
      if (sender == -1) printf("Kolizija.\n");
      send_signal(sender, valueToSend);
    }

    // prijungimas/atjungimas
    char name[FILENAME_MAX];
    if (FD_ISSET(0, &tempFdSet))
    {
      if (NULL != fgets(name, FILENAME_MAX, stdin))
      {
        int len = strlen(name);
        if (len == sizeof(CHEAT) + 1  && strncmp(name, CHEAT, sizeof(CHEAT)))
        {
          send_signal(0, name[sizeof(CHEAT)]);
        }
        if (name[len - 1] == '\n') name[len - 1] = '\0';
        if (name[0] == '\0')
        {
          for (auto it = gNameToSocket.begin(); it != gNameToSocket.end(); it++)
          {
            printf("%s\n", it->first.c_str());
          }
        }
        else
        {
          if (gNameToSocket.find(string(name)) == gNameToSocket.end())
          {
            printf("Prijungiame %s\n", name);
            if (connect_node(name)) printf("Prijungta.\n");
            else perror("Prijungti nepavyko");
          }
          else
          {
            printf("Atjungiame %s\n", name);
            if (disconnect_node(name)) printf("Atjungta.\n");
            else perror("Klaida atjungiant");
          }
        }
      }
    }
  }

  return 0;
}

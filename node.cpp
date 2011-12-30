/**
 * Mazgas.
 * Mazgus galima sujungti laidais. Programos gali naudotis jų tinklo paslauga,
 * naudodamos biblioteką, kuri dar nerealizuota.
 *
 * Naudojimas: node [pavadinimas] adresas
 * Jei pavadinimas nenurodomas, jis sutapatinamas su adresu. Adresas –
 * 12 šešioliktainių skaitmenų, galimai atskirtų minusais arba dvitaškiais.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "common.h"

using namespace std;

void close_and_exit(int sig)
{
  exit(!sig);
}

int main()
{
  // gaudom signalus gražiam išsijungimui
  if (false == set_signals_handler(close_and_exit))
  {
    perror("Nepavyko nustatyti signalų apdorojimo funkcijos");
    return 1;
  }
  return 0;
}

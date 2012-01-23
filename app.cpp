#include <cstdio>
#include "transport_service.h"

int main(int argc, char* argv[])
{
  if (argc == 2)
  {
    if (-1 == transport_service(argv[1]))
    {
      printf("Nepavyko prisijungti prie mazgo %s.\n", argv[1]);
    }
  }
  else
  {
    printf("Nurodykite vieną parametrą – mazgą, prie kurio prisijungti.\n");
    return 1;
  }
  return 0;
}

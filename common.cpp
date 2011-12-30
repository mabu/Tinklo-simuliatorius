#include <csignal>
#include <cstdlib>

#include "common.h"

using namespace std;

bool set_signals_handler(void(*handler)(int))
{
  struct sigaction sa;
  sa.sa_handler = handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGINT,  &sa, NULL) == -1) return false;
  if (sigaction(SIGTERM, &sa, NULL) == -1) return false;
  return true;
}

#include "transport_service.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

int transport_service(const char* nodeSocketName)
{
  static int appSocket = -1;
  if (NULL == nodeSocketName) return appSocket;
  if (-1 != appSocket) close(appSocket);
  appSocket = socket(AF_UNIX, SOCK_STREAM, 0);
  sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, nodeSocketName);
  if (-1 == connect(appSocket, (sockaddr*)&addr,
                    strlen(addr.sun_path) + sizeof(addr.sun_family)))
  {
    close(appSocket);
    appSocket = -1;
  }
  return appSocket;
}

bool listen(unsigned short port)
{
  Byte buffer[3];
  buffer[0] = 1;
  short_to_bytes(buffer + 1, port);
  if (sizeof(buffer) != send(transport_service(), &buffer, sizeof(buffer), 0))
  {
    return false;
  }
  bool result;
  if (sizeof(result) != recv(transport_service(), &result, sizeof(result), 0))
  {
    return false;
  }
  return result;
}

int accept(unsigned short port, IpAddress* pIp, unsigned short* pPort)
{
  Byte buffer[3];
  buffer[0] = 1;
  short_to_bytes(buffer + 1, port);
  if (sizeof(buffer) != send(transport_service(), &buffer, sizeof(buffer), 0))
  {
    return false;
  }
  const int length = sizeof(int) + sizeof(IpAddress) + sizeof(unsigned short);
  Byte result[length];
  if (length != recv(transport_service(), &result, length, 0))
  {
    return false;
  }
  if (pIp != NULL) *pIp = bytes_to_int(result + sizeof(int));
  if (pPort != NULL) *pPort = bytes_to_short(result + 2 * sizeof(int));
  return bytes_to_int(result);
}

int connect(IpAddress ip, unsigned short port)
{
  Byte buffer[7];
  buffer[0] = 3;
  int_to_bytes(buffer + 1, ip);
  short_to_bytes(buffer + 1 + sizeof(IpAddress), port);
  if (sizeof(buffer) != send(transport_service(), &buffer, sizeof(buffer), 0))
  {
    return -1;
  }
  if (sizeof(int) != recv(transport_service(), &buffer, sizeof(int), 0))
  {
    return -1;
  }
  return bytes_to_int(buffer);
}

int send(int socket, void* pData, unsigned short length)
{
  Byte buffer[1 + sizeof(int) + sizeof(unsigned short) + length];
  buffer[0] = 4;
  int_to_bytes(buffer + 1, socket);
  short_to_bytes(buffer + 1 + sizeof(int), length);
  memcpy(buffer + 1 + sizeof(int) + sizeof(unsigned short), pData, length);
  length += 1 + sizeof(int) + sizeof(unsigned short);
  int bytesSent = 0;
  while (bytesSent < length)
  {
    int curBytesSent = send(transport_service(), buffer + bytesSent,
                            length - bytesSent, 0);
    if (0 >= curBytesSent) return -1;
    bytesSent += curBytesSent;
  }
  if (bytesSent != length) return -1;

  if (sizeof(int) != recv(transport_service(), &buffer, sizeof(int), 0))
  {
    return -1;
  }
  return bytes_to_int(buffer);
}

/**
 * Priima duomenis iš jungties.
 *
 * @param socket jungtis
 * @param pData  rodyklė į buferio pradžią, kur išsaugoti duomenis
 * @param length kiek duomenų baitų gauti
 * @return kiek duomenų baitų buvo gauta ir įrašyta į pData arba -1, jei
 *         blogai nurodyta jungtis, arba 0, jei kitas jungties galas atsijungė
 */
int recv(int socket, void* pData, unsigned short length)
{
  Byte buffer[1 + sizeof(int) + sizeof(unsigned short)];
  buffer[0] = 5;
  int_to_bytes(buffer + 1, socket);
  short_to_bytes(buffer + 1 + sizeof(int), length);
  if (sizeof(buffer) != send(transport_service(), buffer, sizeof(buffer), 0))
  {
    return 0;
  }
  if (sizeof(int) != recv(transport_service(), &buffer, sizeof(int), 0))
  {
    return -1;
  }
  length = bytes_to_int(buffer);

  int bytesReceived = 0;
  while (bytesReceived < length)
  {
    int curBytesReceived = recv(transport_service(),
                                (char*)(pData) + bytesReceived,
                                length - bytesReceived, 0);
    if (0 >= curBytesReceived) return -1;
    bytesReceived += curBytesReceived;
  }
  if (bytesReceived != length) return -1;
  return length;
}

bool close(int socket)
{
  Byte buffer[1 + sizeof(int)];
  buffer[0] = 6;
  int_to_bytes(buffer, socket);
  if (sizeof(buffer) != send(transport_service(), buffer, sizeof(buffer), 0))
  {
    return false;
  }
  bool result;
  if (sizeof(result) != recv(transport_service(), &result, sizeof(result), 0))
  {
    return false;
  }
  return result;
}

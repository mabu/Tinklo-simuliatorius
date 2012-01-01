#include "LinkLayer.h"
#include "Node.h"

LinkLayer::LinkLayer(Node* pNode, MacSublayer* pMacSublayer):
  Layer(pNode),
  mpMacSublayer(pMacSublayer)
{ }

void LinkLayer::timer()
{
}

void LinkLayer::fromMacSublayer(MacAddress source, Frame& frame)
{
  if (frame.length == 0)
  {
    info("Gautas tuščias kadras.\n");
    return;
  }
  ControlByte controlByte = frame.data[0];
}

bool LinkLayer::fromNetworkLayer(MacAddress destination, Byte* packet,
                                 int packetLength)
{
  if (packetLength > MAX_DATA_LENGTH - 1)
  {
    info("Per didelis paketas.\n");
    return false;
  }
  return true;
}

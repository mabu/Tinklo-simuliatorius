#include "MacSublayer.h"
#include "Node.h"

MacSublayer::MacSublayer(Node* pNode):
  Layer(pNode)
{ }

void MacSublayer::fromPhysicalLayer(char voltage)
{
  info("Gavo signalą %hhd\n", voltage);
  mpNode->toPhysicalLayer(this, voltage + 1);
}

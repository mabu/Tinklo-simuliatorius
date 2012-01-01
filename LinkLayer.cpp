#include "LinkLayer.h"
#include "Node.h"

LinkLayer::LinkLayer(Node* pNode, MacSublayer* pMacSublayer):
  Layer(pNode),
  mpMacSublayer(pMacSublayer)
{ }

void LinkLayer::timer()
{
}

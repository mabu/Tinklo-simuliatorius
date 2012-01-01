#include "NetworkLayer.h"
#include "LinkLayer.h"
#include "Node.h"

NetworkLayer::NetworkLayer(Node* pNode):
  Layer(pNode)
{ }

void NetworkLayer::timer()
{
}

void NetworkLayer::addLink(LinkLayer* pLinkLayer)
{
  links.insert(pLinkLayer);
}

void NetworkLayer::removeLink(LinkLayer* pLinkLayer)
{
  links.erase(pLinkLayer);
}

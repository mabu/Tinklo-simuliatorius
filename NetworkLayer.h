#ifndef NETWORKLAYER_H
#define NETWORKLAYER_H
#include <set>
#include "Layer.h"

class Node;
class LinkLayer;

/**
 * Tinklo polygis.
 */
class NetworkLayer: public Layer
{
  private:
    std::set<LinkLayer*> links;

  public:
    NetworkLayer(Node* pNode);
    void addLink(LinkLayer* pLinkLayer);
    void removeLink(LinkLayer* pLinkLayer);
};

#endif

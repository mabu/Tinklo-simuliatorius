#ifndef NETWORKLAYER_H
#define NETWORKLAYER_H
#include <set>
#include "types.h"
#include "Layer.h"

class Node;
class LinkLayer;

/**
 * Tinklo polygis.
 */
class NetworkLayer: public Layer
{
  private:
    set<LinkLayer*> links;

  public:
    NetworkLayer(Node* pNode);
    void timer(long long id); // žr. Layer.h
    void addLink(LinkLayer* pLinkLayer);
    void removeLink(LinkLayer* pLinkLayer);

  protected:
    const char* layerName()
      { return "Tinklo lygis"; }
};

#endif

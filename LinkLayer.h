#ifndef LINKLAYER_H
#define LINKLAYER_H
#include "Layer.h"

class Node;
class MacSublayer;

/**
 * Kanalinis lygis.
 */

class LinkLayer: public Layer
{
  private:
    MacSublayer* mpMacSublayer;

  public:
    LinkLayer(Node* pNode, MacSublayer* pMacSublayer);
};

#endif

#ifndef MACSUBLAYER_H
#define MACSUBLAYER_H
#include "Layer.h"

class Node;

/**
 * MAC polygis.
 */
class MacSublayer: public Layer
{
  public:
    MacSublayer(Node* pNode);
    void fromPhysicalLayer(char voltage);
};

#endif

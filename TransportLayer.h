#ifndef TRANSPORTLAYER_H
#define TRANSPORTLAYER_H
#include "Layer.h"

class Node;

/**
 * Transporto lygis.
 */
class TransportLayer: public Layer
{
  public:
    TransportLayer(Node* pNode);
    void timer(); // žr. Layer.h

  protected:
    const char* layerName()
      { return "Transporto lygis"; }
};

#endif

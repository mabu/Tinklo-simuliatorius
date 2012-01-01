#ifndef LAYER_H
#define LAYER_H
/**
 * Bendra klasė tinklo steko lygiams.
 */

class Node;

class Layer
{
  protected:
    Node* mpNode;

  public:
    /**
     * Iškviečiama, kai baigiasi objekto iškviestas laikmatis.
     */
    virtual void timer() = 0;

  protected:
    Layer(Node* pNode);
    void info(const char* format, ...);
};

#endif
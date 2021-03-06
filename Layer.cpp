#include "Layer.h"
#include "Node.h"
#include <cstdarg>
#include <cstring>

Layer::Layer(Node* pNode):
  mpNode(pNode)
{ }

void Layer::info(const char* format, ...)
{
  va_list vl;
  va_start(vl, format);
  mpNode->layerMessage(layerName(), format, vl);
  va_end(vl);
}

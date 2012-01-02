#include "Layer.h"
#include "Node.h"
#include <cstdarg>
#include <cstring>

Layer::Layer(Node* pNode):
  mpNode(pNode)
{ }

void Layer::info(const char* format, ...)
{
  char* name = new char[strlen(layerName()) + 3];
  strcpy(name, layerName());
  strcat(name, ": ");
  mpNode->layerMessage(name);
  delete[] name;
  va_list vl;
  va_start(vl, format);
  mpNode->layerMessage(format, vl);
  va_end(vl);
}

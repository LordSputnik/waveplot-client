#include "error.hpp"

#include <cstdio>
#include <cstdlib>

void error(uint16_t code)
{
  fprintf(stderr,"WavePlot Imager encountered an error! Error Code %u.",code);
  exit(code);
}

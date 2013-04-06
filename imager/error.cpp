/*
 * Copyright 2013 Ben Ockmore
 *
 * This file is part of WavePlot Imager.

 * WavePlot Imager is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * WavePlot Imager is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with WavePlot Imager. If not, see <http://www.gnu.org/licenses/>.
 */

#include "error.hpp"

#include <iostream>

#include <cstdlib>

void error(uint16_t code)
{
  std::cerr << "WavePlot Imager encountered an error! Error Code " << code << std::endl;
  exit(code);
}

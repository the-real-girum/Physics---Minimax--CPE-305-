/*
 * Board.cpp
 *
 *  Created on: Oct 26, 2012
 *      Author: girum
 */

#include "Board.h"
#include <climits>

const long Board::kWinVal = LONG_MAX / 4;
long Board::Move::mOutstanding = 0;
long Board::Key::mOutstanding = 0;

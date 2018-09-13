/*
  Xiphos, a UCI chess engine
  Copyright (C) 2018 Milos Tatarevic

  Xiphos is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Xiphos is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include "eval.h"
#include "game.h"

int pst_mid[P_LIMIT][BOARD_SIZE], pst_end[P_LIMIT][BOARD_SIZE];

const int pst_mid_base[N_PIECES][BOARD_SIZE / 2] = {
  {
    0,   0,   0,   0,
  -31,  -2,   9,  12,
  -14, -15,  26,   3,
  -26, -11, -17,   2,
  -30, -36, -16,  -3,
  -34, -20, -31, -24,
  -33, -14, -19, -30,
    0,   0,   0,   0
  }, {
 -139, -80, -89, -27,
  -47,  -9,  86,  20,
    5,  74,  49,  62,
   31,  45,  42,  52,
   35,  48,  52,  56,
   14,  31,  32,  36,
   17,   9,  20,  23,
  -10,  21,   0,  21
  }, {
  -11,  -4, -87, -73,
  -48,  17,   6, -14,
   15,  26,  52,  31,
    2,  33,  24,  37,
   27,  17,  21,  41,
   13,  28,  27,  18,
   19,  39,  24,   7,
   -1,   8,  11,   8
  }, {
  -12,  15, -24,  24,
   -3,   4,  26,  30,
  -21,  20,   9,   3,
  -23,   0,  13,  20,
  -28,  -1,  -6,   5,
  -34,  -9,  -5,  -4,
  -40,  -8,  -6,   2,
   -8, -11,   7,  16
  }, {
    8,   7,  28,  31,
  -23, -60,  -8, -35,
   16,  12,   9,  -8,
   -9,  -4, -13, -21,
   13,  -1,   5,  -8,
   -1,  22,  -1,   7,
    6,  13,  32,  13,
   15,   4,  10,  26
  }, {
   -5,  15,  -5, -46,
  -18,   3,  -2,   0,
  -41,  34,  74,   9,
  -63,  15,  24, -10,
  -75,  -2,   3, -20,
  -49,  -6, -19, -30,
   12,   9,  -8, -47,
    3,  23, -24,   1
  }
};

const int pst_end_base[N_PIECES][BOARD_SIZE / 2] = {
  {
    0,   0,   0,   0,
    8,  23,  25,  40,
   14,  16,   9,   4,
    0,  -4,  -7, -12,
   -9,  -8, -12, -13,
  -18, -16, -13, -10,
  -17, -14,  -7,  -6,
    0,   0,   0,   0
  }, {
  -68, -42, -23, -20,
  -30, -13, -19,  -2,
  -28, -11,  -1,  -1,
   -9,   3,  11,  15,
   -4,   1,  10,  15,
  -11,  -8,   0,   8,
  -27, -15,  -7,   2,
  -33, -25, -13,  -7
  }, {
  -15, -15, -18, -15,
  -11,  -7,  -6,  -9,
   -3,  -5,  -2,  -5,
   -2,  -2,   3,   4,
   -6,  -3,   4,   3,
   -7,  -2,   2,   5,
  -19, -11,  -8,  -1,
  -22,  -4,  -8,  -7
  }, {
   26,  26,  31,  30,
   24,  26,  24,  19,
   21,  23,  19,  22,
   23,  19,  26,  20,
   22,  24,  25,  22,
   19,  22,  17,  18,
   19,  16,  18,  20,
   10,  18,  16,  12
   }, {
   17,  25,  23,  30,
    7,   9,  15,  33,
    9,   1,  -1,  30,
   34,  29,  17,  19,
   16,  36,  21,  23,
   30,  10,  23,  16,
    6,  -5,  -8,  13,
   -7,  -3,  -8, -14
  }, {
  -38, -23,   3, -12,
   -5,   5,  19,  12,
    5,  22,  22,   7,
   -3,  14,  15,   8,
   -6,   3,  14,   9,
   -2,   9,  12,   8,
  -11,   2,   8,   5,
  -36, -13,  -1, -14
  }
};

void init_pst()
{
  int piece, sq, i, f, v;

  memset(pst_mid, 0, sizeof(pst_mid));
  memset(pst_end, 0, sizeof(pst_end));

  for (piece = 0; piece < N_PIECES; piece ++)
  {
    for (sq = 0; sq < BOARD_SIZE; sq ++)
    {
      f = _file(sq);
      i = _rank(sq) * 4 + ((f <= 3) ? f : 7 - f);

      v = piece_value[piece] + pst_mid_base[piece][i];
      pst_mid[piece][sq] = v;
      pst_mid[piece | CHANGE_SIDE][sq ^ 56] = v;

      v = piece_value[piece] + pst_end_base[piece][i];
      pst_end[piece][sq] = v;
      pst_end[piece | CHANGE_SIDE][sq ^ 56] = v;
    }
  }
}

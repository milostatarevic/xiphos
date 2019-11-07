/*
  Xiphos, a UCI chess engine
  Copyright (C) 2018, 2019 Milos Tatarevic

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

#ifndef GAME_H
#define GAME_H

#include <inttypes.h>

#define VERSION                      "Xiphos 0.6"
#define AUTHOR                       "Milos Tatarevic"

#ifdef _BMI2
  #define ARCH                       "BMI2"
#elif defined(_NOPOPCNT)
  #define ARCH                       "NO-POPCNT"
#else
  #define ARCH                       "SSE"
#endif

#define MAX_THREADS                  128
#define DEFAULT_THREADS              1
#define DEFAULT_HASH_SIZE_IN_MB      128

#define MAX_DEPTH                    100
#define PLY_LIMIT                    128
#define MAX_PLY                      PLY_LIMIT - 8

#define MATE_SCORE                   30000
#define MAX_MOVES                    256
#define MAX_CAPTURES                 32
#define MAX_KILLER_MOVES             2

#define EMPTY                        0xf
#define P_LIMIT                      (EMPTY + 1)

#define SIDE_SHIFT                   3
#define CHANGE_SIDE                  (1 << SIDE_SHIFT)
#define BOARD_SIZE                   64

#define C_FLAG_WL                    0x1
#define C_FLAG_WR                    0x2
#define C_FLAG_BL                    0x4
#define C_FLAG_BR                    0x8
#define C_FLAG_MAX                   0x10

#define _max(a, b)                   ((a) >= (b) ? (a) : (b))
#define _min(a, b)                   ((a) < (b) ? (a) : (b))
#define _abs(a)                      ((a) < 0 ? -(a) : (a))
#define _sqr(a)                      ((a) * (a))
#define _sign(a)                     ((a > 0) - (a < 0))

#define _rank(sq)                    ((sq) >> 3)
#define _file(sq)                    ((sq) & 0x7)
#define _sq_rf(r, f)                 (((r) << 3) + (f))
#define _file_chr(sq)                ('a' + _file(sq))
#define _rank_chr(sq)                ('8' - _rank(sq))
#define _chr_to_sq(f, r)             (((f) - 'a') + ((7 - ((r) - '1')) << 3))

#define _side(piece)                 ((piece) >> SIDE_SHIFT)
#define _to_white(piece)             ((piece) & (CHANGE_SIDE - 1))
#define _equal_to(piece, w_piece)    (_to_white(piece) == (w_piece))

#define _is_mate_score(score)        ((score) <= -MATE_SCORE + MAX_PLY || \
                                      (score) >= MATE_SCORE - MAX_PLY)

enum { WHITE, BLACK, N_SIDES };
enum { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, N_PIECES };

enum { RANK_8, RANK_7, RANK_6, RANK_5, RANK_4, RANK_3, RANK_2, RANK_1, N_RANK };
enum { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, N_FILE };

enum {
  A8, B8, C8, D8, E8, F8, G8, H8,
  A7, B7, C7, D7, E7, F7, G7, H7,
  A6, B6, C6, D6, E6, F6, G6, H6,
  A5, B5, C5, D5, E5, F5, G5, H5,
  A4, B4, C4, D4, E4, F4, G4, H4,
  A3, B3, C3, D3, E3, F3, G3, H3,
  A2, B2, C2, D2, E2, F2, G2, H2,
  A1, B1, C1, D1, E1, F1, G1, H1,
  NO_SQ, SQ_LIMIT,
};

enum { PHASE_MID, PHASE_END, N_PHASES };

#endif

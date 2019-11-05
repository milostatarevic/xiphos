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

#ifndef TABLES_H
#define TABLES_H

#include "game.h"

extern int pst_mid[P_LIMIT][BOARD_SIZE], pst_end[P_LIMIT][BOARD_SIZE];
extern int distance[BOARD_SIZE][BOARD_SIZE];

extern const int piece_value[N_PIECES];
extern const int piece_phase[N_PIECES];

extern const int pawn_shield[BOARD_SIZE];
extern const int pawn_storm[2][BOARD_SIZE];
extern const int connected_pawns[2][N_PHASES][BOARD_SIZE];

extern const int passer_bonus[N_PHASES][BOARD_SIZE];
extern const int distance_bonus[2][N_RANK];

extern const int doubled_penalty[N_PHASES];
extern const int backward_penalty[N_PHASES];
extern const int isolated_penalty[N_PHASES];

extern const int mobility[N_PHASES][N_PIECES - 1][32];

extern const int threats[N_PHASES][N_PIECES - 2][8];
extern const int threat_king[N_PHASES];
extern const int threat_protected_pawn[N_PHASES];
extern const int threat_protected_pawn_push[N_PHASES];
extern const int threats_on_queen[N_PIECES][N_PHASES];

extern const int rook_file_bonus[N_PHASES][2];
extern const int bishop_pair[N_PHASES];
extern const int pawn_mobility[N_PHASES];
extern const int initiative[4];

void init_pst();
void init_distance();

#endif

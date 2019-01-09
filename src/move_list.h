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

#ifndef MOVE_LIST_H
#define MOVE_LIST_H

#include "game.h"
#include "move.h"
#include "position.h"

enum {
  QSEARCH,
  SEARCH,
};

enum {
  IN_CHECK,
  MATERIAL_MOVES,
  KILLER_MOVE_0,
  KILLER_MOVE_1,
  COUNTER_MOVE,
  QUIET_MOVES,
  BAD_CAPTURES,
  END,
};

typedef struct {
  int search_mode, phase, cnt, moves_cnt, bad_captures_cnt, searched_hash_move;
  move_t bad_captures[MAX_CAPTURES], moves[MAX_MOVES];
} move_list_t;

void init_move_list(move_list_t *, int, int);
move_t next_move(move_list_t *, search_data_t *, move_t, int, int);

#endif

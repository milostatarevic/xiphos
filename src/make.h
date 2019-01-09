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

#ifndef MAKE_H
#define MAKE_H

#include "move.h"
#include "search.h"

void make_null_move(search_data_t *);
void make_move(search_data_t *, move_t);
void make_move_rev(search_data_t *, move_t);
void init_rook_c_flag_mask();

static inline void undo_move(search_data_t *sd)
{
  sd->pos --;
  sd->hash_keys_cnt --;
  sd->hash_key = sd->hash_keys[sd->hash_keys_cnt];
}

#endif

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

#ifndef HISTORY_H
#define HISTORY_H

#include "game.h"
#include "move.h"
#include "position.h"
#include "search.h"

#define KILLER_MOVE_SHIFT         15
#define MAX_HISTORY_SCORE         (1 << (KILLER_MOVE_SHIFT - 2))

#define _counter_move_history_item(pos, cmh_ptr, move)                         \
  cmh_ptr[pos->board[_m_from(move)] * BOARD_SIZE + _m_to(move)]

static inline int16_t *_counter_move_history_pointer(search_data_t *sd)
{
  position_t *pos;

  pos = sd->pos;
  if (pos->move)
    return sd->counter_move_history[pos->board[_m_to(pos->move)]][_m_to(pos->move)];
  else
    return NULL;
}

static inline int get_h_score(search_data_t *sd, int16_t *cmh_ptr, move_t move)
{
  int score;

  score = sd->history[sd->pos->side][_m_from(move)][_m_to(move)];
  if (cmh_ptr)
    score += _counter_move_history_item(sd->pos, cmh_ptr, move);

  return score;
}

void clear_history(search_data_t *);
void set_killer_move(search_data_t *, move_t, int);
void set_counter_move(search_data_t *, move_t);
move_t get_counter_move(search_data_t *);
void add_to_history(search_data_t *, int16_t *, move_t, int);

#endif

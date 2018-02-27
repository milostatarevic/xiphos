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
#define MAX_HISTORY_SCORE_SHIFT   (KILLER_MOVE_SHIFT - 1)
#define MAX_HISTORY_SCORE         (1 << MAX_HISTORY_SCORE_SHIFT)

void clear_history(search_data_t *);
void set_killer_move(search_data_t *, move_t, int);
void set_counter_move(search_data_t *, move_t);
move_t get_counter_move(search_data_t *);
void add_to_bad_history(search_data_t *, move_t, int);
void add_to_history(search_data_t *, move_t, int);

static inline int get_h_score(search_data_t *sd, position_t *pos, move_t move)
{
  int piece, m_to, h;

  piece = pos->board[_m_from(move)];
  m_to = _m_to(move);
  h = sd->history[piece][m_to] + 1;

  return (h << MAX_HISTORY_SCORE_SHIFT) / (sd->bad_history[piece][m_to] + h);
}

#endif

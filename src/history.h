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

#ifndef HISTORY_H
#define HISTORY_H

#include "game.h"
#include "move.h"
#include "position.h"
#include "search.h"

#define MAX_CMH_PLY               2
#define MAX_HISTORY_SCORE         (1 << 13)

static inline void set_counter_move_history_pointer
                           (int16_t **cmh_ptr, search_data_t *sd, int ply)
{
  int i, m_to;
  position_t *pos;

  for (i = 0; i < MAX_CMH_PLY; i ++)
  {
    pos = sd->pos - i;
    if (ply > i && pos->move)
    {
      m_to = _m_to(pos->move);
      cmh_ptr[i] = sd->counter_move_history[pos->board[m_to]][m_to];
    }
    else
      cmh_ptr[i] = NULL;
  }
}

static inline int get_h_score(search_data_t *sd, position_t *pos,
                              int16_t **cmh_ptr, move_t move)
{
  int i, score, m_to, m_from, piece_pos;

  m_to = _m_to(move);
  m_from = _m_from(move);
  score = sd->history[pos->side][m_from][m_to];

  if (cmh_ptr)
  {
    piece_pos = pos->board[m_from] * BOARD_SIZE + m_to;
    for (i = 0; i < MAX_CMH_PLY; i ++)
      if (cmh_ptr[i])
        score += cmh_ptr[i][piece_pos];
  }
  return score;
}

void set_killer_move(search_data_t *, move_t, int);
void set_counter_move(search_data_t *, move_t);
move_t get_counter_move(search_data_t *);
void add_to_history(search_data_t *, int16_t **, move_t, int);

#endif

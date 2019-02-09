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

#include <string.h>
#include "history.h"

void set_killer_move(search_data_t *sd, move_t move, int ply)
{
  move_t tmp;

  if (!_m_eq(sd->killer_moves[ply][0], move))
  {
    tmp = sd->killer_moves[ply][0];
    sd->killer_moves[ply][0] = move;
    sd->killer_moves[ply][1] = tmp;
  }
}

void set_counter_move(search_data_t *sd, move_t move)
{
  int m_to;

  if (!_is_m(sd->pos->move)) return;
  m_to = _m_to(sd->pos->move);
  sd->counter_moves[sd->pos->board[m_to]][m_to] = move;
}

move_t get_counter_move(search_data_t *sd)
{
  int m_to;

  if (!_is_m(sd->pos->move)) return 0;
  m_to = _m_to(sd->pos->move);
  return sd->counter_moves[sd->pos->board[m_to]][m_to];
}

void add_to_history(search_data_t *sd, int16_t **cmh_ptr, move_t move, int score)
{
  int i, m_to, m_from, piece_pos;
  int16_t *item;

  m_to = _m_to(move);
  m_from = _m_from(move);
  piece_pos = sd->pos->board[m_from] * BOARD_SIZE + m_to;

  item = &sd->history[sd->pos->side][m_from][m_to];
  *item += score - (*item) * _abs(score) / MAX_HISTORY_SCORE;

  for (i = 0; i < MAX_CMH_PLY; i ++)
    if (cmh_ptr[i])
    {
      item = &cmh_ptr[i][piece_pos];
      *item += score - (*item) * _abs(score) / MAX_HISTORY_SCORE;
    }
}

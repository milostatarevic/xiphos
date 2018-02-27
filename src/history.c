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

void age_history(search_data_t *sd)
{
  int i, j;

  for (i = 0; i < P_LIMIT; i++)
    for (j = 0; j < BOARD_SIZE; j++)
    {
      sd->history[i][j] >>= 1;
      sd->bad_history[i][j] >>= 1;
    }
}

void add_to_history(search_data_t *sd, move_t move, int score)
{
  int m_from, m_to, piece;

  m_from = _m_from(move);
  m_to = _m_to(move);
  piece = sd->pos->board[m_from];
  sd->history[piece][m_to] += score;
  if (sd->history[piece][m_to] >= MAX_HISTORY_SCORE)
    age_history(sd);
}

void add_to_bad_history(search_data_t *sd, move_t move, int score)
{
  int m_from, m_to, piece;

  m_from = _m_from(move);
  m_to = _m_to(move);
  piece = sd->pos->board[m_from];
  sd->bad_history[piece][m_to] += score;
  if (sd->bad_history[piece][m_to] >= MAX_HISTORY_SCORE)
    age_history(sd);
}

void clear_history(search_data_t *sd)
{
  memset(sd->history, 0, sizeof(sd->history));
  memset(sd->bad_history, 0, sizeof(sd->bad_history));

  memset(sd->killer_moves, 0, sizeof(sd->killer_moves));
  memset(sd->counter_moves, 0, sizeof(sd->counter_moves));
}

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

#include "history.h"
#include "move.h"
#include "position.h"

void eval_material_moves(position_t *pos, move_t *moves, int moves_cnt)
{
  int i, piece;
  move_t move;

  for (i = 0; i < moves_cnt; i ++)
  {
    move = moves[i];
    piece = pos->board[_m_to(move)];
    if (!_m_promoted_to(move))
      moves[i] = _m_with_score(move, piece);
  }
}

int eval_quiet_moves(search_data_t *sd, move_t *moves, int moves_cnt, int ply)
{
  int i, cnt;
  move_t move, killer_0, killer_1, counter;
  int16_t *cmh_ptr[MAX_CMH_PLY];

  killer_0 = _m_base(sd->killer_moves[ply][0]);
  killer_1 = _m_base(sd->killer_moves[ply][1]);
  counter = _m_base(get_counter_move(sd));
  set_counter_move_history_pointer(cmh_ptr, sd, ply);

  cnt = 0;
  for (i = 0; i < moves_cnt; i ++)
  {
    move = _m_base(moves[i]);
    if (move != killer_0 && move != killer_1 && move != counter)
      moves[cnt ++] = _m_set_quiet(
        _m_with_score(move, get_h_score(sd, sd->pos, cmh_ptr, move))
      );
  }

  return cnt;
}

void eval_all_moves(search_data_t *sd, move_t *moves, int moves_cnt, int ply)
{
  int i, score, piece;
  move_t move;
  position_t *pos;
  int16_t *cmh_ptr[MAX_CMH_PLY];

  pos = sd->pos;
  set_counter_move_history_pointer(cmh_ptr, sd, ply);

  for (i = 0; i < moves_cnt; i ++)
  {
    move = moves[i];
    if (move_is_quiet(pos, move))
      moves[i] = _m_set_quiet(_m_with_score(move, get_h_score(sd, pos, cmh_ptr, move)));
    else
    {
      if (_m_promoted_to(moves[i]))
        score = 1;
      else
      {
        piece = pos->board[_m_to(move)];
        score = P_LIMIT + ((piece == EMPTY ? 0 : piece) << 5) -
                  pos->board[_m_from(move)];
      }
      moves[i] = _m_with_score(move, score + 3 * MAX_HISTORY_SCORE);
    }
  }
}

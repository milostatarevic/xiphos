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

#include "game.h"
#include "gen.h"
#include "position.h"
#include "util.h"
#include "fathom/tbprobe.h"

const int promotion_table[N_PIECES] = { 0, QUEEN, ROOK, BISHOP, KNIGHT, 0 };

unsigned tablebases_probe_wdl(position_t *pos)
{
  // reverse sides due to the different board representation
  return tb_probe_wdl(
    pos->occ[BLACK],
    pos->occ[WHITE],
    _b(pos->k_sq[WHITE]) | _b(pos->k_sq[BLACK]),
    pos->piece_occ[QUEEN],
    pos->piece_occ[ROOK],
    pos->piece_occ[BISHOP],
    pos->piece_occ[KNIGHT],
    pos->piece_occ[PAWN],
    pos->fifty_cnt,
    pos->c_flag,
    pos->ep_sq != NO_SQ ? pos->ep_sq : 0,
    pos->side
  );
}

move_t tablebases_probe_root(position_t *pos)
{
  int i, m_from, m_to, promoted_to, score, moves_cnt;
  unsigned result, wdl;
  move_t move, moves[MAX_MOVES];

  // reverse sides due to the different board representation
  result =
    tb_probe_root(
      pos->occ[BLACK],
      pos->occ[WHITE],
      _b(pos->k_sq[WHITE]) | _b(pos->k_sq[BLACK]),
      pos->piece_occ[QUEEN],
      pos->piece_occ[ROOK],
      pos->piece_occ[BISHOP],
      pos->piece_occ[KNIGHT],
      pos->piece_occ[PAWN],
      pos->fifty_cnt,
      pos->c_flag,
      pos->ep_sq != NO_SQ ? pos->ep_sq : 0,
      pos->side,
      NULL
    );

  if (result == TB_RESULT_FAILED)
    return 0;

  m_to = TB_GET_TO(result);
  m_from = TB_GET_FROM(result);
  promoted_to = promotion_table[TB_GET_PROMOTES(result)];

  move = _m_with_promo(_m(m_from, m_to), promoted_to);

  wdl = TB_GET_WDL(result);
  switch(wdl)
  {
    case TB_WIN:
      score = MATE_SCORE - MAX_PLY - 1;
      break;
    case TB_LOSS:
      score = -MATE_SCORE + MAX_PLY + 1;
      break;
    default:
      score = 0;
  }

  get_all_moves(pos, moves, &moves_cnt);
  for (i = 0; i < moves_cnt; i ++)
    if (_m_eq(move, moves[i]))
      return _m_with_score(move, score);

  return 0;
}

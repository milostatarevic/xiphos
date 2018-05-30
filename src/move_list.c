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

#include "gen.h"
#include "history.h"
#include "move.h"
#include "move_eval.h"
#include "move_list.h"
#include "position.h"

void init_move_list(move_list_t *ml, int search_mode, int in_check)
{
  ml->search_mode = search_mode;

  ml->moves_cnt = ml->cnt = ml->bad_captures_cnt = ml->searched_hash_move = 0;
  ml->phase = in_check ? IN_CHECK : MATERIAL_MOVES;
}

static inline int is_pseudo_legal_and_quiet(position_t *pos, move_t move)
{
  int piece, m_from, m_to, m_diff;

  m_to = _m_to(move);
  if (pos->board[m_to] != EMPTY)
    return 0;

  m_from = _m_from(move);
  piece = pos->board[m_from];
  if (piece == EMPTY || (piece >> SIDE_SHIFT) != pos->side)
    return 0;

  piece = _to_white(piece);
  m_diff = m_to - m_from;

  if (piece == PAWN)
  {
    if ((m_diff < 0 && pos->side == BLACK) || (m_diff > 0 && pos->side == WHITE))
      return 0;

    // don't make promotions here
    if (m_to <= H8 || m_to >= A1)
      return 0;

    if (m_diff == -8 || m_diff == 8)
      return 1;

    if (m_diff == -16)
      return _rank(m_from) == RANK_2 && pos->board[m_from - 8] == EMPTY;
    else if (m_diff == 16)
      return _rank(m_from) == RANK_7 && pos->board[m_from + 8] == EMPTY;
    return 0;
  }

  if (piece != KING)
  {
    if (!(_b_piece_area[piece][m_from] & _b(m_to)))
      return 0;

    if (piece == KNIGHT)
      return 1;

    return (_b_line[m_from][m_to] & (pos->occ[WHITE] | pos->occ[BLACK])) == 0;
  }

  // castling
  if (m_diff == 2 || m_diff == -2)
  {
    if (!pos->c_flag || pos->board[(m_from + m_to) >> 1] != EMPTY)
      return 0;

    if (m_diff > 0)
      return pos->c_flag & ((pos->side == WHITE) ? C_FLAG_WR : C_FLAG_BR);
    else
      return pos->board[m_to - 1] == EMPTY &&
            (pos->c_flag & ((pos->side == WHITE) ? C_FLAG_WL : C_FLAG_BL));
  }

  return !!(_b_piece_area[KING][m_from] & _b(m_to));
}

void static inline set_move(search_data_t *sd, move_list_t *ml, move_t move)
{
  if (_is_m(move) && is_pseudo_legal_and_quiet(sd->pos, move))
  {
    ml->moves[0] = _m_set_quiet(move);
    ml->moves_cnt = 1;
  }
  else
  {
    ml->moves_cnt = 0;
  }
}

static inline void generate_moves(move_list_t *ml, search_data_t *sd, int depth,
                                  int ply, int minor_promotions)
{
  switch(ml->phase)
  {
    case MATERIAL_MOVES:

      if (depth == 0)
      {
        checks_and_material_moves(sd->pos, ml->moves, &ml->moves_cnt, 0);
        eval_all_moves(sd, ml->moves, ml->moves_cnt);
      }
      else
      {
        material_moves(sd->pos, ml->moves, &ml->moves_cnt, minor_promotions);
        eval_material_moves(sd->pos, ml->moves, ml->moves_cnt);
      }
      break;

    case KILLER_MOVE_0:

      set_move(sd, ml, sd->killer_moves[ply][0]);
      break;

    case KILLER_MOVE_1:

      set_move(sd, ml, sd->killer_moves[ply][1]);
      break;

    case COUNTER_MOVE:

      set_move(sd, ml, get_counter_move(sd));
      break;

    case QUIET_MOVES:

      quiet_moves(sd->pos, ml->moves, &ml->moves_cnt);
      ml->moves_cnt = eval_quiet_moves(sd, ml->moves, ml->moves_cnt, ply);
      break;

    case BAD_CAPTURES:

      ml->moves_cnt = ml->bad_captures_cnt;
      memcpy(ml->moves, ml->bad_captures, ml->moves_cnt * sizeof(move_t));
      break;

    case IN_CHECK:

      check_evasion_moves(sd->pos, ml->moves, &ml->moves_cnt, minor_promotions);
      eval_all_moves(sd, ml->moves, ml->moves_cnt);
      break;
  }
}

static inline void prepare_next_move(move_t *moves, int moves_cnt, int i)
{
  int j, max_j;
  move_t tmp;

  max_j = i;
  for (j = i + 1; j < moves_cnt; j ++)
    if (_m_less_score(moves[max_j], moves[j]))
      max_j = j;

  if (max_j != i)
  {
    tmp = moves[i];
    moves[i] = moves[max_j];
    moves[max_j] = tmp;
  }
}

move_t next_move(move_list_t *ml, search_data_t *sd, move_t hash_move, int depth,
                 int ply, int lmp_started, int minor_promotions)
{
  int see_score;
  move_t move, next_move;

  if (_is_m(hash_move) && !ml->searched_hash_move)
  {
    ml->searched_hash_move = 1;
    return hash_move;
  }

  move = 0;
  while (ml->phase != END)
  {
    // generate moves for the current phase
    if (ml->cnt == 0)
      generate_moves(ml, sd, depth, ply, minor_promotions);

    // move to the next phase
    if (ml->cnt >= ml->moves_cnt)
    {
      ml->cnt = 0;
      if (ml->search_mode == QSEARCH || ml->phase == IN_CHECK)
        ml->phase = END;
      else
        ml->phase ++;
      continue;
    }

    // pull moves in order, skip during LMP
    if (ml->moves_cnt > 1 && (!lmp_started || ml->phase != QUIET_MOVES))
      prepare_next_move(ml->moves, ml->moves_cnt, ml->cnt);

    next_move = ml->moves[ml->cnt ++];

    // don't search hash move again
    if (_m_eq(next_move, hash_move))
      continue;

    // put bad captures in a separate list
    if (ml->phase == MATERIAL_MOVES && (see_score = SEE(sd->pos, next_move)) < 0)
    {
      if (ml->search_mode == SEARCH)
        ml->bad_captures[ml->bad_captures_cnt ++] =
          _m_with_score(next_move, see_score);
      continue;
    }

    // safe to use
    move = next_move;
    break;
  }
  return move;
}

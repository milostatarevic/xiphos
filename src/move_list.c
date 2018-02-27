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
#include "move.h"
#include "move_eval.h"
#include "move_list.h"
#include "position.h"

enum {
  IN_CHECK,
  MATERIAL_MOVES,
  QUIET_MOVES,
  BAD_CAPTURES,
  END,
};

void init_move_list(move_list_t *ml, int search_mode, int in_check)
{
  ml->search_mode = search_mode;

  ml->moves_cnt = ml->cnt = ml->bad_captures_cnt = ml->searched_hash_move = 0;
  ml->phase = in_check ? IN_CHECK : MATERIAL_MOVES;
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

    case QUIET_MOVES:

      quiet_moves(sd->pos, ml->moves, &ml->moves_cnt);
      eval_quiet_moves(sd, ml->moves, ml->moves_cnt, ply);
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
    if (!lmp_started || ml->phase != QUIET_MOVES)
      prepare_next_move(ml->moves, ml->moves_cnt, ml->cnt);

    next_move = ml->moves[ml->cnt ++];

    // don't search hash move again
    if (_m_eq(next_move, hash_move))
      continue;

    // put bad captures in a separate list
    if (ml->phase == MATERIAL_MOVES && bad_SEE(sd->pos, next_move))
    {
      if (ml->search_mode == SEARCH)
        ml->bad_captures[ml->bad_captures_cnt ++] = next_move;
      continue;
    }

    // safe to use
    move = next_move;
    break;
  }
  return move;
}

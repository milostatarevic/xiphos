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

#include "bitboard.h"
#include "eval.h"
#include "game.h"
#include "gen.h"
#include "make.h"
#include "move_eval.h"
#include "search.h"
#include "uci.h"

#include <stdlib.h>

typedef struct {
  char *fen;
  int depth;
  uint64_t nodes;
} test_t;

test_t tests[] =
{
  { .fen = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -",
    .depth = 5,
    .nodes = 89941194 },
  { .fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",
    .depth = 5,
    .nodes = 193690690 },
  { .fen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",
    .depth = 7,
    .nodes = 178633661 },
  { .fen = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ -",
    .depth = 6,
    .nodes = 706045033 },
  { .fen = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -",
    .depth = 5,
    .nodes = 164075551 },
  { .fen = initial_fen,
    .depth = 6,
    .nodes = 119060324 },
};

int test_checks_and_material_moves(search_data_t *sd, move_t *moves, int moves_cnt)
{
  int i, t, test_moves_cnt, match, is_check;
  move_t test_moves[MAX_MOVES], move;
  position_t *pos;

  pos = sd->pos;
  if (pos->in_check)
    return 1;

  checks_and_material_moves(pos, test_moves, &test_moves_cnt);

  eval_all_moves(sd, test_moves, test_moves_cnt, 0);
  eval_all_moves(sd, moves, moves_cnt, 0);

  // test material moves
  for (i = 0; i < moves_cnt; i ++)
  {
    move = moves[i];
    if (_m_is_quiet(move))
      continue;

    match = 0;
    for (t = 0; t < test_moves_cnt; t ++)
      if (_m_eq(test_moves[t], move))
        { match = 1; break; }
    if (!match)
      return 0;
  }

  // test quiet checks
  for (i = 0; i < moves_cnt; i ++)
  {
    move = moves[i];
    if (!_m_is_quiet(move) || !legal_move(pos, move))
      continue;

    make_move(sd, move);
    is_check = sd->pos->in_check;
    undo_move(sd);

    if (!is_check) continue;

    match = 0;
    for (t = 0; t < test_moves_cnt; t ++)
      if (_m_eq(test_moves[t], move))
        { match = 1; break; }
    if (!match)
      return 0;
  }

  return 1;
}

uint64_t perft(search_data_t *sd, int depth, int ply, int additional_tests)
{
  int i, moves_cnt, check_ep;
  uint64_t nodes, p_b;
  position_t *pos;
  move_t moves[MAX_MOVES];

  pos = sd->pos;
  nodes = check_ep = 0;
  if (pos->ep_sq != NO_SQ)
  {
    p_b = pos->piece_occ[PAWN] & pos->occ[pos->side];
    check_ep = !!(_b_piece_area[_o_piece(pos, PAWN)][pos->ep_sq] & p_b);
  }

  if (pos->in_check)
  {
    check_evasion_moves(pos, moves, &moves_cnt);
  }
  else if (depth > 1 || pos->pinned[pos->side] || check_ep)
  {
    get_all_moves(pos, moves, &moves_cnt);
    if (additional_tests)
      if (!test_checks_and_material_moves(sd, moves, moves_cnt))
        return 0;
  }
  else
  {
    nodes += count_non_king_moves(pos);
    king_moves(pos, moves, &moves_cnt);
  }

  if (depth == 1)
  {
    for (i = 0; i < moves_cnt; i ++)
      if (legal_move(pos, moves[i]))
        nodes ++;

    return nodes;
  }

  for (i = 0; i < moves_cnt; i ++)
  {
    if (!legal_move(pos, moves[i]))
      continue;

    make_move(sd, moves[i]);
    nodes += perft(sd, depth - 1, ply + 1, additional_tests);
    undo_move(sd);
  }

  return nodes;
}

void run_tests()
{
  int i, errors;
  uint64_t nodes;
  search_data_t *sd;

  sd = (search_data_t *) malloc(sizeof(search_data_t));
  reset_search_data(sd);

  errors = 0;
  for (i = 0; i < sizeof(tests) / sizeof(test_t); i ++)
  {
    read_fen(sd, tests[i].fen);
    nodes = perft(sd, tests[i].depth, 0, 1);
    if (nodes != tests[i].nodes)
    {
      _p("E");
      errors ++;
    }
    else
      _p(".");
  }
  _p("\nerrors: %d\n", errors);

  free(sd);
}

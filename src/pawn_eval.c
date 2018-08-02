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

#include "bitboard.h"
#include "game.h"
#include "phash.h"
#include "position.h"

#define KING_PAWNS_SHIFT        3
#define DISTANCE_BONUS_SHIFT    2
#define BAD_PAWN_PENALTY        16

const int connected_bonus_mid[N_RANK] = { 0, 10, 15, 25, 40, 65, 110, 0 };
const int connected_bonus_end[N_RANK] = { 0, 5, 5, 10, 25, 60, 110, 0 };
const int passer_bonus[N_RANK] = { 0, 0, 4, 10, 40, 70, 90, 0 };

uint8_t distance[BOARD_SIZE][BOARD_SIZE];

void init_distance()
{
  int i, j;

  for (i = 0; i < BOARD_SIZE; i ++)
    for (j = 0; j < BOARD_SIZE; j ++)
      distance[i][j] = _min(_max(_abs(_rank(i) - _rank(j)), _abs(_file(i) - _file(j))), 5);
}

phash_data_t pawn_eval(position_t *pos)
{
  int m, r, side, sq, ssq, k_sq_f, k_sq_o, d, d_max, score_mid, score_end, reduce;
  uint64_t b, pushed_passers, p_occ, p_occ_f, p_occ_o, p_occ_x;
  phash_data_t phash_data;

  if (get_phash_data(pos, &phash_data))
    return phash_data;

  pushed_passers = 0;
  p_occ = pos->piece_occ[PAWN];
  score_mid = score_end = 0;

  for (side = WHITE; side < N_SIDES; side ++)
  {
    k_sq_f = pos->k_sq[side];
    k_sq_o = pos->k_sq[side ^ 1];
    p_occ_f = p_occ & pos->occ[side];
    p_occ_o = p_occ & pos->occ[side ^ 1];

    m = (side == WHITE) ? 7 : 0;
    b = p_occ_f;
    d_max = 0;

    _loop(b)
    {
      sq = _bsf(b);
      r = m ^ _rank(sq);
      ssq = (side == WHITE) ? (sq - 8) : (sq + 8);

      // distance bonus
      d = distance[ssq][k_sq_o] * r - distance[ssq][k_sq_f] * (r - 1);
      if (d > d_max) d_max = d;

      // passers
      if (!(_b_passer_area[side][sq] & p_occ_o))
      {
        pushed_passers |= _b(ssq);
        score_mid += passer_bonus[r];
        score_end += passer_bonus[r] + (distance[ssq][k_sq_o] << r) -
                                       (distance[ssq][k_sq_f] << (r - 1));
      }

      // doubled/isolated pawns
      p_occ_x = p_occ_f ^ _b(sq);
      if ((_b_doubled_pawn_area[side][sq] & p_occ_x) ||
          !(_b_isolated_pawn_area[_file(sq)] & p_occ_x))
      {
        score_mid -= BAD_PAWN_PENALTY;
        score_end -= BAD_PAWN_PENALTY;
      }

      // connected pawns
      if (_b_connected_pawn_area[side][sq] & p_occ_f)
      {
        reduce = !!(_b_doubled_pawn_area[side][sq] & p_occ_o);
        score_mid += connected_bonus_mid[r] >> (reduce ? 1 : 0);
        score_end += connected_bonus_end[r] >> (reduce ? 1 : 0);
      }
    }
    score_end += d_max << DISTANCE_BONUS_SHIFT;

    // pawns in the king zone
    score_mid += _popcnt(_b_king_zone[k_sq_f] & p_occ_f) << KING_PAWNS_SHIFT;

    score_mid = -score_mid;
    score_end = -score_end;
  }

  return set_phash_data(pos, pushed_passers, score_mid, score_end);
}

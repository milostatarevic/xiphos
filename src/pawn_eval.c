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
#include "game.h"
#include "phash.h"
#include "position.h"
#include "tables.h"

#define DISTANCE_BONUS_SHIFT    2

static inline int eval_pawn_shield(int side, int k_sq, uint64_t p_occ_f, uint64_t p_occ_o)
{
  int f, fi, m, sq, r, r_min, unopposed, score;
  uint64_t b;

  f = _max(_min(_file(k_sq), FILE_G), FILE_B);
  m = (side == WHITE) ? 7 : 0;

  score = 0;
  for (fi = f - 1; fi <= f + 1; fi ++)
  {
    r_min = N_RANK - 1;
    b = _b_file[fi] & p_occ_f;
    _loop(b)
    {
      r = m ^ _rank(_bsf(b));
      if (r_min > r) r_min = r;
    }
    score += pawn_shield[_sq_rf(r_min, fi)];

    b = _b_file[fi] & p_occ_o;
    _loop(b)
    {
      sq = _bsf(b);
      r = m ^ _rank(sq);
      unopposed = !(_b_doubled_pawn_area[side ^ 1][sq] & p_occ_f);
      score += pawn_storm[unopposed][_sq_rf(r, fi)];
    }
  }
  return score;
}

phash_data_t pawn_eval(position_t *pos)
{
  int m, r, f, side, sq, rsq, ssq, k_sq_f, k_sq_o, d, d_max, unopposed,
      score_mid, score_end;
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
      f = _file(sq);
      r = m ^ _rank(sq);
      rsq = (side == WHITE) ? sq : sq ^ 56;
      ssq = (side == WHITE) ? (sq - 8) : (sq + 8);

      // distance bonus
      d = distance[ssq][k_sq_o] * r - distance[ssq][k_sq_f] * (r - 1);
      if (d > d_max) d_max = d;

      // passer
      if (!(_b_passer_area[side][sq] & p_occ_o))
      {
        pushed_passers |= _b(ssq);
        score_mid += passer_bonus[PHASE_MID][rsq];
        score_end += passer_bonus[PHASE_END][rsq] +
                     distance[ssq][k_sq_o] * distance_bonus[0][r] -
                     distance[ssq][k_sq_f] * distance_bonus[1][r];
      }

      // connected pawn
      if (_b_connected_pawn_area[side][sq] & p_occ_f)
      {
        unopposed = !(_b_doubled_pawn_area[side][sq] & p_occ_o);
        score_mid += connected_pawns[unopposed][PHASE_MID][rsq];
        score_end += connected_pawns[unopposed][PHASE_END][rsq];
      }
      else
      {
        p_occ_x = p_occ_f ^ _b(sq);

        // doubled pawn
        if (_b_doubled_pawn_area[side][sq] & p_occ_x)
        {
          score_mid -= doubled_penalty[PHASE_MID];
          score_end -= doubled_penalty[PHASE_END];
        }

        // backward pawn
        if (!(_b_passer_area[side ^ 1][ssq] & ~_b_file[f] & p_occ_x) &&
            ((_b_piece_area[PAWN | (side << SIDE_SHIFT)][ssq] | _b(ssq)) & p_occ_o))
        {
          score_mid -= backward_penalty[PHASE_MID];
          score_end -= backward_penalty[PHASE_END];
        }

        // isolated pawn
        if (!(_b_isolated_pawn_area[f] & p_occ_x))
        {
          score_mid -= isolated_penalty[PHASE_MID];
          score_end -= isolated_penalty[PHASE_END];
        }
      }
    }

    score_mid += eval_pawn_shield(side, k_sq_f, p_occ_f, p_occ_o);
    score_end += d_max << DISTANCE_BONUS_SHIFT;

    score_mid = -score_mid;
    score_end = -score_end;
  }

  return set_phash_data(pos, pushed_passers, score_mid, score_end);
}

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

#define DISTANCE_BONUS_SHIFT    2

const int connected_bonus_mid[N_RANK] = { 0, 13, 21, 23, 35, 42, 82, 0 };
const int connected_bonus_end[N_RANK] = { 0, -4, 6, 5, 13, 50, 69, 0 };
const int passer_bonus[N_RANK] = { 0, -6, -6, 6, 27, 68, 103, 0 };

const int doubled_penalty[2] = {10, 22};
const int backward_penalty[2] = {6, 1};
const int isolated_penalty[2] = {5, 6};

const int pawn_shield[BOARD_SIZE] = {
    0,   0,   0,   0,   0,   0,   0,   0,
    4,  15,  15,   6,   2,  15,  11,   1,
    8,  16,   2,   3,   7,   7,   8,   8,
    7, -10,   1,   4,  -1,  -1,  -4,   6,
   23,  -1,   8,  -1,  -1,  -2,  -9,   3,
   35,  53,  14, -13, -17,  -1,   7,  -4,
  -15, -12,   8,  28,  17,  13, -31,  -4,
  -19, -24, -12,  -8,  -6, -10, -13, -11
};

const int pawn_storm[2][BOARD_SIZE - N_FILE] = {
  {
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
   -6, -26, -16, -26,  -8, -17, -17, -14,
    5,   5,  -2, -12,  -4,  -6,   4,   8,
    8,   1,  -4,   6,   3,  -6,  -2,   3,
    1,  21,  -2,   0,   5,  -6,   2,   5,
   -1,   7,   4,   5,  -1,   3,   0,   1,
  }, {
    0,   0,   0,   0,   0,   0,   0,   0,
  -17,   5,   9, -14, -22, -25,  -1,  -3,
   -6, -26,   1,   3, -16, -19,  -5,  18,
   -3,  21, -16,  -5, -12,  -9, -12,  13,
  -16,   6,  -8,  -6, -17,  -4, -13,  -3,
   14,  17,   3,   2,   3,  -5,  15,   2,
   -8,  -1,  -1,  -2, -12,   9,   4,  -1
  }
};

uint8_t distance[BOARD_SIZE][BOARD_SIZE];

void init_distance()
{
  int i, j;

  for (i = 0; i < BOARD_SIZE; i ++)
    for (j = 0; j < BOARD_SIZE; j ++)
      distance[i][j] = _min(_max(_abs(_rank(i) - _rank(j)), _abs(_file(i) - _file(j))), 5);
}

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
  int m, r, f, side, sq, ssq, k_sq_f, k_sq_o, d, d_max, score_mid, score_end, reduce;
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
      ssq = (side == WHITE) ? (sq - 8) : (sq + 8);

      // distance bonus
      d = distance[ssq][k_sq_o] * r - distance[ssq][k_sq_f] * (r - 1);
      if (d > d_max) d_max = d;

      // passer
      if (!(_b_passer_area[side][sq] & p_occ_o))
      {
        pushed_passers |= _b(ssq);
        score_mid += passer_bonus[r];
        score_end += passer_bonus[r] + (distance[ssq][k_sq_o] << r) -
                                       (distance[ssq][k_sq_f] << (r - 1));
      }

      // connected pawn
      if (_b_connected_pawn_area[side][sq] & p_occ_f)
      {
        reduce = !!(_b_doubled_pawn_area[side][sq] & p_occ_o);
        score_mid += connected_bonus_mid[r] >> (reduce ? 1 : 0);
        score_end += connected_bonus_end[r] >> (reduce ? 1 : 0);
      }
      else {
        p_occ_x = p_occ_f ^ _b(sq);

        // doubled pawn
        if (_b_doubled_pawn_area[side][sq] & p_occ_x)
        {
          score_mid -= doubled_penalty[0];
          score_end -= doubled_penalty[1];
        }

        // backward pawn
        if (!(_b_passer_area[side ^ 1][ssq] & ~_b_file[f] & p_occ_x) &&
            ((_b_piece_area[PAWN | (side << SIDE_SHIFT)][ssq] | _b(ssq)) & p_occ_o))
        {
          score_mid -= backward_penalty[0];
          score_end -= backward_penalty[1];
        }

        // isolated pawn
        if (!(_b_isolated_pawn_area[f] & p_occ_x))
        {
          score_mid -= isolated_penalty[0];
          score_end -= isolated_penalty[1];
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

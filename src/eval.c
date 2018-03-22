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
#include "eval.h"
#include "game.h"
#include "phash.h"
#include "position.h"

#define BAD_PAWNS_SHIFT           4
#define CHECK_SHIFT               2
#define THREAT_SHIFT              4
#define KING_PAWNS_SHIFT          3
#define K_CNT_LIMIT               8

#define BISHOP_PAIR_MID           40
#define BISHOP_PAIR_END           60

#define PHASE_SHIFT               7
#define TOTAL_PHASE               (1 << PHASE_SHIFT)

const int piece_value[N_PIECES] = { 100, 310, 330, 500, 950, 20000 };
const int piece_phase[N_PIECES] = { 0, 6, 6, 13, 28, 0 };

const int k_cnt_mul[K_CNT_LIMIT] = { 0, 1, 6, 16, 18, 20, 21, 22 };

const int m_shift_mid[N_PIECES] = { 0, 3, 3, 2, 1, 0 };
const int m_shift_end[N_PIECES] = { 0, 2, 1, 3, 3, 0 };

const int passer_bonus[8] = { 0, 0, 4, 10, 40, 70, 90, 0 };

uint8_t distance[BOARD_SIZE][BOARD_SIZE];

void init_distance()
{
  int i, j;

  for (i = 0; i < BOARD_SIZE; i ++)
    for (j = 0; j < BOARD_SIZE; j ++)
      distance[i][j] = _max(_abs(_rank(i) - _rank(j)), _abs(_file(i) - _file(j)));
}

phash_data_t eval_pawns(position_t *pos)
{
  int m, r, side, sq, ssq, k_sq_f, k_sq_o, passers_score, bad_pawns, distance_score;
  uint64_t b, p_occ, p_occ_f, p_occ_o, p_occ_x;
  phash_data_t phash_data;

  if (get_phash_data(pos, &phash_data))
    return phash_data;

  p_occ = pos->piece_occ[PAWN];
  passers_score = bad_pawns = distance_score = 0;

  for (side = WHITE; side < N_SIDES; side ++)
  {
    k_sq_f = pos->k_sq[side];
    k_sq_o = pos->k_sq[side ^ 1];
    p_occ_f = p_occ & pos->occ[side];
    p_occ_o = p_occ & pos->occ[side ^ 1];

    m = (side == WHITE) ? 7 : 0;
    b = p_occ_f;

    _loop(b)
    {
      sq = _bsf(b);

      // passers / distance bonus
      if (!(_b_passer_area[side][sq] & p_occ_o))
      {
        r = m ^ _rank(sq);
        ssq = (side == WHITE) ? (sq - 8) : (sq + 8);
        distance_score += (distance[ssq][k_sq_o] << r) -
                          (distance[ssq][k_sq_f] << (r - 1));
        passers_score += passer_bonus[r];
      }

      // doubled / isolated pawns
      p_occ_x = p_occ_f ^ _b(sq);
      if ((_b_doubled_pawn_area[side][sq] & p_occ_x) ||
          !(_b_isolated_pawn_area[_file(sq)] & p_occ_x))
        bad_pawns ++;
    }

    bad_pawns = -bad_pawns;
    passers_score = -passers_score;
    distance_score = -distance_score;
  }

  bad_pawns <<= BAD_PAWNS_SHIFT;
  return set_phash_data(pos, passers_score - bad_pawns,
                             passers_score - bad_pawns + distance_score);
}

int eval(position_t *pos)
{
  int side, score_mid, score_end, k_score, threat_score, k_cnt, pcnt, k_sq, sq;
  uint64_t b, b0, k_zone, occ, occ_f, occ_o, n_occ, p_occ, p_occ_f, p_occ_o,
           n_att, b_att, r_att;
  phash_data_t phash_data;

  phash_data = eval_pawns(pos);
  score_mid = pos->score_mid + phash_data.score_mid;
  score_end = pos->score_end + phash_data.score_end;

  p_occ = pos->piece_occ[PAWN];
  occ = pos->occ[WHITE] | pos->occ[BLACK];

  for (side = WHITE; side < N_SIDES; side ++)
  {
    k_score = k_cnt = threat_score = 0;
    k_sq = pos->k_sq[side ^ 1];
    k_zone = _b_king_zone[k_sq];

    occ_f = pos->occ[side];
    occ_o = pos->occ[side ^ 1];
    p_occ_f = p_occ & occ_f;
    p_occ_o = p_occ & occ_o;
    n_occ = ~(p_occ_f | pawn_attacks(p_occ_o, side ^ 1));

    n_att = knight_attack(occ, k_sq);
    b_att = bishop_attack(occ, k_sq);
    r_att = rook_attack(occ, k_sq);

    #define _score_rook_open_files                                             \
      b = _b_file[_file(sq)];                                                  \
      if (!(b & p_occ_f))                                                      \
        score_mid += (8 - _popcnt(occ & b)) << ((b & p_occ_o) ? 1 : 2);

    #define _score_piece(piece, method, att, additional_computation)           \
      b0 = pos->piece_occ[piece] & occ_f;                                      \
      _loop(b0)                                                                \
      {                                                                        \
        sq = _bsf(b0);                                                         \
        b = method(occ, sq) & n_occ;                                           \
                                                                               \
        /* mobility */                                                         \
        pcnt = _popcnt(b);                                                     \
        score_mid += pcnt << m_shift_mid[piece];                               \
        score_end += pcnt << m_shift_end[piece];                               \
                                                                               \
        /* threats */                                                          \
        threat_score += _popcnt(b & occ_o);                                    \
                                                                               \
        /* king safety */                                                      \
        b &= k_zone | att;                                                     \
        if (b)                                                                 \
        {                                                                      \
          k_cnt ++;                                                            \
          k_score += _popcnt(b & k_zone) + (_popcnt(b & att) << CHECK_SHIFT);  \
        }                                                                      \
        additional_computation                                                 \
      }

    _score_piece(KNIGHT, knight_attack, n_att,);
    _score_piece(BISHOP, bishop_attack, b_att,);
    _score_piece(ROOK, rook_attack, r_att, _score_rook_open_files);

    b_att |= r_att;
    _score_piece(QUEEN, queen_attack, b_att,);

    // threats
    score_end += threat_score << THREAT_SHIFT;

    // scale king safety
    score_mid += k_score * k_cnt_mul[_min(k_cnt, K_CNT_LIMIT - 1)];

    // bishop pair bonus
    if (_popcnt(pos->piece_occ[BISHOP] & occ_f) >= 2)
    {
      score_mid += BISHOP_PAIR_MID;
      score_end += BISHOP_PAIR_END;
    }

    // pawns in the king zone
    score_mid -= _popcnt(k_zone & p_occ_o) << KING_PAWNS_SHIFT;

    if (side == WHITE || pos->side == WHITE)
    {
      score_mid = -score_mid;
      score_end = -score_end;
    }
  }

  if (pos->phase >= TOTAL_PHASE)
    return score_end;
  else
    return ((score_mid * (TOTAL_PHASE - pos->phase)) +
            (score_end * pos->phase)) >> PHASE_SHIFT;
}

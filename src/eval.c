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
#include "pawn_eval.h"
#include "phash.h"
#include "position.h"

#define SAFE_CHECK_SHIFT          3
#define K_SQ_ATTACK_SHIFT         1
#define PUSHED_PASSERS_SHIFT      4
#define THREAT_SHIFT              4
#define PAWN_THREAT_SHIFT         6
#define PUSHED_PAWN_THREAT_SHIFT  4
#define PAWN_MOBILITY_SHIFT       2
#define PAWN_ATTACK_SHIFT         1
#define BISHOP_PAIR_BONUS         40
#define K_CNT_LIMIT               8

#define PHASE_SHIFT               7
#define TOTAL_PHASE               (1 << PHASE_SHIFT)
#define TEMPO                     10

const int piece_value[N_PIECES] = { 100, 310, 330, 500, 1000, 20000 };
const int piece_phase[N_PIECES] = { 0, 6, 6, 13, 28, 0 };

const int k_cnt_mul[K_CNT_LIMIT] = { 0, 2, 10, 16, 18, 20, 21, 22 };

const int m_mul_mid[N_PIECES] = { 0, 8, 6, 3, 2, 0 };
const int m_mul_end[N_PIECES] = { 0, 4, 3, 6, 5, 0 };

int eval(position_t *pos)
{
  int side, score, score_mid, score_end, pcnt, k_sq, sq,
      k_score[N_SIDES], k_cnt[N_SIDES];
  uint64_t b, b0, k_zone, occ, occ_f, occ_o, occ_o_np, occ_x, n_occ,
           p_occ, p_occ_f, p_occ_o, n_att, b_att, r_att, pushed_passers,
           p_pushed_safe, p_pushed[N_SIDES], p_att[N_SIDES],
           att_area[N_SIDES], att_area_nk[N_SIDES], checks[N_SIDES];
  phash_data_t phash_data;

  phash_data = pawn_eval(pos);
  score_mid = pos->score_mid + phash_data.score_mid;
  score_end = pos->score_end + phash_data.score_end;

  p_occ = pos->piece_occ[PAWN];
  occ = pos->occ[WHITE] | pos->occ[BLACK];
  pushed_passers = phash_data.pushed_passers;

  for (side = WHITE; side < N_SIDES; side ++)
  {
    p_occ_f = p_occ & pos->occ[side];
    p_att[side] = pawn_attacks(p_occ_f, side);
    p_pushed[side] = pushed_pawns(p_occ_f, ~occ, side);
  }

  for (side = WHITE; side < N_SIDES; side ++)
  {
    k_sq = pos->k_sq[side ^ 1];
    k_zone = _b_king_zone[k_sq];

    occ_f = pos->occ[side];
    occ_o = pos->occ[side ^ 1];
    p_occ_f = p_occ & occ_f;
    p_occ_o = p_occ & occ_o;
    n_occ = ~(p_occ_f | p_att[side ^ 1]);
    occ_o_np = occ_o & ~p_occ_o;

    n_att = knight_attack(occ, k_sq);
    b_att = bishop_attack(occ, k_sq);
    r_att = rook_attack(occ, k_sq);

    checks[side] = 0;
    att_area_nk[side] = p_att[side];
    k_score[side] = k_cnt[side] = 0;

    #define _score_rook_open_files                                             \
      b = _b_file[_file(sq)];                                                  \
      if (!(b & p_occ_f))                                                      \
        score_mid += (8 - _popcnt(occ & b)) << ((b & p_occ_o) ? 1 : 2);

    #define _score_piece(piece, method, att, additional_computation)           \
      b0 = pos->piece_occ[piece] & occ_f;                                      \
      if (b0)                                                                  \
      {                                                                        \
        pcnt = 0;                                                              \
        _loop(b0)                                                              \
        {                                                                      \
          sq = _bsf(b0);                                                       \
          att_area_nk[side] |= b = method(occ_x, sq);                          \
                                                                               \
          b &= n_occ;                                                          \
          pcnt += _popcnt(b);                                                  \
                                                                               \
          /* king safety */                                                    \
          b &= k_zone | att;                                                   \
          if (b)                                                               \
          {                                                                    \
            k_cnt[side] ++;                                                    \
            k_score[side] += _popcnt(b & k_zone);                              \
                                                                               \
            checks[side] |= b &= att;                                          \
            k_score[side] += _popcnt(b);                                       \
          }                                                                    \
          additional_computation                                               \
        }                                                                      \
        /* mobility */                                                         \
        score_mid += pcnt * m_mul_mid[piece];                                  \
        score_end += pcnt * m_mul_end[piece];                                  \
      }

    occ_x = occ ^ pos->piece_occ[QUEEN];
    _score_piece(KNIGHT, knight_attack, n_att,);
    _score_piece(BISHOP, bishop_attack, b_att,);

    b_att |= r_att;
    _score_piece(QUEEN, queen_attack, b_att,);

    occ_x ^= pos->piece_occ[ROOK] & occ_f & ~(side == WHITE ? _B_RANK_1 : _B_RANK_8);
    _score_piece(ROOK, rook_attack, r_att, _score_rook_open_files);

    // include king attack
    att_area[side] = att_area_nk[side] | _b_piece_area[KING][pos->k_sq[side]];

    // passer protection/attacks
    score_end += _popcnt(att_area[side] & pushed_passers) << PUSHED_PASSERS_SHIFT;

    // threats
    score_end += _popcnt(att_area[side] & occ_o & n_occ) << THREAT_SHIFT;

    // threats by protected pawns
    pcnt = _popcnt(
      pawn_attacks(att_area[side] & p_occ_f, side) & occ_o_np
    ) << PAWN_THREAT_SHIFT;

    score_mid += pcnt;
    score_end += pcnt;

    // threats by protected pawns (after push)
    pcnt = _popcnt(
      pawn_attacks(att_area[side] & p_pushed[side], side) & occ_o_np
    ) << PUSHED_PAWN_THREAT_SHIFT;

    score_mid += pcnt;
    score_end += pcnt >> 1;

    // bishop pair bonus
    if (_popcnt(pos->piece_occ[BISHOP] & occ_f) >= 2)
    {
      score_mid += BISHOP_PAIR_BONUS;
      score_end += BISHOP_PAIR_BONUS;
    }

    score_mid = -score_mid;
    score_end = -score_end;
  }

  // use precalculated attacks (a separate loop is required)
  for (side = WHITE; side < N_SIDES; side ++)
  {
    p_pushed_safe = p_pushed[side] & (att_area[side] | ~att_area[side ^ 1]);

    // pawn mobility
    score_end += _popcnt(p_pushed_safe) << PAWN_MOBILITY_SHIFT;

    // pawn attacks on the king zone
    if ((b = p_pushed_safe & _b_king_zone[pos->k_sq[side ^ 1]]))
    {
      k_score[side] += _popcnt(b) << PAWN_ATTACK_SHIFT;
      k_cnt[side] ++;
    }

    // bonus for safe checks
    if ((b = checks[side] & ~att_area[side ^ 1]))
    {
      k_cnt[side] ++;
      k_score[side] += _popcnt(b) << SAFE_CHECK_SHIFT;
    }

    // attacked squares next to the king
    b = _b_piece_area[KING][pos->k_sq[side ^ 1]] &
         att_area[side] & ~att_area_nk[side ^ 1];
    k_score[side] += _popcnt(b) << K_SQ_ATTACK_SHIFT;

    // scale king safety
    score_mid += k_score[side] * k_cnt_mul[_min(k_cnt[side], K_CNT_LIMIT - 1)];

    score_mid = -score_mid;
    score_end = -score_end;
  }

  if (pos->side == BLACK)
  {
    score_mid = -score_mid;
    score_end = -score_end;
  }

  if (pos->phase >= TOTAL_PHASE)
    score = score_end;
  else
    score = ((score_mid * (TOTAL_PHASE - pos->phase)) +
             (score_end * pos->phase)) >> PHASE_SHIFT;

  return score + TEMPO;
}

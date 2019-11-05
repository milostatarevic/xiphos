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
#include "pawn_eval.h"
#include "phash.h"
#include "position.h"
#include "tables.h"

#define SAFE_CHECK_BONUS          3
#define PUSHED_PASSERS_BONUS      9
#define BEHIND_PAWN_BONUS         9
#define PAWN_ATTACK_BONUS         2
#define K_SQ_ATTACK               2
#define K_CNT_LIMIT               8

#define PHASE_SHIFT               7
#define TOTAL_PHASE               (1 << PHASE_SHIFT)
#define TEMPO                     10

const int k_cnt_mul[K_CNT_LIMIT] = { 0, 3, 7, 12, 16, 18, 19, 20 };

int eval(position_t *pos)
{
  int side, score, score_mid, score_end, pcnt, sq, k_sq_f, k_sq_o,
      piece_o, open_file, initiative_bonus, k_score[N_SIDES], k_cnt[N_SIDES];
  uint64_t b, b0, b1, k_zone, occ, occ_f, occ_o, occ_o_np, occ_o_nk, occ_x,
           p_occ, p_occ_f, p_occ_o, n_att, b_att, r_att, pushed_passers, safe_area,
           p_safe_att, p_pushed[N_SIDES], mob_area[N_SIDES], att_area[N_SIDES],
           d_att_area[N_SIDES], checks[N_SIDES], piece_att[N_SIDES][N_PIECES];
  phash_data_t phash_data;

  phash_data = pawn_eval(pos);
  score_mid = pos->score_mid + phash_data.score_mid;
  score_end = pos->score_end + phash_data.score_end;

  p_occ = pos->piece_occ[PAWN];
  occ = _occ(pos);
  pushed_passers = phash_data.pushed_passers;

  for (side = WHITE; side < N_SIDES; side ++)
  {
    p_occ_f = p_occ & pos->occ[side];
    p_pushed[side] = pushed_pawns(p_occ_f, ~occ, side);
    piece_att[side][PAWN] = pawn_attacks(p_occ_f, side);
  }

  for (side = WHITE; side < N_SIDES; side ++)
  {
    k_sq_f = pos->k_sq[side];
    k_sq_o = pos->k_sq[side ^ 1];
    k_zone = _b_king_zone[k_sq_o];

    occ_f = pos->occ[side];
    occ_o = pos->occ[side ^ 1];
    p_occ_f = p_occ & occ_f;
    p_occ_o = p_occ & occ_o;
    occ_o_nk = occ_o & ~_b(k_sq_o);
    occ_x = occ ^ pos->piece_occ[QUEEN];

    n_att = knight_attack(occ, k_sq_o);
    b_att = bishop_attack(occ_x, k_sq_o);
    r_att = rook_attack(occ_x, k_sq_o);

    mob_area[side] = ~(p_occ_f | piece_att[side ^ 1][PAWN] | _b(k_sq_f));

    piece_att[side][KING] = _b_piece_area[KING][pos->k_sq[side]];
    att_area[side] = piece_att[side][PAWN] | piece_att[side][KING];
    d_att_area[side] = piece_att[side][PAWN] & piece_att[side][KING];

    checks[side] = 0;
    k_score[side] = k_cnt[side] = 0;

    #define _score_rook_open_files                                             \
      b1 = _b_file[_file(sq)];                                                 \
      if (!(b1 & p_occ_f))                                                     \
      {                                                                        \
        open_file = !(b1 & p_occ_o);                                           \
        score_mid += rook_file_bonus[PHASE_MID][open_file];                    \
        score_end += rook_file_bonus[PHASE_END][open_file];                    \
      }

    #define _score_threats(piece)                                              \
      b1 = b & occ_o_nk;                                                       \
      _loop(b1)                                                                \
      {                                                                        \
        piece_o = _to_white(pos->board[_bsf(b1)]);                             \
        score_mid += threats[PHASE_MID][piece][piece_o];                       \
        score_end += threats[PHASE_END][piece][piece_o];                       \
      }
    #define _nop(...)

    #define _score_piece(piece, method, att, _threats, _rook_bonus)            \
      piece_att[side][piece] = 0;                                              \
      b0 = pos->piece_occ[piece] & occ_f;                                      \
      _loop(b0)                                                                \
      {                                                                        \
        sq = _bsf(b0);                                                         \
        b = method(occ_x, sq);                                                 \
        piece_att[side][piece] |= b;                                           \
        d_att_area[side] |= att_area[side] & b;                                \
        att_area[side] |= b;                                                   \
        b &= mob_area[side];                                                   \
                                                                               \
        _threats(piece);                                                       \
                                                                               \
        /* mobility */                                                         \
        pcnt = _popcnt(b);                                                     \
        score_mid += mobility[PHASE_MID][piece][pcnt];                         \
        score_end += mobility[PHASE_END][piece][pcnt];                         \
                                                                               \
        /* king safety */                                                      \
        b &= k_zone | att;                                                     \
        if (b)                                                                 \
        {                                                                      \
          k_cnt[side] ++;                                                      \
          k_score[side] += _popcnt(b & k_zone);                                \
                                                                               \
          checks[side] |= b &= att;                                            \
          k_score[side] += _popcnt(b);                                         \
        }                                                                      \
                                                                               \
        _rook_bonus                                                            \
      }

    _score_piece(KNIGHT, knight_attack, n_att, _score_threats, );
    _score_piece(BISHOP, bishop_attack, b_att, _score_threats, );

    b_att |= r_att;
    _score_piece(QUEEN, queen_attack, b_att, _nop, );

    occ_x ^= pos->piece_occ[ROOK] & occ_f & ~(side == WHITE ? _B_RANK_1 : _B_RANK_8);
    _score_piece(ROOK, rook_attack, r_att, _score_threats, _score_rook_open_files);

    // passer protection/attacks
    score_end += _popcnt(att_area[side] & pushed_passers) * PUSHED_PASSERS_BONUS;

    // threat by king
    if(piece_att[side][KING] & mob_area[side] & occ_o)
    {
      score_mid += threat_king[PHASE_MID];
      score_end += threat_king[PHASE_END];
    }

    // N/B behind pawns
    b = (side == WHITE ? p_occ << 8 : p_occ >> 8) & occ_f &
        (pos->piece_occ[KNIGHT] | pos->piece_occ[BISHOP]);
    score_mid += _popcnt(b) * BEHIND_PAWN_BONUS;

    // bishop pair bonus
    if (_popcnt(pos->piece_occ[BISHOP] & occ_f) >= 2)
    {
      score_mid += bishop_pair[PHASE_MID];
      score_end += bishop_pair[PHASE_END];
    }

    score_mid = -score_mid;
    score_end = -score_end;
  }

  // use precalculated attacks (a separate loop is required)
  for (side = WHITE; side < N_SIDES; side ++)
  {
    occ_f = pos->occ[side];
    occ_o = pos->occ[side ^ 1];
    p_occ_f = p_occ & occ_f;
    p_occ_o = p_occ & occ_o;
    occ_o_np = occ_o & ~p_occ_o;
    safe_area = att_area[side] | ~att_area[side ^ 1];

    // pawn mobility
    pcnt = _popcnt(p_pushed[side] & safe_area);
    score_mid += pcnt * pawn_mobility[PHASE_MID];
    score_end += pcnt * pawn_mobility[PHASE_END];

    // pawn attacks on the king zone
    p_safe_att = pawn_attacks(p_occ_f & safe_area, side);
    k_score[side] +=
      _popcnt(p_safe_att & _b_king_zone[pos->k_sq[side ^ 1]]) * PAWN_ATTACK_BONUS;

    // threats by protected pawns
    pcnt = _popcnt(p_safe_att & occ_o_np);
    score_mid += pcnt * threat_protected_pawn[PHASE_MID];
    score_end += pcnt * threat_protected_pawn[PHASE_END];

    // threats by protected pawns (after push)
    pcnt = _popcnt(pawn_attacks(p_pushed[side] & safe_area, side) & occ_o_np);
    score_mid += pcnt * threat_protected_pawn_push[PHASE_MID];
    score_end += pcnt * threat_protected_pawn_push[PHASE_END];

    // bonus for safe checks
    b = checks[side] & ~occ_f;
    b &= ~att_area[side ^ 1] |
        (d_att_area[side] & ~d_att_area[side ^ 1] &
        (piece_att[side ^ 1][KING] | piece_att[side ^ 1][QUEEN]));
    if (b)
    {
      k_cnt[side] ++;
      k_score[side] += _popcnt(b) * SAFE_CHECK_BONUS;
    }

    // attacked squares next to the king
    b = piece_att[side ^ 1][KING] & att_area[side] & ~d_att_area[side ^ 1];
    k_score[side] += _popcnt(b) * K_SQ_ATTACK;

    // scale king safety
    score_mid += _sqr(k_score[side]) * k_cnt_mul[_min(k_cnt[side], K_CNT_LIMIT - 1)] / 8;

    // potential threats on the opponent's queen
    #define _score_threats_on_queen(piece, method)                             \
      b = piece_att[side][piece] & method(occ, sq) & safe_area;                \
      if (piece != KNIGHT) b &= d_att_area[side];                              \
      if (b)                                                                   \
      {                                                                        \
        pcnt = _popcnt(b);                                                     \
        score_mid += pcnt * threats_on_queen[piece][PHASE_MID];                \
        score_end += pcnt * threats_on_queen[piece][PHASE_END];                \
      }

    b = pos->piece_occ[QUEEN] & occ_o;
    if (b)
    {
      sq = _bsf(b);
      safe_area =
        mob_area[side] &
        (~att_area[side ^ 1] | (d_att_area[side] & ~d_att_area[side ^ 1]));

      _score_threats_on_queen(KNIGHT, knight_attack);
      _score_threats_on_queen(BISHOP, bishop_attack);
      _score_threats_on_queen(ROOK, rook_attack);
    }

    score_mid = -score_mid;
    score_end = -score_end;
  }

  if (pos->side == BLACK)
  {
    score_mid = -score_mid;
    score_end = -score_end;
  }

  // initiative
  initiative_bonus =
    initiative[0] * _popcnt(p_occ) +
    initiative[1] * ((p_occ & _B_Q_SIDE) && (p_occ & _B_K_SIDE)) +
    initiative[2] * (_popcnt(occ & ~p_occ) == 2) -
    initiative[3];

  score_end += _sign(score_end) * _max(initiative_bonus, -_abs(score_end));

  // score interpolation
  if (pos->phase >= TOTAL_PHASE)
    score = score_end;
  else
    score = ((score_mid * (TOTAL_PHASE - pos->phase)) +
             (score_end * pos->phase)) >> PHASE_SHIFT;

  return score + TEMPO;
}

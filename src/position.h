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

#ifndef POSITION_H
#define POSITION_H

#ifndef _NOPOPCNT
  #include <xmmintrin.h>
#endif
#include "bitboard.h"
#include "game.h"
#include "move.h"

#define _occ(pos)                 (pos->occ[WHITE] | pos->occ[BLACK])
#define _f_piece(pos, w_piece)    ((w_piece) | ((pos)->side << SIDE_SHIFT))
#define _o_piece(pos, w_piece)    ((w_piece) | (((pos)->side ^ 1) << SIDE_SHIFT))

typedef struct {
  uint8_t  side,
           ep_sq,
           c_flag,
           in_check,
           see_pins,
           fifty_cnt,
           phase,
           k_sq[N_SIDES];
  int16_t  score_mid,
           score_end,
           static_score;
  uint64_t phash_key,
           occ[N_SIDES],
           piece_occ[N_PIECES - 1],
           pinned[N_SIDES],
           pinners[N_SIDES];
  move_t   move;
  uint8_t  board[BOARD_SIZE];
} __attribute__ ((aligned (16))) position_t;
_Static_assert(sizeof(position_t) == 192, "position_t size error");

void set_pins_and_checks(position_t *);
int is_pseudo_legal(position_t *pos, move_t move);
int legal_move(position_t *, move_t);
int SEE(position_t *, move_t, int);

int insufficient_material(position_t *);
int non_pawn_material(position_t *);
void set_phase(position_t *);
void reevaluate_position(position_t *);

static inline void position_cpy(position_t *dest, position_t *src)
{
#ifdef _NOPOPCNT
  *dest = *src;
#else
  register __m128 x0, x1, x2, x3, x4, x5, *s, *d;

  s = (__m128 *)src;
  d = (__m128 *)dest;

  x0 = s[0]; x1 = s[1]; x2 = s[2]; x3 = s[3]; x4 = s[4]; x5 = s[5];
  d[0] = x0; d[1] = x1; d[2] = x2; d[3] = x3; d[4] = x4; d[5] = x5;

  x0 = s[6]; x1 = s[7]; x2 = s[8]; x3 = s[9]; x4 = s[10]; x5 = s[11];
  d[6] = x0; d[7] = x1; d[8] = x2; d[9] = x3; d[10] = x4; d[11] = x5;
#endif
}

static inline void pins_and_attacks_to(
  position_t *pos, int sq, int att_side, int pin_side,
  uint64_t *pinned, uint64_t *pinners, uint64_t *b_att, uint64_t *r_att)
{
  int p_sq;
  uint64_t b, bq, rq, occ, occ_att, occ_pin, line;

  occ_att = pos->occ[att_side];
  occ_pin = pos->occ[pin_side];
  occ = occ_att | pos->occ[att_side ^ 1];

  bq = (pos->piece_occ[BISHOP] | pos->piece_occ[QUEEN]) & occ_att;
  rq = (pos->piece_occ[ROOK]   | pos->piece_occ[QUEEN]) & occ_att;

  *b_att = bishop_attack(occ, sq);
  *r_att =   rook_attack(occ, sq);

  b  = (*b_att ^ bishop_attack(occ ^ (occ_pin & *b_att), sq)) & bq;
  b |= (*r_att ^   rook_attack(occ ^ (occ_pin & *r_att), sq)) & rq;

  *pinned = *pinners = 0;
  _loop(b)
  {
    p_sq = _bsf(b);
    line = _b_line[sq][p_sq];
    if (line)
    {
      *pinned |= line;
      *pinners |= _b(p_sq);
    }
  }
  *pinned &= occ_pin;
}

static inline int move_is_quiet(position_t *pos, move_t move)
{
  if (pos->board[_m_to(move)] != EMPTY)
    return 0;

  if (_equal_to(pos->board[_m_from(move)], PAWN) &&
     (_m_promoted_to(move) || _m_to(move) == pos->ep_sq))
    return 0;

  return 1;
}

#endif

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

#ifndef POSITION_H
#define POSITION_H

#ifndef _NOPOPCNT
  #include <xmmintrin.h>
#endif
#include "bitboard.h"
#include "game.h"
#include "move.h"

#define _f_piece(pos, w_piece)    ((w_piece) | ((pos)->side << SIDE_SHIFT))
#define _o_piece(pos, w_piece)    ((w_piece) | (((pos)->side ^ 1) << SIDE_SHIFT))

typedef struct {
  uint8_t  side,
           ep_sq,
           c_flag,
           in_check,
           fifty_cnt,
           phase,
           k_sq[N_SIDES];
  int16_t  score_mid,
           score_end;
  uint64_t phash_key,
           occ[N_SIDES],
           piece_occ[N_PIECES - 1],
           pinned;
  move_t   move;
  uint8_t  board[BOARD_SIZE];
} __attribute__ ((aligned (16))) position_t;
_Static_assert(sizeof(position_t) == 160, "position_t size error");

void set_pins_and_checks(position_t *);
int legal_move(position_t *, move_t);
int gives_check(position_t *, move_t, uint64_t, uint64_t, uint64_t);
int bad_SEE(position_t *, move_t);

int insufficient_material(position_t *);
int non_pawn_material(position_t *);
void reevaluate_position(position_t *);

static inline void position_cpy(position_t *dest, position_t *src)
{
#ifdef _NOPOPCNT
  *dest = *src;
#else
  register __m128 x0, x1, x2, x3, x4, *s, *d;

  s = (__m128 *)src;
  d = (__m128 *)dest;

  x0 = s[0]; x1 = s[1]; x2 = s[2]; x3 = s[3]; x4 = s[4];
  d[0] = x0; d[1] = x1; d[2] = x2; d[3] = x3; d[4] = x4;

  x0 = s[5]; x1 = s[6]; x2 = s[7]; x3 = s[8]; x4 = s[9];
  d[5] = x0; d[6] = x1; d[7] = x2; d[8] = x3; d[9] = x4;
#endif
}

static inline void pins_and_attacks_to(
  position_t *pos, int sq, int att_side, int pin_side, uint64_t *pinned,
  uint64_t *b_att, uint64_t *r_att)
{
  uint64_t b, bq, rq, occ, occ_att, occ_pin;

  occ_att = pos->occ[att_side];
  occ_pin = pos->occ[pin_side];
  occ = occ_att | pos->occ[att_side ^ 1];

  bq = (pos->piece_occ[BISHOP] | pos->piece_occ[QUEEN]) & occ_att;
  rq = (pos->piece_occ[ROOK]   | pos->piece_occ[QUEEN]) & occ_att;

  *b_att = bishop_attack(occ, sq);
  *r_att =   rook_attack(occ, sq);

  b  = (*b_att ^ bishop_attack(occ ^ (occ_pin & *b_att), sq)) & bq;
  b |= (*r_att ^   rook_attack(occ ^ (occ_pin & *r_att), sq)) & rq;

  *pinned = 0;
  _loop(b)
    *pinned |= _b_line[sq][_bsf(b)];
  *pinned &= occ_pin;
}

#endif

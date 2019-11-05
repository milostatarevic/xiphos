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

#ifndef BITBOARD_H
#define BITBOARD_H

#include "game.h"
#ifndef _BMI2
  #include "magic.h"
#endif

#define _B_RANK_1       0xff00000000000000ULL
#define _B_RANK_2       0x00ff000000000000ULL
#define _B_RANK_4       0x000000ff00000000ULL
#define _B_RANK_5       0x00000000ff000000ULL
#define _B_RANK_7       0x000000000000ff00ULL
#define _B_RANK_8       0x00000000000000ffULL

#define _B_FILE_A       0x0101010101010101ULL
#define _B_FILE_H       0x8080808080808080ULL

#define _B_Q_SIDE       0x0f0f0f0f0f0f0f0fULL
#define _B_K_SIDE       0xf0f0f0f0f0f0f0f0ULL

#define _B_RING         0x007e7e7e7e7e7e00ULL
#define _B_RING_C       0x817e7e7e7e7e7e81ULL

#define _b(sq)          (1ULL << (sq))
#define _loop(b)        for(; (b); (b) &= (b) - 1)

typedef struct {
  uint64_t *attacks;
  uint64_t mask;
#ifndef _BMI2
  uint64_t magic;
  uint32_t shift;
#endif
} attack_lookup_t;

extern attack_lookup_t bishop_attack_lookup[BOARD_SIZE],
                       rook_attack_lookup[BOARD_SIZE];

extern uint64_t _b_piece_area[P_LIMIT][BOARD_SIZE],
                _b_passer_area[N_SIDES][BOARD_SIZE],
                _b_connected_pawn_area[N_SIDES][BOARD_SIZE],
                _b_doubled_pawn_area[N_SIDES][BOARD_SIZE],
                _b_isolated_pawn_area[N_FILE], _b_file[N_FILE],
                _b_king_zone[BOARD_SIZE],
                _b_line[BOARD_SIZE][BOARD_SIZE];

static inline int _bsf(uint64_t b)
{
#ifdef _NOPOPCNT
  return __builtin_ctzll(b);
#else
  uint64_t r;
  asm("bsfq %1, %0" : "=r" (r) : "r" (b));
  return r;
#endif
}

static inline int _popcnt(uint64_t b)
{
#if defined(_NOPOPCNT)
  extern uint8_t popcnt_lookup[];
  return popcnt_lookup[b & 0xffff] +
         popcnt_lookup[(b >> 16) & 0xffff] +
         popcnt_lookup[(b >> 32) & 0xffff] +
         popcnt_lookup[b >> 48];
#else
  uint64_t r;
  asm("popcntq %1, %0" : "=r" (r) : "r" (b));
  return r;
#endif
}

#ifdef _BMI2
  static inline uint64_t _pext(uint64_t occ, uint64_t mask)
  {
    uint64_t r;
    asm("pextq %2, %1, %0" : "=r" (r) : "r" (occ), "r" (mask));
    return r;
  }
#endif

static inline uint64_t bishop_attack(uint64_t occ, int sq)
{
  attack_lookup_t *att;

  att = bishop_attack_lookup + sq;
#ifdef _BMI2
  return att->attacks[_pext(occ, att->mask)];
#else
  return att->attacks[((occ & att->mask) * att->magic) >> att->shift];
#endif
}

static inline uint64_t rook_attack(uint64_t occ, int sq)
{
  attack_lookup_t *att;

  att = rook_attack_lookup + sq;
#ifdef _BMI2
  return att->attacks[_pext(occ, att->mask)];
#else
  return att->attacks[((occ & att->mask) * att->magic) >> att->shift];
#endif
}

static inline uint64_t queen_attack(uint64_t occ, int sq)
{
  return bishop_attack(occ, sq) | rook_attack(occ, sq);
}

// keep because of compatibility
static inline uint64_t knight_attack(uint64_t _occ, int sq)
{
  return _b_piece_area[KNIGHT][sq];
}

// keep because of compatibility
static inline uint64_t king_attack(uint64_t _occ, int sq)
{
  return _b_piece_area[KING][sq];
}

static inline uint64_t pawn_attacks(uint64_t p_occ, int side)
{
  if (side == WHITE)
    return ((p_occ >> 7) & ~_B_FILE_A) | ((p_occ >> 9) & ~_B_FILE_H);
  else
    return ((p_occ << 9) & ~_B_FILE_A) | ((p_occ << 7) & ~_B_FILE_H);
}

static inline uint64_t pushed_pawns(uint64_t p_occ, uint64_t n_occ, int side)
{
  uint64_t b;

  if (side == WHITE)
  {
    b = (p_occ >> 8) & n_occ;
    return b | ((b >> 8) & n_occ & _B_RANK_4);
  }
  else
  {
    b = (p_occ << 8) & n_occ;
    return b | ((b << 8) & n_occ & _B_RANK_5);
  }
}

void init_bitboards();

#endif

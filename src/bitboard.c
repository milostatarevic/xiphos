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

typedef struct {
  int r, f;
} attack_vec_t;

const attack_vec_t attack_vec[N_PIECES][9] = {
  { { -1, 1 }, { -1, -1 }, { 0, 0 } },
  { { 1, 2 }, { 1, -2 }, { -1, 2 }, { -1, -2 }, { 2, 1 }, { -2, 1 }, { 2, -1 }, { -2, -1 }, { 0, 0 } },
  { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }, { 0, 0 } },
  { { 1, 0 }, { -1, 0 }, {  0, 1 }, {  0, -1 }, { 0, 0 } },
  { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }, { 1, 0 }, { -1, 0 }, {  0, 1 }, {  0, -1 }, { 0, 0 } },
  { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }, { 1, 0 }, { -1, 0 }, {  0, 1 }, {  0, -1 }, { 0, 0 } },
};

uint64_t bishop_attacks_table[5248],
         rook_attacks_table[102400];

attack_lookup_t bishop_attack_lookup[BOARD_SIZE],
                rook_attack_lookup[BOARD_SIZE];

uint64_t _b_piece_area[P_LIMIT][BOARD_SIZE],
         _b_passer_area[N_SIDES][BOARD_SIZE],
         _b_connected_pawn_area[N_SIDES][BOARD_SIZE],
         _b_doubled_pawn_area[N_SIDES][BOARD_SIZE],
         _b_isolated_pawn_area[N_FILE], _b_file[N_FILE],
         _b_king_zone[BOARD_SIZE],
         _b_line[BOARD_SIZE][BOARD_SIZE];

#ifdef _NOPOPCNT
  uint8_t popcnt_lookup[1 << 16];
  void init_popcnt_lookup()
  {
    int i;
    for(i = 0; i < 1 << 16; i ++)
      popcnt_lookup[i] = __builtin_popcount(i);
  }
#endif

uint64_t _piece_attack(uint64_t occ, int piece, int sq)
{
  int wp, vr, vf;
  unsigned r, f, r_sq, f_sq;
  attack_vec_t const *v;
  uint64_t b, b_sq;

  b = 0;
  wp = _to_white(piece);
  v = attack_vec[wp];
  r_sq = _rank(sq); f_sq = _file(sq);
  for (; v->r || v->f; v ++)
  {
    vr = v->r; vf = v->f;
    if (piece == (PAWN | CHANGE_SIDE)) vr = -vr;
    r = r_sq + vr; f = f_sq + vf;
    while (r < N_RANK && f < N_FILE)
    {
      b_sq = _b(_sq_rf(r, f));
      b |= b_sq;

      if(wp == PAWN || wp == KNIGHT || wp == KING || (occ & b_sq)) break;
      r += vr; f += vf;
    }
  }
  return b;
}

void init_piece_area()
{
  int p, sq, side, piece;

  for (side = 0; side < N_SIDES; side ++)
    for (p = PAWN; p <= KING; p ++)
      for (sq = 0; sq < BOARD_SIZE; sq ++)
      {
        piece = p | (side ? CHANGE_SIDE : 0);
        _b_piece_area[piece][sq] = _piece_attack(0,  piece, sq);
      }
}

void init_pawn_boards()
{
  int sq, side, r, ri, f, fi, inc;
  uint64_t b, bp, bd, bc, bi, bf;

  for (sq = 0; sq < BOARD_SIZE; sq ++)
    for (side = 0; side < N_SIDES; side ++)
    {
      bp = bd = bc = bi = 0;
      inc = side == WHITE ? -1 : 1;
      r = _rank(sq); f = _file(sq);
      for (fi = f - 1; fi <= f + 1; fi ++)
        if (fi >= 0 && fi < N_FILE)
        {
          for (ri = r + inc; ri >= 0 && ri < N_RANK; ri += inc)
          {
            b = _b(_sq_rf(ri, fi));
            bp |= b;
            if (fi == f)
              bd |= b;
          }
          if (fi != f)
            bc |= _b(_sq_rf(r, fi)) | _b(_sq_rf(r - inc, fi));
        }

      _b_passer_area[side][sq] = bp;
      _b_doubled_pawn_area[side][sq] = bd;
      _b_connected_pawn_area[side][sq] = bc;
    }

  for (f = 0; f < N_FILE; f ++)
  {
    bi = bf = 0;
    for (fi = f - 1; fi <= f + 1; fi ++)
      if (fi >= 0 && fi < N_FILE)
        for (r = 0; r < N_RANK; r ++)
        {
          b = _b(_sq_rf(r, fi));
          bi |= b;
          if (fi == f)
            bf |= b;
        }
    _b_isolated_pawn_area[f] = bi;
    _b_file[f] = bf;
  }
}

void init_king_zone()
{
  int sq;
  uint64_t b;

  for (sq = 0; sq < BOARD_SIZE; sq ++)
  {
    b = _b_piece_area[KING][sq] | _b(sq);
    if (((_rank(sq) + 1) & 6) == 0)
      b |= _b_piece_area[KING][sq ^ 8];
    _b_king_zone[sq] = b;
  }

  _b_king_zone[A1] = _b_king_zone[B1];
  _b_king_zone[A8] = _b_king_zone[B8];
  _b_king_zone[H1] = _b_king_zone[G1];
  _b_king_zone[H8] = _b_king_zone[G8];
}

uint64_t _line(int a, int b)
{
  int df, dr;
  uint64_t bb;

  df = _abs(_file(a) - _file(b));
  dr = _abs(_rank(a) - _rank(b));
  if (df != dr && dr && df)
    return 0;

  bb = (-1ULL << a) ^ (-1ULL << b);
  if (!dr || !df)
    bb &= _b_piece_area[ROOK][a] & _b_piece_area[ROOK][b];
  else
    bb &= _b_piece_area[BISHOP][a] & _b_piece_area[BISHOP][b];
  return bb;
}

void init_lines()
{
  int i, j;

  for (i = 0; i < BOARD_SIZE; i ++)
    for (j = 0; j < BOARD_SIZE; j ++)
      _b_line[i][j] = _line(i, j);
}

uint64_t _mask(int piece, int sq)
{
  uint64_t b, m;

  if (piece == BISHOP)
    b = _b_piece_area[BISHOP][sq] & _B_RING;
  else
  {
    m = _B_RING_C;
    if (_rank(sq) == RANK_1) m ^= _B_RANK_1;
    if (_rank(sq) == RANK_8) m ^= _B_RANK_8;
    if (_file(sq) == FILE_A) m ^= _B_FILE_A;
    if (_file(sq) == FILE_H) m ^= _B_FILE_H;
    b = _b_piece_area[ROOK][sq] & m;
  }

  return b;
}

static inline uint64_t _pdep(uint64_t occ, uint64_t mask)
{
#ifdef _BMI2
  uint64_t r;
  asm("pdepq %2, %1, %0" : "=r" (r) : "r" (occ), "r" (mask));
#else
  uint64_t r, i;
  r = 0; i = 1;
  _loop(mask)
  {
    if (occ & i) r |= _b(_bsf(mask));
    i <<= 1;
  }
#endif
  return r;
}

void init_attack_bitboards()
{
  int i, j, sq, p, piece, pcnt;
  int pieces[] = { BISHOP, ROOK, 0 };
  uint64_t mask, occ, *att,
           *att_tables[] = { bishop_attacks_table, rook_attacks_table };
  attack_lookup_t *att_lookup,
                  *att_lookups[] = { bishop_attack_lookup, rook_attack_lookup };

  for (p = 0; (piece = pieces[p]); p ++)
  {
    att = att_tables[p];
    for (sq = 0; sq < BOARD_SIZE; sq ++)
    {
      mask = _mask(piece, sq);
      att_lookup = att_lookups[p] + sq;
      pcnt = _popcnt(mask);

      att_lookup->mask = mask;
      att_lookup->attacks = att;
#ifndef _BMI2
      att_lookup->magic = magic[p][sq];
      att_lookup->shift = BOARD_SIZE - pcnt;
#endif

      for(i = 0; i < (1 << pcnt); i ++)
      {
        occ = _pdep(i, mask);
#ifdef _BMI2
        j = _pext(occ, mask);
#else
        j = (occ * att_lookup->magic) >> att_lookup->shift;
#endif
        att[j] = _piece_attack(occ, piece, sq);
      }
      att += i;
    }
  }
}

void init_bitboards()
{
#ifdef _NOPOPCNT
  init_popcnt_lookup();
#endif

  init_piece_area();
  init_attack_bitboards();
  init_pawn_boards();
  init_king_zone();
  init_lines();
}

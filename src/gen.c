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
#include "position.h"

// locals: m_ptr
#define _add_move(m_from, m_to)                                                \
  { *m_ptr = _m(m_from, m_to); m_ptr ++; }

// locals: m_ptr
#define _add_promotion(m_from, m_to, piece)                                    \
  {                                                                            \
    *m_ptr = _m_with_promo(                                                    \
      _m_with_score(_m(m_from, m_to), -P_LIMIT + piece),                       \
      piece                                                                    \
    );                                                                         \
    m_ptr ++;                                                                  \
  }

// locals: m_ptr, m_from, pos, b0, b1, occ, occ_f
#define _add_moves(pp, method, oc)                                             \
  b0 = pos->piece_occ[pp] & occ_f;                                             \
  _loop(b0)                                                                    \
  {                                                                            \
    m_from = _bsf(b0);                                                         \
    b1 = method(occ, m_from) & (oc);                                           \
    m_from = _m(m_from, 0);                                                    \
    _loop(b1)                                                                  \
      { *m_ptr = m_from | _bsf(b1); m_ptr ++; }                                \
  }

// locals: m_ptr, m_from, pos, b0
#define _add_king_moves(oc)                                                    \
  m_from = pos->k_sq[pos->side];                                               \
  b0 = _b_piece_area[KING][m_from] & (oc);                                     \
  m_from = _m(m_from, 0);                                                      \
  _loop(b0)                                                                    \
    { *m_ptr = m_from | _bsf(b0); m_ptr ++; }

static inline void _add_quiet_pawn_moves(
  position_t *pos, move_t **moves, uint64_t occ_f, uint64_t n_occ, uint64_t occ_x)
{
  int m_to;
  uint64_t b, p_occ, p_sh_occ;
  move_t *m_ptr;

  m_ptr = *moves;
  p_occ = pos->piece_occ[PAWN] & occ_f;
  if (pos->side == WHITE)
  {
    p_sh_occ = ((p_occ & ~_B_RANK_7) >> 8) & n_occ;
    b = p_sh_occ & occ_x;
    _loop(b)
      { m_to = _bsf(b); _add_move(m_to + 8, m_to); }

    b = (p_sh_occ >> 8) & n_occ & _B_RANK_4 & occ_x;
    _loop(b)
      { m_to = _bsf(b); _add_move(m_to + 16, m_to); }
  }
  else
  {
    p_sh_occ = ((p_occ & ~_B_RANK_2) << 8) & n_occ;
    b = p_sh_occ & occ_x;
    _loop(b)
      { m_to = _bsf(b); _add_move(m_to - 8, m_to); }

    b = (p_sh_occ << 8) & n_occ & _B_RANK_5 & occ_x;
    _loop(b)
      { m_to = _bsf(b); _add_move(m_to - 16, m_to); }
  }
  *moves = m_ptr;
}

static inline void _add_pawn_captures(
  position_t *pos, move_t **moves, uint64_t occ_f, uint64_t occ_o,
  int minor_promotions)
{
  int m_from, m_to, piece;
  uint64_t b0, b1, bp, p_occ, p_occ_c;
  move_t *m_ptr;

  m_ptr = *moves;
  p_occ = pos->piece_occ[PAWN] & occ_f;
  b0 = pawn_attacks(p_occ, pos->side);
  p_occ_c = occ_o;
  if (pos->ep_sq != NO_SQ) p_occ_c |= _b(pos->ep_sq);
  b0 &= p_occ_c;
  piece = _o_piece(pos, PAWN);

  bp = b0 & (pos->side == WHITE ? _B_RANK_8 : _B_RANK_1);

  // captures
  b0 ^= bp;
  _loop(b0)
  {
    m_to = _bsf(b0);
    b1 = _b_piece_area[piece][m_to] & p_occ;
    _loop(b1)
      _add_move(_bsf(b1), m_to);
  }

  // promotions
  _loop(bp)
  {
    m_to = _bsf(bp);
    b1 = _b_piece_area[piece][m_to] & p_occ;
    _loop(b1)
    {
      m_from = _bsf(b1);
      _add_promotion(m_from, m_to, QUEEN);
      if (minor_promotions)
      {
        _add_promotion(m_from, m_to, ROOK);
        _add_promotion(m_from, m_to, BISHOP);
        _add_promotion(m_from, m_to, KNIGHT);
      }
    }
  }
  *moves = m_ptr;
}

static inline void _add_non_capture_promotions(
  position_t *pos, move_t **moves, uint64_t occ_f, uint64_t n_occ, uint64_t occ_x,
  int minor_promotions)
{
  int m_from, m_to;
  uint64_t b, p_occ;
  move_t *m_ptr;

  m_ptr = *moves;
  p_occ = pos->piece_occ[PAWN] & occ_f;

  if (pos->side == WHITE)
    b = ((p_occ & _B_RANK_7) >> 8) & n_occ & occ_x;
  else
    b = ((p_occ & _B_RANK_2) << 8) & n_occ & occ_x;
  _loop(b)
  {
    m_to = _bsf(b);
    m_from = m_to ^ 8;
    _add_promotion(m_from, m_to, QUEEN);
    if (minor_promotions)
    {
      _add_promotion(m_from, m_to, ROOK);
      _add_promotion(m_from, m_to, BISHOP);
      _add_promotion(m_from, m_to, KNIGHT);
    }
  }
  *moves = m_ptr;
}

static inline void _add_castling_moves(position_t *pos, move_t **moves)
{
  move_t *m_ptr;

  m_ptr = *moves;
  if (pos->side == WHITE)
  {
    if ((pos->c_flag & C_FLAG_WR) && pos->board[F1] == EMPTY && pos->board[G1] == EMPTY)
      _add_move(E1, G1);
    if ((pos->c_flag & C_FLAG_WL) &&
        pos->board[B1] == EMPTY && pos->board[C1] == EMPTY && pos->board[D1] == EMPTY)
      _add_move(E1, C1);
  }
  else if (pos->side == BLACK)
  {
    if ((pos->c_flag & C_FLAG_BR) && pos->board[F8] == EMPTY && pos->board[G8] == EMPTY)
      _add_move(E8, G8);
    if ((pos->c_flag & C_FLAG_BL) &&
        pos->board[B8] == EMPTY && pos->board[C8] == EMPTY && pos->board[D8] == EMPTY)
      _add_move(E8, C8);
  }
  *moves = m_ptr;
}

void material_moves(position_t *pos, move_t *moves, int *moves_cnt,
                    int minor_promotions)
{
  uint64_t b0, b1, occ, n_occ, occ_f, occ_o;
  int m_from;
  move_t *m_ptr;

  m_ptr = moves;
  occ_f = pos->occ[pos->side];
  occ_o = pos->occ[pos->side ^ 1];
  occ = occ_f | occ_o;
  n_occ = ~occ;

  // keep lva order
  _add_king_moves(occ_o);
  _add_pawn_captures(pos, &m_ptr, occ_f, occ_o, minor_promotions);
  _add_moves(KNIGHT, knight_attack, occ_o);
  _add_moves(BISHOP, bishop_attack, occ_o);
  _add_moves(ROOK, rook_attack, occ_o);
  _add_moves(QUEEN, queen_attack, occ_o);
  _add_non_capture_promotions(pos, &m_ptr, occ_f, n_occ, n_occ, minor_promotions);

  *moves_cnt = m_ptr - moves;
}

void quiet_moves(position_t *pos, move_t *moves, int *moves_cnt)
{
  uint64_t b0, b1, occ, n_occ, occ_f;
  int m_from;
  move_t *m_ptr;

  m_ptr = moves;
  occ_f = pos->occ[pos->side];
  occ = occ_f | pos->occ[pos->side ^ 1];
  n_occ = ~occ;

  if (pos->c_flag)
    _add_castling_moves(pos, &m_ptr);

  _add_king_moves(n_occ);
  _add_quiet_pawn_moves(pos, &m_ptr, occ_f, n_occ, n_occ);

  _add_moves(KNIGHT, knight_attack, n_occ);
  _add_moves(BISHOP, bishop_attack, n_occ);
  _add_moves(ROOK, rook_attack, n_occ);
  _add_moves(QUEEN, queen_attack, n_occ);

  *moves_cnt = m_ptr - moves;
}

void get_all_moves(position_t *pos, move_t *moves, int *moves_cnt)
{
  int quiet_moves_cnt;

  material_moves(pos, moves, moves_cnt, 1);
  quiet_moves(pos, moves + *moves_cnt, &quiet_moves_cnt);

  *moves_cnt += quiet_moves_cnt;
}

void check_evasion_moves(position_t *pos, move_t *moves, int *moves_cnt)
{
  int m_from, k_sq, att_sq;
  uint64_t b0, b1, occ, n_occ, occ_f, occ_o, occ_att, att, att_line;
  move_t *m_ptr;

  m_ptr = moves;
  occ_f = pos->occ[pos->side];
  occ_o = pos->occ[pos->side ^ 1];
  occ = occ_f | occ_o;
  n_occ = ~occ;

  k_sq = pos->k_sq[pos->side];
  _add_king_moves(~occ_f);

  att = _b_piece_area[_f_piece(pos, PAWN)][k_sq] & pos->piece_occ[PAWN];
  att |= _b_piece_area[KNIGHT][k_sq] & pos->piece_occ[KNIGHT];
  att |= bishop_attack(occ, k_sq) & (pos->piece_occ[BISHOP] | pos->piece_occ[QUEEN]);
  att |= rook_attack(occ, k_sq) & (pos->piece_occ[ROOK] | pos->piece_occ[QUEEN]);
  att &= occ_o;

  if (_popcnt(att) == 1)
  {
    att_sq = _bsf(att);
    att_line = _b_line[att_sq][k_sq];
    occ_att = att_line | att;

    _add_pawn_captures(pos, &m_ptr, occ_f, att, 1);
    _add_non_capture_promotions(pos, &m_ptr, occ_f, n_occ, att_line, 1);
    _add_quiet_pawn_moves(pos, &m_ptr, occ_f, n_occ, occ_att);

    _add_moves(KNIGHT, knight_attack, occ_att);
    _add_moves(BISHOP, bishop_attack, occ_att);
    _add_moves(ROOK, rook_attack, occ_att);
    _add_moves(QUEEN, queen_attack, occ_att);
  }

  *moves_cnt = m_ptr - moves;
}

void king_moves(position_t *pos, move_t *moves, int *moves_cnt)
{
  uint64_t b0, n_occ_f, occ_f;
  int m_from;
  move_t *m_ptr;

  m_ptr = moves;
  occ_f = pos->occ[pos->side];
  n_occ_f = ~occ_f;

  if (pos->c_flag)
    _add_castling_moves(pos, &m_ptr);
  _add_king_moves(n_occ_f);

  *moves_cnt = m_ptr - moves;
}

void checks_and_material_moves(position_t *pos, move_t *moves, int *moves_cnt)
{
  uint64_t b0, b1, oc, occ, n_occ, n_occ_f, occ_f, occ_o, p_occ,
           n_att, b_att, r_att, pinned, pinners;
  int m_from, k_sq;
  move_t *m_ptr;

  // doesn't have a sense if in check
  if (pos->in_check)
    return;

  m_ptr = moves;
  occ_f = pos->occ[pos->side];
  occ_o = pos->occ[pos->side ^ 1];
  occ = occ_f | occ_o;
  n_occ_f = ~occ_f;
  n_occ = ~occ;

  k_sq = pos->k_sq[pos->side ^ 1];
  n_att = knight_attack(occ, k_sq);
  pins_and_attacks_to(pos, k_sq, pos->side, pos->side,
                      &pinned, &pinners, &b_att, &r_att);

  // queen can't make discovered checks
  _add_moves(QUEEN, queen_attack, occ_o | (n_occ & (r_att | b_att)));

  //
  // discovered checks

  occ_f &= pinned;
  if (occ_f)
  {
    // knights
    _add_moves(KNIGHT, knight_attack, n_occ_f);

    // pawns
    p_occ = pos->piece_occ[PAWN] & occ_f;

    _loop(p_occ)
    {
      m_from = _bsf(p_occ);
      oc = n_occ & ~_b_line[k_sq][m_from];

      if (oc)
      {
        // slow, but probably ok as this is rarely happening
        _add_pawn_captures(pos, &m_ptr, _b(m_from), occ_o, 1);
        _add_quiet_pawn_moves(pos, &m_ptr, _b(m_from), n_occ, oc);
        _add_non_capture_promotions(pos, &m_ptr, _b(m_from), n_occ, oc, 1);
      }
    }

    // other pieces
    #define _add_captures_and_checks(method)                                   \
      _loop(b0)                                                                \
      {                                                                        \
        m_from = _bsf(b0);                                                     \
        b1 = method(occ, m_from) & (n_occ_f & ~_b_line[k_sq][m_from]);         \
        m_from = _m(m_from, 0);                                                \
        _loop(b1)                                                              \
          { *m_ptr = m_from | _bsf(b1); m_ptr ++; }                            \
      }

    b0 = _b(pos->k_sq[pos->side]);
    if (b0 & occ_f)
      _add_captures_and_checks(king_attack);

    b0 = pos->piece_occ[BISHOP] & occ_f;
    _add_captures_and_checks(bishop_attack);

    b0 = pos->piece_occ[ROOK] & occ_f;
    _add_captures_and_checks(rook_attack);

    b0 = pos->piece_occ[QUEEN] & occ_f;
    _add_captures_and_checks(queen_attack);
  }

  //
  // other checks and captures

  occ_f = pos->occ[pos->side] & ~pinned;

  _add_king_moves(occ_o);
  _add_pawn_captures(pos, &m_ptr, occ_f, occ_o, 1);
  _add_quiet_pawn_moves(pos, &m_ptr, occ_f, n_occ, _b_piece_area[_o_piece(pos, PAWN)][k_sq]);
  _add_non_capture_promotions(pos, &m_ptr, occ_f, n_occ, n_occ, 1);

  _add_moves(KNIGHT, knight_attack, occ_o | (n_occ & n_att));
  _add_moves(BISHOP, bishop_attack, occ_o | (n_occ & b_att));
  _add_moves(ROOK, rook_attack, occ_o | (n_occ & r_att));

  *moves_cnt = m_ptr - moves;
}

int count_non_king_moves(position_t *pos)
{
  uint64_t b0, b1, p_occ, p_sh_occ, p_occ_c, occ, n_occ_f, n_occ, occ_f, occ_o;
  int moves_cnt, piece;

  moves_cnt = 0;
  occ_f = pos->occ[pos->side];
  occ_o = pos->occ[pos->side ^ 1];
  occ = occ_f | occ_o;
  n_occ_f = ~occ_f;
  n_occ = ~occ;

  #define _count_moves(pp, method)                                             \
    b0 = pos->piece_occ[pp] & occ_f;                                           \
    _loop(b0)                                                                  \
      moves_cnt += _popcnt(method(occ, _bsf(b0)) & n_occ_f);                   \

  _count_moves(KNIGHT, knight_attack);
  _count_moves(QUEEN, queen_attack);
  _count_moves(BISHOP, bishop_attack);
  _count_moves(ROOK, rook_attack);

  // pawns
  p_occ = pos->piece_occ[PAWN] & occ_f;

  // pawn captures
  b0 = pawn_attacks(p_occ, pos->side);
  p_occ_c = occ_o;
  if (pos->ep_sq != NO_SQ) p_occ_c |= _b(pos->ep_sq);
  b0 &= p_occ_c;
  piece = _o_piece(pos, PAWN);

  b1 = b0 & (pos->side == WHITE ? _B_RANK_8 : _B_RANK_1);
  b0 ^= b1;
  _loop(b0)
    moves_cnt += _popcnt(_b_piece_area[piece][_bsf(b0)] & p_occ);
  _loop(b1)
    moves_cnt += _popcnt(_b_piece_area[piece][_bsf(b1)] & p_occ) << 2;

  // quiet pawn moves
  p_sh_occ = p_occ;
  if (pos->side == WHITE)
  {
    p_sh_occ = (p_sh_occ >> 8) & n_occ;
    moves_cnt += _popcnt(p_sh_occ & ~_B_RANK_8);
    moves_cnt += _popcnt(p_sh_occ & _B_RANK_8) << 2;

    p_sh_occ = (p_sh_occ >> 8) & n_occ & _B_RANK_4;
    moves_cnt += _popcnt(p_sh_occ);
  }
  else
  {
    p_sh_occ = (p_sh_occ << 8) & n_occ;
    moves_cnt += _popcnt(p_sh_occ & ~_B_RANK_1);
    moves_cnt += _popcnt(p_sh_occ & _B_RANK_1) << 2;

    p_sh_occ = (p_sh_occ << 8) & n_occ & _B_RANK_5;
    moves_cnt += _popcnt(p_sh_occ);
  }

  return moves_cnt;
}

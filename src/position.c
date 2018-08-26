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
#include "hash.h"
#include "history.h"
#include "position.h"
#include "pst.h"

void set_pins_and_checks(position_t *pos)
{
  int k_sq, in_check;
  uint64_t occ_o, bq, rq, b_att, r_att, pinned;

  k_sq = pos->k_sq[pos->side];
  if (k_sq == NO_SQ) return;

  occ_o = pos->occ[pos->side ^ 1];
  pins_and_attacks_to(pos, k_sq, pos->side ^ 1, pos->side, &pinned, &b_att, &r_att);
  pos->pinned = pinned;

  bq = (pos->piece_occ[BISHOP] | pos->piece_occ[QUEEN]) & occ_o;
  rq = (pos->piece_occ[ROOK] | pos->piece_occ[QUEEN]) & occ_o;
  in_check = (b_att & bq) || (r_att & rq) ? 1 : 0;

  if (!in_check)
  {
    if (_b_piece_area[_f_piece(pos, PAWN)][k_sq] & pos->piece_occ[PAWN] & occ_o)
      in_check = 1;
    else if (_b_piece_area[KNIGHT][k_sq] & pos->piece_occ[KNIGHT] & occ_o)
      in_check = 1;
    else if (_b_piece_area[KING][k_sq] & _b(pos->k_sq[pos->side ^ 1]))
      in_check = 1;
  }
  pos->in_check = in_check;
}

int attacked(position_t *pos, int sq)
{
  uint64_t acc, full_occ, occ_o;

  occ_o = pos->occ[pos->side ^ 1];

  if (_b_piece_area[_f_piece(pos, PAWN)][sq] & pos->piece_occ[PAWN] & occ_o)
    return 1;
  if (_b_piece_area[KNIGHT][sq] & pos->piece_occ[KNIGHT] & occ_o)
    return 1;
  if (_b_piece_area[KING][sq] & _b(pos->k_sq[pos->side ^ 1]))
    return 1;

  full_occ = pos->occ[pos->side] | occ_o;

  acc = (pos->piece_occ[BISHOP] | pos->piece_occ[QUEEN]) & occ_o;
  if (bishop_attack(full_occ, sq) & acc)
    return 1;

  acc = (pos->piece_occ[ROOK] | pos->piece_occ[QUEEN]) & occ_o;
  if (rook_attack(full_occ, sq) & acc)
    return 1;

  return 0;
}

int attacked_after_move(position_t *pos, int sq, move_t move)
{
  int m_from, m_to, w_piece;
  uint64_t occ, occ_f, occ_o, occ_x, fb, tb;

  m_from = _m_from(move);
  m_to = _m_to(move);
  w_piece = _to_white(pos->board[m_from]);

  fb = _b(m_from);
  tb = _b(m_to);

  occ_o = pos->occ[pos->side ^ 1];
  if (pos->board[m_to] != EMPTY)
    occ_o ^= tb;
  else if (w_piece == PAWN && pos->ep_sq == m_to)
    occ_o ^= _b(m_to ^ 8);

  if (_b_piece_area[_f_piece(pos, PAWN)][sq] & pos->piece_occ[PAWN] & occ_o)
    return 1;
  if (_b_piece_area[KNIGHT][sq] & pos->piece_occ[KNIGHT] & occ_o)
    return 1;
  if (_b_piece_area[KING][sq] & _b(pos->k_sq[pos->side ^ 1]))
    return 1;

  occ_f = (pos->occ[pos->side] ^ fb) | tb;
  occ = occ_f | occ_o;

  occ_x = (pos->piece_occ[BISHOP] | pos->piece_occ[QUEEN]) & occ_o;
  if (bishop_attack(occ, sq) & occ_x)
    return 1;

  occ_x = (pos->piece_occ[ROOK] | pos->piece_occ[QUEEN]) & occ_o;
  if (rook_attack(occ, sq) & occ_x)
    return 1;

  return 0;
}

// TODO this is ugly; try to refactor
// used for hash move validation
int is_pseudo_legal(position_t *pos, move_t move)
{
  int piece, t_piece, m_from, m_to, m_diff;

  m_from = _m_from(move);
  piece = pos->board[m_from];
  if (piece == EMPTY || _side(piece) != pos->side)
    return 0;

  m_to = _m_to(move);
  t_piece = pos->board[m_to];
  if (t_piece != EMPTY && _side(piece) == _side(t_piece))
    return 0;

  piece = _to_white(piece);
  m_diff = m_to - m_from;

  if (piece == PAWN)
  {
    if ((m_diff < 0 && pos->side == BLACK) || (m_diff > 0 && pos->side == WHITE))
      return 0;

    if (_rank(m_from) == RANK_1 || _rank(m_from) == RANK_8)
      return 0;

    if (m_diff == -9 || m_diff == 9 || m_diff == -7 || m_diff == 7)
    {
      if (t_piece != EMPTY)
      {
        if (_file(m_from) == FILE_A)
        {
          if (pos->side == WHITE && m_diff == -7)
            return 1;
          if (pos->side == BLACK && m_diff == 9)
            return 1;
        }
        else if (_file(m_from) == FILE_H)
        {
          if (pos->side == WHITE && m_diff == -9)
            return 1;
          if (pos->side == BLACK && m_diff == 7)
            return 1;
        }
        else
          return 1;
      }
      else if (m_to == pos->ep_sq)
        return 1;
    }
    else if (t_piece == EMPTY)
    {
      if (m_diff == -8 || m_diff == 8)
        return 1;
      else if (m_diff == -16)
        return _rank(m_from) == RANK_2 && pos->board[m_from - 8] == EMPTY;
      else if (m_diff == 16)
        return _rank(m_from) == RANK_7 && pos->board[m_from + 8] == EMPTY;
    }

    return 0;
  }

  if (piece != KING)
  {
    if (!(_b_piece_area[piece][m_from] & _b(m_to)))
      return 0;

    if (piece == KNIGHT)
      return 1;

    return (_b_line[m_from][m_to] & (pos->occ[WHITE] | pos->occ[BLACK])) == 0;
  }

  // castling
  if (m_diff == 2 || m_diff == -2)
  {
    if (!pos->c_flag || pos->board[(m_from + m_to) >> 1] != EMPTY)
      return 0;

    if (m_diff > 0)
      return pos->c_flag & ((pos->side == WHITE) ? C_FLAG_WR : C_FLAG_BR);
    else
      return pos->board[m_to - 1] == EMPTY &&
            (pos->c_flag & ((pos->side == WHITE) ? C_FLAG_WL : C_FLAG_BL));
  }

  return !!(_b_piece_area[KING][m_from] & _b(m_to));
}

int legal_move(position_t *pos, move_t move)
{
  int m_from, m_to, w_piece, m_diff, k_sq;

  k_sq = pos->k_sq[pos->side];
  if (k_sq == NO_SQ) return 0;

  m_from = _m_from(move);
  m_to = _m_to(move);

  w_piece = _to_white(pos->board[m_from]);
  if (pos->pinned == 0 && w_piece != KING && !pos->in_check &&
     (pos->ep_sq != m_to || w_piece != PAWN))
    return 1;

  if (w_piece == KING)
  {
    m_diff = m_to - m_from;
    if ((m_diff == 2 || m_diff == -2) &&
        (pos->in_check || attacked(pos, (_m_from(move) + _m_to(move)) >> 1)))
      return 0;
    return !attacked_after_move(pos, m_to, move);
  }

  if (pos->in_check || (pos->ep_sq == m_to && w_piece == PAWN))
    return !attacked_after_move(pos, k_sq, move);

  if ((pos->pinned & _b(m_from)) == 0)
    return 1;

  return (_b_line[k_sq][m_to] & _b(m_from)) || (_b_line[k_sq][m_from] & _b(m_to));
}

int SEE(position_t *pos, move_t move)
{
  int cnt, sq, p, pv, side, m_from, captured, captured_value, is_promotion, pqv,
      gain[MAX_CAPTURES];
  uint64_t occ, pb, bq, rq, att, side_att;

  sq = _m_to(move);
  m_from = _m_from(move);
  p = pos->board[m_from];
  captured = pos->board[sq];

  side = _side(p);
  p = _to_white(p);
  pv = piece_value[p];
  captured_value = 0;

  if (captured != EMPTY)
  {
    captured_value = piece_value[_to_white(captured)];
    if (pv <= captured_value)
      return 0;
  }

  is_promotion = _m_promoted_to(move);
  pqv = piece_value[QUEEN] - piece_value[PAWN];
  occ = (pos->occ[WHITE] | pos->occ[BLACK]) ^ (1ULL << m_from);

  gain[0] = captured_value;
  if (is_promotion && p == PAWN)
  {
    pv += pqv;
    gain[0] += pqv;
  }
  else if (sq == pos->ep_sq && p == PAWN)
  {
    occ ^= (1ULL << (sq ^ 8));
    gain[0] = piece_value[PAWN];
  }

  bq = pos->piece_occ[BISHOP] | pos->piece_occ[QUEEN];
  rq = pos->piece_occ[ROOK] | pos->piece_occ[QUEEN];

  // attackers
  att = _b_piece_area[PAWN | CHANGE_SIDE][sq] & pos->piece_occ[PAWN] & pos->occ[WHITE];
  att |= _b_piece_area[PAWN][sq] & pos->piece_occ[PAWN] & pos->occ[BLACK];
  att |= king_attack(occ, sq) & (_b(pos->k_sq[WHITE]) | _b(pos->k_sq[BLACK]));
  att |= knight_attack(occ, sq) & pos->piece_occ[KNIGHT];
  att |= bishop_attack(occ, sq) & bq;
  att |= rook_attack(occ, sq) & rq;
  att &= occ;

  cnt = 1;
  while(att)
  {
    side ^= 1;
    side_att = att & pos->occ[side];
    if (side_att == 0) break;

    for (p = PAWN; p <= QUEEN; p ++)
      if ((pb = side_att & pos->piece_occ[p]))
        break;
    if (!pb) pb = side_att;

    pb = pb & -pb;
    occ ^= pb;
    if (p == PAWN || p == BISHOP || p == QUEEN)
      att |= bishop_attack(occ, sq) & bq;
    if (p == ROOK || p == QUEEN)
      att |= rook_attack(occ, sq) & rq;
    att &= occ;

    gain[cnt] = pv - gain[cnt - 1];
    pv = piece_value[p];
    if (is_promotion && p == PAWN)
      { pv += pqv; gain[cnt] += pqv; }

    cnt ++;
  }

  while (--cnt)
    if (gain[cnt - 1] > -gain[cnt])
      gain[cnt - 1] = -gain[cnt];

  return gain[0];
}

int insufficient_material(position_t *pos)
{
  uint64_t occ;

  occ = pos->occ[WHITE] | pos->occ[BLACK];
  return _popcnt(occ) == 3 &&
         (occ & (pos->piece_occ[KNIGHT] | pos->piece_occ[BISHOP]));
}

int non_pawn_material(position_t *pos)
{
  uint64_t occ_f;

  occ_f = pos->occ[pos->side];
  if ((pos->piece_occ[QUEEN] | pos->piece_occ[ROOK]) & occ_f)
    return 2;
  if ((pos->piece_occ[BISHOP] | pos->piece_occ[KNIGHT]) & occ_f)
    return 1;
  return 0;
}

void reevaluate_position(position_t *pos)
{
  int i, piece, score_mid[N_SIDES], score_end[N_SIDES];

  score_mid[WHITE] = score_mid[BLACK] = 0;
  score_end[WHITE] = score_end[BLACK] = 0;
  for (i = 0; i < BOARD_SIZE; i ++)
  {
    piece = pos->board[i];
    if(piece != EMPTY)
    {
      score_mid[_side(piece)] += pst_mid[piece][i];
      score_end[_side(piece)] += pst_end[piece][i];
    }
  }
  pos->score_mid = score_mid[WHITE] - score_mid[BLACK];
  pos->score_end = score_end[WHITE] - score_end[BLACK];
}

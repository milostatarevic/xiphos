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

#include "eval.h"
#include "hash.h"
#include "move.h"
#include "tables.h"
#include "search.h"

int rook_c_flag_mask[BOARD_SIZE];
const int king_c_flag_mask[N_SIDES] = {
  ~(C_FLAG_WL | C_FLAG_WR),
  ~(C_FLAG_BL | C_FLAG_BR),
};

#define _update_position_hash_key(pos, hash_key, piece, sq)                    \
  {                                                                            \
    hash_key ^= shared_z_keys.positions[sq][piece];                            \
    if (_equal_to(piece, PAWN) || _equal_to(piece, KING))                      \
      (pos)->phash_key ^= shared_z_keys.positions[sq][piece];                  \
  }

#define _reset_eq_sq(pos, hash_key)                                            \
  {                                                                            \
    if ((pos)->ep_sq != NO_SQ)                                                 \
      hash_key ^= shared_z_keys.positions[(pos)->ep_sq][Z_KEYS_EP_FLAG];       \
    (pos)->ep_sq = NO_SQ;                                                      \
  }

// this method is rarely called, so we don't care much about the speed
static inline void set_piece(position_t *pos, uint64_t *hash_key, int piece, int sq)
{
  uint64_t b;
  int old_piece;

  old_piece = pos->board[sq];
  _update_position_hash_key(pos, *hash_key, old_piece, sq);
  pos->board[sq] = piece;
  _update_position_hash_key(pos, *hash_key, piece, sq);

  b = _b(sq);
  if (piece == EMPTY)
  {
    if (old_piece != EMPTY)
    {
      b = ~b;
      pos->occ[_side(old_piece)] &= b;
      pos->piece_occ[_to_white(old_piece)] &= b;
    }
  }
  else
  {
    pos->occ[pos->side] |= b;
    pos->occ[pos->side ^ 1] &= ~b;

    if (_equal_to(piece, KING))
      pos->k_sq[_side(piece)] = sq;
    else
    {
      pos->piece_occ[_to_white(piece)] |= b;
      if (old_piece != EMPTY)
        pos->piece_occ[_to_white(old_piece)] &= ~b;
    }
  }
}

void make_null_move(search_data_t *sd)
{
  position_cpy(sd->pos + 1, sd->pos);
  sd->pos ++;

  _reset_eq_sq(sd->pos, sd->hash_key);
  sd->pos->move = 0;
  sd->pos->side ^= 1;

  sd->hash_key ^= shared_z_keys.side_flag;
  sd->hash_keys[++ sd->hash_keys_cnt] = sd->hash_key;

  set_pins_and_checks(sd->pos);
}

void make_move(search_data_t *sd, move_t move)
{
  int m_from, m_to, m_diff, score_mid, score_end, piece, w_piece,
      target_piece, w_target_piece, corner_sq, rook_sq, pawn_sq, promoted_to;
  uint64_t hash_key, b_mask, bt_mask;
  position_t *pos;

  // we are only counting moves we made
  sd->nodes ++;

  // migrate from the previous positions
  sd->pos ++;
  pos = sd->pos;
  position_cpy(sd->pos, sd->pos - 1);
  hash_key = sd->hash_key;

  // ...make move
  pos->move = move;
  m_from = _m_from(move);
  m_to = _m_to(move);
  m_diff = m_to - m_from;
  piece = pos->board[m_from];
  target_piece = pos->board[m_to];
  w_piece = _to_white(piece);
  w_target_piece = _to_white(target_piece);

  score_mid = pst_mid[piece][m_to] - pst_mid[piece][m_from];
  score_end = pst_end[piece][m_to] - pst_end[piece][m_from];

  pos->fifty_cnt ++;
  hash_key ^= shared_z_keys.side_flag;
  if (pos->c_flag)
    hash_key ^= shared_z_keys.c_flags[pos->c_flag];
  _update_position_hash_key(pos, hash_key, piece, m_from);
  _update_position_hash_key(pos, hash_key, piece, m_to);
  _reset_eq_sq(pos, hash_key);

  // set pieces
  pos->board[m_from] = EMPTY;
  pos->board[m_to] = piece;
  pos->c_flag &= rook_c_flag_mask[m_from];

  b_mask = _b(m_from) | _b(m_to);
  pos->occ[pos->side] ^= b_mask;

  // captures
  if (target_piece != EMPTY)
  {
    pos->fifty_cnt = 0;

    bt_mask = _b(m_to);
    pos->occ[pos->side ^ 1] ^= bt_mask;
    if (w_target_piece == KING)
      pos->k_sq[pos->side] = NO_SQ;
    else
      pos->piece_occ[w_target_piece] ^= bt_mask;

    score_mid += pst_mid[target_piece][m_to];
    score_end += pst_end[target_piece][m_to];
    pos->phase += piece_phase[w_target_piece];

    _update_position_hash_key(pos, hash_key, target_piece, m_to);
    pos->c_flag &= rook_c_flag_mask[m_to];
  }

  // king moves
  if (w_piece == KING)
  {
    pos->k_sq[pos->side] = m_to;
    pos->c_flag &= king_c_flag_mask[pos->side];

    // TODO simplify
    if (m_diff == 2 || m_diff == -2)
    {
      rook_sq = (m_from + m_to) >> 1;
      if (pos->side == WHITE)
        corner_sq = (m_to < m_from) ? A1 : H1;
      else
        corner_sq = (m_to < m_from) ? A8 : H8;

      set_piece(pos, &hash_key, EMPTY, corner_sq);
      set_piece(pos, &hash_key, _f_piece(pos, ROOK), rook_sq);
      score_mid += CASTLING_BONUS;
    }
  }
  else
  {
    pos->piece_occ[w_piece] ^= b_mask;
  }

  // update castling flag hash
  if (pos->c_flag)
    hash_key ^= shared_z_keys.c_flags[pos->c_flag];

  // pawn moves
  if (w_piece == PAWN)
  {
    pos->fifty_cnt = 0;

    if (_m_promoted_to(move))
    {
      promoted_to = _m_promoted_to(move);
      set_piece(pos, &hash_key, _f_piece(pos, promoted_to), m_to);
      score_mid += pst_mid[_f_piece(pos, promoted_to)][m_to];
      score_mid -= pst_mid[_f_piece(pos, PAWN)][m_to];
      score_end += pst_end[_f_piece(pos, promoted_to)][m_to];
      score_end -= pst_end[_f_piece(pos, PAWN)][m_to];
    }
    else
    {
      pawn_sq = m_to ^ 8;
      if (m_diff == 16 || m_diff == -16)
      {
        pos->ep_sq = pawn_sq;
        hash_key ^= shared_z_keys.positions[pos->ep_sq][Z_KEYS_EP_FLAG];
      }
      else if (target_piece == EMPTY && m_diff != 8 && m_diff != -8)
      {
        score_mid += pst_mid[_o_piece(pos, PAWN)][pawn_sq];
        score_end += pst_end[_o_piece(pos, PAWN)][pawn_sq];
        set_piece(pos, &hash_key, EMPTY, pawn_sq);
      }
    }
  }

  if (pos->side == WHITE)
  {
    pos->score_mid += score_mid;
    pos->score_end += score_end;
  }
  else
  {
    pos->score_mid -= score_mid;
    pos->score_end -= score_end;
  }

  // save hash_keys in the separete list
  sd->hash_key = hash_key;
  sd->hash_keys[++ sd->hash_keys_cnt] = hash_key;

  pos->side ^= 1;
  set_pins_and_checks(pos);
}

// make move and rewind the position
void make_move_rev(search_data_t *sd, move_t move)
{
  make_move(sd, move);
  sd->pos --;
  position_cpy(sd->pos, sd->pos + 1);
}

void init_rook_c_flag_mask()
{
  int i, c_flag_init;

  c_flag_init =
      C_FLAG_WL | C_FLAG_WR |
      C_FLAG_BL | C_FLAG_BR;

  for (i = 0; i < BOARD_SIZE; i ++)
    rook_c_flag_mask[i] = c_flag_init;

  rook_c_flag_mask[A1] ^= C_FLAG_WL;
  rook_c_flag_mask[H1] ^= C_FLAG_WR;
  rook_c_flag_mask[A8] ^= C_FLAG_BL;
  rook_c_flag_mask[H8] ^= C_FLAG_BR;
}

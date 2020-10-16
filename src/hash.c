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

#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "move.h"

typedef struct {
  uint64_t mask;
  hash_data_t data;
} hash_item_t;

struct {
  hash_item_t *items;
  uint64_t size, mask;
  uint32_t iter;
} hash_store;

shared_z_keys_t shared_z_keys;

uint64_t rand64()
{
  int i;
  uint64_t r;

  r = 0;
  for (i = 0; i < 8; i ++)
    r |= (uint64_t)(rand() & 0xff) << (i << 3);
  return r;
}

void init_z_keys()
{
  int i, j;

  for (i = 0; i < BOARD_SIZE; i ++)
    for (j = 0; j < Z_KEYS_MAX_INDEX; j ++)
      shared_z_keys.positions[i][j] = rand64();

  for (i = 0; i < C_FLAG_MAX; i ++)
    shared_z_keys.c_flags[i] = rand64();

  shared_z_keys.side_flag = rand64();
}

int adjust_hash_score(int score, int ply)
{
  if (score >= MATE_SCORE - MAX_PLY)
    score -= ply;
  else if (score <= -MATE_SCORE + MAX_PLY)
    score += ply;
  return score;
}

int to_hash_score(int score, int ply)
{
  if (score >= MATE_SCORE - MAX_PLY)
    score += ply;
  else if (score <= -MATE_SCORE + MAX_PLY)
    score -= ply;
  return score;
}

hash_data_t get_hash_data(search_data_t *sd)
{
  hash_item_t *hash_item;
  hash_data_t hash_data;
  move_t hash_move;

  hash_item = hash_store.items + (sd->hash_key & hash_store.mask);
  hash_data = hash_item->data;

  // invalid entry
  if ((sd->hash_key ^ hash_item->mask) != hash_data.raw)
    hash_data.raw = 0;

  // corrupted move
  hash_move = hash_data.move;
  if (_is_m(hash_move) && !is_pseudo_legal(sd->pos, hash_move))
    hash_data.raw = 0;

  return hash_data;
}

void set_hash_data(search_data_t *sd, move_t move, int score, int static_score,
                   int depth, int ply, int bound)
{
  hash_item_t *hash_item;
  hash_data_t hash_data;

  hash_item = hash_store.items + (sd->hash_key & hash_store.mask);
  hash_data = hash_item->data;

  if ((sd->hash_key ^ hash_item->mask) == hash_data.raw ||
      hash_data.depth <= depth || hash_data.iter != hash_store.iter)
  {
    hash_data.move = _m_with_score(move, to_hash_score(score, ply));
    hash_data.static_score = static_score;
    hash_data.depth = depth;
    hash_data.bound = bound;
    hash_data.iter = hash_store.iter;

    hash_item->data = hash_data;
    hash_item->mask = sd->hash_key ^ hash_data.raw;
  }
}

void set_hash_iteration()
{
  hash_store.iter ++;
}

void reset_hash_key(search_data_t *sd)
{
  memset(hash_store.items, 0, hash_store.size);
  hash_store.iter = 0;

  srand(time(0));
  init_z_keys();

  sd->hash_key = rand64();
  sd->hash_keys_cnt = 0;
  sd->hash_keys[sd->hash_keys_cnt] = sd->hash_key;
}

uint64_t init_hash(int size_in_mb)
{
  uint64_t size, rounded_size;

  size = ((uint64_t)size_in_mb << 20) / sizeof(hash_item_t);
  rounded_size = 1;
  while (size >>= 1)
    rounded_size <<= 1;

  hash_store.mask = rounded_size - 1;
  hash_store.size = rounded_size * sizeof(hash_item_t);
  hash_store.items = (hash_item_t *)realloc(hash_store.items, hash_store.size);

  return hash_store.size;
}

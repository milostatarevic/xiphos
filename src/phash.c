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

#include <stdlib.h>
#include "phash.h"

#define PHASH_BASE_SIZE    (1 << 16)

typedef struct {
  uint64_t mask;
  phash_data_t data;
} phash_item_t;

struct {
  phash_item_t *items;
  uint64_t mask;
} phash_store;

static inline uint64_t xor_data(phash_data_t *phash_data)
{
  return phash_data->raw[0] ^ phash_data->raw[1];
}

int get_phash_data(position_t *pos, phash_data_t *phash_data)
{
  phash_item_t *phash_item;

  phash_item = phash_store.items + (pos->phash_key & phash_store.mask);
  *phash_data = phash_item->data;
  return (pos->phash_key ^ phash_item->mask) == xor_data(phash_data);
}

phash_data_t set_phash_data(position_t *pos, uint64_t pushed_passers,
                            int score_mid, int score_end)
{
  phash_item_t *phash_item;
  phash_data_t phash_data;

  phash_data.pushed_passers = pushed_passers;
  phash_data.score_mid = score_mid;
  phash_data.score_end = score_end;

  phash_item = phash_store.items + (pos->phash_key & phash_store.mask);
  phash_item->data = phash_data;
  phash_item->mask = pos->phash_key ^ xor_data(&phash_data);

  return phash_data;
}

void init_phash(int max_threads)
{
  uint64_t size, rounded_size;

  size = (2 * max_threads - 1) * PHASH_BASE_SIZE;
  rounded_size = 1;
  while (size >>= 1)
    rounded_size <<= 1;

  phash_store.mask = rounded_size - 1;
  phash_store.items =
    (phash_item_t *)realloc(phash_store.items, rounded_size * sizeof(phash_item_t));
}

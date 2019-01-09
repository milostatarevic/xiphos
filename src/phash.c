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

#define PHASH_SIZE_BITS     18
#define PHASH_SIZE          (1 << PHASH_SIZE_BITS)
#define PHASH_MASK          (PHASH_SIZE - 1)

typedef struct {
  uint64_t mask;
  phash_data_t data;
} phash_item_t;

phash_item_t *shared_phash;

static inline uint64_t xor_data(phash_data_t *phash_data)
{
  return phash_data->raw[0] ^ phash_data->raw[1];
}

int get_phash_data(position_t *pos, phash_data_t *phash_data)
{
  phash_item_t *phash_item;

  phash_item = shared_phash + (pos->phash_key & PHASH_MASK);
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

  phash_item = shared_phash + (pos->phash_key & PHASH_MASK);
  phash_item->data = phash_data;
  phash_item->mask = pos->phash_key ^ xor_data(&phash_data);

  return phash_data;
}

void init_phash()
{
  shared_phash = (phash_item_t *) malloc(PHASH_SIZE * sizeof(phash_item_t));
}

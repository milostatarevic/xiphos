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

#ifndef PHASH_H
#define PHASH_H

#include "game.h"
#include "position.h"

typedef union {
  struct {
    int16_t score_mid;
    int16_t score_end;
    uint64_t pushed_passers;
  };
  uint64_t raw[2];
} phash_data_t;

int get_phash_data(position_t *, phash_data_t *);
phash_data_t set_phash_data(position_t *, uint64_t, int, int);
void init_phash();

#endif

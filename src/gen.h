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

#ifndef GEN_H
#define GEN_H

#include "move.h"
#include "position.h"

void quiet_moves(position_t *, move_t *, int *);
void checks_and_material_moves(position_t *, move_t *, int *, int);
void material_moves(position_t *, move_t *, int *, int);
void check_evasion_moves(position_t *, move_t *, int *, int);
void king_moves(position_t *, move_t *, int *);
int count_non_king_moves(position_t *);

#endif

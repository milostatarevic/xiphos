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

#ifndef MOVE_H
#define MOVE_H

#include "game.h"

// [score: 16 bits][flags: 4 bits][from: 6 bits][to: 6 bits]
// flags: [promo piece: 3 bits][is_quiet: 1 bit]
typedef uint32_t move_t;

#define _m_to(m)               ((m) & 0x3f)
#define _m_from(m)             (((m) >> 6) & 0x3f)
#define _m_score(m)            ((int16_t)((m) >> 16))
#define _m_is_quiet(m)         ((m) & 0x1000)
#define _m_promoted_to(m)      (((m) >> 13) & 7)
#define _m(from, to)           (((from) << 6) | (to))
#define _m_base(m)             ((m) & 0xefff)
#define _is_m(m)               (_m_base(m))
#define _m_eq(m0, m1)          (_m_base(m0) == _m_base(m1))
#define _m_set_quiet(m)        ((m) | 0x1000)
#define _m_with_score(m, s)    (((m) & 0xffff) | (((uint32_t)(s)) << 16))
#define _m_with_promo(m, p)    (((m) & 0xffff1fff) | (_to_white(p) << 13))
#define _m_less_score(m0, m1)  ((int32_t)(m0) < (int32_t)(m1))

#endif

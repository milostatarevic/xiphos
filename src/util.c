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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "position.h"

uint64_t time_in_ms()
{
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

void sleep_ms(int sleep_in_ms)
{
  struct timespec t;
  t.tv_sec = sleep_in_ms / 1000;
  t.tv_nsec = (sleep_in_ms % 1000) * 1000000;
  nanosleep(&t, NULL);
}

void print(const char* format, ...)
{
  char dest[256];

  va_list args;
  va_start(args, format);
  vsprintf(dest, format, args);
  va_end(args);

  fputs(dest, stdout);
  fflush(stdout);

#ifdef _SAVE_LOG
  FILE *f_out;

  f_out = fopen("output.log", "a");
  fputs(dest, f_out);
  fclose(f_out);
#endif
}

void write_input_log(char *buf)
{
  FILE *f;

  f = fopen("input.log", "a");
  fputs(buf, f);
  fclose(f);
}

char *m_to_str(move_t move)
{
  int m_from, m_to, promoted_to;
  static char buffer[8];
  const char promotion_codes[] = { 0, 'n', 'b', 'r', 'q'};

  if (!_is_m(move))
  {
    sprintf(buffer, "(none)");
  }
  else
  {
    m_from = _m_from(move);
    m_to = _m_to(move);
    promoted_to = _m_promoted_to(move);
    sprintf(buffer, "%c%c%c%c%c", _file_chr(m_from), _rank_chr(m_from),
                                  _file_chr(m_to), _rank_chr(m_to),
                                  promotion_codes[promoted_to]);
  }
  return buffer;
}

move_t str_to_m(char *line)
{
  move_t move;
  int piece;

  move = _m(_chr_to_sq(line[0], line[1]), _chr_to_sq(line[2], line[3]));
  piece = 0;
  if (strlen(line) > 5)
    switch (line[4])
    {
      case 'q': piece = QUEEN; break;
      case 'r': piece = ROOK; break;
      case 'b': piece = BISHOP; break;
      case 'n': piece = KNIGHT; break;
    }
  return _m_with_promo(move, piece);
}

void print_board(position_t *pos)
{
  const char display_pieces[N_PIECES] = { 'P', 'N', 'B', 'R', 'Q', 'K' };
  int i, piece, display_piece;

  print("\n");
  for (i = 0; i < BOARD_SIZE; i ++)
  {
    piece = pos->board[i];
    if(piece == EMPTY)
      display_piece = '.';
    else
    {
      display_piece = display_pieces[_to_white(piece)];
      if ((piece >> SIDE_SHIFT) == BLACK)
        display_piece += 32;
    }

    print(" %c", display_piece);
    if((i & 7) == 7) print("\n");
  }
  print("\n a b c d e f g h \n\n");
}

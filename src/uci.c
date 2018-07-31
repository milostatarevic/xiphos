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
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "hash.h"
#include "make.h"
#include "perft.h"
#include "position.h"
#include "search.h"

#define CMD_UCI                     "uci\n"
#define CMD_READY                   "isready"
#define CMD_QUIT                    "quit"
#define CMD_GO                      "go"
#define CMD_NEW_GAME                "ucinewgame"
#define CMD_POSITION_FEN            "position fen"
#define CMD_POSITION_STARTPOS       "position startpos"

#define CMD_PERFT                   "perft"
#define CMD_TEST                    "test"
#define CMD_PRINT                   "print"

#define OPTION_HASH                 "setoption name Hash value"
#define OPTION_THREADS              "setoption name Threads value"

#define MAX_REDUCE_TIME             1000
#define REDUCE_TIME                 80
#define REDUCE_TIME_PERCENT         5
#define MAX_TIME_MOVES_TO_GO        3
#define MIN_TIME_RATIO              0.5
#define MAX_TIME_RATIO              1.2

#define MAX_MOVES_TO_GO             25
#define READ_BUFFER_SIZE            65536

search_data_t *threads_search_data;
char initial_fen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";

void set_piece(position_t *pos, int piece, int sq)
{
  int w_piece, piece_side;

  w_piece = _to_white(piece);
  piece_side = piece >> SIDE_SHIFT;

  pos->board[sq] = piece;
  pos->occ[piece_side] |= _b(sq);
  if (w_piece == KING)
    pos->k_sq[piece_side] = sq;
  else
    pos->piece_occ[w_piece] |= _b(sq);
}

void read_fen(search_data_t *sd, char *buf, int full_reset)
{
  position_t *pos;
  char c, *moves_buf;
  int i, sq, side;

  if (full_reset)
    full_reset_search_data(sd, threads_search_data);
  else
    reset_search_data(sd);

  pos = sd->pos;

  for(i = 0; i < BOARD_SIZE; i ++)
    sd->pos->board[i] = EMPTY;

  for (i = sq = 0; i < strlen(buf); i ++)
  {
    c = buf[i];
    if (c == ' ') break;
    if (c == '/') continue;
    if (c >= '0' && c <= '9')
      { sq += c - '0'; continue; }

    side = c > 'a' ? CHANGE_SIDE : 0;
    c |= 0x60;
    switch (c)
    {
      case 'k':
        set_piece(pos, KING   | side, sq); break;
      case 'q':
        set_piece(pos, QUEEN  | side, sq); break;
      case 'r':
        set_piece(pos, ROOK   | side, sq); break;
      case 'b':
        set_piece(pos, BISHOP | side, sq); break;
      case 'n':
        set_piece(pos, KNIGHT | side, sq); break;
      case 'p':
        set_piece(pos, PAWN   | side, sq);
    }
    sq ++;
  }

  i ++;
  pos->side = buf[i] == 'w' ? WHITE : BLACK;
  i += 2;

  pos->c_flag = 0;
  for (; i < strlen(buf); i ++)
  {
    c = buf[i];
    if (c == ' ') break;
    if (c == '-') { i ++; break; }

    switch (c)
    {
      case 'K':
        pos->c_flag |= C_FLAG_WR; break;
      case 'Q':
        pos->c_flag |= C_FLAG_WL; break;
      case 'k':
        pos->c_flag |= C_FLAG_BR; break;
      case 'q':
        pos->c_flag |= C_FLAG_BL; break;
    }
  }

  pos->ep_sq = NO_SQ;
  i ++;
  if (buf[i] != '-')
    pos->ep_sq = _chr_to_sq(buf[i], buf[i + 1]);
  reevaluate_position(pos);
  set_pins_and_checks(pos);

  moves_buf = strstr(buf, "moves");
  if (moves_buf)
    while ((moves_buf = strchr(moves_buf, ' ')))
    {
      while (*moves_buf == ' ') moves_buf++;
      make_move_rev(sd, str_to_m(moves_buf));
    }
}

void uci_position_startpos(search_data_t *sd, char *buf)
{
  read_fen(sd, initial_fen, 0);
  buf = strstr(buf, "moves");
  if(buf == NULL) return;

  while ((buf = strchr(buf, ' ')))
  {
    while (*buf == ' ') buf++;
    make_move_rev(sd, str_to_m(buf));
  }
}

void uci_go(search_data_t *sd, char *buf)
{
  int i, max_threads, max_time_allowed, target_time, max_time, reduce_time, moves_to_go;
  double ratio;
  char *t;
  position_t *pos;

  pos = sd->pos;

  // TODO fix
  max_threads = shared_search_info.max_threads;
  memset(&shared_search_info, 0, sizeof(shared_search_info));
  shared_search_info.max_threads = max_threads;

  for (t = strtok(buf, " "); t; t = strtok(NULL, " "))
  {
    if ((!strcmp(t, "wtime") && pos->side == WHITE) ||
        (!strcmp(t, "btime") && pos->side == BLACK))
    {
      t = strtok(NULL, " ");
      shared_search_info.go.time = atoi(t);
    }
    if ((!strcmp(t, "winc") && pos->side == WHITE) ||
        (!strcmp(t, "binc") && pos->side == BLACK))
    {
      t = strtok(NULL, " ");
      shared_search_info.go.inc = atoi(t);
    }
    else if (!strcmp(t, "movestogo"))
    {
      t = strtok(NULL, " ");
      shared_search_info.go.movestogo = atoi(t);
    }
    else if (!strcmp(t, "depth"))
    {
      t = strtok(NULL, " ");
      shared_search_info.go.depth = atoi(t);
    }
    else if (!strcmp(t, "movetime"))
    {
      t = strtok(NULL, " ");
      shared_search_info.go.movetime = atoi(t);
    }
  }

  shared_search_info.max_depth = MAX_DEPTH - 1;

  if (shared_search_info.go.depth > 0)
  {
    shared_search_info.max_time = (1 << 30);
    shared_search_info.max_depth =
      _min(shared_search_info.go.depth, shared_search_info.max_depth);
  }
  else
  {
    if (shared_search_info.go.movetime > 0)
      shared_search_info.max_time =
        _max(shared_search_info.go.movetime - REDUCE_TIME, 1);
    else
    {
      moves_to_go = _min(shared_search_info.go.movestogo, MAX_MOVES_TO_GO);
      if (moves_to_go == 0)
        moves_to_go = MAX_MOVES_TO_GO;

      reduce_time = _min(
        shared_search_info.go.time * REDUCE_TIME_PERCENT / 100, MAX_REDUCE_TIME
      ) + REDUCE_TIME;
      max_time_allowed = _max(shared_search_info.go.time - reduce_time, 1);

      target_time = max_time_allowed / moves_to_go + shared_search_info.go.inc;
      for (i = 0; i < TM_STEPS; i ++)
      {
        ratio = MIN_TIME_RATIO +
                i * (MAX_TIME_RATIO - MIN_TIME_RATIO) / (TM_STEPS - 1);
        shared_search_info.target_time[i] =
          _min(ratio * target_time, max_time_allowed);
      }

      max_time =
        max_time_allowed / _min(moves_to_go, MAX_TIME_MOVES_TO_GO) +
        shared_search_info.go.inc;
      shared_search_info.max_time = _min(max_time, max_time_allowed);
    }
  }

  search(sd, threads_search_data);
}

void uci_perft(search_data_t *sd, char *buf)
{
  int depth;
  uint64_t nodes, time_ms;

  depth = strlen(buf) ? atoi(buf) : 0;
  if (depth < 1)
  {
    print("specify depth\n");
    return;
  }

  time_ms = time_in_ms();
  nodes = perft(sd, depth, 0, 0);

  time_ms = time_in_ms() - time_ms;
  print("perft(%d)=%"PRIu64", time: %"PRIu64"ms, "
        "nps: %"PRIu64" (no hashing, bulk counting)\n",
        depth, nodes, time_ms, nodes * 1000 / (time_ms + 1));
}

void uci_info()
{
  int i, elapsed_time, score;
  char score_string[64];
  uint64_t nodes;

  score = shared_search_info.score;
  if (_is_mate_score(score))
    sprintf(score_string, "mate %d",
            (MATE_SCORE - _abs(score)) / 2 * ((score >= 0) ? 1 : -1));
  else
    sprintf(score_string, "cp %d", score);

  nodes = 0;
  for (i = 0; i < shared_search_info.max_threads; i ++)
    nodes += threads_search_data[i].nodes;

  elapsed_time = time_in_ms() - shared_search_info.time_in_ms;

  print("info depth %d score %s nodes %"PRIu64" time %d nps %d ",
        shared_search_info.depth, score_string, nodes, elapsed_time,
        1000ULL * nodes / (elapsed_time + 1));

  print("pv %s\n", m_to_str(shared_search_info.best_move));
}

void set_hash_size(search_data_t *sd, int hash_size_in_mb)
{
  uint64_t allocated_memory;

  if (hash_size_in_mb < 1)
    return;

  allocated_memory = init_hash(hash_size_in_mb);
  reset_hash_key(sd);
  print("hash=%dMB\n", allocated_memory >> 20);
}

void set_max_threads(int thread_cnt)
{
  shared_search_info.max_threads = _max(_min(thread_cnt, MAX_THREADS), 1);
  threads_search_data =
    (search_data_t *) realloc(
      threads_search_data, shared_search_info.max_threads * sizeof(search_data_t)
    );
  reset_threads_search_data(threads_search_data);
  print("threads=%d\n", shared_search_info.max_threads);
}

int _cmd_cmp(char **buf, const char *cmd_str)
{
  if(!strncmp(*buf, cmd_str, strlen(cmd_str)))
  {
    *buf += strlen(cmd_str) + 1;
    return 1;
  }
  return 0;
}

void uci()
{
  search_data_t *sd;
  char *buf, input_buf[READ_BUFFER_SIZE];

  print("%s %s by %s\n", VERSION, ARCH, AUTHOR);

  sd = (search_data_t *) malloc(sizeof(search_data_t));

  threads_search_data = NULL;
  set_max_threads(DEFAULT_THREADS);
  set_hash_size(sd, DEFAULT_HASH_SIZE_IN_MB);

  read_fen(sd, initial_fen, 1);
  setbuf(stdout, NULL);

  while(1)
  {
    if (fgets(input_buf, READ_BUFFER_SIZE, stdin) == NULL)
    {
      sleep_ms(10);
      continue;
    }

#ifdef _SAVE_LOG
    write_input_log(input_buf);
#endif

    buf = input_buf;
    if (_cmd_cmp(&buf, CMD_UCI))
    {
      print("id name %s %s\n", VERSION, ARCH);
      print("id author %s\n", AUTHOR);
      print("option name Hash type spin default %d min 1 max 32768\n", DEFAULT_HASH_SIZE_IN_MB);
      print("option name Threads type spin default 1 min 1 max %d\n", MAX_THREADS);
      print("uciok\n");
    }

    else if (_cmd_cmp(&buf, CMD_READY))
      print("readyok\n");

    else if (_cmd_cmp(&buf, CMD_QUIT))
      break;

    else if (_cmd_cmp(&buf, CMD_NEW_GAME))
      read_fen(sd, initial_fen, 1);

    else if (_cmd_cmp(&buf, CMD_POSITION_FEN))
      read_fen(sd, buf, 0);

    else if (_cmd_cmp(&buf, CMD_POSITION_STARTPOS))
      uci_position_startpos(sd, buf);

    else if (_cmd_cmp(&buf, CMD_GO))
      uci_go(sd, buf);

    else if (_cmd_cmp(&buf, CMD_PERFT))
      uci_perft(sd, buf);

    else if (_cmd_cmp(&buf, CMD_TEST))
      run_tests(sd);

    else if (_cmd_cmp(&buf, CMD_PRINT))
      print_board(sd->pos);

    else if (_cmd_cmp(&buf, OPTION_HASH))
      set_hash_size(sd, atoi(buf));

    else if (_cmd_cmp(&buf, OPTION_THREADS))
      set_max_threads(atoi(buf));
  }
}

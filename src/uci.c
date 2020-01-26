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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "hash.h"
#include "make.h"
#include "perft.h"
#include "phash.h"
#include "position.h"
#include "search.h"
#include "fathom/tbprobe.h"

#define CMD_UCI                     "uci\n"
#define CMD_GO                      "go"
#define CMD_QUIT                    "quit"
#define CMD_STOP                    "stop"
#define CMD_READY                   "isready"
#define CMD_PONDERHIT               "ponderhit"
#define CMD_NEW_GAME                "ucinewgame"
#define CMD_POSITION_FEN            "position fen"
#define CMD_POSITION_STARTPOS       "position startpos"

#define CMD_PERFT                   "perft"
#define CMD_TEST                    "test"
#define CMD_PRINT                   "print"

#define OPTION_HASH                 "setoption name Hash value"
#define OPTION_THREADS              "setoption name Threads value"
#define OPTION_PONDER               "setoption name Ponder value"
#define OPTION_SYZYGY_PATH          "setoption name SyzygyPath value"
#define OPTION_SYZYGY_PROBE_DEPTH   "setoption name SyzygyProbeDepth value"

#define MAX_REDUCE_TIME             1000
#define REDUCE_TIME                 150
#define REDUCE_TIME_PERCENT         5
#define MAX_TIME_MOVES_TO_GO        3
#define MIN_TIME_RATIO              0.5
#define MAX_TIME_RATIO              1.2

#define MAX_MOVES_TO_GO             25
#define BUFFER_LINE_SIZE            256
#define READ_BUFFER_SIZE            65536

char initial_fen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";

int starts_with(char *buf, const char *str)
{
  return !strncmp(buf, str, strlen(str));
}

int _cmd_cmp(char **buf, const char *cmd_str)
{
  if(starts_with(*buf, cmd_str))
  {
    *buf += strlen(cmd_str) + 1;
    return 1;
  }
  return 0;
}

void set_piece(position_t *pos, int piece, int sq)
{
  int w_piece, piece_side;

  w_piece = _to_white(piece);
  piece_side = _side(piece);

  pos->board[sq] = piece;
  pos->occ[piece_side] |= _b(sq);
  if (w_piece == KING)
    pos->k_sq[piece_side] = sq;
  else
    pos->piece_occ[w_piece] |= _b(sq);
}

void read_fen(search_data_t *sd, char *buf)
{
  position_t *pos;
  char c, *moves_buf;
  int i, sq, side;

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

  set_phase(pos);
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

void uci_position_startpos(char *buf)
{
  read_fen(search_settings.sd, initial_fen);
  buf = strstr(buf, "moves");
  if(buf == NULL) return;

  while ((buf = strchr(buf, ' ')))
  {
    while (*buf == ' ') buf++;
    make_move_rev(search_settings.sd, str_to_m(buf));
  }
}

void parse_go_cmd(char *buf)
{
  int i, max_time_allowed, target_time, max_time, reduce_time, moves_to_go;
  double ratio;
  char *t;
  position_t *pos;

  pos = search_settings.sd->pos;
  memset(&search_status, 0, sizeof(search_status));

  for (t = strtok(buf, " "); t; t = strtok(NULL, " "))
  {
    if ((!strcmp(t, "wtime") && pos->side == WHITE) ||
        (!strcmp(t, "btime") && pos->side == BLACK))
    {
      t = strtok(NULL, " ");
      search_status.go.time = atoi(t);
    }
    if ((!strcmp(t, "winc") && pos->side == WHITE) ||
        (!strcmp(t, "binc") && pos->side == BLACK))
    {
      t = strtok(NULL, " ");
      search_status.go.inc = atoi(t);
    }
    else if (!strcmp(t, "movestogo"))
    {
      t = strtok(NULL, " ");
      search_status.go.movestogo = atoi(t);
    }
    else if (!strcmp(t, "depth"))
    {
      t = strtok(NULL, " ");
      search_status.go.depth = atoi(t);
    }
    else if (starts_with(buf, "ponder"))
    {
      search_status.go.ponder = 1;
    }
    else if (starts_with(buf, "infinite"))
    {
      search_status.go.infinite = 1;
    }
    else if (!strcmp(t, "movetime"))
    {
      t = strtok(NULL, " ");
      search_status.go.movetime = atoi(t);
    }
  }

  search_status.max_time = (1 << 30);
  search_status.max_depth = MAX_DEPTH - 1;

  if (search_status.go.infinite)
    ;
  else if (search_status.go.depth > 0)
  {
    search_status.max_depth =
      _min(search_status.go.depth, search_status.max_depth);
  }
  else
  {
    if (search_status.go.movetime > 0)
      search_status.max_time =
        _max(search_status.go.movetime - REDUCE_TIME, 1);
    else
    {
      moves_to_go = _min(search_status.go.movestogo, MAX_MOVES_TO_GO);
      if (moves_to_go == 0)
        moves_to_go = MAX_MOVES_TO_GO;

      reduce_time = _min(
        search_status.go.time * REDUCE_TIME_PERCENT / 100, MAX_REDUCE_TIME
      ) + REDUCE_TIME;
      max_time_allowed = _max(search_status.go.time - reduce_time, 1);

      target_time = max_time_allowed / moves_to_go + search_status.go.inc;

      // move faster at the beginning of the game
      if (_popcnt(pos->occ[WHITE] & (_B_RANK_1 | _B_RANK_2)) >= 11 ||
          _popcnt(pos->occ[BLACK] & (_B_RANK_7 | _B_RANK_8)) >= 11)
        target_time /= 2;

      for (i = 0; i < TM_STEPS; i ++)
      {
        ratio = MIN_TIME_RATIO +
                i * (MAX_TIME_RATIO - MIN_TIME_RATIO) / (TM_STEPS - 1);

        if (search_settings.ponder_mode)
          ratio *= 1.25;

        search_status.target_time[i] =
          _min(ratio * target_time, max_time_allowed);
      }

      max_time =
        max_time_allowed / _min(moves_to_go, MAX_TIME_MOVES_TO_GO) +
        search_status.go.inc;
      search_status.max_time = _min(max_time, max_time_allowed);
    }
  }
}

void uci_perft(char *buf)
{
  int depth;
  uint64_t nodes, time_ms;

  depth = strlen(buf) ? atoi(buf) : 0;
  if (depth < 1)
  {
    _p("specify depth\n");
    return;
  }

  time_ms = time_in_ms();
  nodes = perft(search_settings.sd, depth, 0, 0);

  time_ms = time_in_ms() - time_ms;
  _p("perft(%d)=%"PRIu64", time: %"PRIu64"ms, nps: %"PRIu64" (no hashing, bulk counting)\n",
      depth, nodes, time_ms, nodes * 1000 / (time_ms + 1));
}

void uci_info(move_t *pv)
{
  int i, score;
  char buf[BUFFER_LINE_SIZE], pv_string[PLY_LIMIT * 8];
  uint64_t nodes, tbhits, elapsed_time;

  score = search_status.score;
  if (_is_mate_score(score))
    sprintf(buf, "mate %d", (score > 0 ? MATE_SCORE - score + 1 : -MATE_SCORE - score) / 2);
  else
    sprintf(buf, "cp %d", score);

  nodes = tbhits = 0;
  for (i = 0; i < search_settings.max_threads; i ++)
  {
    nodes += search_settings.threads_search_data[i].nodes;
    tbhits += search_settings.threads_search_data[i].tbhits;
  }

  elapsed_time = time_in_ms() - search_status.time_in_ms;

  _p("info depth %d score %s nodes %"PRIu64" tbhits %"PRIu64" time %"PRIu64" nps %"PRIu64" ",
      search_status.depth, buf, nodes, tbhits, elapsed_time,
      nodes * UINT64_C(1000) / (elapsed_time + 1));

  sprintf(pv_string, "pv ");
  for (; *pv; pv ++)
  {
    sprintf(buf, "%s ", m_to_str(*pv));
    strcat(pv_string, buf);
  }
  strcat(pv_string, "\n");

  _p(pv_string);
}

void set_hash_size(int hash_size_in_mb)
{
  uint64_t allocated_memory;

  if (hash_size_in_mb < 1)
    return;

  allocated_memory = init_hash(hash_size_in_mb);
  reset_hash_key(search_settings.sd);
  _p("hash=%"PRIu64"MB\n", allocated_memory >> 20);
}

void set_max_threads(int thread_cnt)
{
  search_settings.max_threads = _max(_min(thread_cnt, MAX_THREADS), 1);
  search_settings.threads_search_data =
    (search_data_t *) realloc(
      search_settings.threads_search_data, search_settings.max_threads * sizeof(search_data_t)
    );
  init_phash(search_settings.max_threads);
  reset_threads_search_data();
  _p("threads=%d\n", search_settings.max_threads);
}

void set_ponder(char *buf)
{
  search_settings.ponder_mode = starts_with(buf, "true");
  _p("ponder=%d\n", search_settings.ponder_mode);
}

void set_syzygy_path(char *buf)
{
  buf[strlen(buf) - 1] = 0;
  tb_init(buf);
  if (TB_LARGEST > 0)
    _p("syzygy_path=%s\n", buf);
}

void set_syzygy_probe_depth(int depth)
{
  search_settings.tb_probe_depth = _max(_min(depth, MAX_DEPTH), 1);
  _p("syzygy_probe_depth=%d\n", search_settings.tb_probe_depth);
}

void uci()
{
  pthread_t main_search_thread;
  int searching;
  char *buf, input_buf[READ_BUFFER_SIZE];

  _p("%s %s by %s\n", VERSION, ARCH, AUTHOR);

  searching = 0;
  search_settings.sd = (search_data_t *) malloc(sizeof(search_data_t));
  search_settings.threads_search_data = NULL;
  pthread_mutex_init(&search_settings.mutex, NULL);

  set_max_threads(DEFAULT_THREADS);
  set_hash_size(DEFAULT_HASH_SIZE_IN_MB);
  search_settings.ponder_mode = 0;
  search_settings.tb_probe_depth = 1;

  full_reset_search_data();
  read_fen(search_settings.sd, initial_fen);

  setbuf(stdout, NULL);

  while(1)
  {
    if (fgets(input_buf, READ_BUFFER_SIZE, stdin) == NULL)
    {
      sleep_ms(2);
      continue;
    }

    buf = input_buf;

    if (_cmd_cmp(&buf, CMD_UCI))
    {
      _p("id name %s %s\n", VERSION, ARCH);
      _p("id author %s\n", AUTHOR);
      _p("option name Hash type spin default %d min 1 max %d\n",
         DEFAULT_HASH_SIZE_IN_MB, MAX_HASH_SIZE_IN_MB);
      _p("option name Threads type spin default 1 min 1 max %d\n", MAX_THREADS);
      _p("option name Ponder type check default false\n");
      _p("option name SyzygyPath type string default <empty>\n");
      _p("option name SyzygyProbeDepth type spin default 1 min 1 max %d\n", MAX_DEPTH);
      _p("uciok\n");
    }

    if (_cmd_cmp(&buf, CMD_READY))
      _p("readyok\n");

    else if (_cmd_cmp(&buf, CMD_QUIT))
      break;

    else if (_cmd_cmp(&buf, CMD_STOP))
    {
      pthread_mutex_lock(&search_settings.mutex);
      search_status.done = 1;
      search_status.go.infinite = 0;
      search_status.go.ponder = 0;
      pthread_mutex_unlock(&search_settings.mutex);

      if (searching)
      {
        pthread_join(main_search_thread, NULL);
        searching = 0;
      }
    }
    else if (_cmd_cmp(&buf, CMD_PONDERHIT))
    {
      pthread_mutex_lock(&search_settings.mutex);
      search_status.go.ponder = 0;
      if (search_status.search_finished)
        search_status.done = 1;
      pthread_mutex_unlock(&search_settings.mutex);

      if (search_status.done && searching)
      {
        pthread_join(main_search_thread, NULL);
        searching = 0;
      }
    }

    else if (_cmd_cmp(&buf, CMD_NEW_GAME))
    {
      full_reset_search_data();
      read_fen(search_settings.sd, initial_fen);
    }

    else if (_cmd_cmp(&buf, CMD_POSITION_FEN))
      read_fen(search_settings.sd, buf);

    else if (_cmd_cmp(&buf, CMD_POSITION_STARTPOS))
      uci_position_startpos(buf);

    else if (_cmd_cmp(&buf, CMD_PERFT))
      uci_perft(buf);

    else if (_cmd_cmp(&buf, CMD_TEST))
      run_tests();

    else if (_cmd_cmp(&buf, CMD_PRINT))
      print_board(search_settings.sd->pos);

    else if (_cmd_cmp(&buf, OPTION_HASH))
      set_hash_size(atoi(buf));

    else if (_cmd_cmp(&buf, OPTION_THREADS))
      set_max_threads(atoi(buf));

    else if (_cmd_cmp(&buf, OPTION_PONDER))
      set_ponder(buf);

    else if (_cmd_cmp(&buf, OPTION_SYZYGY_PATH))
      set_syzygy_path(buf);

    else if (_cmd_cmp(&buf, OPTION_SYZYGY_PROBE_DEPTH))
      set_syzygy_probe_depth(atoi(buf));

    else if (_cmd_cmp(&buf, CMD_GO))
    {
      if (searching)
        pthread_join(main_search_thread, NULL);

      parse_go_cmd(buf);
      pthread_create(&main_search_thread, NULL, search, NULL);
      searching = 1;
    }
  }
}

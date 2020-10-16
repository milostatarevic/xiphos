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

#ifndef SEARCH_H
#define SEARCH_H

#include <pthread.h>

#include "move.h"
#include "position.h"
#include "util.h"

#define MAX_GAME_PLY  1024
#define TM_STEPS      10

typedef struct {
  int tid, hash_keys_cnt;
  uint64_t nodes, tbhits, hash_key;
  position_t *pos,
              pos_list[PLY_LIMIT];
  move_t   killer_moves[PLY_LIMIT][MAX_KILLER_MOVES],
           counter_moves[P_LIMIT][BOARD_SIZE];
  int16_t  history[N_SIDES][BOARD_SIZE][BOARD_SIZE],
           counter_move_history[P_LIMIT][BOARD_SIZE][P_LIMIT * BOARD_SIZE];
  uint64_t hash_keys[MAX_GAME_PLY];
} search_data_t;

typedef struct {
  int max_depth, done, search_finished, score, depth, tm_steps;
  uint64_t time_in_ms, max_time, target_time[TM_STEPS];
  struct {
    int infinite, ponder, time, inc, movestogo, depth, movetime;
  } go;
} search_status_t;

typedef struct {
  int max_threads, ponder_mode, tb_probe_depth;
  search_data_t *sd, *threads_search_data;
  pthread_mutex_t mutex;
} search_settings_t;

extern search_settings_t search_settings;
extern search_status_t search_status;

void init_lmr();
void reset_search_data(search_data_t *);
void reset_threads_search_data();
void full_reset_search_data();
void *search();

#endif

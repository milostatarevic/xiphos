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

#include <math.h>
#include <pthread.h>
#include <string.h>

#include "eval.h"
#include "hash.h"
#include "history.h"
#include "game.h"
#include "make.h"
#include "move_list.h"
#include "position.h"
#include "search.h"
#include "uci.h"

#define RAZOR_DEPTH                   3
#define FUTILITY_DEPTH                6
#define PROBCUT_DEPTH                 5
#define IID_DEPTH                     5
#define LMP_DEPTH                     4
#define LMR_DEPTH                     3
#define SE_DEPTH                      8
#define MIN_DEPTH_TO_REACH            4
#define START_THREADS_DEPTH           4
#define START_ASPIRATION_DEPTH        4

#define RAZOR_MARGIN                  200
#define PROBCUT_MARGIN                80
#define INIT_ASPIRATION_WINDOW        10

#define _futility_margin(depth)       (80 * (depth))
#define _null_move_reduction(depth)   ((depth) / 4 + 3)
#define _lmp_count(depth)             (2 + (depth) * 2)

int lmr[MAX_DEPTH][MAX_MOVES];

void init_lmr()
{
  int d, m;

  for (d = 0; d < MAX_DEPTH; d ++)
    for (m = 0; m < MAX_MOVES; m ++)
      if (m <= 2)
        lmr[d][m] = 0;
      else if (m <= 4)
        lmr[d][m] = 1;
      else
        lmr[d][m] = sqrt(d * m / 8);
}

static inline int draw(search_data_t *sd, int repetitions)
{
  int i, cnt, fifty_cnt;

  fifty_cnt = sd->pos->fifty_cnt;
  if (fifty_cnt >= 100 || insufficient_material(sd->pos))
    return 1;

  if (fifty_cnt < 4)
    return 0;

  cnt = 0;
  for (i = sd->hash_keys_cnt - 2; i >= (sd->hash_keys_cnt - fifty_cnt); i -= 2)
  {
    if (sd->hash_keys[i] == sd->hash_key)
      cnt ++;
    if (cnt >= repetitions)
      return 1;
  }
  return 0;
}

int qsearch(search_data_t *sd, int alpha, int beta, int depth, int ply)
{
  int use_hash, hash_depth, hash_bound, best_score, score;
  move_t move, best_move, hash_move;
  hash_data_t hash_data;
  position_t *pos;
  move_list_t move_list;

  alpha = _max(alpha, -MATE_SCORE + ply);
  beta = _min(beta, MATE_SCORE - ply + 1);
  if (alpha >= beta) return alpha;

  pos = sd->pos;
  if (ply >= MAX_PLY) return eval(pos);
  if (draw(sd, 1)) return 0;

  hash_depth = (pos->in_check || depth == 0) ? 0 : -1;
  use_hash = hash_depth == 0 || shared_search_info.max_threads > 1;
  hash_move = 0;

  if (use_hash)
  {
    hash_data = get_hash_data(sd);
    if (hash_data.raw && hash_data.depth >= hash_depth)
    {
      hash_move = hash_data.move;
      hash_bound = hash_data.bound;

      score = adjust_hash_score(_m_score(hash_move), ply);
      if ((hash_bound == HASH_LOWER_BOUND && score >= beta) ||
          (hash_bound == HASH_UPPER_BOUND && score <= alpha) ||
          (hash_bound == HASH_EXACT))
        return score;
    }
  }

  if (pos->in_check)
  {
    best_score = -MATE_SCORE + ply;
  }
  else
  {
    best_score = eval(pos);
    if (best_score >= beta) return best_score;
    if (alpha < best_score) alpha = best_score;
  }

  best_move = hash_move;
  hash_bound = HASH_UPPER_BOUND;
  init_move_list(&move_list, QSEARCH, pos->in_check);

  while ((move = next_move(&move_list, sd, hash_move, depth, ply, 0, 0)))
  {
    if (!legal_move(pos, move))
      continue;

    make_move(sd, move);
    score = -qsearch(sd, -beta, -alpha, depth - 1, ply + 1);
    undo_move(sd);

    if (score > best_score)
    {
      best_score = score;
      if (score > alpha)
      {
        best_move = move;
        alpha = score;
        hash_bound = HASH_EXACT;

        if (alpha >= beta) {
          hash_bound = HASH_LOWER_BOUND;
          break;
        }
      }
    }
  }
  if (use_hash)
    set_hash_data(sd, ply, best_move, best_score, hash_depth, hash_bound);
  return best_score;
}

int pvs(search_data_t *sd, int root_node, int pv_node, int alpha, int beta,
        int depth, int ply, int use_pruning, move_t skip_move)
{
  int i, searched_cnt, best_score, use_hash, hash_bound, score, hash_score,
      static_score, beta_cut, lmp_started, new_depth, reduction, h_score;
  move_t move, best_move, hash_move;
  uint64_t pinned, b_att, r_att;
  hash_data_t hash_data;
  position_t *pos;
  move_list_t move_list;

  if (depth <= 0)
      return qsearch(sd, alpha, beta, 0, ply);

  alpha = _max(alpha, -MATE_SCORE + ply);
  beta = _min(beta, MATE_SCORE - ply + 1);
  if (alpha >= beta) return alpha;

  pos = sd->pos;
  if (ply >= MAX_PLY) return eval(pos);

  if (shared_search_info.done) return 0;
  if (!root_node && draw(sd, 1)) return 0;

  // load hash data
  use_hash = !skip_move;
  hash_move = hash_data.raw = 0;
  hash_score = -MATE_SCORE;
  hash_bound = HASH_BOUND_NOT_USED;

  if (use_hash)
  {
    hash_data = get_hash_data(sd);
    if (hash_data.raw)
    {
      hash_move = hash_data.move;
      hash_bound = hash_data.bound;
      hash_score = adjust_hash_score(_m_score(hash_move), ply);

      if(!pv_node && hash_data.depth >= depth)
        if ((hash_bound == HASH_LOWER_BOUND && hash_score >= beta) ||
            (hash_bound == HASH_UPPER_BOUND && hash_score <= alpha) ||
            (hash_bound == HASH_EXACT))
          return hash_score;
    }
  }

  //////////////////////////////////////////////////////////////////////////////

  // evaluate
  if (pos->in_check)
    static_score = -MATE_SCORE + ply;
  else
  {
    if (hash_data.raw && hash_bound == HASH_EXACT)
      static_score = hash_score;
    else
    {
      static_score = eval(pos);
      if (hash_data.raw)
      {
        if ((hash_score < static_score && hash_bound == HASH_UPPER_BOUND) ||
            (hash_score > static_score && hash_bound == HASH_LOWER_BOUND))
          static_score = hash_score;
      }
    }
  }

  if (use_pruning && !pos->in_check && !_is_mate_score(beta))
  {
    if (!pv_node)
    {
      // razoring
      if (depth <= RAZOR_DEPTH && static_score + RAZOR_MARGIN < beta)
      {
        score = qsearch(sd, alpha, beta, 0, ply);
        if (score < beta) return score;
      }

      if (non_pawn_material(pos))
      {
        // futility
        if (depth <= FUTILITY_DEPTH && static_score >= beta + _futility_margin(depth))
          return static_score;

        // null move
        if (depth >= 2 && static_score >= beta)
        {
          make_null_move(sd);
          score = -pvs(sd, 0, 0, -beta, -beta + 1,
                       depth - _null_move_reduction(depth) - 1, ply + 1, 0, 0);
          undo_move(sd);

          if (shared_search_info.done) return 0;
          if (score >= beta)
            return _is_mate_score(score) ? beta : score;
        }
      }

      // ProbCut
      if (depth >= PROBCUT_DEPTH)
      {
        beta_cut = beta + PROBCUT_MARGIN;
        init_move_list(&move_list, QSEARCH, pos->in_check);

        while ((move = next_move(&move_list, sd, hash_move, depth, ply, 0, 0)))
        {
          if (_m_eq(move, hash_move) && (_m_is_quiet(move) || bad_SEE(pos, move)))
            continue;

          if (!legal_move(pos, move))
            continue;

          make_move(sd, move);
          score = -pvs(sd, 0, 0, -beta_cut, -beta_cut + 1,
                       depth - PROBCUT_DEPTH + 1, ply + 1, 1, 0);
          undo_move(sd);

          if (score >= beta_cut)
            return score;
        }
      }
    }

    // IID
    if (depth >= IID_DEPTH && pv_node && !_is_m(hash_move))
    {
      pvs(sd, 0, 1, alpha, beta, depth - 2, ply, 0, 0);
      hash_data = get_hash_data(sd);
      if (hash_data.raw)
        hash_move = hash_data.move;
    }
  }

  //////////////////////////////////////////////////////////////////////////////

  // init move_list
  init_move_list(&move_list, SEARCH, pos->in_check);

  hash_bound = HASH_UPPER_BOUND;
  best_score = -MATE_SCORE + ply;
  best_move = hash_move;
  searched_cnt = lmp_started = 0;
  pinned = b_att = r_att = 0;

  while ((move = next_move(&move_list, sd, hash_move, depth, ply, lmp_started, root_node)))
  {
    if (_m_eq(move, skip_move))
      continue;

    // LMP
    if (depth <= LMP_DEPTH && !pos->in_check && !pv_node &&
        _m_is_quiet(move) && searched_cnt >= _lmp_count(depth))
    {
      // init lookup for gives_check
      if (!lmp_started)
        pins_and_attacks_to(pos, pos->k_sq[pos->side ^ 1], pos->side, pos->side,
                            &pinned, &b_att, &r_att);

      lmp_started = 1;
      if (!gives_check(pos, move, pinned, b_att, r_att))
        continue;
    }

    if (!legal_move(pos, move))
      continue;

    new_depth = depth - 1;

    // singular extensions
    if (depth >= SE_DEPTH && !skip_move && _m_eq(move, hash_move) &&
        !root_node && !_is_mate_score(hash_score) &&
        hash_data.bound == HASH_LOWER_BOUND && hash_data.depth >= depth - 3)
    {
      beta_cut = hash_score - depth;
      score = pvs(sd, 0, 0, beta_cut - 1, beta_cut, depth >> 1, ply, 0, move);
      if (score < beta_cut)
        new_depth ++;
    }

    // make move
    make_move(sd, move);
    searched_cnt ++;

    // search
    if (searched_cnt == 1)
      score = -pvs(sd, 0, pv_node, -beta, -alpha, new_depth, ply + 1, 1, 0);
    else
    {
      // LMR
      reduction = 0;
      if (depth >= LMR_DEPTH && !pos->in_check && _m_is_quiet(move))
        reduction = lmr[depth][searched_cnt];

      score = -pvs(sd, 0, 0, -alpha - 1, -alpha, new_depth - reduction, ply + 1, 1, 0);
      if (reduction && score > alpha)
        score = -pvs(sd, 0, 0, -alpha - 1, -alpha, new_depth, ply + 1, 1, 0);

      if (score > alpha && score < beta)
        score = -pvs(sd, 0, 1, -beta, -alpha, new_depth, ply + 1, 1, 0);
    }
    undo_move(sd);

    if (shared_search_info.done)
      return 0;

    if (score > best_score)
    {
      best_score = score;
      if (score > alpha)
      {
        best_move = move;
        if (ply == 0 && shared_search_info.depth <= depth)
        {
          shared_search_info.depth = depth;
          shared_search_info.best_move = move;

          uci_info(depth, score);
        }

        alpha = score;
        hash_bound = HASH_EXACT;

        if (alpha >= beta) {
          hash_bound = HASH_LOWER_BOUND;

          // save history / killer / counter moves
          if (!pos->in_check && _m_is_quiet(best_move))
          {
            h_score = depth * depth;
            set_killer_move(sd, best_move, ply);
            set_counter_move(sd, best_move);
            add_to_history(sd, best_move, h_score);

            if (_is_m(hash_move) && !_m_eq(hash_move, best_move) &&
                _m_is_quiet(hash_move))
              add_to_bad_history(sd, hash_move, h_score);

            for (i = 0; i < move_list.cnt; i ++)
            {
              move = move_list.moves[i];
              if (!_m_eq(hash_move, move) && !_m_eq(best_move, move))
                add_to_bad_history(sd, move, h_score);
            }
          }
          break;
        }
      }
    }
  }

  // mate/stalemate
  if (searched_cnt == 0)
    return pos->in_check || skip_move ? (-MATE_SCORE + ply) : 0;

  // save hash item
  if (use_hash)
    set_hash_data(sd, ply, best_move, best_score, depth, hash_bound);

  return best_score;
}

void *search_thread(void *thread_data)
{
  int depth, start_depth, score, alpha, beta, delta, threads_cnt, threads_max;
  search_data_t *sd;

  sd = (search_data_t *)thread_data;

  // start additional threads with delay
  start_depth = (sd->tid == 0) ? 1 : START_THREADS_DEPTH;
  if (sd->tid)
  {
    while(!shared_search_info.search_depth_cnt[start_depth] && !shared_search_info.done)
    {
      sleep_ms(1);
      continue;
    }
  }

  score = 0;
  threads_cnt = threads_max = (shared_search_info.max_threads + 1) >> 1;
  for (depth = start_depth; depth <= shared_search_info.max_depth; depth ++)
  {
    if (shared_search_info.search_depth_cnt[depth] >= threads_cnt)
    {
      threads_cnt = _max(threads_cnt >> 1, 1);
      continue;
    }
    shared_search_info.search_depth_cnt[depth] ++;
    threads_cnt = threads_max;

    delta = (depth >= START_ASPIRATION_DEPTH) ? INIT_ASPIRATION_WINDOW : MATE_SCORE;
    alpha = _max(score - delta, -MATE_SCORE);
    beta = _min(score + delta, MATE_SCORE);

    while (1)
    {
      score = pvs(sd, 1, 1, alpha, beta, depth, 0, 0, 0);
      if (shared_search_info.done) break;

      delta <<= 1;
      if (score <= alpha)
        alpha = _max(score - delta, -MATE_SCORE);
      else if (score >= beta)
        beta = _min(score + delta, MATE_SCORE);
      else
        break;
    }
    if (shared_search_info.done) break;
  }

  if (depth > shared_search_info.max_depth)
    shared_search_info.done = 1;

  pthread_exit(NULL);
}

void reset_search_data(search_data_t *sd)
{
  uint64_t hash_key;

  hash_key = sd->hash_keys[0];

  memset(sd, 0, sizeof(search_data_t));
  sd->pos = sd->pos_list;

  // restore hash key
  sd->hash_key = hash_key;
  sd->hash_keys[0] = hash_key;
}

void full_reset_search_data(search_data_t *sd)
{
  reset_search_data(sd);
  reset_hash_key(sd);
}

void init_search_data(search_data_t *sd, search_data_t *src_sd, int t)
{
  sd->tid = t;
  sd->pos = sd->pos_list;
  position_cpy(sd->pos, src_sd->pos);

  sd->hash_key = src_sd->hash_key;
  sd->hash_keys_cnt = src_sd->hash_keys_cnt;
  memcpy(sd->hash_keys, src_sd->hash_keys, (src_sd->hash_keys_cnt + 1) * sizeof(uint64_t));

  clear_history(sd);
}

void search(search_data_t *sd, search_data_t *threads_search_data)
{
  int t;
  pthread_attr_t attr;
  pthread_t threads[MAX_THREADS];

  if (draw(sd, 2))
  {
    print("bestmove (none)\n");
    return;
  }

  shared_search_info.time_in_ms = time_in_ms();
  set_hash_iteration();
  reevaluate_position(sd->pos);

  // launch search threads
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  for (t = 0; t < shared_search_info.max_threads; t ++)
  {
    init_search_data(&threads_search_data[t], sd, t);
    pthread_create(&threads[t], &attr, search_thread, (void *) &threads_search_data[t]);
  }

  while (!shared_search_info.done)
  {
    if (time_in_ms() - shared_search_info.time_in_ms >= shared_search_info.max_time &&
        shared_search_info.depth >= MIN_DEPTH_TO_REACH)
      shared_search_info.done = 1;
    sleep_ms(2);
  }

  pthread_attr_destroy(&attr);
  for (t = 0; t < shared_search_info.max_threads; t ++)
    pthread_join(threads[t], NULL);

  print("bestmove %s\n", m_to_str(shared_search_info.best_move));
}

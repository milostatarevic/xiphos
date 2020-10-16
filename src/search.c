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

#include <math.h>
#include <string.h>

#include "eval.h"
#include "hash.h"
#include "history.h"
#include "game.h"
#include "make.h"
#include "move_list.h"
#include "position.h"
#include "search.h"
#include "tablebases.h"
#include "uci.h"
#include "fathom/tbprobe.h"

#define RAZOR_DEPTH                   3
#define FUTILITY_DEPTH                6
#define PROBCUT_DEPTH                 5
#define IID_DEPTH                     5
#define LMP_DEPTH                     10
#define CMHP_DEPTH                    3
#define LMR_DEPTH                     3
#define SE_DEPTH                      8
#define MIN_DEPTH_TO_REACH            4
#define START_ASPIRATION_DEPTH        4

#define RAZOR_MARGIN                  200
#define PROBCUT_MARGIN                80
#define INIT_ASPIRATION_WINDOW        6
#define MIN_HASH_DEPTH                -2

#define _futility_margin(depth)       (80 * (depth))
#define _see_quiets_margin(depth)     (-15 * _sqr((depth) - 1))
#define _see_captures_margin(depth)   (-100 * (depth))
#define _h_score(depth)               (_sqr(_min(depth, 16)) * 32)

search_settings_t search_settings;
search_status_t search_status;

const int lmp[][LMP_DEPTH + 1] = {
  { 0, 2, 3, 5, 9, 13, 18, 25, 34, 45, 55 },
  { 0, 5, 6, 9, 14, 21, 30, 41, 55, 69, 84 }
};

int lmr[MAX_DEPTH][MAX_MOVES];
int shared_search_depth_cnt[MAX_DEPTH];
move_t pv[PLY_LIMIT * PLY_LIMIT];

void init_lmr()
{
  int d, m;

  for (d = 0; d < MAX_DEPTH; d ++)
    for (m = 0; m < MAX_MOVES; m ++)
      lmr[d][m] = 1.0 + log(d) * log(m) * 0.5;
}

static inline int draw(search_data_t *sd)
{
  int i, fifty_cnt;

  fifty_cnt = sd->pos->fifty_cnt;
  if (fifty_cnt >= 100 || insufficient_material(sd->pos))
    return 1;

  if (fifty_cnt < 4)
    return 0;

  for (i = sd->hash_keys_cnt - 2; i >= (sd->hash_keys_cnt - fifty_cnt); i -= 2)
    if (sd->hash_keys[i] == sd->hash_key)
      return 1;
  return 0;
}

void update_pv(move_t move, int ply)
{
  move_t *dest, *src;

  dest = pv + ply * PLY_LIMIT;
  src = dest + PLY_LIMIT;

  *dest++ = move;
  while ((*dest++ = *src++));
}

int qsearch(search_data_t *sd, int pv_node, int alpha, int beta, int depth, int ply)
{
  int hash_depth, hash_bound, hash_score, static_score, best_score, score;
  move_t move, best_move, hash_move;
  hash_data_t hash_data;
  position_t *pos;
  move_list_t move_list;

  alpha = _max(alpha, -MATE_SCORE + ply);
  beta = _min(beta, MATE_SCORE - ply + 1);
  if (alpha >= beta) return alpha;

  pos = sd->pos;
  if (ply >= MAX_PLY) return eval(pos);
  if (draw(sd)) return 0;

  hash_move = 0;
  hash_score = -MATE_SCORE;
  hash_bound = HASH_BOUND_NOT_USED;
  hash_depth = (pos->in_check || depth == 0) ? 0 : -1;

  hash_data = get_hash_data(sd);
  if (hash_data.raw)
  {
    hash_move = hash_data.move;
    hash_bound = hash_data.bound;
    hash_score = adjust_hash_score(_m_score(hash_move), ply);

    if (!pv_node && hash_data.depth >= hash_depth)
    {
      if ((hash_bound == HASH_LOWER_BOUND && hash_score >= beta) ||
          (hash_bound == HASH_UPPER_BOUND && hash_score <= alpha) ||
          (hash_bound == HASH_EXACT))
        return hash_score;
    }
  }

  if (pos->in_check)
  {
    best_score = static_score = -MATE_SCORE + ply;
  }
  else
  {
    best_score = static_score = hash_data.raw ? hash_data.static_score : eval(pos);
    if (hash_data.raw)
    {
      if ((hash_bound == HASH_LOWER_BOUND && hash_score > static_score) ||
          (hash_bound == HASH_UPPER_BOUND && hash_score < static_score) ||
          (hash_bound == HASH_EXACT))
        best_score = hash_score;
    }

    if (best_score >= beta) return best_score;
    if (alpha < best_score) alpha = best_score;
  }

  best_move = hash_move;
  hash_bound = HASH_UPPER_BOUND;
  init_move_list(&move_list, QSEARCH, pos->in_check);

  while ((move = next_move(&move_list, sd, hash_move, depth, ply)))
  {
    if (!pos->in_check && SEE(pos, move, 1) < 0)
      continue;

    if (!legal_move(pos, move))
      continue;

    make_move(sd, move);
    score = -qsearch(sd, 0, -beta, -alpha, depth - 1, ply + 1);
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
  set_hash_data(sd, best_move, best_score, static_score, hash_depth, ply, hash_bound);
  return best_score;
}

int pvs(search_data_t *sd, int root_node, int pv_node, int alpha, int beta,
        int depth, int ply, int use_pruning, move_t skip_move)
{
  int i, searched_cnt, quiet_moves_cnt, lmp_cnt, best_score, static_score,
      score, use_hash, hash_bound, hash_score, improving, beta_cut,
      new_depth, piece_pos, reduction, h_score, piece_cnt;
  unsigned tb_result;
  move_t move, best_move, hash_move;
  hash_data_t hash_data;
  int16_t *cmh_ptr[MAX_CMH_PLY];
  position_t *pos;
  move_list_t move_list;
  move_t quiet_moves[MAX_MOVES];

  if (depth <= 0)
    return qsearch(sd, pv_node, alpha, beta, 0, ply);

  alpha = _max(alpha, -MATE_SCORE + ply);
  beta = _min(beta, MATE_SCORE - ply + 1);
  if (alpha >= beta) return alpha;

  pos = sd->pos;
  if (ply >= MAX_PLY) return eval(pos);

  if (search_status.done) return 0;
  if (!root_node && draw(sd)) return 0;

  set_counter_move_history_pointer(cmh_ptr, sd, ply);

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
        {
          if (_m_is_quiet(hash_move))
          {
            if (hash_bound == HASH_LOWER_BOUND)
            {
              set_killer_move(sd, hash_move, ply);
              set_counter_move(sd, hash_move);
              add_to_history(sd, cmh_ptr, hash_move, _h_score(depth));
            }
            else if (hash_bound == HASH_UPPER_BOUND)
              add_to_history(sd, cmh_ptr, hash_move, -_h_score(depth));
          }
          return hash_score;
        }
    }
  }

  // probe tablebases
  if (TB_LARGEST > 0 && !root_node && !pos->fifty_cnt && !pos->c_flag)
  {
    piece_cnt = _popcnt(_occ(pos));
    if (piece_cnt < TB_LARGEST ||
       (piece_cnt == TB_LARGEST && depth >= search_settings.tb_probe_depth))
    {
      tb_result = tablebases_probe_wdl(pos);
      if (tb_result != TB_RESULT_FAILED)
      {
        sd->tbhits ++;

        switch(tb_result)
        {
          case TB_WIN:
            score = MATE_SCORE - MAX_PLY - ply - 1;
            hash_bound = HASH_LOWER_BOUND;
            break;
          case TB_LOSS:
            score = -MATE_SCORE + MAX_PLY + ply + 1;
            hash_bound = HASH_UPPER_BOUND;
            break;
          default:
            score = 0;
            hash_bound = HASH_EXACT;
        }

        if ((hash_bound == HASH_LOWER_BOUND && score >= beta) ||
            (hash_bound == HASH_UPPER_BOUND && score <= alpha) ||
            (hash_bound == HASH_EXACT))
        {
          set_hash_data(sd, 0, score, score, MAX_PLY - 1, 0, hash_bound);
          return score;
        }
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////

  // evaluate
  if (pos->in_check)
    static_score = -MATE_SCORE + ply;
  else
  {
    if (hash_data.raw)
      static_score = hash_data.static_score;
    else
    {
      static_score = eval(pos);
      if (use_hash)
        set_hash_data(sd, 0, 0, static_score, MIN_HASH_DEPTH, ply, HASH_BOUND_NOT_USED);
    }
  }
  pos->static_score = static_score;
  improving = !pos->in_check && ply >= 2 && static_score >= (sd->pos-2)->static_score;

  best_score = static_score;
  if (hash_data.raw && !pos->in_check)
  {
    if ((hash_bound == HASH_LOWER_BOUND && hash_score > static_score) ||
        (hash_bound == HASH_UPPER_BOUND && hash_score < static_score) ||
        (hash_bound == HASH_EXACT))
      best_score = hash_score;
  }

  if (use_pruning && !pos->in_check && !_is_mate_score(beta))
  {
    if (!pv_node)
    {
      // razoring
      if (depth <= RAZOR_DEPTH && best_score + RAZOR_MARGIN < beta)
      {
        score = qsearch(sd, 0, alpha, beta, 0, ply);
        if (score < beta) return score;
      }

      if (non_pawn_material(pos))
      {
        // futility
        if (depth <= FUTILITY_DEPTH && best_score >= beta + _futility_margin(depth))
          return best_score;

        // null move
        if (depth >= 2 && best_score >= beta)
        {
          reduction = (depth) / 4 + 3 + _min((best_score - beta) / 80, 3);

          make_null_move(sd);
          score = -pvs(sd, 0, 0, -beta, -beta + 1, depth - reduction, ply + 1, 0, 0);
          undo_move(sd);

          if (search_status.done) return 0;
          if (score >= beta)
            return _is_mate_score(score) ? beta : score;
        }
      }

      // ProbCut
      if (depth >= PROBCUT_DEPTH)
      {
        beta_cut = beta + PROBCUT_MARGIN;
        init_move_list(&move_list, QSEARCH, pos->in_check);

        while ((move = next_move(&move_list, sd, hash_move, depth, ply)))
        {
          if (_m_is_quiet(move) || SEE(pos, move, 0) < beta_cut - static_score)
            continue;

          if (!legal_move(pos, move))
            continue;

          make_move(sd, move);
          score = -qsearch(sd, 0, -beta_cut, -beta_cut + 1, 0, ply);
          if (score >= beta_cut)
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
  searched_cnt = quiet_moves_cnt = lmp_cnt = 0;

  sd->killer_moves[ply + 1][0] = sd->killer_moves[ply + 1][1] = 0;

  while ((move = next_move(&move_list, sd, hash_move, depth, ply)))
  {
    if (_m_eq(move, skip_move))
      continue;

    lmp_cnt ++;
    if (!root_node && searched_cnt >= 1)
    {
      if (_m_is_quiet(move))
      {
        // LMP
        if (depth <= LMP_DEPTH && lmp_cnt > lmp[improving][depth])
        {
          move_list.cnt = move_list.moves_cnt;
          continue;
        }

        // CMH pruning
        if (depth <= CMHP_DEPTH)
        {
          piece_pos = pos->board[_m_from(move)] * BOARD_SIZE + _m_to(move);
          if ((!cmh_ptr[0] || cmh_ptr[0][piece_pos] < 0) &&
              (!cmh_ptr[1] || cmh_ptr[1][piece_pos] < 0))
            continue;
        }

        // SEE pruning
        if (SEE(pos, move, 1) < _see_quiets_margin(depth))
          continue;
      }

      // prune bad captures
      if (move_list.phase == BAD_CAPTURES &&
          _m_score(move) < _see_captures_margin(depth))
        continue;
    }

    if (!legal_move(pos, move))
    {
      lmp_cnt --;
      continue;
    }

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
    // cmh extension
    else if (_m_is_quiet(move))
    {
      piece_pos = pos->board[_m_from(move)] * BOARD_SIZE + _m_to(move);
      if (cmh_ptr[0] && cmh_ptr[1] &&
          cmh_ptr[0][piece_pos] >= MAX_HISTORY_SCORE / 2 &&
          cmh_ptr[1][piece_pos] >= MAX_HISTORY_SCORE / 2)
        new_depth ++;
    }

    // make move
    make_move(sd, move);
    searched_cnt ++;

    if (sd->tid == 0 && pv_node)
      pv[(ply + 1) * PLY_LIMIT] = 0;

    // search
    if (searched_cnt == 1)
      score = -pvs(sd, 0, pv_node, -beta, -alpha, new_depth, ply + 1, 1, 0);
    else
    {
      // LMR
      reduction = 0;
      if (depth >= LMR_DEPTH && _m_is_quiet(move))
      {
        reduction = lmr[depth][searched_cnt];

        if (!improving) reduction ++;
        if (reduction && pv_node) reduction --;

        reduction -= 2 * get_h_score(sd, pos, cmh_ptr, move) / MAX_HISTORY_SCORE;

        if (reduction >= new_depth)
          reduction = new_depth - 1;
        else if (reduction < 0)
          reduction = 0;
      }

      score = -pvs(sd, 0, 0, -alpha - 1, -alpha, new_depth - reduction, ply + 1, 1, 0);
      if (reduction && score > alpha)
        score = -pvs(sd, 0, 0, -alpha - 1, -alpha, new_depth, ply + 1, 1, 0);

      if (score > alpha && score < beta)
        score = -pvs(sd, 0, 1, -beta, -alpha, new_depth, ply + 1, 1, 0);
    }
    undo_move(sd);

    if (search_status.done)
      return 0;

    if (score > best_score)
    {
      best_score = score;
      if (score > alpha)
      {
        best_move = move;

        if (sd->tid == 0 && pv_node)
        {
          if (root_node)
          {
            if (_m_eq(best_move, pv[0]))
            {
              if (search_status.tm_steps > 0)
                search_status.tm_steps --;
            }
            else
              search_status.tm_steps = TM_STEPS - 1;

            search_status.score = score;
            search_status.depth = depth;
          }
          update_pv(move, ply);
        }

        alpha = score;
        hash_bound = HASH_EXACT;

        if (alpha >= beta) {
          hash_bound = HASH_LOWER_BOUND;

          // save history / killer / counter moves
          if (_m_is_quiet(best_move))
          {
            h_score = _h_score(depth + (score > beta + 80));

            set_killer_move(sd, best_move, ply);
            set_counter_move(sd, best_move);
            add_to_history(sd, cmh_ptr, best_move, h_score);

            for (i = 0; i < quiet_moves_cnt; i ++)
              add_to_history(sd, cmh_ptr, quiet_moves[i], -h_score);
          }
          break;
        }
      }
    }

    if (_m_is_quiet(move))
      quiet_moves[quiet_moves_cnt ++] = move;
  }

  // mate/stalemate
  if (searched_cnt == 0)
    return pos->in_check || skip_move ? (-MATE_SCORE + ply) : 0;

  // save hash item
  if (use_hash)
    set_hash_data(sd, best_move, best_score, static_score, depth, ply, hash_bound);

  return best_score;
}

void *search_thread(void *thread_data)
{
  int depth, search_depth_cnt, score, prev_score, alpha, beta, delta;
  uint64_t target_time;
  search_data_t *sd;

  sd = (search_data_t *)thread_data;

  score = prev_score = 0;
  for (depth = 1; depth <= search_status.max_depth; depth ++)
  {
    pthread_mutex_lock(&search_settings.mutex);
    search_depth_cnt = ++shared_search_depth_cnt[depth];
    pthread_mutex_unlock(&search_settings.mutex);

    if (sd->tid && depth > 1 && depth < search_status.max_depth &&
        search_depth_cnt > _max((search_settings.max_threads + 1) / 2, 2))
      continue;

    delta = (depth >= START_ASPIRATION_DEPTH) ? INIT_ASPIRATION_WINDOW : MATE_SCORE;
    alpha = _max(score - delta, -MATE_SCORE);
    beta = _min(score + delta, MATE_SCORE);

    while (delta <= MATE_SCORE)
    {
      score = pvs(sd, 1, 1, alpha, beta, depth, 0, 0, 0);
      if (search_status.done) break;

      delta += 2 + delta / 2;
      if (score <= alpha)
      {
        beta = (alpha + beta) / 2;
        alpha = _max(score - delta, -MATE_SCORE);
      }
      else if (score >= beta)
        beta = _min(score + delta, MATE_SCORE);
      else
        break;
    }
    if (search_status.done) break;

    if (sd->tid == 0)
    {
      uci_info(pv);

      target_time = search_status.target_time[search_status.tm_steps];
      if (prev_score > score)
        target_time *= _min(1.0 + (double)(prev_score - score) / 80.0, 2.0);

      prev_score = score;

      if (!search_status.go.ponder &&
           target_time > 0 && depth >= MIN_DEPTH_TO_REACH &&
           time_in_ms() - search_status.time_in_ms >= target_time)
        break;
    }
  }

  if (sd->tid == 0)
  {
    uci_info(pv);

    pthread_mutex_lock(&search_settings.mutex);
    search_status.search_finished = 1;
    if (!search_status.go.ponder)
      search_status.done = 1;
    pthread_mutex_unlock(&search_settings.mutex);
  }

  return NULL;
}

void print_best_move(search_data_t *sd)
{
  move_t best_move, ponder_move;
  hash_data_t hash_data;

  best_move = pv[0];
  ponder_move = pv[1];

  if (!_is_m(ponder_move))
  {
    make_move(sd, best_move);

    hash_data = get_hash_data(sd);
    if (hash_data.raw)
    {
      ponder_move = hash_data.move;
      if (!_is_m(ponder_move) || !legal_move(sd->pos, ponder_move))
        ponder_move = 0;
    }

    undo_move(sd);
  }

  _p("bestmove %s", m_to_str(best_move));
  if (_is_m(ponder_move))
    _p(" ponder %s", m_to_str(ponder_move));
  _p("\n");
}

void reset_search_data(search_data_t *sd)
{
  int i, j, k;
  uint64_t hash_key;

  hash_key = sd->hash_keys[0];
  memset(sd, 0, sizeof(search_data_t));

  sd->pos = sd->pos_list;
  sd->hash_key = hash_key;

  for (i = 0; i < P_LIMIT; i ++)
    for (j = 0; j < BOARD_SIZE; j ++)
      for (k = 0; k < P_LIMIT * BOARD_SIZE; k ++)
        sd->counter_move_history[i][j][k] = -1;
}

void reset_threads_search_data()
{
  int t;

  for (t = 0; t < search_settings.max_threads; t ++)
    reset_search_data(&search_settings.threads_search_data[t]);
}

void full_reset_search_data()
{
  reset_search_data(search_settings.sd);
  reset_hash_key(search_settings.sd);
  reset_threads_search_data();
}

void init_search_data(search_data_t *sd, search_data_t *src_sd, int t)
{
  sd->tid = t;
  sd->pos = sd->pos_list;
  position_cpy(sd->pos, src_sd->pos);

  sd->hash_key = src_sd->hash_key;
  sd->hash_keys_cnt = src_sd->hash_keys_cnt;
  memcpy(sd->hash_keys, src_sd->hash_keys, sizeof(sd->hash_keys));

  sd->nodes = sd->tbhits = 0;
}

void *search()
{
  int t;
  move_t tb_move;
  search_data_t *sd;
  pthread_t threads[MAX_THREADS];

  sd = search_settings.sd;
  search_status.tm_steps = 0;
  search_status.time_in_ms = time_in_ms();

  set_hash_iteration();
  reevaluate_position(sd->pos);

  memset(pv, 0, sizeof(pv));
  memset(shared_search_depth_cnt, 0, sizeof(shared_search_depth_cnt));

  // prepare search threads
  for (t = 0; t < search_settings.max_threads; t ++)
    init_search_data(&search_settings.threads_search_data[t], sd, t);

  // probe tablebases
  if (TB_LARGEST > 0 && !sd->pos->c_flag && _popcnt(_occ(sd->pos)) <= TB_LARGEST)
  {
    tb_move = tablebases_probe_root(sd->pos);
    if (_is_m(tb_move))
    {
      pv[0] = tb_move; pv[1] = 0;

      search_status.depth = 1;
      search_status.score = _m_score(tb_move);
      search_settings.threads_search_data[0].tbhits = 1;

      uci_info(pv);
      print_best_move(sd);

      return NULL;
    }
  }

  // launch search threads
  for (t = 0; t < search_settings.max_threads; t ++)
    pthread_create(&threads[t], NULL, search_thread, (void *) &search_settings.threads_search_data[t]);

  while (!search_status.done)
  {
    pthread_mutex_lock(&search_settings.mutex);
    if (!search_status.go.infinite &&
        !search_status.go.ponder &&
        time_in_ms() - search_status.time_in_ms >= search_status.max_time &&
        search_status.depth >= MIN_DEPTH_TO_REACH)
      search_status.done = 1;
    pthread_mutex_unlock(&search_settings.mutex);

    sleep_ms(2);
  }

  for (t = 0; t < search_settings.max_threads; t ++)
    pthread_join(threads[t], NULL);

  print_best_move(sd);
  return NULL;
}

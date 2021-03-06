/**
 * (C) 2010-2011 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * Version: $Id$
 *
 * ob_merger.cc for ...
 *
 * Authors:
 *   rizhao <rizhao.ych@taobao.com>
 *
 */
#include "ob_merger.h"
#include "ob_action_flag.h"

namespace sb {
namespace common {
ObMerger :: ObMerger(bool is_asc) {
  reset();
  is_asc_ = is_asc;
}

ObMerger :: ~ObMerger() {
}

void ObMerger :: reset() {
  iter_num_ = 0;
  memset(iters_, 0x00, sizeof(iters_));
  memset(iter_status_, 0x00, sizeof(iter_status_));

  memset(cur_iter_arr_, 0x00, sizeof(cur_iter_arr_));
  cur_iter_arr_idx_ = 0;
  cur_iter_arr_num_ = 0;
  cur_iter_cell_ = NULL;
  iter_row_changed_ = false;
}

int ObMerger :: add_iterator(ObIterator* iterator) {
  int err = OB_SUCCESS;

  if (NULL == iterator) {
    TBSYS_LOG(WARN, "invalid param, iterator=%p", iterator);
    err = OB_INVALID_ARGUMENT;
  } else if (iter_num_ >= MAX_ITER_NUM) {
    TBSYS_LOG(WARN, "exceeds limit, iter_num_=%ld", iter_num_);
    err = OB_ERROR;
  } else {
    iter_status_[iter_num_] = ITER_BEFORE_NEXT;
    iters_[iter_num_] = iterator;
    ++iter_num_;
  }

  return err;
}

int ObMerger :: next_cell() {
  int err = OB_SUCCESS;

  if (iter_num_ < 0) {
    TBSYS_LOG(ERROR, "error occurs, iter_num=%ld", iter_num_);
    err = OB_ERROR;
  } else if (0 == iter_num_) {
    err = OB_ITER_END;
  } else {
    bool find_success = false;

    if (OB_SUCCESS == err && cur_iter_arr_idx_ < cur_iter_arr_num_) { // current row is iterating
      err = next_cell_(cur_iter_arr_[cur_iter_arr_idx_]);
      if (OB_ITER_END == err) {
        ++cur_iter_arr_idx_;
        err = OB_SUCCESS; // wrap error
      } else if (OB_SUCCESS != err) {
        TBSYS_LOG(WARN, "failed to call next_cell of the %ld-th iter, err=%d",
                  cur_iter_arr_[cur_iter_arr_idx_], err);
      } else {
        ObCellInfo* cur_cell = NULL;

        bool is_row_changed = false;
        err = get_cell_(cur_iter_arr_[cur_iter_arr_idx_], &cur_cell, &is_row_changed);
        if (OB_SUCCESS != err || NULL == cur_cell) {
          TBSYS_LOG(WARN, "failed to get cell, cur_iter_idx=%ld, err=%d",
                    cur_iter_arr_[cur_iter_arr_idx_], err);
          err = OB_ERROR;
        } else if (is_row_changed) {
          ++cur_iter_arr_idx_;
        } else {
          cur_iter_cell_ = cur_cell;
          iter_row_changed_ = false;
          find_success = true;
        }
      }
    }

    if (OB_SUCCESS == err && !find_success) {
      if (cur_iter_arr_idx_ < cur_iter_arr_num_) {
        // find next iter with the same row key
        ObCellInfo* cur_cell = NULL;
        err = get_cell_(cur_iter_arr_[cur_iter_arr_idx_], &cur_cell, NULL);
        if (OB_SUCCESS != err || NULL == cur_cell) {
          TBSYS_LOG(WARN, "failed to get cell, cur_iter_idx=%ld, err=%d",
                    cur_iter_arr_[cur_iter_arr_idx_], err);
          err = OB_ERROR;
        } else {
          cur_iter_cell_ = cur_cell;
          iter_row_changed_ = false;
          find_success = true;
        }
      } else {
        // not init or iter current row end, find another row
        err = find_next_row_();
        if (OB_ITER_END == err) {
          err = OB_ITER_END;
        } else if (OB_SUCCESS != err) {
          TBSYS_LOG(WARN, "failed to find smallest row, err=%d", err);
        } else {
          iter_row_changed_ = true;
          find_success = true;
        }
      }
    }

    // set iter status
    if (OB_SUCCESS == err && cur_iter_arr_idx_ < cur_iter_arr_num_) {
      iter_status_[cur_iter_arr_[cur_iter_arr_idx_]] = ITER_BEFORE_NEXT;
    }
  }

  return err;
}

int ObMerger :: get_cell(ObCellInfo** cell, bool* is_row_changed) {
  int err = OB_SUCCESS;

  if (NULL == cell) {
    TBSYS_LOG(WARN, "invalid param, cell=%p", cell);
    err = OB_INVALID_ARGUMENT;
  } else {
    *cell = cur_iter_cell_;
    if (NULL != is_row_changed) {
      *is_row_changed = iter_row_changed_;
    }
  }

  return err;
}

int ObMerger :: find_next_row_() {
  int err = OB_SUCCESS;

  int64_t min_max_idx = -1;
  ObCellInfo* min_max_cell = NULL;
  ObCellInfo* cur_cell = NULL;
  int64_t iter_end_num = 0;
  int cmp = 0;
  int64_t last_del_row_idx = -1;

  for (int64_t i = 0; OB_SUCCESS == err && i < iter_num_; ++i) {
    err = next_cell_(i);
    if (OB_ITER_END == err) {
      ++iter_end_num;
      err = OB_SUCCESS;
    } else if (OB_SUCCESS != err) {
      TBSYS_LOG(WARN, "failed to call next_cell of the %ld-th iter, err=%d",
                i, err);
    } else {
      err = get_cell_(i, &cur_cell, NULL);
      if (OB_SUCCESS != err || NULL == cur_cell) {
        TBSYS_LOG(WARN, "failed to get cell of the %ld-th iter, cur_cell=%p, err=%d",
                  i, cur_cell, err);
        err = OB_ERROR;
      } else {
        if (-1 == min_max_idx) {
          min_max_idx = i;
          min_max_cell = cur_cell;

          cur_iter_arr_num_ = 0;
          cur_iter_arr_[cur_iter_arr_num_] = i;
          if (is_del_row_(*cur_cell)) {
            last_del_row_idx = cur_iter_arr_num_;
          }

          ++cur_iter_arr_num_;
        } else {
          int err = cmp_row_key_(min_max_cell, cur_cell, cmp);
          if (OB_SUCCESS != err) {
            TBSYS_LOG(WARN, "failed to cmp_row_key, min_max_cell=%p, cur_cell=%p, err=%d",
                      min_max_cell, cur_cell, err);
          } else if ((cmp > 0 && is_asc_)
                     || (cmp < 0 && !is_asc_)) {
            min_max_cell = cur_cell;
            min_max_idx = i;

            cur_iter_arr_num_ = 0;
            cur_iter_arr_[cur_iter_arr_num_] = i;
            if (is_del_row_(*cur_cell)) {
              last_del_row_idx = cur_iter_arr_num_;
            } else {
              // reset last del row idx
              last_del_row_idx = -1;
            }
            ++cur_iter_arr_num_;
          } else if (0 == cmp) {
            cur_iter_arr_[cur_iter_arr_num_] = i;
            if (is_del_row_(*cur_cell)) {
              last_del_row_idx = cur_iter_arr_num_;
            }
            ++cur_iter_arr_num_;
          }
        }
      }
    }
  }

  if ((OB_SUCCESS == err) && (iter_end_num == iter_num_)) { // all iter ends
    err = OB_ITER_END;
  }

  if (OB_SUCCESS == err) {
    if (-1 == last_del_row_idx) {
      cur_iter_arr_idx_ = 0;
      cur_iter_cell_ = min_max_cell;
    } else {
      for (int64_t i = 0; OB_SUCCESS == err && i < last_del_row_idx; ++i) {
        // pop deleted cell of the current row
        err = pop_current_row_(cur_iter_arr_[i]);
        if (OB_SUCCESS != err) {
          TBSYS_LOG(WARN, "failed to pop current row, i=%ld, idx=%ld", i, cur_iter_arr_[i]);
        }
      }

      if (OB_SUCCESS == err) {
        cur_iter_arr_idx_ = last_del_row_idx;
        err = get_cell_(cur_iter_arr_[cur_iter_arr_idx_], &cur_iter_cell_, NULL);
        if (OB_SUCCESS != err || NULL == cur_iter_cell_) {
          TBSYS_LOG(WARN, "failed to get cell, idx=%ld, err=%d",
                    cur_iter_arr_[cur_iter_arr_idx_], err);
          err = OB_ERROR;
        }
      }
    }
  }

  return err;
}

int ObMerger :: cmp_row_key_(const ObCellInfo* first_cell, const ObCellInfo* second_cell,
                             int& cmp_val) {
  int err = OB_SUCCESS;

  if (NULL == first_cell || NULL == second_cell) {
    TBSYS_LOG(WARN, "invalid param, first_cell=%p, second_cell=%p", first_cell, second_cell);
    err = OB_INVALID_ARGUMENT;
  } else {
    if (first_cell->table_id_ != second_cell->table_id_) {
      cmp_val = first_cell->table_id_ - second_cell->table_id_;
    } else {
      cmp_val = first_cell->row_key_.compare(second_cell->row_key_);
    }
  }

  return err;
}

bool ObMerger :: is_del_row_(const ObCellInfo& cell_info) {
  bool is_del_row = false;
  is_del_row = (cell_info.value_.get_ext() == ObActionFlag::OP_DEL_ROW);
  return is_del_row;
}

int ObMerger :: next_cell_(const int64_t iter_idx) {
  int err = OB_SUCCESS;

  if (iter_idx < 0 || iter_idx >= iter_num_) {
    TBSYS_LOG(WARN, "invalid param, iter_idx=%ld, iter_num_=%ld",
              iter_idx, iter_num_);
    err = OB_INVALID_ARGUMENT;
  } else if (NULL != iters_[iter_idx]) { // double check
    if (ITER_END == iter_status_[iter_idx]) {
      err = OB_ITER_END;
    } else {
      if (ITER_BEFORE_NEXT == iter_status_[iter_idx]) {
        err = iters_[iter_idx]->next_cell();
        if (OB_ITER_END == err) {
          iter_status_[iter_idx] = ITER_END;
        } else if (OB_SUCCESS != err) {
          TBSYS_LOG(WARN, "failed to call next_cell of the %ld-th iter, err=%d",
                    iter_idx, err);
        } else {
          iter_status_[iter_idx] = ITER_AFTER_NEXT;
        }
      }
    }
  }

  return err;
}

int ObMerger :: pop_current_row_(const int64_t iter_idx) {
  int err = OB_SUCCESS;

  if (iter_idx < 0 || iter_idx >= iter_num_) {
    TBSYS_LOG(WARN, "invalid param, iter_idx=%ld, iter_num_=%ld", iter_idx, iter_num_);
    err = OB_INVALID_ARGUMENT;
  } else {
    ObCellInfo* tmp_cell = NULL;
    bool is_row_changed = false;
    iter_status_[iter_idx] = ITER_BEFORE_NEXT;
    err = next_cell_(iter_idx);

    while (OB_SUCCESS == err) {
      err = get_cell_(iter_idx, &tmp_cell, &is_row_changed);
      if (OB_SUCCESS != err || NULL == tmp_cell) {
        TBSYS_LOG(WARN, "failed to call get_cell, iter_idx=%ld, err=%d", iter_idx, err);
        err = OB_ERROR;
      } else if (is_row_changed) {
        break;
      } else {
        iter_status_[iter_idx] = ITER_BEFORE_NEXT;
        err = next_cell_(iter_idx);
      }
    }

    if (OB_ITER_END == err) {
      // wrap error
      err = OB_SUCCESS;
    } else if (OB_SUCCESS != err) {
      TBSYS_LOG(WARN, "failed to pop current row, err=%d", err);
    }
  }

  return err;
}

int ObMerger :: get_cell_(const int64_t iter_idx, ObCellInfo** cell, bool* is_row_changed) {
  int err = OB_SUCCESS;

  if (iter_idx < 0 || iter_idx >= iter_num_ || NULL == cell) {
    TBSYS_LOG(WARN, "invalid param, iter_idx=%ld, iter_num_=%ld, cell=%p",
              iter_idx, iter_num_, cell);
    err = OB_INVALID_ARGUMENT;
  } else if (NULL != iters_[iter_idx]) { // double check
    err = iters_[iter_idx]->get_cell(cell, is_row_changed);
    if (OB_SUCCESS != err || NULL == cell) {
      TBSYS_LOG(WARN, "failed to get_cell of the %ld-th iter, err=%d, cell=%p",
                iter_idx, err, cell);
      err = OB_ERROR;
    }
  }

  return err;
}
}
}


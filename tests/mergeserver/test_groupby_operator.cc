/**
 * (C) 2010-2011 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * Version: $Id$
 *
 * test_groupby_operator.cc for ...
 *
 * Authors:
 *   wushi <wushi.ly@taobao.com>
 *
 */
#include <gtest/gtest.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include "common/ob_cell_array.h"
#include "common/ob_define.h"
#include "common/ob_cache.h"
#include "common/ob_string.h"
#include "common/ob_action_flag.h"
#include "common/ob_groupby.h"
#include "mergeserver/ob_groupby_operator.h"
using namespace sb;
using namespace sb::common;
using namespace sb::mergeserver;
using namespace testing;
using namespace std;


TEST(ObGroupByParam, function) {
  char serialize_buf[2048];
  int64_t buf_len = sizeof(serialize_buf);
  int64_t pos = 0;
  int64_t data_len = 0;
  /// row width 7;
  /// 0, not used int
  /// 1, groupby int
  /// 2, return int
  /// 3, group str
  /// 4, group int
  /// 5, aggregate column int
  /// 6. not used string
  ObCellArray org_cell_array;
  /// row width 6
  /// 0, group int
  /// 1, group str
  /// 2, return int
  /// 3, return str
  /// 4, aggregate sum
  /// 5, aggregate count
  /// ObCellArray agg_cell_array;
  ObGroupByOperator agg_operator;
  /// ObCellArray all_in_one_group_agg_cell_array;
  ObGroupByOperator all_in_one_group_agg_operator;
  ObStringBuf buffer;
  ObGroupByParam param;
  ObGroupByParam all_in_one_group_param;
  ObGroupByParam decoded_param;
  int64_t basic_column_num = 4;
  for (int64_t i = 0; i <= basic_column_num; i++) {
    /// 1, 3
    if (i % 2) {
      EXPECT_EQ(param.add_groupby_column(i), OB_SUCCESS);
    }
  }
  for (int64_t i = 0; i <= basic_column_num; i++) {
    /// 2, 4
    if (i % 2 == 0 && i > 0) {
      EXPECT_EQ(param.add_return_column(i), OB_SUCCESS);
    }
  }
  /// 5
  EXPECT_EQ(param.add_aggregate_column(basic_column_num + 1, SUM), OB_SUCCESS);
  EXPECT_EQ(param.add_aggregate_column(basic_column_num + 1, COUNT), OB_SUCCESS);
  EXPECT_EQ(all_in_one_group_param.add_aggregate_column(basic_column_num + 1, SUM), OB_SUCCESS);
  EXPECT_EQ(all_in_one_group_param.add_aggregate_column(basic_column_num + 1, COUNT), OB_SUCCESS);

  pos = 0;
  EXPECT_EQ(param.serialize(serialize_buf, buf_len, pos), OB_SUCCESS);
  data_len = pos;
  pos = 0;
  EXPECT_EQ(decoded_param.deserialize(serialize_buf, data_len, pos), OB_SUCCESS);
  EXPECT_EQ(pos, data_len);
  EXPECT_TRUE(decoded_param == param);

  EXPECT_EQ(agg_operator.init(param, 1024 * 1024 * 1024), OB_SUCCESS);
  EXPECT_EQ(all_in_one_group_agg_operator.init(all_in_one_group_param, 1024 * 1024 * 1024), OB_SUCCESS);

  int64_t agg_sum = 0;
  int64_t agg_count = 0;
  ObCellInfo cell;
  ObCellInfo* cell_out = NULL;
  ObString str;
  ObObj return_str_obj;
  ObObj groupby_str_obj;
  const int64_t no_int_value = -1;
  const int64_t return_int_value = 0;
  ObObj return_int_obj;
  return_int_obj.set_int(return_int_value);
  const int64_t groupby_int_value = 1;
  ObObj groupby_int_obj;
  groupby_int_obj.set_int(groupby_int_value);

  ObObj agg_sum_obj;
  ObObj agg_count_obj;
  /// prepare first row in org cell array
  /// cell not used, 0
  cell.value_.set_int(no_int_value);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  /// groupby cell, 1
  cell.value_.set_int(groupby_int_value);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  /// return cell, 2
  cell.value_.set_int(return_int_value);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  /// groupby cell, 3
  str.assign(const_cast<char*>("groupby"), strlen("groupby"));
  cell.value_.set_varchar(str);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  groupby_str_obj = cell_out->value_;
  /// return cell, 4
  str.assign(const_cast<char*>("return"), strlen("return"));
  cell.value_.set_varchar(str);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  return_str_obj = cell_out->value_;
  /// aggregate value, 5
  cell.value_.set_int(5);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  agg_count += 1;
  agg_sum += 5;
  /// cell not used, 6
  str.assign(const_cast<char*>("nouse"), strlen("nouse"));
  cell.value_.set_varchar(str);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);

  /// aggregate first cell
  EXPECT_EQ(agg_operator.add_row(org_cell_array, 0, 6), OB_SUCCESS);
  agg_sum_obj.set_int(agg_sum);
  agg_count_obj.set_int(agg_count);
  EXPECT_TRUE(agg_operator[0].value_ == groupby_int_obj);
  EXPECT_TRUE(agg_operator[1].value_ == groupby_str_obj);
  EXPECT_TRUE(agg_operator[2].value_ == return_int_obj);
  EXPECT_TRUE(agg_operator[3].value_ == return_str_obj);
  EXPECT_TRUE(agg_operator[4].value_ == agg_sum_obj);
  EXPECT_TRUE(agg_operator[5].value_ == agg_count_obj);

  EXPECT_EQ(all_in_one_group_agg_operator.add_row(org_cell_array, 0, 6), OB_SUCCESS);
  EXPECT_TRUE(all_in_one_group_agg_operator[0].value_ == agg_sum_obj);
  EXPECT_TRUE(all_in_one_group_agg_operator[1].value_ == agg_count_obj);

  /// prepare second row in org cell, only change return column value
  /// cell not used, 0
  cell.value_.set_int(no_int_value + 5);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  /// groupby cell, 1
  cell.value_.set_int(groupby_int_value);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  /// return cell, 2
  cell.value_.set_int(return_int_value + 6);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  /// groupby cell, 3
  str.assign(const_cast<char*>("groupby"), strlen("groupby"));
  cell.value_.set_varchar(str);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  groupby_str_obj = cell_out->value_;
  /// return cell, 4
  str.assign(const_cast<char*>("return1"), strlen("return1"));
  cell.value_.set_varchar(str);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  /// aggregate value, 5
  cell.value_.set_int(-8);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  agg_count += 1;
  agg_sum += -8;
  /// cell not used, 6
  str.assign(const_cast<char*>("nouse1"), strlen("nouse1"));
  cell.value_.set_varchar(str);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);

  /// aggregate second cell
  EXPECT_EQ(agg_operator.add_row(org_cell_array, 7, 13), OB_SUCCESS);
  agg_sum_obj.set_int(agg_sum);
  agg_count_obj.set_int(agg_count);
  EXPECT_TRUE(agg_operator[0].value_ == groupby_int_obj);
  EXPECT_TRUE(agg_operator[1].value_ == groupby_str_obj);
  EXPECT_TRUE(agg_operator[2].value_ == return_int_obj);
  EXPECT_TRUE(agg_operator[3].value_ == return_str_obj);
  EXPECT_TRUE(agg_operator[4].value_ == agg_sum_obj);
  EXPECT_TRUE(agg_operator[5].value_ == agg_count_obj);

  EXPECT_EQ(all_in_one_group_agg_operator.add_row(org_cell_array, 7, 13), OB_SUCCESS);
  EXPECT_TRUE(all_in_one_group_agg_operator[0].value_ == agg_sum_obj);
  EXPECT_TRUE(all_in_one_group_agg_operator[1].value_ == agg_count_obj);

  /// prepare third org row, change groupby column value
  /// cell not used, 0
  cell.value_.set_int(no_int_value + 5);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  /// groupby cell, 1
  cell.value_.set_int(groupby_int_value);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  /// return cell, 2
  cell.value_.set_int(return_int_value + 6);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  /// groupby cell, 3
  str.assign(const_cast<char*>("groupby1"), strlen("groupby1"));
  cell.value_.set_varchar(str);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  groupby_str_obj = cell_out->value_;
  /// return cell, 4
  str.assign(const_cast<char*>("return1"), strlen("return1"));
  cell.value_.set_varchar(str);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  /// aggregate value, 5
  cell.value_.set_int(-8);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);
  agg_count += 1;
  agg_sum += -8;
  /// cell not used, 6
  str.assign(const_cast<char*>("nouse1"), strlen("nouse1"));
  cell.value_.set_varchar(str);
  EXPECT_EQ(org_cell_array.append(cell, cell_out), OB_SUCCESS);

  /// aggregate third row
  agg_sum_obj.set_int(agg_sum);
  agg_count_obj.set_int(agg_count);
  EXPECT_EQ(all_in_one_group_agg_operator.add_row(org_cell_array, 14, 20), OB_SUCCESS);
  EXPECT_TRUE(all_in_one_group_agg_operator[0].value_ == agg_sum_obj);
  EXPECT_TRUE(all_in_one_group_agg_operator[1].value_ == agg_count_obj);
}

TEST(ObGroupByParam, row_change) {
  ObGroupByParam param;
  ObGroupByOperator groupby_operator;
  int64_t groupby_idx =  0;
  int64_t agg_idx = 1;
  ObAggregateFuncType agg_func = SUM;
  EXPECT_EQ(param.add_groupby_column(groupby_idx), OB_SUCCESS);
  EXPECT_EQ(param.add_aggregate_column(agg_idx, agg_func), OB_SUCCESS);
  ObStringBuf buffer;
  ObString str;
  const char* c_str = NULL;
  ObCellInfo org_cell_info;
  ObCellArray org_cells;
  ObCellInfo*  cell_out;
  ObString table_name;
  c_str = "table";
  str.assign((char*)c_str, strlen(c_str));
  EXPECT_EQ(buffer.write_string(str, &(table_name)), OB_SUCCESS);
  org_cell_info.table_name_ = table_name;

  c_str = "abc";
  str.assign((char*)c_str, strlen(c_str));
  EXPECT_EQ(buffer.write_string(str, &(org_cell_info.row_key_)), OB_SUCCESS);

  int groupby_one_val = 1;
  org_cell_info.value_.set_int(groupby_one_val);
  EXPECT_EQ(org_cells.append(org_cell_info, cell_out), OB_SUCCESS);
  EXPECT_EQ(org_cells.append(org_cell_info, cell_out), OB_SUCCESS);

  int groupby_two_val = 2;
  c_str = "def";
  str.assign((char*)c_str, strlen(c_str));
  EXPECT_EQ(buffer.write_string(str, &(org_cell_info.row_key_)), OB_SUCCESS);
  org_cell_info.value_.set_int(groupby_two_val);
  EXPECT_EQ(org_cells.append(org_cell_info, cell_out), OB_SUCCESS);
  EXPECT_EQ(org_cells.append(org_cell_info, cell_out), OB_SUCCESS);

  EXPECT_EQ(groupby_operator.init(param, 1024 * 1024 * 10), OB_SUCCESS);
  EXPECT_EQ(groupby_operator.add_row(org_cells, 0, 1), OB_SUCCESS);
  EXPECT_EQ(groupby_operator.add_row(org_cells, 2, 3), OB_SUCCESS);

  EXPECT_EQ(groupby_operator.get_cell_size(), 4);

  bool is_row_changed = false;
  EXPECT_EQ(groupby_operator.next_cell(), OB_SUCCESS);
  EXPECT_EQ(groupby_operator.get_cell(&cell_out, &is_row_changed), OB_SUCCESS);
  EXPECT_TRUE(is_row_changed);

  EXPECT_EQ(groupby_operator.next_cell(), OB_SUCCESS);
  EXPECT_EQ(groupby_operator.get_cell(&cell_out, &is_row_changed), OB_SUCCESS);
  EXPECT_FALSE(is_row_changed);

  EXPECT_EQ(groupby_operator.next_cell(), OB_SUCCESS);
  EXPECT_EQ(groupby_operator.get_cell(&cell_out, &is_row_changed), OB_SUCCESS);
  EXPECT_TRUE(is_row_changed);

  EXPECT_EQ(groupby_operator.next_cell(), OB_SUCCESS);
  EXPECT_EQ(groupby_operator.get_cell(&cell_out, &is_row_changed), OB_SUCCESS);
  EXPECT_FALSE(is_row_changed);

  EXPECT_EQ(groupby_operator.next_cell(), OB_ITER_END);
}

int main(int argc, char** argv) {
  srandom(time(NULL));
  ob_init_memory_pool(64 * 1024);
  InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}



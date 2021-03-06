/**
 * (C) 2010-2011 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * Version: $Id$
 *
 * test_scan.cc for ...
 *
 * Authors:
 *   rizhao <rizhao.ych@taobao.com>
 *
 */
#include <iostream>
#include <sstream>
#include <algorithm>
#include <gtest/gtest.h>
#include "tblog.h"
#include "test_helper.h"
#include "test_init.h"
#include "updateserver/ob_ups_table_mgr.h"
#include "test_ups_table_mgr_helper.h"
#include "updateserver/ob_update_server_main.h"

using namespace std;
using namespace sb::common;
using namespace sb::updateserver;


namespace sb {
namespace tests {
namespace updateserver {

class TestScan : public ::testing::Test {
 public:
  virtual void SetUp() {

  }

  virtual void TearDown() {

  }
};

TEST_F(TestScan, test_scan) {
  int err = 0;
  CommonSchemaManagerWrapper schema_mgr;
  tbsys::CConfig config;
  bool ret_val = schema_mgr.parse_from_file("scan_schema.ini", config);
  ASSERT_TRUE(ret_val);
  ObUpsTableMgr& mgr = ObUpdateServerMain::get_instance()->get_update_server().get_table_mgr();
  err = mgr.init();
  ASSERT_EQ(0, err);
  err = mgr.set_schemas(schema_mgr);
  ASSERT_EQ(0, err);
  TestUpsTableMgrHelper test_helper(mgr);

  TableMgr& table_mgr = test_helper.get_table_mgr();
  table_mgr.sstable_scan_finished(10);

  TableItem* active_memtable_item = table_mgr.get_active_memtable();
  MemTable& active_memtable = active_memtable_item->get_memtable();

  // construct multi-row mutator
  static const int64_t ROW_NUM = 10;
  static const int64_t COL_NUM = 1;

  ObCellInfo cell_infos[ROW_NUM][COL_NUM];
  char row_key_strs[ROW_NUM][50];
  uint64_t table_id = 10;
  // init cell infos
  for (int64_t i = 0; i < ROW_NUM; ++i) {
    sprintf(row_key_strs[i], "row_key_%08ld", i);
    for (int64_t j = 0; j < COL_NUM; ++j) {
      cell_infos[i][j].table_id_ = table_id;
      cell_infos[i][j].row_key_.assign(row_key_strs[i], strlen(row_key_strs[i]));
      //cell_infos[i][j].op_info_.set_sem_ob();

      cell_infos[i][j].column_id_ = j + 1;

      cell_infos[i][j].value_.set_int(1000 + i * COL_NUM + j);
    }
  }

  ObUpsMutator ups_mutator;
  ObMutator& mutator = ups_mutator.get_mutator();;
  // add cell to array
  for (int64_t i = 0; i < ROW_NUM; ++i) {
    for (int64_t j = 0; j < COL_NUM; ++j) {
      ObMutatorCellInfo cell_mutator;
      cell_mutator.cell_info = cell_infos[i][j];
      cell_mutator.op_type.set_ext(ObActionFlag::OP_UPDATE);
      //err = mutator.add_cell(cell_infos[i][j]);
      err = mutator.add_cell(cell_mutator);
      EXPECT_EQ(0, err);
    }
  }

  // write row to active memtable
  MemTableTransHandle write_handle;
  err = active_memtable.start_transaction(WRITE_TRANSACTION, write_handle);
  ASSERT_EQ(0, err);
  int64_t mutate_timestamp = 0;
  ups_mutator.set_mutate_timestamp(mutate_timestamp);
  err = active_memtable.set(write_handle, ups_mutator);
  EXPECT_EQ(0, err);
  err = active_memtable.end_transaction(write_handle);
  ASSERT_EQ(0, err);

  // scan memtable
  ObScanner scanner;
  ObScanParam scan_param;
  //ObOperateInfo op_info;
  ObString table_name;
  ObRange range;
  range.start_key_ = cell_infos[0][0].row_key_;
  range.end_key_ = cell_infos[ROW_NUM - 1][0].row_key_;
  range.border_flag_.set_inclusive_start();
  range.border_flag_.set_inclusive_end();

  scan_param.set(table_id, table_name, range);
  //scan_param.set_timestamp(version);
  //scan_param.set_is_read_frozen_only(false);
  ObVersionRange version_range;
  version_range.start_version_ = 2;
  version_range.end_version_ = 2;
  version_range.border_flag_.set_inclusive_start();
  version_range.border_flag_.set_inclusive_end();
  scan_param.set_version_range(version_range);
  for (int64_t i = 0; i < COL_NUM; ++i) {
    scan_param.add_column(cell_infos[0][i].column_id_);
  }

  err = mgr.scan(scan_param, scanner, tbsys::CTimeUtil::getTime(), 2 * 1000L * 1000L);
  EXPECT_EQ(0, err);
  // check result
  int64_t count = 0;
  ObScannerIterator iter;
  for (iter = scanner.begin(); iter != scanner.end(); iter++) {
    ObCellInfo ci;
    ObCellInfo expected = cell_infos[count / COL_NUM][count % COL_NUM];
    EXPECT_EQ(OB_SUCCESS, iter.get_cell(ci));
    check_cell(expected, ci);
    ++count;
  }
  EXPECT_EQ(ROW_NUM * COL_NUM, count);
}

} // end namespace updateserver
} // end namespace tests
} // end namespace sb


int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}




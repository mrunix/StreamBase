/*
 *  (C) 2007-2010 Taobao Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *         ob_disk_path.cc is for what ...
 *
 *  Version: $Id: ob_disk_path.cc 2011��03��22�� 09ʱ54��22�� qushan Exp $
 *
 *  Authors:
 *     qushan < qushan@taobao.com >
 *        - some work details if you want
 */


#include "sstable/ob_disk_path.h"
#include "ob_tablet.h"


using namespace oceanbase::common;
using namespace oceanbase::sstable;

namespace oceanbase {
namespace sstable {
uint64_t get_sstable_disk_no(const uint64_t sstable_file_id) {
  return (sstable_file_id & DISK_NO_MASK);
}

int get_sstable_directory(const int32_t disk_no, char* path, const int64_t path_len) {
  int ret = OB_SUCCESS;
  if (disk_no < 0 || NULL == path || path_len < 0) {
    ret = OB_INVALID_ARGUMENT;
  } else {
    // read from configure file
    snprintf(path, path_len, "%s", "./");
  }
  return ret;
}

int get_sstable_path(const ObSSTableId& sstable_id, char* path, const int64_t path_len) {
  int ret = OB_SUCCESS;
  if (sstable_id.sstable_file_offset_ < 0 || NULL == path || path_len < 0) {
    ret = OB_INVALID_ARGUMENT;
  } else {
    // read from configure file
    snprintf(path, path_len, "%s/%ld", "./", sstable_id.sstable_file_id_);
  }
  return ret;
}

int get_meta_path(const int32_t disk_no, const bool current, char* path, const int32_t path_len) {
  int ret = OB_SUCCESS;
  if (disk_no <= 0 || NULL == path ||  path_len <= 0) {
    ret = OB_INVALID_ARGUMENT;
  }
  if (OB_SUCCESS == ret) {
    snprintf(path, path_len, "idx_%d", disk_no);
  }
  return ret;
}

int get_meta_path(const int64_t version, const int32_t disk_no,
                  const bool current, char* path, const int32_t path_len) {
  int ret = OB_SUCCESS;
  if (disk_no <= 0 || NULL == path ||  path_len <= 0) {
    ret = OB_INVALID_ARGUMENT;
  }
  if (OB_SUCCESS == ret) {
    snprintf(path, path_len, "idx_%ld_%d", version, disk_no);
  }
  return ret;
}
}
}
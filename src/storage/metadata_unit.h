// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef HUSTLE_BLOCK_METADATA_H
#define HUSTLE_BLOCK_METADATA_H

namespace hustle::storage {

class MetadataUnit {
 public:

  /**
   * Get the status of the metadata's construction.
   *
   * @return arrow::Status if the constructor created a problem status.
   * If IsOkay is true, this method should try to return arrow::Status::OK.
   */
  virtual arrow::Status GetStatus() = 0;

  /**
   * Search the underlying metadata segment.
   *
   * @param val_ptr comparison value used for searching
   * @param compare_operator type of search query being performed
   * @return false if the value is guaranteed to not be contained in the block,
   * otherwise true.
   */
  virtual bool Search(const arrow::Datum& val_ptr,
                      arrow::compute::CompareOperator compare_operator) = 0;
};

}  // namespace hustle::storage
#endif  // HUSTLE_BLOCK_METADATA_H

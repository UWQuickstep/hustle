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

#ifndef HUSTLE_HASH_AGGREGATE_H
#define HUSTLE_HASH_AGGREGATE_H

#include "operator.h"
#include "aggregate_const.h"

namespace hustle{
namespace operators{

class HashAggregateStrategy {
public:
  HashAggregateStrategy(int partitions, int chunks);

private:
  int chunks;
  int partitions;

  int suggestedNumTasks();
  static std::tuple<int, int> getChunkID(int tid,
                                         int totalThreads,
                                         int totalNumChunks);
};

class HashAggregate : public Operator {

public:
  HashAggregate(const std::size_t query_id,
            std::shared_ptr<OperatorResult> prev_result,
            std::shared_ptr<OperatorResult> output_result,
            std::vector<AggregateReference> aggregate_units,
            std::vector<ColumnReference> group_by_refs,
            std::vector<ColumnReference> order_by_refs);

  HashAggregate(const std::size_t query_id,
            std::shared_ptr<OperatorResult> prev_result,
            std::shared_ptr<OperatorResult> output_result,
            std::vector<AggregateReference> aggregate_units,
            std::vector<ColumnReference> group_by_refs,
            std::vector<ColumnReference> order_by_refs,
            std::shared_ptr<OperatorOptions> options);

  void execute(Task* ctx) override;

  // TODO: Documentation
  static std::tuple<int, int> getChunkID(int tid,
                                         int totalThreads,
                                         int totalNumChunks);

private:
  // Operator result from an upstream operator and output result will be stored
  std::shared_ptr<OperatorResult> prev_result_, output_result_;
  // The new output table containing the group columns and aggregate columns.
  std::shared_ptr<Table> output_table_;
  // The output result of each aggregate group (length = num_aggs_)
  std::atomic<int64_t>* aggregate_data_;
  // Hold the aggregate column data (in chunks)
  std::vector<const int64_t*> aggregate_col_data_;

  // References denoting which columns we want to perform an aggregate on
  // and which aggregate to perform.
  std::vector<AggregateReference> aggregate_refs_;
  // References denoting which columns we want to group by and order by
  std::vector<ColumnReference> group_by_refs_, order_by_refs_;

  // Map group by column names to the actual group column
  std::vector<arrow::Datum> group_by_cols_;

  arrow::Datum agg_col_;
  // A StructType containing the types of all group by columns
  std::shared_ptr<arrow::DataType> group_type_;
  // We append each aggregate to this after it is computed.
  std::shared_ptr<arrow::ArrayBuilder> aggregate_builder_;
  // We append each group to this after we compute the aggregate for that
  // group.
  std::shared_ptr<arrow::StructBuilder> group_builder_;
  // Construct the group-by key. Initialized in Dynamic Depth Nested Loop.
  std::vector<std::vector<int>> group_id_vec_;

  // Map group-by column name to group_index in the group_by_refs_ table.
  std::unordered_map<std::string, int> group_by_index_map_;
  std::vector<LazyTable> group_by_tables_;
  LazyTable agg_lazy_table_;

  // Two phase hashing requires two types of hash tables.
  // Local hash table that holds the aggregated values for each group.
  std::unordered_map<size_t, int> local_maps;

  // Global hash table handles the second phase of hashing.


  // If a thread wants to insert a group and its aggregate into group_builder_
  // and aggregate_builder_, then it must grab this mutex to ensure that the
  // groups and its aggregates are inserted at the same row.
  std::mutex mutex_;


  void Initialize(Task* internal);

  void ComputeAggregates(Task* internal);

  void Finish();

  /**
   * @return A vector of ArrayBuilders, one for each of the group by columns.
   */
  std::vector<std::shared_ptr<arrow::ArrayBuilder>> CreateGroupBuilderVector();

  /**
   * Construct an ArrayBuilder for the aggregate values.
   *
   * @param kernel The type of aggregate we want to compute
   * @return An ArrayBuilder of the correct type for the aggregate we want
   * to compute.
   */
  std::shared_ptr<arrow::ArrayBuilder> CreateAggregateBuilder(
    AggregateKernel kernel);

  /**
   * Construct the schema for the output table.
   *
   * @param kernel The type of aggregate we want to compute
   * @param agg_col_name The column over which we want to compute the
   * aggregate.
   *
   * @return The schema for the output table.
   */
  std::shared_ptr<arrow::Schema> OutputSchema(AggregateKernel kernel,
                                              const std::string& agg_col_name);


};

}
}



#endif //HUSTLE_HASH_AGGREGATE_H

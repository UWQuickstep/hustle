#include "Select.h"

#include <utility>
#include <arrow/api.h>
#include <arrow/compute/api.h>
#include <table/util.h>
#include <iostream>

#define BLOCK_SIZE 1024

namespace hustle {
namespace operators {

Select::Select(arrow::compute::CompareOperator compare_operator, std::string column_name, arrow::compute::Datum column_value) {
  compare_operator_ = compare_operator;
  column_name_ = std::move(column_name);
  column_value_ = std::move(column_value);
}

// TODO(nicholas): returned blocks are generally NOT full. In the future, we
//   will want to fill the blocks are much as possible.
std::shared_ptr<Block> Select::runOperatorOnBlock(std::shared_ptr<Block>
        input_block) {

    arrow::Status status;

    // TODO(nicholas): Should the function context be created in the inner or
    //  outer loop? I think we only need it in the inner loop if selects are
    //  being executed in parallel
    std::vector<std::shared_ptr<arrow::ArrayData>> out_data;
    auto memory_pool = arrow::default_memory_pool();
    arrow::compute::FunctionContext function_context(memory_pool);
    arrow::compute::CompareOptions compare_options(compare_operator_);
    auto* filter = new arrow::compute::Datum();

    status = arrow::compute::Compare(
            &function_context,
            input_block->get_column_by_name(column_name_),
            column_value_,
            compare_options,
            filter
    );
    evaluate_status(status, __FUNCTION__, __LINE__);

    // Check if selectivity is 0 i.e. no tuples match the select predicate
    if (filter->make_array()->null_count() == filter->length() ) {
        return nullptr;
    }

    auto* out_col = new arrow::compute::Datum();
    for (int j = 0; j < input_block->get_records()->num_columns(); j++) {
        status = arrow::compute::Filter(&function_context,
                input_block->get_column(j),
                                        *filter, out_col);

        evaluate_status(status, __FUNCTION__, __LINE__);
        out_data.push_back(out_col->array());
    }

    // TODO(nicholas): Block constructor should accept vector of ArrayData,
    //  not vector of RecordBatches
    auto out_batch = arrow::RecordBatch::Make(std::make_shared<arrow::Schema>
            (arrow::Schema(*input_block->get_records()->schema())),
            out_data[0]->length, out_data);

    return  std::make_shared<Block>(Block(rand(), out_batch, BLOCK_SIZE));
}

std::vector<std::shared_ptr<Block>> Select::runOperator(std::vector<std::vector<std::shared_ptr<Block>>> block_groups) {
  // operator only uses first block group, ignore others
  auto blocks = block_groups[0];
  //
  arrow::Status status;
  auto out = std::vector<std::shared_ptr<Block>>();
  for (int i = 0; i < blocks.size(); i++) {
      auto block = runOperatorOnBlock(blocks[i]);
      if (block != nullptr){
          out.push_back(block);
      }
  }
  return out;
}

} // namespace operators
} // namespace hustle
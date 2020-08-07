#include <assert.h>
#include <thread>
#include "arrow_compute_wrappers.h"
#include "../table/util.h"

namespace hustle {

void Context::compare(
    Task* ctx,
    const arrow::Datum& left,
    const arrow::Datum& right,
    arrow::compute::CompareOperator compare_operator,
    arrow::Datum* out) {

    clear_data();

    ctx->spawnTask(CreateTaskChain(
        CreateLambdaTask([this, left, right, compare_operator, &out](Task* internal) {
            auto chunked_values = left.chunked_array();

            array_vec_.resize(chunked_values->num_chunks());

            int batch_size = chunked_values->num_chunks() / std::thread::hardware_concurrency() / 20;
            if (batch_size == 0) batch_size = chunked_values->num_chunks();
            int num_batches = chunked_values->num_chunks() / batch_size + 1; // if num_chunks is a multiple of batch_size, we don't actually want the +1
            if (num_batches == 0) num_batches = 1;

            for (int batch_i=0; batch_i<num_batches; batch_i++) {
                internal->spawnLambdaTask([this, batch_i, &out, right, chunked_values, compare_operator, batch_size] {
                    int base_i = batch_i * batch_size;
                    for (int i=base_i; i<base_i+batch_size && i<chunked_values->num_chunks(); i++) {

                        arrow::Status status;
                        arrow::compute::CompareOptions compare_options(compare_operator);
                        arrow::Datum block_values;

                        status = arrow::compute::Compare(chunked_values->chunk(i), right, compare_options).Value(&block_values);
                        evaluate_status(status, __FUNCTION__, __LINE__);

                        array_vec_[i] = block_values.make_array();
                    }
                });
            }
        }),
        CreateLambdaTask([this, left, right, compare_operator, &out](Task* internal) {
//            out->value = std::make_shared<arrow::ChunkedArray>(array_vec_);
            out_.value = std::make_shared<arrow::ChunkedArray>(array_vec_);
        })
    ));
}

arrow::Datum Context::apply_filter_block(
    const std::shared_ptr<arrow::Array>& values,
    const std::shared_ptr<arrow::Array>& filter,
    arrow::ArrayVector& out) {

    arrow::Status status;
    arrow::Datum block_filter;
    //                std::cout << "apply filter" << std::endl;
    //                std::cout << chunked_values->length() << " " << chunked_filter->length() << std::endl;
    //                std::cout << chunked_values->num_chunks() << " " << chunked_filter->num_chunks()<< std::endl;

//    arrow::Datum filter_indices;
//    status = arrow::compute::internal::GetTakeIndices(*filter->data(), arrow::compute::FilterOptions::EMIT_NULL).Value(&filter_indices);
//    evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);
//    status = arrow::compute::Take(values, filter_indices).Value(&block_filter);
//    evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);

    status = arrow::compute::Filter(values, filter).Value(&block_filter);
    evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);
    return block_filter.make_array();
}

void Context::apply_filter_internal(
    Task* ctx,
    const arrow::Datum& values,
    const arrow::Datum& filter,
    arrow::ArrayVector& out) {

    ctx->spawnTask(CreateTaskChain(
        CreateLambdaTask([this, values, filter, &out](Task* internal) {
            const auto& chunked_values = values.chunked_array();
            const auto& chunked_filter = filter.chunked_array();

            out.resize(chunked_values->num_chunks());

            int batch_size = chunked_values->num_chunks() /  std::thread::hardware_concurrency() / 20;
            if (batch_size == 0) batch_size = chunked_values->num_chunks();
            int num_batches = chunked_values->num_chunks() / batch_size + 1; // if num_chunks is a multiple of batch_size, we don't actually want the +1
            if (num_batches == 0) num_batches = 1;

            for (int batch_i=0; batch_i<num_batches; batch_i++) {
                internal->spawnLambdaTask([this, batch_i, &out, chunked_filter, chunked_values, batch_size] {
                    int base_i = batch_i * batch_size;
                    for (int i=base_i; i<base_i+batch_size && i<chunked_values->num_chunks(); i++) {
                        auto block_filter = apply_filter_block(chunked_values->chunk(i), chunked_filter->chunk(i), out);
                        out[i] = block_filter.make_array();
                    }
                });
            }
        })
    ));
}

void Context::apply_filter(
    Task* ctx,
    const arrow::Datum& values,
    const arrow::Datum& filter,
    arrow::Datum& out) {

    ctx->spawnTask(CreateTaskChain(
        CreateLambdaTask([this, values, filter, &out](Task* internal) {
            clear_data();

            arrow::Status status;

            switch(values.kind()) {
                case arrow::Datum::NONE:
                    break;
                case arrow::Datum::ARRAY: {
                    status = arrow::compute::Filter(values, filter.make_array()).Value(&out);
                    break;
                }
                case arrow::Datum::CHUNKED_ARRAY: {

            apply_filter_internal(internal, values, filter, array_vec_);
                    break;
                }
                default: {
                    std::cerr << "Value kind not supported" << std::endl;
                }
            }

            evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);
        }),
        CreateLambdaTask([this, values, filter, &out](Task* internal) {
            if (values.kind() == arrow::Datum::CHUNKED_ARRAY) {
                out.value = std::make_shared<arrow::ChunkedArray>(array_vec_);
            }
        })
    ));

}

void apply_filter(
    const arrow::Datum& values,
    const arrow::Datum& filter,
    arrow::Datum* out) {

    arrow::Status status;
    switch(values.kind()) {
        case arrow::Datum::NONE:
            break;
        case arrow::Datum::ARRAY: {
            status = arrow::compute::Filter(values,filter.make_array()).Value(out);
            break;
        }
        case arrow::Datum::CHUNKED_ARRAY: {
            status = arrow::compute::Filter(values,filter.chunked_array()).Value(out);
            break;
        }
        default: {
            std::cerr << "Value kind not supported" << std::endl;
        }
    }
    evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);
}

void apply_indices(
    const arrow::Datum& values,
    const arrow::Datum& indices,
    arrow::Datum* out) {

    arrow::Status status;
    arrow::compute::TakeOptions take_options;

    switch(values.kind()) {
        case arrow::Datum::NONE:
            break;
        case arrow::Datum::ARRAY:
            status = arrow::compute::Take(values.make_array(), indices.make_array()).Value(out);
            break;
        case arrow::Datum::CHUNKED_ARRAY: {
            std::shared_ptr<arrow::ChunkedArray> temp_chunked_array;
            status = arrow::compute::Take(
                *values.chunked_array(), *indices.make_array()).Value(&temp_chunked_array);
            out->value = temp_chunked_array;
            break;
        }
        default:
            std::cerr << "Value kind not supported" << std::endl;
    }
    evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);
}

Context::Context() {
    slice_length_ = 30000;
}

void Context::apply_indices_internal(
    const std::shared_ptr<arrow::ChunkedArray>& chunked_values,
    const std::shared_ptr<arrow::Array>& indices_array,
    const std::shared_ptr<arrow::Array>& offsets,
    int i) {

    int num_slices = indices_array->length()/slice_length_ + 1;

    std::shared_ptr<arrow::Array> sliced_indices;
    if (i == num_slices-1) sliced_indices = indices_array->Slice(i*slice_length_, indices_array->length() - (i-1)*slice_length_);
    else sliced_indices = indices_array->Slice(i*slice_length_, slice_length_);

    if (sliced_indices->length() == 0) {
        array_vec_[i] = arrow::MakeArrayOfNull(chunked_values->type(), 0, arrow::default_memory_pool()).ValueOrDie();
        return;
    }
    arrow::Datum temp;
    arrow::Status status;

    // Assume that indices are correct and that boundschecking is unecessary.
    // CHANGE TO TRUE IF YOU ARE DEBUGGING
    arrow::compute::TakeOptions take_options(true);

    status = arrow::compute::Take(chunked_values, sliced_indices, offsets, take_options).Value(&temp);

    evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);
    array_vec_[i] = temp.chunked_array()->chunk(0);
}

template<typename T>
void Context::apply_indices_internal(
    const std::shared_ptr<arrow::ChunkedArray>& chunked_values,
    const T ** values_data_vec,
    const std::shared_ptr<arrow::Array>& indices_array,
    const std::shared_ptr<arrow::Array>& offsets,
    int slice_i) {

    int num_slices = indices_array->length()/slice_length_ + 1;

    std::shared_ptr<arrow::Array> sliced_indices;
    if (slice_i == num_slices-1) sliced_indices = indices_array->Slice(slice_i*slice_length_, indices_array->length() - (slice_i-1)*slice_length_);
    else sliced_indices = indices_array->Slice(slice_i*slice_length_, slice_length_);

    if (sliced_indices->length() == 0) {
        array_vec_[slice_i] = arrow::MakeArrayOfNull(chunked_values->type(), 0, arrow::default_memory_pool()).ValueOrDie();
        return;
    }

//    const T * values_data_vec[chunked_values->num_chunks()];
//    for (int i=0; i<chunked_values->num_chunks(); ++i) {
//        values_data_vec[i] = chunked_values->chunk(i)->data()->GetValues<T>(1);
//    }

//    T** values_data_vec = (T**) values_data_vec_;

    auto offsets_data = offsets->data()->GetValues<int64_t>(1, 0);
    auto offsets_data_end = offsets_data + offsets->length();

    auto sliced_indices_data = sliced_indices->data()->GetValues<uint32_t>(1);

    std::shared_ptr<arrow::Buffer> out_buffer;
    auto status = arrow::AllocateBuffer(sliced_indices->length()*sizeof(int64_t)).Value(&out_buffer);
    evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);

    //@TODO: this forces the output to be INT64!
    auto out_data = arrow::ArrayData::Make(arrow::int64(), sliced_indices->length(), {nullptr, out_buffer});
    auto out = out_data->GetMutableValues<int64_t>(1);

    for (uint32_t i=0; i<sliced_indices->length(); ++i) {
        auto index = sliced_indices_data[i];
        // Find the chunk to which index belongs
        auto chunk_j = (std::upper_bound(offsets_data, offsets_data_end, index) - offsets_data) - 1;
        out[i] = values_data_vec[chunk_j][index - offsets_data[chunk_j]];
    }

    array_vec_[slice_i] = arrow::MakeArray(out_data);

}

void Context::apply_indices_internal_str(
    const std::shared_ptr<arrow::ChunkedArray>& chunked_values,
    const std::shared_ptr<arrow::Array>& indices_array,
    const std::shared_ptr<arrow::Array>& offsets,
    int slice_i) {

    int num_slices = indices_array->length()/slice_length_ + 1;

    std::shared_ptr<arrow::Array> sliced_indices;
    if (slice_i == num_slices-1) sliced_indices = indices_array->Slice(slice_i*slice_length_, indices_array->length() - (slice_i-1)*slice_length_);
    else sliced_indices = indices_array->Slice(slice_i*slice_length_, slice_length_);

    if (sliced_indices->length() == 0) {
        array_vec_[slice_i] = arrow::MakeArrayOfNull(chunked_values->type(), 0, arrow::default_memory_pool()).ValueOrDie();
        return;
    }

    int num_chunks = chunked_values->num_chunks();

    const uint8_t* values_data_vec[num_chunks];
    const int32_t * values_offset_vec[num_chunks];

    int64_t num_bytes = 0;
    std::shared_ptr<arrow::Array> chunk;
    for (int i=0; i<num_chunks; ++i) {
        chunk = chunked_values->chunk(i);
        values_data_vec[i] = chunk->data()->GetValues<uint8_t >(2, 0);
        values_offset_vec[i] = chunk->data()->GetValues<int32_t>(1, 0);
        num_bytes += values_offset_vec[i][chunk->length()];
    }
    auto sliced_indices_data = sliced_indices->data()->GetValues<uint32_t>(1);

    arrow::TypedBufferBuilder<uint32_t> offset_builder;
    arrow::TypedBufferBuilder<uint8_t> data_builder;

    auto output_length = sliced_indices->length();

    auto chunk_offsets = offsets->data()->GetValues<int64_t>(1);
    auto chunk_offsets_end = chunk_offsets + offsets->length();
    // Presize the data builder with a rough estimate of the required data size
    if (chunked_values->length() > 0) {
        const double mean_value_length =
            num_bytes / static_cast<double>(chunked_values->length());

        // TODO: See if possible to reduce output_length for take/filter cases
        // where there are nulls in the selection array
        auto status =  data_builder.Reserve(static_cast<int64_t>(mean_value_length * output_length));
        evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);

    }

    int64_t space_available = data_builder.capacity();
    int32_t offset = 0;

    for (uint32_t i=0; i<output_length; ++i) {
        auto index = sliced_indices_data[i];
        auto status = offset_builder.Append(offset);
        evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);

        auto chunk_i = std::upper_bound(chunk_offsets, chunk_offsets_end, index) - chunk_offsets - 1;
        const int32_t *raw_offsets = values_offset_vec[chunk_i];
        const uint8_t *raw_data = values_data_vec[chunk_i];

        index -= chunk_offsets[chunk_i];
        uint32_t val_offset = raw_offsets[index];
        uint32_t val_size = raw_offsets[index + 1] - val_offset;

        offset += val_size;
        if (ARROW_PREDICT_FALSE(val_size > space_available)) {
            auto status = data_builder.Reserve(val_size);
            evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);
            space_available = data_builder.capacity() - data_builder.length();
        }
        data_builder.UnsafeAppend(raw_data + val_offset, val_size);
        space_available -= val_size;
    }
    offset_builder.UnsafeAppend(offset); // append final offset (number of bytes in str array)

    std::shared_ptr<arrow::Buffer> data_buffer;
    std::shared_ptr<arrow::Buffer> offset_buffer;

    auto status = data_builder.Finish(&data_buffer);
    evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);

    status = offset_builder.Finish(&offset_buffer);
    evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);

    array_vec_[slice_i] = arrow::MakeArray(arrow::ArrayData::Make(arrow::utf8(),output_length,{nullptr, offset_buffer, data_buffer}));

}

void Context::apply_indices(
    Task* ctx,
    const arrow::Datum values,
    const arrow::Datum indices,
    bool has_sorted_indices,
    arrow::Datum& out) {

    clear_data();
    ctx->spawnTask(CreateTaskChain(
        CreateLambdaTask([this, values, indices, has_sorted_indices, &out](Task* internal)  {

            arrow::Status status;
            std::shared_ptr<arrow::ChunkedArray> chunked_values;

            switch(values.kind()){
                case arrow::Datum::ARRAY:
                    chunked_values = std::make_shared<arrow::ChunkedArray>(values.make_array());
                    break;
                case arrow::Datum::CHUNKED_ARRAY:
                    chunked_values = values.chunked_array();
                    break;
                default: {
                    std::cerr << "Unexpected values kind" << std::endl;
                }
            }
            std::shared_ptr<arrow::Array> indices_array;

            if (indices.kind() == arrow::Datum::CHUNKED_ARRAY) {
                assert(indices.chunked_array()->num_chunks() == 1);
                indices_array = indices.chunked_array()->chunk(0);
            }
            else {
                indices_array = indices.make_array();
            }
            auto indices_data = indices_array->data()->GetMutableValues<int32_t>(1, 0);

            int num_chunks = chunked_values->num_chunks();
            std::vector<unsigned int> chunk_row_offsets;
            chunk_row_offsets.resize(num_chunks+1);
            chunk_row_offsets[0] = 0;
            for (int i = 1; i < num_chunks; i++) {
                chunk_row_offsets[i] =
                    chunk_row_offsets[i - 1] + chunked_values->chunk(i - 1)->length();
            }
            chunk_row_offsets[num_chunks] = chunk_row_offsets[num_chunks-1] +
                                            chunked_values->chunk(num_chunks-1)->length();

            arrow::Int64Builder b;
            b.AppendValues(chunk_row_offsets.begin(), chunk_row_offsets.end());
            std::shared_ptr<arrow::Array> offsets;
            b.Finish(&offsets);

            int num_slices = indices_array->length()/slice_length_ + 1;

            array_vec_.resize(num_slices);

            for (int i=0; i<num_slices; i++) {
                internal->spawnLambdaTask([this, indices_array, chunked_values, offsets, i]{
//                    apply_indices_internal(chunked_values, indices_array, offsets, i);
                    switch(chunked_values->type()->id()) {
                        case arrow::Type::INT64: {
                            std::vector<const int64_t *> values_data_vec(chunked_values->num_chunks());
                            for (int i=0; i<chunked_values->num_chunks(); ++i) {
                                values_data_vec[i] = chunked_values->chunk(i)->data()->GetValues<int64_t>(1);
                            }
                            apply_indices_internal<int64_t>(chunked_values, values_data_vec.data(), indices_array, offsets, i);
                            break;
                        }
                        case arrow::Type::UINT32: {
                            std::vector<const uint32_t *> values_data_vec(chunked_values->num_chunks());
                            for (int i=0; i<chunked_values->num_chunks(); ++i) {
                                values_data_vec[i] = chunked_values->chunk(i)->data()->GetValues<uint32_t>(1);
                            }
                            apply_indices_internal<uint32_t>(chunked_values, values_data_vec.data(), indices_array, offsets, i);
                            break;
                        }
                        case arrow::Type::STRING: {
//                            apply_indices_internal_str(chunked_values, indices_array, offsets, i);
                            apply_indices_internal(chunked_values, indices_array, offsets, i);
                            break;
                        }
                    }
                });
            }
        }),
        CreateLambdaTask([this, values, indices, has_sorted_indices, &out](Task* internal)  {

            arrow::Status status;
            std::shared_ptr<arrow::Array> arr;
            out.value = std::make_shared<arrow::ChunkedArray>(array_vec_);
            out_ = std::make_shared<arrow::ChunkedArray>(array_vec_);

        })
    ));
}

void Context::clear_data() {
    array_vec_.clear();

}


void sort_to_indices(const arrow::Datum& values, arrow::Datum* out) {

    arrow::Status status;

    std::shared_ptr<arrow::Array> temp;
    if (values.kind() == arrow::Datum::CHUNKED_ARRAY) {
        std::shared_ptr<arrow::Array> combined_chunks;
        status = arrow::Concatenate(values.chunked_array()->chunks(),
                                    arrow::default_memory_pool(),
                                    &combined_chunks);
        evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);

        status = arrow::compute::SortToIndices(*combined_chunks).Value(&temp);
    } else {
        status = arrow::compute::SortToIndices(*values.make_array().get()).Value(&temp);
        evaluate_status(status, __PRETTY_FUNCTION__, __LINE__);
    }
    out->value = temp->data();
}

void sort_datum(const arrow::Datum& values, arrow::Datum* out) {

    arrow::Datum sorted_indices;

    sort_to_indices(values, &sorted_indices);
    apply_indices(values, sorted_indices, out);
}


void compare(
    const arrow::Datum& left,
    const arrow::Datum& right,
    arrow::compute::CompareOperator compare_operator,
    arrow::Datum* out) {

    arrow::Status status;
    arrow::compute::CompareOptions compare_options(compare_operator);
    status = arrow::compute::Compare(left, right, compare_options).Value(out);
    evaluate_status(status, __FUNCTION__, __LINE__);
}

void unique(const arrow::Datum& values, arrow::Datum* out) {

    arrow::Status status;
    std::shared_ptr<arrow::Array> unique_values;

    std::shared_ptr<arrow::Array> temp_array;
    // Fetch the unique values in group_by_col
    status = arrow::compute::Unique(values).Value(&temp_array);
    evaluate_status(status, __FUNCTION__, __LINE__);


    out->value = temp_array->data();
}


void Context::match(
    Task* ctx,
    const arrow::Datum& values,
    const arrow::Datum& keys,
    arrow::Datum& out) {

    ctx->spawnTask(CreateTaskChain(
        CreateLambdaTask([this, values, keys, &out](Task* internal) {
            clear_data();

            auto vals = values.chunked_array();
            array_vec_.resize(values.chunked_array()->num_chunks());


            for (int j=0; j<vals->num_chunks(); ++j) {
                internal->spawnLambdaTask([this, j, vals, keys]{
                    arrow::Status status;
                    arrow::Datum temp;
                    status = arrow::compute::Match(vals->chunk(j), keys).Value(&temp);
                    evaluate_status(status, __FUNCTION__, __LINE__);

                    array_vec_[j] = temp.make_array();
                });
            }
        }),
        CreateLambdaTask([this, &out](Task* internal) {
//            out.value = std::make_shared<arrow::ChunkedArray>(array_vec_);
            out_.value = std::make_shared<arrow::ChunkedArray>(array_vec_);
        })
    ));

}



}
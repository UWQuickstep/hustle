#ifndef HUSTLE_JOIN_H
#define HUSTLE_JOIN_H

#include <string>
#include <table/block.h>
#include <table/table.h>
#include <arrow/compute/api.h>
#include <utils/parallel_hashmap/phmap.h>
#include <utils/BloomFilter.h>
#include "OperatorResult.h"
#include "JoinGraph.h"
#include "Operator.h"

namespace hustle::operators {

/**
 * The Join operator updates the index arrays of each LazyTable in the inputted
 * OperatorResults. After execution, the index arrays of each LazyTable contains
 * only the indices of rows that join with all other LazyTables this LazyTable
 * was joined with. Filters are unchanged.
 *
 * See slides 18-27 for an in-depth example:
 * https://docs.google.com/presentation/d/1KlNdwwTy5k-cwlRwY_hRg-AQ9dt3mh_k_MVuIsmyQbQ/edit#slide=id.p
 */
class Join : public Operator {
public:

    /**
     * Construct an Join operator to perform joins on two or more tables.
     *
     * @param prev_result OperatorResult form an upstream operator.
     * @param graph A graph specifying all join predicates
     */
    Join(const std::size_t query_id,
         std::vector<std::shared_ptr<OperatorResult>> prev_result,
         std::shared_ptr<OperatorResult> output_result,
         JoinGraph graph);

    /**
    * Perform a natural join on two tables using hash join.
    *
    * @return An OperatorResult containing the same LazyTables passed in as
     * prev_result, but now their index arrays are updated, i.e. all indices
     * that did not satisfy all join predicates specificed in the join graph
     * are not included.
    */
    void execute(Task *ctx) override;

private:

    // lefts[i] = the left table in the ith join
    std::vector<LazyTable> lefts;
    // rights[i] = the right table in the ith join
    std::vector<LazyTable> rights;
    // left_col_names[i] = the left join col name in the ith join
    std::vector<std::string> left_col_names;
    // right_col_names[i] = the right join col name in the ith join
    std::vector<std::string> right_col_names;

    // Results from upstream operators
    std::vector<std::shared_ptr<OperatorResult>> prev_result_vec_;
    // Results from upstream operators condensed into one object
    std::shared_ptr<OperatorResult> prev_result_;
    // Where the output result will be stored once the operator is executed.
    std::shared_ptr<OperatorResult> output_result_;
    std::shared_ptr<OperatorResult> output_result_lip_;


    // A graph specifying all join predicates
    JoinGraph graph_;

    // Hash table for the right table in each join
    phmap::flat_hash_map<int64_t, uint32_t> hash_table_;
//    std::unordered_map<int64_t, uint64_t> hash_table_;

    // new_left_indices_vector[i] = the indices of rows joined in chunk i in
    // the left table
    std::vector<std::vector<uint32_t>> new_left_indices_vector;
    // new_right_indices_vector[i] = the indices of rows joined in chunk i in
    // the right table
    std::vector<std::vector<uint32_t>> new_right_indices_vector;

    // It would make much more sense to use an ArrayVector instead of a vector of
    // vectors, since we can make a ChunkedArray out from an ArrayVector. But
    // Arrow's Take function is very inefficient when the indices are in a
    // ChunkedArray. So, we instead use a vector of vectors to store the indices,
    // and then construct an Array from the vector of vectors.

    // joined_indices[0] = new_left_indices_vector stored as an Array
    // joined_indices[1] = new_right_indices_vector stored as an Array
    std::vector<arrow::Datum> joined_indices_;

    arrow::Datum left_join_col_;
    arrow::Datum right_join_col_;

    LazyTable left_;
    LazyTable right_;

    std::unordered_map<std::shared_ptr<Table>, bool> finished_;


    /**
     * Build a hash table on a column. It is assumed that the column will be
     * of INT64 type.
     *
     * @param col the column
     * @return a hash table mapping key values to their index location in the
     * table.
     */
//    void build_hash_table
//        (const std::shared_ptr<arrow::ChunkedArray> &col, Task *ctx);

    /**
     * Perform the probe phase of hash join.
     *
     * @param probe_col The column we want to probe into the hash table
     * @return A pair of index arrays corresponding to rows of the left table
     * that join with rows of the right table.
     */
//    void probe_hash_table
//        (const std::shared_ptr<arrow::ChunkedArray> &probe_col, Task *ctx);

    /**
     * Perform a single hash join.
     *
     * @return An OperatorResult containing the same LazyTables passed in as
     * prev_result, but now their index arrays are updated, i.e. all indices
     * that did not satisfy the join predicate are not included.
     */
    void hash_join(int i, Task *ctx);

    /**
     * After performing a single join, we must eliminate rows from other
     * LazyTables in prev_result that do are not included in the join result.
     *
     * @param joined_indices A pair of index arrays corresponding to rows of
     * the left table that join with rows of the right table.
     * @return An OperatorResult containing the same LazyTables passed in as
     * prev_result, but now their index arrays are updated, i.e. all indices
     * that did not satisfy the join predicate are not included.
     *
     * e.g. Suppose we are join R with S and rows [0, 1] or R join with rows
     * [2, 3] of S. Further suppose that rows [3] of S join with rows [4] of T.
     * At this point, we know we can exclude one of the rows from the first join
     * on R and S. back_propogate_result() would produce the following index
     * arrays:
     *
     * R S T
     * 1 3 4
     *
     */
    std::shared_ptr<OperatorResult> back_propogate_result
        (const LazyTable& left, LazyTable right,
         const std::vector<arrow::Datum>& joined_indices);

    /**
     * probe_hash_table() populates new_left_indices_vector
     * and new_right_indices_vector. This function converts these into Arrow
     * Arrays.
     */
    void finish_probe(Task* ctx);

    /*
     * Create the output result from the raw data computed during execution.
     */
    void finish();

//    void probe_hash_table_block(const std::shared_ptr<arrow::ChunkedArray> &probe_col, int batch_i, int batch_size,
//                                std::vector<uint64_t> chunk_row_offsets);

    void probe_hash_table_block(const std::shared_ptr<arrow::ChunkedArray> &probe_col,
                                const std::shared_ptr<arrow::ChunkedArray> &probe_filter, int batch_i, int batch_size,
                                std::vector<uint64_t> chunk_row_offsets);

    void probe_hash_table_block_indices(const std::shared_ptr<arrow::ChunkedArray> &probe_col,
                                const std::shared_ptr<arrow::ChunkedArray> &probe_indices, int batch_i, int batch_size,
                                std::vector<uint64_t> chunk_row_offsets);

    void
    build_hash_table(const std::shared_ptr<arrow::ChunkedArray> &col,
                     const std::shared_ptr<arrow::ChunkedArray> &filter,
                     Task *ctx);

    void probe_hash_table_block(const std::shared_ptr<arrow::ChunkedArray> &probe_col, int batch_i, int batch_size,
                                std::vector<uint64_t> chunk_row_offsets);


    void probe_hash_table(const std::shared_ptr<arrow::ChunkedArray> &probe_col, const arrow::Datum &probe_filter,
                          const arrow::Datum &probe_indices, Task *ctx);

    std::shared_ptr<OperatorResult>
    back_propogate_result2(const LazyTable &left, LazyTable right, const std::vector<arrow::Datum> &joined_indices);

    void build_filters(Task *ctx);







    // Row indices of the fact table that successfully probed all Bloom filters.
    std::vector<uint32_t*> lip_indices_raw_;
    std::vector<std::vector<uint32_t>> lip_indices_;
    const uint32_t* fact_indices_;

    // Number of blocks that are probed (in parallel) before sorting the the filters
    int batch_size_;

    // chunk_row_offsets_[i] = the row index at which the ith block of the fact
    // table starts
    std::vector<int64_t> chunk_row_offsets_;

    // Map of (fact table foreign key col name, fact table foreign key col)
    std::unordered_map<std::string, arrow::Datum> fact_fk_cols_;
    std::unordered_map<std::string, arrow::Datum> dim_pk_cols_;

    std::unordered_map<std::string, std::shared_ptr<arrow::ChunkedArray>> fact_fk_cols2_;


    // Primary key cols of all dimension tables.

    // Bloom filters of all dimension tables.
    std::vector<std::shared_ptr<BloomFilter>> dim_filters_;

    std::vector<std::shared_ptr<arrow::ChunkedArray>> dim_col_filters_;
    std::vector<std::shared_ptr<arrow::ChunkedArray>> fact_col_filters_;

    // Dimension (lazy) tables
    std::vector<LazyTable> dim_tables_;
    // Dimension primary key col names
    std::vector<std::string> dim_pk_col_names_;
    // Total number of chunks in each dimension table.
    std::vector<int> dim_join_col_num_chunks_;

    LazyTable fact_table_;
    // Fact table foreign key col names to probe Bloom filters.
    std::vector<std::string> fact_fk_col_names_;

    void probe_filters(int chunk_start, int chunk_end, int filter_j, Task *ctx);

    void probe_filters(Task *ctx);

    void initialize(Task *ctx);

    void finish_lip();
};

} // namespace hustle

#endif //HUSTLE_JOIN_H
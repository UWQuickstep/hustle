#ifndef HUSTLE_JOIN_H
#define HUSTLE_JOIN_H

#include <string>
#include <table/block.h>
#include <table/table.h>
#include <arrow/compute/api.h>

#include "Operator.h"

namespace hustle {
namespace operators {


class Join : public Operator{
public:

    // TODO(nicholas): Joinable iterface?

    // TODO(nicholas): Join should accept ColumnReferences instead of
    //  tables and column names. This is more consistent with the other
    //  operators, and it prevents anny issues arising when tables hav
    //  columns with the same name.
    /**
     * Construct a Join operator to perform a natural join using hash join.
     *
     * @param left_table outer table (i.e. we probe values from this table)
     * @param left_selection If a selection was previously performed on
     * left_table,
     * @param left_column_name name of the join column in left_table
     * @param right_table inner table (i.e. we build a hash table on this table)
     * @param right_selection
     * @param right_column_name name of the join column in the right table
     */
    Join(ColumnReference left,
               arrow::compute::Datum& left_selection,
               ColumnReference right,
               arrow::compute::Datum& right_selection);

    Join(std::vector<JoinResult>& left_join_result,
               ColumnReference left,
               ColumnReference right,
               arrow::compute::Datum& right_selection);

//TODO(nicholas): Should these be implemented?

//    Join(const std::shared_ptr<Table>& left_table,
//         const std::string& left_column_name,
//         const std::shared_ptr<Table>& right_table,
//         const std::string& right_column_name);
//
//    Join(const std::shared_ptr<Table>& left_table,
//         const std::string& left_column_name,
//         const arrow::compute::Datum& left_selection,
//         const std::shared_ptr<Table>& right_table,
//         const std::string& right_column_name);
//
//    Join(const std::shared_ptr<Table>& left_table,
//         const std::string& left_column_name,
//         const std::shared_ptr<Table>& right_table,
//         const std::string& right_column_name,
//         const arrow::compute::Datum& right_selection);

    /**
    * Perform a natural join on two tables using hash join. Projections are not
    * yet supported; all columns from both tables will be returned in the
    * resulting table (excluding the duplicate join column).
    *
    * @param left_table The table that will probe the hash table
    * @param right_table The table for which a hash table is built
    * @return A new table containing the results of the join
    */
    std::vector<JoinResult> hash_join();

private:
    arrow::compute::Datum left_filter_;
    arrow::compute::Datum right_filter_;

    std::shared_ptr<arrow::ChunkedArray> left_join_col_;
    std::shared_ptr<arrow::ChunkedArray> right_join_col_;

    arrow::compute::Datum left_selection_;
    arrow::compute::Datum right_selection_;

    //TODO(nicholas): a better name?
    std::vector<JoinResult> left_join_result_;
    std::vector<JoinResult> right_join_result_;

    std::shared_ptr<Table> left_table_;
    std::shared_ptr<Table> right_table_;

    std::string left_join_col_name_;
    std::string right_join_col_name_;

    std::shared_ptr<arrow::Array> left_indices_;
    std::shared_ptr<arrow::Array> right_indices_;

    std::unordered_map<int64_t, int64_t> hash_table_;

    std::vector<JoinResult> hash_join(
            std::vector<JoinResult>&,
            const std::shared_ptr<Table>& right_table);
    std::vector<JoinResult> hash_join(
            const std::shared_ptr<Table>& left_table,
            const std::shared_ptr<Table>& right_table);

    std::vector<JoinResult> probe_hash_table
        (std::shared_ptr<arrow::ChunkedArray> probe_col);
        std::vector<JoinResult> probe_hash_table_2
                (std::shared_ptr<arrow::ChunkedArray> probe_col);

    arrow::compute::Datum get_left_indices();
    arrow::compute::Datum get_right_indices();
    arrow::compute::Datum get_indices_for_table(
            const std::shared_ptr<Table> &other);

    std::unordered_map<int64_t, int64_t> build_hash_table
            (std::shared_ptr<arrow::ChunkedArray> col);
    std::shared_ptr<arrow::ChunkedArray> apply_selection
            (std::shared_ptr<arrow::ChunkedArray> col, arrow::compute::Datum
            selection);

};

} // namespace operators
} // namespace hustle

#endif //HUSTLE_JOIN_H

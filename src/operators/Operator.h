#ifndef HUSTLE_OPERATOR_H
#define HUSTLE_OPERATOR_H

#include <string>
#include <table/block.h>
#include <table/table.h>
#include <arrow/compute/api.h>

namespace hustle {
namespace operators {

enum FilterOperator {
    AND,
    OR,
    NONE,
};


enum ResultType {
    SELECT,
    JOIN,
    AGGREGATE,
    PROJECT,
};


    class LazyTable {

    public:

        LazyTable();
        LazyTable(
                std::shared_ptr<Table> table,
                arrow::compute::Datum filter,
                arrow::compute::Datum indices
        );
        std::shared_ptr<arrow::ChunkedArray> get_column(int i);
        std::shared_ptr<arrow::ChunkedArray> get_column_by_name(std::string
        col_name);


        // TODO(nicholas): Combine into a ColumnReference?
        std::shared_ptr<Table> table;
        arrow::compute::Datum filter; // filters are ChunkedArrays
        arrow::compute::Datum indices; // indicess are Arrays

    private:

    };

class OperatorResult {
public:
    OperatorResult();
    OperatorResult(std::vector<LazyTable> units);
    void append(std::shared_ptr<Table> table);
    void append(std::shared_ptr<OperatorResult> result);
    LazyTable get_table(int i);
    LazyTable get_table(std::shared_ptr<Table> table);
//    void append(LazyTable lazy_table);
        std::vector<LazyTable> lazy_tables_;
};

struct ColumnReference {
    std::shared_ptr<Table> table;
    std::string col_name;
};

struct GroupReference {
    std::shared_ptr<Table> table;
    std::vector<std::string> col_names;
    std::vector<int> col_indices;
};

class Operator {
    virtual std::shared_ptr<OperatorResult> run() = 0;
public:

};




} // namespace operators
} // namespace hustle

#endif //HUSTLE_OPERATOR_H

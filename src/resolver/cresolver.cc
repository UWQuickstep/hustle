#include "resolver/cresolver.h"

#include <iostream>

#include "api/hustle_db.h"
#include "catalog/catalog.h"
#include "execution/execution_plan.h"
#include "operators/select.h"
#include "operators/utils/operator_result.h"
#include "resolver/select_resolver.h"

std::shared_ptr<hustle::storage::DBTable> execute(
    std::shared_ptr<hustle::ExecutionPlan> plan,
    hustle::resolver::SelectResolver* select_resolver, Catalog* catalog);

std::shared_ptr<hustle::ExecutionPlan> createPlan(
    hustle::resolver::SelectResolver* select_resolver, Catalog* catalog);

int resolveSelect(char* dbName, Sqlite3Select* queryTree) {
  ExprList* pEList = queryTree->pEList;
  Expr* pWhere = queryTree->pWhere;
  ExprList* pGroupBy = queryTree->pGroupBy;
  Expr* pHaving = queryTree->pHaving;
  ExprList* pOrderBy = queryTree->pOrderBy;

  // TODO: (@srsuryadev) resolve the select query
  // return 0 if query is supported in column store else return 1
  using hustle::resolver::SelectResolver;
  Catalog* catalog = hustle::HustleDB::getCatalog(dbName).get();
  if (dbName == NULL || catalog == nullptr) return 0;

  SelectResolver* select_resolver = new SelectResolver(catalog);
  bool is_resolvable = select_resolver->ResolveSelectTree(queryTree);
  if (is_resolvable) {
    std::shared_ptr<hustle::ExecutionPlan> plan =
        createPlan(select_resolver, catalog);
    if (plan != nullptr) {
      std::shared_ptr<hustle::storage::DBTable> outTable =
          execute(plan, select_resolver, catalog);
    } else {
      return 0;
    }
  }
  delete select_resolver;
  return is_resolvable ? 1 : 0;
}

std::shared_ptr<hustle::ExecutionPlan> createPlan(
    hustle::resolver::SelectResolver* select_resolver, Catalog* catalog) {
  using namespace hustle::operators;
  std::unordered_map<std::string, std::shared_ptr<PredicateTree>>
      select_predicates = select_resolver->get_select_predicates();

  std::vector<std::shared_ptr<OperatorResult>> select_result;
  std::vector<std::unique_ptr<hustle::operators::Select>> select_operators;

  /**
   * Iterate the through select predicates and src tables
   * to construct the select operators.
   */
  for (auto const& [table_name, predicate_tree] : select_predicates) {
    std::shared_ptr<OperatorResult> input_result =
        std::make_shared<OperatorResult>();
    std::shared_ptr<OperatorResult> output_result =
        std::make_shared<OperatorResult>();
    auto table_ptr = catalog->getTable(table_name);
    if (table_ptr == nullptr) return nullptr;
    if (predicate_tree == nullptr) {
      output_result->append(table_ptr);
    } else {
      input_result->append(table_ptr);
      std::unique_ptr<hustle::operators::Select> select =
          std::make_unique<hustle::operators::Select>(
              0, table_ptr, input_result, output_result, predicate_tree);
      select_operators.emplace_back(std::move(select));
    }
    select_result.emplace_back(output_result);
  }

  /**
   * Get the join predicates and the previous results to construct
   * the join operators.
   */
  JoinGraph join_graph({*(select_resolver->get_join_predicates())});

  std::shared_ptr<OperatorResult> join_result_out =
      std::make_shared<OperatorResult>();
  std::unique_ptr<Join> join_op =
      std::make_unique<Join>(0, select_result, join_result_out, join_graph);

  std::shared_ptr<std::vector<AggregateReference>> agg_refs =
      (select_resolver->get_agg_references());

  assert(agg_refs->size() == 1);  // currently supports one agg op
  std::shared_ptr<OperatorResult> agg_result_out =
      std::make_shared<OperatorResult>();

  /**
   * Group by references and order by references from select resolver
   */
  auto group_by_ref_ptrs = *(select_resolver->get_groupby_references());
  std::vector<ColumnReference> group_by_refs(group_by_ref_ptrs.size());
  std::transform(
      group_by_ref_ptrs.begin(), group_by_ref_ptrs.end(), group_by_refs.begin(),
      [](std::shared_ptr<hustle::operators::ColumnReference> x) { return *x; });

  auto order_by_ref_ptrs = *(select_resolver->get_orderby_references());
  std::vector<ColumnReference> order_by_refs(order_by_ref_ptrs.size());
  std::transform(
      order_by_ref_ptrs.begin(), order_by_ref_ptrs.end(), order_by_refs.begin(),
      [](std::shared_ptr<hustle::operators::ColumnReference> x) { return *x; });

  std::unique_ptr<Aggregate> agg_op =
      std::make_unique<Aggregate>(0, join_result_out, agg_result_out, *agg_refs,
                                  group_by_refs, order_by_refs);

  // Build the output columns for the result
  std::vector<std::shared_ptr<hustle::resolver::ProjectReference>>
      project_references = *(select_resolver->get_project_references());
  std::vector<ColumnReference> agg_project_cols;
  for (auto project_ref : project_references) {
    if (!project_ref->alias.empty()) {
      agg_project_cols.emplace_back(
          ColumnReference{nullptr, project_ref->alias});
    } else {
      agg_project_cols.emplace_back(
          ColumnReference{nullptr, project_ref->colRef.col_name});
    }
  }

  std::shared_ptr<hustle::ExecutionPlan> plan =
      std::make_shared<hustle::ExecutionPlan>(0);

  auto join_id = plan->addOperator(std::move(join_op));

  for (auto& select_op : select_operators) {
    auto select_id = plan->addOperator(std::move(select_op));
    plan->createLink(select_id, join_id);
  }
  auto agg_id = plan->addOperator(std::move(agg_op));
  // Declare aggregate dependency on join operator
  plan->createLink(join_id, agg_id);
  plan->setResultColumns(agg_project_cols);
  plan->setOperatorResult(agg_result_out);
  return plan;
}

std::shared_ptr<hustle::storage::DBTable> execute(
    std::shared_ptr<hustle::ExecutionPlan> plan,
    hustle::resolver::SelectResolver* select_resolver, Catalog* catalog) {
  hustle::Scheduler& scheduler = hustle::HustleDB::getScheduler();
  scheduler.addTask(plan.get());
  scheduler.start();
  scheduler.join();

  using namespace hustle::operators;

  std::shared_ptr<OperatorResult> agg_result_out = plan->getOperatorResult();
  std::shared_ptr<hustle::storage::DBTable> out_table =
      agg_result_out->materialize(plan->getResultColumns());
  out_table->print();
  return out_table;
}

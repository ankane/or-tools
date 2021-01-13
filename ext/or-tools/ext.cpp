// or-tools
#include <ortools/algorithms/knapsack_solver.h>
#include <ortools/base/version.h>
#include <ortools/constraint_solver/routing.h>
#include <ortools/constraint_solver/routing_parameters.h>
#include <ortools/graph/assignment.h>
#include <ortools/graph/max_flow.h>
#include <ortools/graph/min_cost_flow.h>
#include <ortools/linear_solver/linear_solver.h>
#include <ortools/sat/cp_model.h>

// rice
#include <rice/Array.hpp>
#include <rice/Class.hpp>
#include <rice/Constructor.hpp>
#include <rice/Hash.hpp>
#include <rice/Module.hpp>
#include <rice/String.hpp>
#include <rice/Symbol.hpp>

using operations_research::ArcIndex;
using operations_research::DefaultRoutingSearchParameters;
using operations_research::Domain;
using operations_research::FirstSolutionStrategy;
using operations_research::FlowQuantity;
using operations_research::KnapsackSolver;
using operations_research::LinearExpr;
using operations_research::LinearRange;
using operations_research::LocalSearchMetaheuristic;
using operations_research::MPConstraint;
using operations_research::MPObjective;
using operations_research::MPSolver;
using operations_research::MPVariable;
using operations_research::NodeIndex;
using operations_research::RoutingDimension;
using operations_research::RoutingIndexManager;
using operations_research::RoutingModel;
using operations_research::RoutingNodeIndex;
using operations_research::RoutingSearchParameters;
using operations_research::SimpleLinearSumAssignment;
using operations_research::SimpleMaxFlow;
using operations_research::SimpleMinCostFlow;

using operations_research::sat::BoolVar;
using operations_research::sat::CpModelBuilder;
using operations_research::sat::CpSolverResponse;
using operations_research::sat::CpSolverStatus;
using operations_research::sat::NewFeasibleSolutionObserver;
using operations_research::sat::SatParameters;
using operations_research::sat::SolutionIntegerValue;

using Rice::Array;
using Rice::Class;
using Rice::Constructor;
using Rice::Hash;
using Rice::Module;
using Rice::Object;
using Rice::String;
using Rice::Symbol;
using Rice::define_module;
using Rice::define_class_under;

template<>
inline
KnapsackSolver::SolverType from_ruby<KnapsackSolver::SolverType>(Object x)
{
  std::string s = Symbol(x).str();
  if (s == "branch_and_bound") {
    return KnapsackSolver::KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER;
  } else {
    throw std::runtime_error("Unknown solver type: " + s);
  }
}

template<>
inline
MPSolver::OptimizationProblemType from_ruby<MPSolver::OptimizationProblemType>(Object x)
{
  std::string s = Symbol(x).str();
  if (s == "glop") {
    return MPSolver::OptimizationProblemType::GLOP_LINEAR_PROGRAMMING;
  } else if (s == "cbc") {
    return MPSolver::OptimizationProblemType::CBC_MIXED_INTEGER_PROGRAMMING;
  } else {
    throw std::runtime_error("Unknown optimization problem type: " + s);
  }
}

template<>
inline
RoutingNodeIndex from_ruby<RoutingNodeIndex>(Object x)
{
  const RoutingNodeIndex index{from_ruby<int>(x)};
  return index;
}

template<>
inline
Object to_ruby<RoutingNodeIndex>(RoutingNodeIndex const &x)
{
  return to_ruby<int>(x.value());
}

std::vector<RoutingNodeIndex> nodeIndexVector(Array x) {
  std::vector<RoutingNodeIndex> res;
  for (auto const& v : x) {
    res.push_back(from_ruby<RoutingNodeIndex>(v));
  }
  return res;
}

template<>
inline
operations_research::sat::LinearExpr from_ruby<operations_research::sat::LinearExpr>(Object x)
{
  operations_research::sat::LinearExpr expr;

  if (x.respond_to("to_i")) {
    expr = from_ruby<int64>(x.call("to_i"));
  } else if (x.respond_to("vars")) {
    Array vars = x.call("vars");
    for(auto const& var: vars) {
      auto cvar = (Array) var;
      // TODO clean up
      Object o = cvar[0];
      std::string type = ((String) o.call("class").call("name")).str();
      if (type == "ORTools::BoolVar") {
        expr.AddTerm(from_ruby<operations_research::sat::BoolVar>(cvar[0]), from_ruby<int64>(cvar[1]));
      } else if (type == "Integer") {
        expr.AddConstant(from_ruby<int64>(cvar[0]));
      } else {
        expr.AddTerm(from_ruby<operations_research::sat::IntVar>(cvar[0]), from_ruby<int64>(cvar[1]));
      }
    }
  } else {
    std::string type = ((String) x.call("class").call("name")).str();
    if (type == "ORTools::BoolVar") {
      expr = from_ruby<operations_research::sat::BoolVar>(x);
    } else {
      expr = from_ruby<operations_research::sat::IntVar>(x);
    }
  }

  return expr;
}

// need a wrapper class due to const
class Assignment {
  const operations_research::Assignment* self;
  public:
    Assignment(const operations_research::Assignment* v) {
      self = v;
    }
    int64 ObjectiveValue() {
      return self->ObjectiveValue();
    }
    int64 Value(const operations_research::IntVar* const var) const {
      return self->Value(var);
    }
    int64 Min(const operations_research::IntVar* const var) const {
      return self->Min(var);
    }
    int64 Max(const operations_research::IntVar* const var) const {
      return self->Max(var);
    }
};

Class rb_cMPVariable;
Class rb_cMPConstraint;
Class rb_cMPObjective;
Class rb_cIntVar;
Class rb_cIntervalVar;
Class rb_cRoutingDimension;
Class rb_cConstraint;
Class rb_cSolver2;

template<>
inline
Object to_ruby<MPVariable*>(MPVariable* const &x)
{
  return Rice::Data_Object<MPVariable>(x, rb_cMPVariable, nullptr, nullptr);
}

template<>
inline
Object to_ruby<MPConstraint*>(MPConstraint* const &x)
{
  return Rice::Data_Object<MPConstraint>(x, rb_cMPConstraint, nullptr, nullptr);
}

template<>
inline
Object to_ruby<MPObjective*>(MPObjective* const &x)
{
  return Rice::Data_Object<MPObjective>(x, rb_cMPObjective, nullptr, nullptr);
}

template<>
inline
Object to_ruby<operations_research::IntVar*>(operations_research::IntVar* const &x)
{
  return Rice::Data_Object<operations_research::IntVar>(x, rb_cIntVar, nullptr, nullptr);
}

template<>
inline
Object to_ruby<operations_research::IntervalVar*>(operations_research::IntervalVar* const &x)
{
  return Rice::Data_Object<operations_research::IntervalVar>(x, rb_cIntervalVar, nullptr, nullptr);
}

template<>
inline
Object to_ruby<RoutingDimension*>(RoutingDimension* const &x)
{
  return Rice::Data_Object<RoutingDimension>(x, rb_cRoutingDimension, nullptr, nullptr);
}

template<>
inline
Object to_ruby<operations_research::Constraint*>(operations_research::Constraint* const &x)
{
  return Rice::Data_Object<operations_research::Constraint>(x, rb_cConstraint, nullptr, nullptr);
}

template<>
inline
Object to_ruby<operations_research::Solver*>(operations_research::Solver* const &x)
{
  return Rice::Data_Object<operations_research::Solver>(x, rb_cSolver2, nullptr, nullptr);
}

// need a wrapper class since absl::Span doesn't own
class IntVarSpan {
  std::vector<operations_research::sat::IntVar> vec;
  public:
    IntVarSpan(Object x) {
      Array a = Array(x);
      for (std::size_t i = 0; i < a.size(); ++i) {
        vec.push_back(from_ruby<operations_research::sat::IntVar>(a[i]));
      }
    }
    operator absl::Span<const operations_research::sat::IntVar>() {
      return absl::Span<const operations_research::sat::IntVar>(vec);
    }
};

template<>
inline
IntVarSpan from_ruby<IntVarSpan>(Object x)
{
  return IntVarSpan(x);
}

// need a wrapper class since absl::Span doesn't own
class IntervalVarSpan {
  std::vector<operations_research::sat::IntervalVar> vec;
  public:
    IntervalVarSpan(Object x) {
      Array a = Array(x);
      for (std::size_t i = 0; i < a.size(); ++i) {
        vec.push_back(from_ruby<operations_research::sat::IntervalVar>(a[i]));
      }
    }
    operator absl::Span<const operations_research::sat::IntervalVar>() {
      return absl::Span<const operations_research::sat::IntervalVar>(vec);
    }
};

template<>
inline
IntervalVarSpan from_ruby<IntervalVarSpan>(Object x)
{
  return IntervalVarSpan(x);
}

// need a wrapper class since absl::Span doesn't own
class BoolVarSpan {
  std::vector<operations_research::sat::BoolVar> vec;
  public:
    BoolVarSpan(Object x) {
      Array a = Array(x);
      for (std::size_t i = 0; i < a.size(); ++i) {
        vec.push_back(from_ruby<operations_research::sat::BoolVar>(a[i]));
      }
    }
    operator absl::Span<const operations_research::sat::BoolVar>() {
      return absl::Span<const operations_research::sat::BoolVar>(vec);
    }
};

template<>
inline
BoolVarSpan from_ruby<BoolVarSpan>(Object x)
{
  return BoolVarSpan(x);
}

extern "C"
void Init_ext()
{
  Module rb_mORTools = define_module("ORTools")
    .define_singleton_method("default_routing_search_parameters", &DefaultRoutingSearchParameters)
    .define_singleton_method(
      "lib_version",
      *[]() {
        return std::to_string(operations_research::OrToolsMajorVersion()) + "."
          + std::to_string(operations_research::OrToolsMinorVersion());
      });

  define_class_under<RoutingSearchParameters>(rb_mORTools, "RoutingSearchParameters")
    .define_method(
      "first_solution_strategy=",
      *[](RoutingSearchParameters& self, Symbol value) {
        std::string s = Symbol(value).str();

        FirstSolutionStrategy::Value v;
        if (s == "path_cheapest_arc") {
          v = FirstSolutionStrategy::PATH_CHEAPEST_ARC;
        } else if (s == "path_most_constrained_arc") {
          v = FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC;
        } else if (s == "evaluator_strategy") {
          v = FirstSolutionStrategy::EVALUATOR_STRATEGY;
        } else if (s == "savings") {
          v = FirstSolutionStrategy::SAVINGS;
        } else if (s == "sweep") {
          v = FirstSolutionStrategy::SWEEP;
        } else if (s == "christofides") {
          v = FirstSolutionStrategy::CHRISTOFIDES;
        } else if (s == "all_unperformed") {
          v = FirstSolutionStrategy::ALL_UNPERFORMED;
        } else if (s == "best_insertion") {
          v = FirstSolutionStrategy::BEST_INSERTION;
        } else if (s == "parallel_cheapest_insertion") {
          v = FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION;
        } else if (s == "sequential_cheapest_insertion") {
          v = FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION;
        } else if (s == "local_cheapest_insertion") {
          v = FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION;
        } else if (s == "global_cheapest_arc") {
          v = FirstSolutionStrategy::GLOBAL_CHEAPEST_ARC;
        } else if (s == "local_cheapest_arc") {
          v = FirstSolutionStrategy::LOCAL_CHEAPEST_ARC;
        } else if (s == "first_unbound_min_value") {
          v = FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE;
        } else {
          throw std::runtime_error("Unknown first solution strategy: " + s);
        }

        return self.set_first_solution_strategy(v);
      })
    .define_method(
      "local_search_metaheuristic=",
      *[](RoutingSearchParameters& self, Symbol value) {
        std::string s = Symbol(value).str();

        LocalSearchMetaheuristic::Value v;
        if (s == "guided_local_search") {
          v = LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH;
        } else if (s == "tabu_search") {
          v = LocalSearchMetaheuristic::TABU_SEARCH;
        } else if (s == "generic_tabu_search") {
          v = LocalSearchMetaheuristic::GENERIC_TABU_SEARCH;
        } else if (s == "simulated_annealing") {
          v = LocalSearchMetaheuristic::SIMULATED_ANNEALING;
        } else {
          throw std::runtime_error("Unknown local search metaheuristic: " + s);
        }

        return self.set_local_search_metaheuristic(v);
      })
    .define_method(
      "log_search=",
      *[](RoutingSearchParameters& self, bool value) {
        self.set_log_search(value);
      })
    .define_method(
      "solution_limit=",
      *[](RoutingSearchParameters& self, int64 value) {
        self.set_solution_limit(value);
      })
    .define_method(
      "time_limit=",
      *[](RoutingSearchParameters& self, int64 value) {
        self.mutable_time_limit()->set_seconds(value);
      })
    .define_method(
      "lns_time_limit=",
      *[](RoutingSearchParameters& self, int64 value) {
        self.mutable_lns_time_limit()->set_seconds(value);
      });

  rb_cMPVariable = define_class_under<MPVariable>(rb_mORTools, "MPVariable")
    .define_method("name", &MPVariable::name)
    .define_method("solution_value", &MPVariable::solution_value)
    .define_method(
      "+",
      *[](MPVariable& self, LinearExpr& other) {
        LinearExpr s(&self);
        return s + other;
      })
    .define_method(
      "-",
      *[](MPVariable& self, LinearExpr& other) {
        LinearExpr s(&self);
        return s - other;
      })
    .define_method(
      "*",
      *[](MPVariable& self, double other) {
        LinearExpr s(&self);
        return s * other;
      })
    .define_method(
      "inspect",
      *[](MPVariable& self) {
        return "#<ORTools::MPVariable @name=\"" + self.name() + "\">";
      });

  define_class_under<LinearExpr>(rb_mORTools, "LinearExpr")
    .define_constructor(Constructor<LinearExpr>())
    .define_method(
      "_add_linear_expr",
      *[](LinearExpr& self, LinearExpr& other) {
        return self + other;
      })
    .define_method(
      "_add_mp_variable",
      *[](LinearExpr& self, MPVariable &other) {
        LinearExpr o(&other);
        return self + o;
      })
    .define_method(
      "_gte_double",
      *[](LinearExpr& self, double other) {
        LinearExpr o(other);
        return self >= o;
      })
    .define_method(
      "_gte_linear_expr",
      *[](LinearExpr& self, LinearExpr& other) {
        return self >= other;
      })
    .define_method(
      "_lte_double",
      *[](LinearExpr& self, double other) {
        LinearExpr o(other);
        return self <= o;
      })
    .define_method(
      "_lte_linear_expr",
      *[](LinearExpr& self, LinearExpr& other) {
        return self <= other;
      })
    .define_method(
      "==",
      *[](LinearExpr& self, double other) {
        LinearExpr o(other);
        return self == o;
      })
    .define_method(
      "to_s",
      *[](LinearExpr& self) {
        return self.ToString();
      })
    .define_method(
      "inspect",
      *[](LinearExpr& self) {
        return "#<ORTools::LinearExpr \"" + self.ToString() + "\">";
      });

  define_class_under<LinearRange>(rb_mORTools, "LinearRange");

  rb_cMPConstraint = define_class_under<MPConstraint>(rb_mORTools, "MPConstraint")
    .define_method("set_coefficient", &MPConstraint::SetCoefficient);

  rb_cMPObjective = define_class_under<MPObjective>(rb_mORTools, "MPObjective")
    .define_method("value", &MPObjective::Value)
    .define_method("set_coefficient", &MPObjective::SetCoefficient)
    .define_method("set_maximization", &MPObjective::SetMaximization);

  define_class_under<MPSolver>(rb_mORTools, "Solver")
    .define_constructor(Constructor<MPSolver, std::string, MPSolver::OptimizationProblemType>())
    .define_method("infinity", &MPSolver::infinity)
    .define_method(
      "int_var",
      *[](MPSolver& self, double min, double max, const std::string& name) {
        return self.MakeIntVar(min, max, name);
      })
    .define_method("num_var", &MPSolver::MakeNumVar)
    .define_method("bool_var", &MPSolver::MakeBoolVar)
    .define_method("num_variables", &MPSolver::NumVariables)
    .define_method("num_constraints", &MPSolver::NumConstraints)
    .define_method("wall_time", &MPSolver::wall_time)
    .define_method("iterations", &MPSolver::iterations)
    .define_method("nodes", &MPSolver::nodes)
    .define_method("objective", &MPSolver::MutableObjective)
    .define_method(
      "maximize",
      *[](MPSolver& self, LinearExpr& expr) {
        return self.MutableObjective()->MaximizeLinearExpr(expr);
      })
    .define_method(
      "minimize",
      *[](MPSolver& self, LinearExpr& expr) {
        return self.MutableObjective()->MinimizeLinearExpr(expr);
      })
    .define_method(
      "add",
      *[](MPSolver& self, const LinearRange& range) {
        return self.MakeRowConstraint(range);
      })
    .define_method(
      "constraint",
      *[](MPSolver& self, double lb, double ub) {
        return self.MakeRowConstraint(lb, ub);
      })
    .define_method(
      "solve",
      *[](MPSolver& self) {
        auto status = self.Solve();

        if (status == MPSolver::ResultStatus::OPTIMAL) {
          return Symbol("optimal");
        } else if (status == MPSolver::ResultStatus::FEASIBLE) {
          return Symbol("feasible");
        } else if (status == MPSolver::ResultStatus::INFEASIBLE) {
          return Symbol("infeasible");
        } else if (status == MPSolver::ResultStatus::UNBOUNDED) {
          return Symbol("unbounded");
        } else if (status == MPSolver::ResultStatus::ABNORMAL) {
          return Symbol("abnormal");
        } else if (status == MPSolver::ResultStatus::MODEL_INVALID) {
          return Symbol("model_invalid");
        } else if (status == MPSolver::ResultStatus::NOT_SOLVED) {
          return Symbol("not_solved");
        } else {
          throw std::runtime_error("Unknown status");
        }
      });

  define_class_under<operations_research::sat::IntVar>(rb_mORTools, "SatIntVar")
    .define_method("name", &operations_research::sat::IntVar::Name);

  define_class_under<operations_research::sat::IntervalVar>(rb_mORTools, "SatIntervalVar")
    .define_method("name", &operations_research::sat::IntervalVar::Name);

  define_class_under<operations_research::sat::Constraint>(rb_mORTools, "SatConstraint");

  define_class_under<BoolVar>(rb_mORTools, "BoolVar")
    .define_method("name", &BoolVar::Name)
    .define_method("index", &BoolVar::index)
    .define_method("not", &BoolVar::Not)
    .define_method(
      "inspect",
      *[](BoolVar& self) {
        String name(self.Name());
        return "#<ORTools::BoolVar @name=" + name.inspect().str() + ">";
      });

  define_class_under<CpModelBuilder>(rb_mORTools, "CpModel")
    .define_constructor(Constructor<CpModelBuilder>())
    .define_method(
      "new_int_var",
      *[](CpModelBuilder& self, int64 start, int64 end, std::string name) {
        const Domain domain(start, end);
        return self.NewIntVar(domain).WithName(name);
      })
    .define_method(
      "new_bool_var",
      *[](CpModelBuilder& self, std::string name) {
        return self.NewBoolVar().WithName(name);
      })
    .define_method(
      "new_interval_var",
      *[](CpModelBuilder& self, operations_research::sat::IntVar start, operations_research::sat::IntVar size, operations_research::sat::IntVar end, std::string name) {
        return self.NewIntervalVar(start, size, end).WithName(name);
      })
    .define_method(
      "add_equality",
      *[](CpModelBuilder& self, operations_research::sat::LinearExpr x, operations_research::sat::LinearExpr y) {
        self.AddEquality(x, y);
      })
    .define_method(
      "add_not_equal",
      *[](CpModelBuilder& self, operations_research::sat::LinearExpr x, operations_research::sat::LinearExpr y) {
        self.AddNotEqual(x, y);
      })
    .define_method(
      "add_greater_than",
      *[](CpModelBuilder& self, operations_research::sat::LinearExpr x, operations_research::sat::LinearExpr y) {
        self.AddGreaterThan(x, y);
      })
    .define_method(
      "add_greater_or_equal",
      *[](CpModelBuilder& self, operations_research::sat::LinearExpr x, operations_research::sat::LinearExpr y) {
        self.AddGreaterOrEqual(x, y);
      })
    .define_method(
      "add_less_than",
      *[](CpModelBuilder& self, operations_research::sat::LinearExpr x, operations_research::sat::LinearExpr y) {
        self.AddLessThan(x, y);
      })
    .define_method(
      "add_less_or_equal",
      *[](CpModelBuilder& self, operations_research::sat::LinearExpr x, operations_research::sat::LinearExpr y) {
        self.AddLessOrEqual(x, y);
      })
    .define_method(
      "add_all_different",
      *[](CpModelBuilder& self, IntVarSpan vars) {
        self.AddAllDifferent(vars);
      })
    .define_method(
      "add_max_equality",
      *[](CpModelBuilder& self, operations_research::sat::IntVar target, IntVarSpan vars) {
        self.AddMaxEquality(target, vars);
      })
    .define_method(
      "add_no_overlap",
      *[](CpModelBuilder& self, IntervalVarSpan vars) {
        self.AddNoOverlap(vars);
      })
    .define_method(
      "add_bool_or",
      *[](CpModelBuilder& self, BoolVarSpan literals) {
        self.AddBoolOr(literals);
      })
    .define_method("add_implication", &CpModelBuilder::AddImplication)
    .define_method(
      "maximize",
      *[](CpModelBuilder& self, operations_research::sat::LinearExpr expr) {
        self.Maximize(expr);
      })
    .define_method(
      "minimize",
      *[](CpModelBuilder& self, operations_research::sat::LinearExpr expr) {
        self.Minimize(expr);
      });

  define_class_under<SatParameters>(rb_mORTools, "SatParameters")
    .define_constructor(Constructor<SatParameters>())
    .define_method("max_time_in_seconds=",
    *[](SatParameters& self, double value) {
      self.set_max_time_in_seconds(value);
    });

  define_class_under(rb_mORTools, "CpSolver")
    .define_method(
      "_solve_with_observer",
      *[](Object self, CpModelBuilder& model, SatParameters& parameters, Object callback, bool all_solutions) {
        operations_research::sat::Model m;

        if (all_solutions) {
          // set parameters for SearchForAllSolutions
          parameters.set_enumerate_all_solutions(true);
        }
        m.Add(NewSatParameters(parameters));

        m.Add(NewFeasibleSolutionObserver(
          [callback](const CpSolverResponse& r) {
            // TODO find a better way to do this
            callback.call("response=", r);
            callback.call("on_solution_callback");
          })
        );
        return SolveCpModel(model.Build(), &m);
      })
    .define_method(
      "_solve",
      *[](Object self, CpModelBuilder& model, SatParameters& parameters) {
        operations_research::sat::Model m;
        m.Add(NewSatParameters(parameters));
        return SolveCpModel(model.Build(), &m);
      })
    .define_method(
      "_solution_integer_value",
      *[](Object self, CpSolverResponse& response, operations_research::sat::IntVar& x) {
        return SolutionIntegerValue(response, x);
      })
    .define_method(
      "_solution_boolean_value",
      *[](Object self, CpSolverResponse& response, operations_research::sat::BoolVar& x) {
        return SolutionBooleanValue(response, x);
      });

  define_class_under<CpSolverResponse>(rb_mORTools, "CpSolverResponse")
    .define_method("objective_value", &CpSolverResponse::objective_value)
    .define_method("num_conflicts", &CpSolverResponse::num_conflicts)
    .define_method("num_branches", &CpSolverResponse::num_branches)
    .define_method("wall_time", &CpSolverResponse::wall_time)
    .define_method(
      "solution_integer_value",
      *[](CpSolverResponse& self, operations_research::sat::IntVar& x) {
        operations_research::sat::LinearExpr expr(x);
        return SolutionIntegerValue(self, expr);
      })
    .define_method("solution_boolean_value", &operations_research::sat::SolutionBooleanValue)
    .define_method(
      "status",
      *[](CpSolverResponse& self) {
        auto status = self.status();

        if (status == CpSolverStatus::OPTIMAL) {
          return Symbol("optimal");
        } else if (status == CpSolverStatus::FEASIBLE) {
          return Symbol("feasible");
        } else if (status == CpSolverStatus::INFEASIBLE) {
          return Symbol("infeasible");
        } else if (status == CpSolverStatus::MODEL_INVALID) {
          return Symbol("model_invalid");
        } else if (status == CpSolverStatus::UNKNOWN) {
          return Symbol("unknown");
        } else {
          throw std::runtime_error("Unknown solver status");
        }
      });

  define_class_under<RoutingIndexManager>(rb_mORTools, "RoutingIndexManager")
    .define_singleton_method(
      "_new_depot",
      *[](int num_nodes, int num_vehicles, RoutingNodeIndex depot) {
        return RoutingIndexManager(num_nodes, num_vehicles, depot);
      })
    .define_singleton_method(
      "_new_starts_ends",
      *[](int num_nodes, int num_vehicles, Array starts, Array ends) {
        return RoutingIndexManager(num_nodes, num_vehicles, nodeIndexVector(starts), nodeIndexVector(ends));
      })
    .define_method("index_to_node", &RoutingIndexManager::IndexToNode)
    .define_method("node_to_index", &RoutingIndexManager::NodeToIndex);

  define_class_under<Assignment>(rb_mORTools, "Assignment")
    .define_method("objective_value", &Assignment::ObjectiveValue)
    .define_method("value", &Assignment::Value)
    .define_method("min", &Assignment::Min)
    .define_method("max", &Assignment::Max);

  // not to be confused with operations_research::sat::IntVar
  rb_cIntVar = define_class_under<operations_research::IntVar>(rb_mORTools, "IntVar")
    .define_method(
      "set_range",
      *[](operations_research::IntVar& self, int64 new_min, int64 new_max) {
        self.SetRange(new_min, new_max);
      });

  rb_cIntervalVar = define_class_under<operations_research::IntervalVar>(rb_mORTools, "IntervalVar");

  rb_cRoutingDimension = define_class_under<RoutingDimension>(rb_mORTools, "RoutingDimension")
    .define_method("global_span_cost_coefficient=", &RoutingDimension::SetGlobalSpanCostCoefficient)
    .define_method("cumul_var", &RoutingDimension::CumulVar);

  rb_cConstraint = define_class_under<operations_research::Constraint>(rb_mORTools, "Constraint");

  rb_cSolver2 = define_class_under<operations_research::Solver>(rb_mORTools, "Solver2")
    .define_method(
      "add",
      *[](operations_research::Solver& self, Object o) {
        operations_research::Constraint* constraint;
        if (o.respond_to("left")) {
          operations_research::IntExpr* left(from_ruby<operations_research::IntVar*>(o.call("left")));
          operations_research::IntExpr* right(from_ruby<operations_research::IntVar*>(o.call("right")));
          auto op = o.call("operator").to_s().str();
          if (op == "==") {
            constraint = self.MakeEquality(left, right);
          } else if (op == "<=") {
            constraint = self.MakeLessOrEqual(left, right);
          } else {
            throw std::runtime_error("Unknown operator");
          }
        } else {
          constraint = from_ruby<operations_research::Constraint*>(o);
        }
        self.AddConstraint(constraint);
      })
    .define_method(
      "fixed_duration_interval_var",
      *[](operations_research::Solver& self, operations_research::IntVar* const start_variable, int64 duration, const std::string& name) {
        return self.MakeFixedDurationIntervalVar(start_variable, duration, name);
      })
    .define_method(
      "cumulative",
      *[](operations_research::Solver& self, Array rb_intervals, Array rb_demands, int64 capacity, const std::string& name) {
        std::vector<operations_research::IntervalVar*> intervals;
        for (std::size_t i = 0; i < rb_intervals.size(); ++i) {
          intervals.push_back(from_ruby<operations_research::IntervalVar*>(rb_intervals[i]));
        }

        std::vector<int64> demands;
        for (std::size_t i = 0; i < rb_demands.size(); ++i) {
          demands.push_back(from_ruby<int64>(rb_demands[i]));
        }

        return self.MakeCumulative(intervals, demands, capacity, name);
      });

  define_class_under<RoutingModel>(rb_mORTools, "RoutingModel")
    .define_constructor(Constructor<RoutingModel, RoutingIndexManager>())
    .define_method(
      "register_transit_callback",
      *[](RoutingModel& self, Object callback) {
        return self.RegisterTransitCallback(
          [callback](int64 from_index, int64 to_index) -> int64 {
            return from_ruby<int64>(callback.call("call", from_index, to_index));
          }
        );
      })
    .define_method(
      "register_unary_transit_callback",
      *[](RoutingModel& self, Object callback) {
        return self.RegisterUnaryTransitCallback(
          [callback](int64 from_index) -> int64 {
            return from_ruby<int64>(callback.call("call", from_index));
          }
        );
      })
    .define_method("depot", &RoutingModel::GetDepot)
    .define_method("size", &RoutingModel::Size)
    .define_method("status", *[](RoutingModel& self) {
        auto status = self.status();

        if (status == RoutingModel::ROUTING_NOT_SOLVED ) {
          return Symbol("not_solved");
        } else if (status == RoutingModel::ROUTING_SUCCESS ) {
          return Symbol("success");
        } else if (status == RoutingModel::ROUTING_FAIL ) {
          return Symbol("fail");
        } else if (status == RoutingModel::ROUTING_FAIL_TIMEOUT ) {
          return Symbol("fail_timeout");
        } else if (status == RoutingModel::ROUTING_INVALID ) {
          return Symbol("invalid");
        } else {
          throw std::runtime_error("Unknown solver status");
        }
      })
    .define_method("vehicle_var", &RoutingModel::VehicleVar)
    .define_method("set_arc_cost_evaluator_of_all_vehicles", &RoutingModel::SetArcCostEvaluatorOfAllVehicles)
    .define_method("set_arc_cost_evaluator_of_vehicle", &RoutingModel::SetArcCostEvaluatorOfVehicle)
    .define_method("set_fixed_cost_of_all_vehicles", &RoutingModel::SetFixedCostOfAllVehicles)
    .define_method("set_fixed_cost_of_vehicle", &RoutingModel::SetFixedCostOfVehicle)
    .define_method("fixed_cost_of_vehicle", &RoutingModel::GetFixedCostOfVehicle)
    .define_method("add_dimension", &RoutingModel::AddDimension)
    .define_method(
      "add_dimension_with_vehicle_capacity",
      *[](RoutingModel& self, int evaluator_index, int64 slack_max, Array vc, bool fix_start_cumul_to_zero, const std::string& name) {
        std::vector<int64> vehicle_capacities;
        for (std::size_t i = 0; i < vc.size(); ++i) {
          vehicle_capacities.push_back(from_ruby<int64>(vc[i]));
        }
        self.AddDimensionWithVehicleCapacity(evaluator_index, slack_max, vehicle_capacities, fix_start_cumul_to_zero, name);
      })
    .define_method(
      "add_dimension_with_vehicle_transits",
      *[](RoutingModel& self, Array rb_indices, int64 slack_max, int64 capacity, bool fix_start_cumul_to_zero, const std::string& name) {
        std::vector<int> evaluator_indices;
        for (std::size_t i = 0; i < rb_indices.size(); ++i) {
          evaluator_indices.push_back(from_ruby<int>(rb_indices[i]));
        }
        self.AddDimensionWithVehicleTransits(evaluator_indices, slack_max, capacity, fix_start_cumul_to_zero, name);
      })
    .define_method(
      "add_disjunction",
      *[](RoutingModel& self, Array rb_indices, int64 penalty) {
        std::vector<int64> indices;
        for (std::size_t i = 0; i < rb_indices.size(); ++i) {
          indices.push_back(from_ruby<int64>(rb_indices[i]));
        }
        self.AddDisjunction(indices, penalty);
      })
    .define_method("add_pickup_and_delivery", &RoutingModel::AddPickupAndDelivery)
    .define_method("solver", &RoutingModel::solver)
    .define_method("start", &RoutingModel::Start)
    .define_method("end", &RoutingModel::End)
    .define_method("start?", &RoutingModel::IsStart)
    .define_method("end?", &RoutingModel::IsEnd)
    .define_method("vehicle_index", &RoutingModel::VehicleIndex)
    .define_method("next", &RoutingModel::Next)
    .define_method("vehicle_used?", &RoutingModel::IsVehicleUsed)
    .define_method("next_var", &RoutingModel::NextVar)
    .define_method("arc_cost_for_vehicle", &RoutingModel::GetArcCostForVehicle)
    .define_method("mutable_dimension", &RoutingModel::GetMutableDimension)
    .define_method("add_variable_minimized_by_finalizer", &RoutingModel::AddVariableMinimizedByFinalizer)
    .define_method(
      "solve_with_parameters",
      *[](RoutingModel& self, const RoutingSearchParameters& search_parameters) {
        auto assignment = self.SolveWithParameters(search_parameters);
        // std::cout << assignment->DebugString();
        return (Assignment) assignment;
      });

  define_class_under<KnapsackSolver>(rb_mORTools, "KnapsackSolver")
    .define_constructor(Constructor<KnapsackSolver, KnapsackSolver::SolverType, std::string>())
    .define_method("_solve", &KnapsackSolver::Solve)
    .define_method("best_solution_contains?", &KnapsackSolver::BestSolutionContains)
    .define_method(
      "init",
      *[](KnapsackSolver& self, Array rb_values, Array rb_weights, Array rb_capacities) {
        std::vector<int64> values;
        for (std::size_t i = 0; i < rb_values.size(); ++i) {
          values.push_back(from_ruby<int64>(rb_values[i]));
        }

        std::vector<std::vector<int64>> weights;
        for (std::size_t i = 0; i < rb_weights.size(); ++i) {
          Array rb_w = Array(rb_weights[i]);
          std::vector<int64> w;
          for (std::size_t j = 0; j < rb_w.size(); ++j) {
            w.push_back(from_ruby<int64>(rb_w[j]));
          }
          weights.push_back(w);
        }

        std::vector<int64> capacities;
        for (std::size_t i = 0; i < rb_capacities.size(); ++i) {
          capacities.push_back(from_ruby<int64>(rb_capacities[i]));
        }

        self.Init(values, weights, capacities);
      });

  define_class_under<SimpleMaxFlow>(rb_mORTools, "SimpleMaxFlow")
    .define_constructor(Constructor<SimpleMaxFlow>())
    .define_method("add_arc_with_capacity", &SimpleMaxFlow::AddArcWithCapacity)
    .define_method("num_nodes", &SimpleMaxFlow::NumNodes)
    .define_method("num_arcs", &SimpleMaxFlow::NumArcs)
    .define_method("tail", &SimpleMaxFlow::Tail)
    .define_method("head", &SimpleMaxFlow::Head)
    .define_method("capacity", &SimpleMaxFlow::Capacity)
    .define_method("optimal_flow", &SimpleMaxFlow::OptimalFlow)
    .define_method("flow", &SimpleMaxFlow::Flow)
    .define_method(
      "solve",
      *[](SimpleMaxFlow& self, NodeIndex source, NodeIndex sink) {
        auto status = self.Solve(source, sink);

        if (status == SimpleMaxFlow::Status::OPTIMAL) {
          return Symbol("optimal");
        } else if (status == SimpleMaxFlow::Status::POSSIBLE_OVERFLOW) {
          return Symbol("possible_overflow");
        } else if (status == SimpleMaxFlow::Status::BAD_INPUT) {
          return Symbol("bad_input");
        } else if (status == SimpleMaxFlow::Status::BAD_RESULT) {
          return Symbol("bad_result");
        } else {
          throw std::runtime_error("Unknown status");
        }
      })
    .define_method(
      "source_side_min_cut",
      *[](SimpleMaxFlow& self) {
        std::vector<NodeIndex> result;
        self.GetSourceSideMinCut(&result);

        Array ret;
        for(auto const& it: result) {
          ret.push(it);
        }
        return ret;
      })
    .define_method(
      "sink_side_min_cut",
      *[](SimpleMaxFlow& self) {
        std::vector<NodeIndex> result;
        self.GetSinkSideMinCut(&result);

        Array ret;
        for(auto const& it: result) {
          ret.push(it);
        }
        return ret;
      });

  define_class_under<SimpleMinCostFlow>(rb_mORTools, "SimpleMinCostFlow")
    .define_constructor(Constructor<SimpleMinCostFlow>())
    .define_method("add_arc_with_capacity_and_unit_cost", &SimpleMinCostFlow::AddArcWithCapacityAndUnitCost)
    .define_method("set_node_supply", &SimpleMinCostFlow::SetNodeSupply)
    .define_method("optimal_cost", &SimpleMinCostFlow::OptimalCost)
    .define_method("maximum_flow", &SimpleMinCostFlow::MaximumFlow)
    .define_method("flow", &SimpleMinCostFlow::Flow)
    .define_method("num_nodes", &SimpleMinCostFlow::NumNodes)
    .define_method("num_arcs", &SimpleMinCostFlow::NumArcs)
    .define_method("tail", &SimpleMinCostFlow::Tail)
    .define_method("head", &SimpleMinCostFlow::Head)
    .define_method("capacity", &SimpleMinCostFlow::Capacity)
    .define_method("supply", &SimpleMinCostFlow::Supply)
    .define_method("unit_cost", &SimpleMinCostFlow::UnitCost)
    .define_method(
      "solve",
      *[](SimpleMinCostFlow& self) {
        auto status = self.Solve();

        if (status == SimpleMinCostFlow::Status::NOT_SOLVED) {
          return Symbol("not_solved");
        } else if (status == SimpleMinCostFlow::Status::OPTIMAL) {
          return Symbol("optimal");
        } else if (status == SimpleMinCostFlow::Status::FEASIBLE) {
          return Symbol("feasible");
        } else if (status == SimpleMinCostFlow::Status::INFEASIBLE) {
          return Symbol("infeasible");
        } else if (status == SimpleMinCostFlow::Status::UNBALANCED) {
          return Symbol("unbalanced");
        } else if (status == SimpleMinCostFlow::Status::BAD_RESULT) {
          return Symbol("bad_result");
        } else if (status == SimpleMinCostFlow::Status::BAD_COST_RANGE) {
          return Symbol("bad_cost_range");
        } else {
          throw std::runtime_error("Unknown status");
        }
      });

  define_class_under<SimpleLinearSumAssignment>(rb_mORTools, "LinearSumAssignment")
    .define_constructor(Constructor<SimpleLinearSumAssignment>())
    .define_method("add_arc_with_cost", &SimpleLinearSumAssignment::AddArcWithCost)
    .define_method("num_nodes", &SimpleLinearSumAssignment::NumNodes)
    .define_method("num_arcs", &SimpleLinearSumAssignment::NumArcs)
    .define_method("left_node", &SimpleLinearSumAssignment::LeftNode)
    .define_method("right_node", &SimpleLinearSumAssignment::RightNode)
    .define_method("cost", &SimpleLinearSumAssignment::Cost)
    .define_method("optimal_cost", &SimpleLinearSumAssignment::OptimalCost)
    .define_method("right_mate", &SimpleLinearSumAssignment::RightMate)
    .define_method("assignment_cost", &SimpleLinearSumAssignment::AssignmentCost)
    .define_method(
      "solve",
      *[](SimpleLinearSumAssignment& self) {
        auto status = self.Solve();

        if (status == SimpleLinearSumAssignment::Status::OPTIMAL) {
          return Symbol("optimal");
        } else if (status == SimpleLinearSumAssignment::Status::INFEASIBLE) {
          return Symbol("infeasible");
        } else if (status == SimpleLinearSumAssignment::Status::POSSIBLE_OVERFLOW) {
          return Symbol("possible_overflow");
        } else {
          throw std::runtime_error("Unknown status");
        }
      });
}

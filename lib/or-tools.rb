# ext
require "or_tools/ext"

# modules
require "or_tools/comparison"
require "or_tools/comparison_operators"
require "or_tools/bool_var"
require "or_tools/cp_model"
require "or_tools/cp_solver"
require "or_tools/cp_solver_solution_callback"
require "or_tools/int_var"
require "or_tools/knapsack_solver"
require "or_tools/linear_expr"
require "or_tools/routing_index_manager"
require "or_tools/routing_model"
require "or_tools/sat_linear_expr"
require "or_tools/sat_int_var"
require "or_tools/solver"
require "or_tools/version"

# solution printers
require "or_tools/objective_solution_printer"
require "or_tools/var_array_solution_printer"
require "or_tools/var_array_and_objective_solution_printer"

# higher level interfaces
require "or_tools/basic_scheduler"
require "or_tools/seating"
require "or_tools/sudoku"
require "or_tools/tsp"
require "or_tools/vrp"

module ORTools
  class Error < StandardError; end
end

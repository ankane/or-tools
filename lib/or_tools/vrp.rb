module ORTools
  class VRP
    attr_reader :manager, :routing, :locations, :vehicle_count

    DISTANCE_SCALE = 1
    DEGREES_TO_RADIANS = Math::PI / 180

    def initialize(locations, vehicle_count, depot=0)
      raise ArgumentError, "Locations must have latitude and longitude" unless locations.all? { |l| l[:latitude] && l[:longitude] }
      raise ArgumentError, "Latitude must be between -90 and 90" unless locations.all? { |l| l[:latitude] >= -90 && l[:latitude] <= 90 }
      raise ArgumentError, "Longitude must be between -180 and 180" unless locations.all? { |l| l[:longitude] >= -180 && l[:longitude] <= 180 }
      raise ArgumentError, "Must be at least two locations" unless locations.size >= 2

      @locations = locations
      @vehicle_count = vehicle_count

      @manager = ORTools::RoutingIndexManager.new(distance_matrix.length, vehicle_count, depot)
      @routing = ORTools::RoutingModel.new(manager)
    end

    def add_dimension(dimension_name, slack_max=0, capacity=3000, fix_start_cumulatve_to_zero=true) 
      distance_callback = lambda do |from_index, to_index|
        from_node = manager.index_to_node(from_index)
        to_node = manager.index_to_node(to_index)
        distance_matrix[from_node][to_node]
      end

      transit_callback_index = routing.register_transit_callback(distance_callback)
      routing.set_arc_cost_evaluator_of_all_vehicles(transit_callback_index)

      # add a dimension
      routing.add_dimension(
        transit_callback_index,
        slack_max,
        (capacity / DISTANCE_SCALE).to_i,
        fix_start_cumulatve_to_zero,
        dimension_name
      )
      distance_dimension = routing.mutable_dimension(dimension_name)
      distance_dimension.global_span_cost_coefficient = 100
      return self
    end

    def routes
      # calculate routes
      routes = []
      vehicle_count.times do |vehicle_id|
        route_indexes = []
        index = routing.start(vehicle_id)
        while !routing.end?(index)
          route_indexes << manager.index_to_node(index)
          previous_index = index
          index = solution.value(routing.next_var(index))
        end
        routes << locations.values_at(*route_indexes)
      end
      return routes
    end

    private

    def distance(from, to)
      from_lat = from[:latitude] * DEGREES_TO_RADIANS
      from_lng = from[:longitude] * DEGREES_TO_RADIANS
      to_lat = to[:latitude] * DEGREES_TO_RADIANS
      to_lng = to[:longitude] * DEGREES_TO_RADIANS
      2 * 6371 * Math.asin(Math.sqrt(Math.sin((to_lat - from_lat) / 2.0)**2 + Math.cos(from_lat) * Math.cos(to_lat) * Math.sin((from_lng - to_lng) / 2.0)**2))
    end

    def distance_matrix
      @distance_matrix ||=
        locations.map do |from|
          locations.map do |to|
            # must be integers
            (distance(from, to) * DISTANCE_SCALE).to_i
          end
        end
    end

    def solution
      @solution ||= routing.solve(first_solution_strategy: :path_cheapest_arc)
    end
  end
end

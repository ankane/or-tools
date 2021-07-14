require_relative "test_helper"

class VRPTest < Minitest::Test
  def test_works
    locations = [
      {:reference=>"HEETS01727", :latitude=>51.470462, :longitude=>-0.255665},
      {:reference=>"HEETS01768", :latitude=>51.476025, :longitude=>-0.205112},
      {:reference=>"160620LON01D", :latitude=>51.523957, :longitude=>-0.127069},
      {:reference=>"HEETS01739", :latitude=>51.481615, :longitude=>-0.181795},
      {:reference=>"160620LON02C", :latitude=>51.523957, :longitude=>-0.127069},
      {:reference=>"HEETS01736", :latitude=>51.493276, :longitude=>-0.219917},
      {:reference=>"HEETS01737", :latitude=>51.497367, :longitude=>-0.222679},
      {:reference=>"HEETS01772", :latitude=>51.497363, :longitude=>-0.312997},
      {:reference=>"PPE0004", :latitude=>51.514495, :longitude=>-0.135257},
      {:reference=>"PPE0003", :latitude=>51.498691, :longitude=>-0.199912},
      {:reference=>"HEETS01771", :latitude=>51.5275759, :longitude=>-0.3510861},
      {:reference=>"HEETS01756", :latitude=>51.528199, :longitude=>-0.354785},
      {:reference=>"PPE0005", :latitude=>51.5122015, :longitude=>-3.2797795},
      {:reference=>"HEETS01747", :latitude=>51.634133, :longitude=>-0.102818},
      {:reference=>"HEETS01757", :latitude=>51.559737, :longitude=>0.077569},
      {:reference=>"HEETS01725", :latitude=>51.571479, :longitude=>0.134208},
      {:reference=>"HEETS01763", :latitude=>51.581164, :longitude=>0.021524},
      {:reference=>"HEETS01764", :latitude=>51.562728, :longitude=>0.090968},
      {:reference=>"HEETS01734", :latitude=>51.590121, :longitude=>-0.024429},
      {:reference=>"HEETS01721", :latitude=>51.566335, :longitude=>0.000885},
      {:reference=>"HEETS01752", :latitude=>51.559565, :longitude=>-0.098628},
      {:reference=>"160620LON02D", :latitude=>51.523957, :longitude=>-0.127069}
    ]
    vrp = ORTools::VRP.new(locations, 2).add_dimension("Distance")
    assert_equal 2, vrp.routes.count
  end
end


/*
 *   Copyright (C) 2018,  CentraleSupelec
 *
 *   Author : Hervé Frezza-Buet
 *
 *   Contributor :
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public
 *   License (GPL) as published by the Free Software Foundation; either
 *   version 3 of the License, or any later version.
 *   
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *   General Public License for more details.
 *   
 *   You should have received a copy of the GNU General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *   Contact : herve.frezza-buet@centralesupelec.fr
 *
 */


#pragma once

#include <functional>
#include <vector>
#include <iterator>
#include <algorithm>

#include <vq3Graph.hpp>
#include <vq3Epoch.hpp>
#include <vq3Utils.hpp>
#include <vq3Stats.hpp>

namespace vq3 {
  namespace algo {
    namespace gngt {
      
      namespace by_default {
	template<typename RANDOM_DEVICE>
	struct Evolution {
	  double T          =   0;
	  double density    =   0;
	  double sigma_coef = 1.5;
	  RANDOM_DEVICE& rd;
	  
	  Evolution(RANDOM_DEVICE& rd) : rd(rd) {}
	  Evolution()                 = delete;
	  Evolution(const Evolution&) = default;
	  
	  template<typename GRAPH, typename BMU_RESULT, typename VERTICES, typename CLONE_PROTOTYPE>
	  void operator()(GRAPH& g,
			  const BMU_RESULT& bmu_epoch_result,
			  const VERTICES& vertices,
			  const CLONE_PROTOTYPE& clone_prototype) {

	    std::vector<typename GRAPH::ref_vertex> above;
	    std::vector<typename GRAPH::ref_vertex> below;
	    auto out_above = std::back_inserter(above);
	    auto out_below = std::back_inserter(below);
    
	    double NT = density*T;
    
	    vq3::stats::MeanStd mean_std;
    
	    for(auto& res : bmu_epoch_result)
	      if(res.vq3_bmu_accum.nb != 0)
		mean_std  = res.vq3_bmu_accum.value;
	    double spatial_dmean = std::sqrt(mean_std.variance())*sigma_coef;
    
	    unsigned int idx = 0;
	    for(auto& res : bmu_epoch_result) {
	      auto& ref_v = vertices(idx++);
	      if(res.vq3_bmu_accum.nb == 0)
		ref_v->kill(); // We kill a vertex which has never won the competition.
	      else if(auto& omstd = (*ref_v)().vq3_online_mean_std; omstd) {
		auto [m, std] = omstd(); // We get the vertex distortion statistics.
		auto dm = sigma_coef*std;
	  
		double radius = dm + spatial_dmean;
	  
		if(NT < m - radius) // There are not enough nodes (cold color).
		  *(out_above++) = ref_v;
		else if(m + radius < NT)  // There are too many nodes (hot color).
		  *(out_below++) = ref_v;
	      }
	    }

	    if(above.size() > 0)
	      for(auto it = above.begin(); it != above.end(); ++it)
		g += clone_prototype((*(*it))().vq3_value);
    
	    if(below.size() > 0)
	      for(auto it = below.begin(); it != below.end(); ++it)
		(*it)->kill();

	  }
	};

	template<typename RANDOM_DEVICE>
	Evolution<RANDOM_DEVICE> evolution(RANDOM_DEVICE& rd) {
	  return Evolution<RANDOM_DEVICE>(rd);
	}
      }
      
      template<typename PROTOTYPE, typename SAMPLE, typename GRAPH>
      class Processor {
      public:
	
	using ref_vertex = typename GRAPH::ref_vertex;
	using ref_edge   = typename GRAPH::ref_edge;
	using vertex     = typename GRAPH::vertex_value_type;
	using edge       = typename GRAPH::edge_value_type;
      
	
	using epoch_bmu = vq3::epoch::data::online::bmu_mean_std<vq3::epoch::data::none<SAMPLE> >;
	using epoch_wta = vq3::epoch::data::wta<vq3::epoch::data::none<SAMPLE> >;
	using epoch_wtm = vq3::epoch::data::wtm<vq3::epoch::data::none<SAMPLE> >;

	GRAPH& g;

	vq3::utils::Vertices<ref_vertex>&  vertices;
	vq3::epoch::wtm::Processor<GRAPH> wtm;
	vq3::epoch::wta::Processor<GRAPH> wta;
	vq3::epoch::wta::Processor<GRAPH> bmu;
	vq3::epoch::chl::Processor<GRAPH> chl;

	

      private:

	
	void wtm_topo() {
	  wtm.update_topology([this](unsigned int edge_distance) {
	      if(edge_distance == 0)
		return 1.0;
	      return this->neighbour_weight;},
	    1.1, 0);
	}
		       

      public:

	unsigned int nb_threads = 1;

	/**
	 * This gives the importance in [0, 1] of vertices update for
	 * those which are neighbours of the BMU (for which importance
	 * is 1).
	 */
	double neighbour_weight = .1;

	/**
	 * Compares the vertex value to a sample.
	 */
	std::function<double (const vertex&, const SAMPLE&)> distance;

	/** 
	 * Returns a reference to the prototype from the vertex value.
	 */
	std::function<PROTOTYPE& (vertex&)> prototype;

	Processor(GRAPH& g, vq3::utils::Vertices<typename GRAPH::ref_vertex>& vertices)
	  : g(g), vertices(vertices), wtm(g, vertices), wta(g, vertices), bmu(g, vertices), chl(g) {
	  vertices.update_topology(g);
	  wtm_topo();
	}
	
	Processor()                            = delete;
	Processor(const Processor&)            = default;
	Processor(Processor&&)                 = default;
	Processor& operator=(const Processor&) = default;
	Processor& operator=(Processor&&)      = default;

	/**
	 * This updates the prototypes and the graph topology (i.e vertex and/or edge modification).
	 * @param nb_wtm_before the number (> 0) of SOM-like passes before the graph evolution.
	 * @param nb_wtmchl_after the number (>= 0) of SOM-like passes after the graph evolution.
	 * @param begin, end The samples
	 * @param sample_of The samples are obtained from sample_of(*it).
	 * @param clone_prototype Computes a prototype value that is close to (*ref_v)().vq3_value.
	 * @param evolution modifies the graph. See vq3::algo::gngt::by_default::evolution for an example.
	 */
	template<typename ITER, typename SAMPLE_OF, typename EVOLUTION, typename CLONE_PROTOTYPE>
	void epoch(unsigned int nb_wtm_before,
		   unsigned int nb_wtmchl_after,
		   const ITER& begin, const ITER& end,
		   const SAMPLE_OF& sample_of,
		   const CLONE_PROTOTYPE& clone_prototype,
		   EVOLUTION& evolution) {
 	  if(begin == end) {
	    g.foreach_vertex([](const ref_vertex& ref_v) {ref_v->kill();});
	    vertices.update_topology(g);
	    wtm_topo();
	    return;
	  }
	  
	  auto wtm_results = wtm.template update_prototypes<epoch_wtm>(nb_threads, begin, end, sample_of, prototype, distance);
	  auto nb_vertices = wtm_results.size();

	  if(nb_vertices == 0) {
	    // empty graph, we create one vertex, and do one wta pass.
	    g += sample_of(*begin);
	    vertices.update_topology(g);
	    wta.template update_prototypes<epoch_wta>(nb_threads, begin, end, sample_of, prototype, distance);
	    wtm_topo();
	    return;
	  }

	  for(unsigned int i = 1; i < nb_wtm_before; ++i)
	    wtm.template update_prototypes<epoch_wtm>(nb_threads, begin, end, sample_of, prototype, distance);
	  vertices.update_topology(g);
	  auto bmu_results = bmu.template update_prototypes<epoch_bmu>(nb_threads, begin, end, sample_of, prototype, distance);
	  
	  evolution(g, bmu_results, vertices, clone_prototype);
	  vertices.update_topology(g);
	  wtm_topo();
	  
	  for(unsigned int i = 0; i < nb_wtmchl_after; ++i) {
	    chl.update_edges(nb_threads, begin, end, sample_of, prototype, distance, edge());
	    wtm_topo();
	    wtm.template update_prototypes<epoch_wtm>(nb_threads, begin, end, sample_of, prototype, distance);
	  }
	}
	
      };

      template<typename PROTOTYPE, typename SAMPLE, typename GRAPH>
      Processor<PROTOTYPE, SAMPLE, GRAPH> processor(GRAPH& g, vq3::utils::Vertices<typename GRAPH::ref_vertex>& vertices) {
	return Processor<PROTOTYPE, SAMPLE, GRAPH>(g, vertices);
      }
      
    }
  }
}
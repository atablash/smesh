#pragma once


#include "smesh.hpp"
#include "mesh-utils.hpp"



#include <list>
#include <map>


struct Cap_Hole_Result {
	int num_polys_created = 0;
};

//
// for triangle meshes
//
template<class EDGE>
Cap_Hole_Result cap_hole(const EDGE& edge) {
	Cap_Hole_Result r;

	auto& m = edge.mesh;

	std::list<decltype(edge.handle)> perimeter;

	// iterators to perimeter, ordered by score
	std::multimap<double, typename decltype(perimeter)::iterator> cands;

	std::unordered_map<std::remove_cv_t<decltype(edge.handle)>, typename decltype(cands)::iterator> where_cands;


	auto get_score = [](auto e0, auto e1){
		auto t0 = e0.segment.trace();
		auto t1 = e1.segment.trace();

		auto my_normal = t1.cross( t0 ).eval();
		my_normal.normalize();

		auto normal0 = compute_poly_normal(e0.poly);
		auto normal1 = compute_poly_normal(e1.poly);

		auto score0 = normal0.dot(my_normal) + 1;
		auto score1 = normal1.dot(my_normal) + 1;

		// shape score: area / perimeter
		double min_angle = std::min({
			compute_angle(-t0, t1),
			compute_angle(t0, t0+t1),
			compute_angle(-t1, -(t0+t1))
		});

		return score0 * score1 * min_angle;
	};


	auto add_cand = [&](const auto& it){

		auto next = it;
		++next;
		if(next == perimeter.end()) next = perimeter.begin();

		DCHECK_EQ(it->get(m).verts[1].key, next->get(m).verts[0].key) << "edges not adjacent";

		auto cands_it = cands.insert({get_score(it->get(m), next->get(m)), it});
		where_cands.insert({*it, cands_it});

		//LOG(INFO) << "where_cands.insert(" << (*it) << ")";
	};


	auto del_cand = [&](const auto& e) {

		auto it = where_cands.find(e);

		DCHECK(it != where_cands.end()) << e << " not found in where_cands";
		DCHECK(it->second != cands.end()) << e << " not found in cands";

		cands.erase(it->second);
		where_cands.erase(it);
	};


	auto he = edge.handle;
	do {
		while(he(m).next().has_link) {
			he = he(m).next().link().handle;
		}

		he = he(m).next().handle;

		perimeter.push_back(he);
	} while(he != edge.handle);


	for(typename decltype(perimeter)::iterator iter = perimeter.begin(); iter != perimeter.end(); ++iter) {
		add_cand(iter);
	}


	while(perimeter.size() >= 3) {
		auto iter = cands.end(); --iter;
		auto curr = iter->second;

		//LOG(INFO) << "score: " << it->first;

		// get next edge in perimeter
		auto next = curr; ++next;
		if(next == perimeter.end()) next = perimeter.begin();

		DCHECK_EQ(curr->get(m).verts[1].key, next->get(m).verts[0].key) << "edges not adjacent";

		auto i0 = curr->get(m).verts[0].key;
		auto i1 = next->get(m).verts[1].key;
		auto i2 = curr->get(m).verts[1].key;
		// add cand, cand+1 poly
		auto p = m.polys.add(i0, i1, i2);

		++r.num_polys_created;

		//LOG(INFO) << "add " << i0 << " " << i1 << " " << i2;

		// create missing edge-links
		curr->get(m).link(p.edges[2]);
		next->get(m).link(p.edges[1]);

		// create missing vertex-poly links
		if constexpr(EDGE::Mesh::Has_Vert_Poly_Links) {
			for(auto pv : p.verts) {
				pv.vert.poly_links.add(pv);
			}
		}

		// get prev edge in perimeter
		auto prev = curr;
		if(prev == perimeter.begin()) prev = perimeter.end();
		--prev;

		// insert new edge
		auto new_edge = perimeter.insert(curr, p.edges[0].handle);

		DCHECK_EQ(p.edges[0].verts[0].key, prev->get(m).verts[1].key) << "new_edge not adjacent";
		DCHECK_EQ(p.edges[0].verts[1].key, next->get(m).verts[1].key) << "new_edge not adjacent";

		// erase 3 candidate polys from priority queue
		del_cand(*prev);
		del_cand(*next);
		del_cand(*iter->second);

		// remove 2 old edges
		perimeter.erase(curr);
		perimeter.erase(next);

		// add 2 new candidate polys
		add_cand(prev);
		add_cand(new_edge);

		DCHECK_EQ(perimeter.size(), cands.size());
		DCHECK_EQ(where_cands.size(), cands.size());
	}

	// add last edge-link
	{
		auto it = perimeter.begin();
		auto e0 = *it;
		++it;
		auto e1 = *it;

		e0.get(m).link( e1.get(m) );
	}

	return r;
}





struct Cap_Holes_Result {
	int num_holes_capped = 0;
	int num_polys_created = 0;
};

template<class MESH>
Cap_Holes_Result cap_holes(MESH& mesh) {
	Cap_Holes_Result r;

	for(auto p : mesh.polys) {
		for(auto pe : p.edges) {
			if(!pe.has_link) {
				auto lr = cap_hole(pe);
				++r.num_holes_capped;
				r.num_polys_created += lr.num_polys_created;
			}
		}
	}

	return r;
}



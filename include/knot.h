#pragma once

#include <algorithm>

#include "polygonal_curve.h"

namespace knot
{

	struct SimulationParams
	{
		// The (average?) length of each line segment ("stick"), prior to relaxation
		float starting_length = 0.25f;

		// The maximum distance a bead can travel per time-step
		float d_max = starting_length * 0.025f;

		// The closest any two sticks can be (note that this should be larger than `d_max`)
		float d_close = starting_length * 0.25f;

		// The mass of each node ("bead"): we leave this unchanged for now
		float mass = 1.0f;

		// Velocity damping factor
		float damping = 0.25f;

		// How much each bead wants to stay near its original position (0 means that we ignore this force)
		float anchor_weight = 0.01f;

		float beta = 1.0f;
		float h = 1.0f;
		float alpha = 4.0f;
		float k = 1.0f;
		float epsilon = 0.001f;
	};

	class Bead
	{

	public:

		Bead(const glm::vec3& position, size_t index, size_t neighbor_l_index, size_t neighbor_r_index) :
			position{ position },
			velocity{ glm::vec3{0.0f, 0.0f, 0.0f} },
			acceleration{ glm::vec3{0.0f, 0.0f, 0.0f} },
			index{ index },
			neighbor_l_index{ neighbor_l_index },
			neighbor_r_index{ neighbor_r_index },
			is_stuck{ false }
		{}

		/// Returns `true` if this bead and `other` are neighbors and `false` otherwise.
		bool are_neighbors(const Bead& other)
		{
			return index == other.neighbor_l_index || index == other.neighbor_r_index;
		}

		/// Apply forces to this bead and update its position, velocity, and acceleration, accordingly.
		void apply_forces(const glm::vec3& force, const SimulationParams& params)
		{
			// Integrate acceleration and velocity (with damping)
			acceleration += force / params.mass;
			velocity += acceleration;
			velocity *= params.damping;

			// Zero out the acceleration for the next time step
			acceleration = glm::vec3{};

			// Set new position
			const auto old = position;

			// Each particle can travel (at most) `d_max` units each time step
			const auto clamped = glm::length(velocity) > params.d_max ? glm::normalize(velocity) * params.d_max : velocity;
			position += clamped;

			// TODO: prevent segments from intersecting
			// ...
		}

	private:

		// The position of the bead in 3-space
		glm::vec3 position;

		// The velocity of the bead
		glm::vec3 velocity;

		// The acceleration of the bead
		glm::vec3 acceleration;

		// The index of the polyline vertex corresponding to this bead
		size_t index;

		// The cached index of this bead's left neighbor in the underlying polyline
		size_t neighbor_l_index;

		// The cached index of this bead's right neighbor in the underlying polyline
		size_t neighbor_r_index;

		// Whether or not this bead is active in the physics simulation
		bool is_stuck;

		friend class Knot;
		friend bool operator==(const Bead& a, const Bead& b);
	};

	bool operator== (const Bead& a, const Bead& b)
	{
		return a.index == b.index &&
			a.neighbor_l_index == b.neighbor_l_index &&
			a.neighbor_r_index == b.neighbor_r_index;
	}

	bool operator!= (const Bead& a, const Bead& b)
	{
		return !(a == b);
	}

	/// A struct representing a knot, which is a curve embedded in 3-dimensional space
	/// with a particular set of over- / under-crossings. In this program, a "knot" also
	/// refers to a dynamical model, where the underlying curve is treated as a mass-spring
	/// system.
	class Knot
	{

	public:

		Knot(const geom::PolygonalCurve& curve, const SimulationParams& params = SimulationParams{}) :
			rope{ curve },
			anchors{ curve },
			params{ params }
		{
			std::cout << "Constructing a new knot..." << std::endl;

			// Initialize beads
			for (size_t i = 0; i < rope.get_number_of_vertices(); ++i)
			{
				auto [l, r] = rope.get_neighboring_indices_wrapped(i);

				beads.push_back(Bead{ rope.get_vertices()[i], i, l, r });
			}
		}

		/// Returns an immutable reference to the polyline that formed this knot, prior
		/// to relaxation.
		const geom::PolygonalCurve& get_rope() const
		{
			return rope;
		}

		/// Performs a pseudo-physical form of topological refinement, based on spring
		/// physics.
		void relax(bool use_anchors = true)
		{
			for (auto& bead : beads)
			{
				// Sum all of the forces acting on this particular bead
				auto force = glm::vec3{};

				// Iterate over all potential neighbors
				for (auto& other : beads) {

					// Don't accumulate forces on itself
					if (other != bead)
					{

						// Grab the "other" bead, which may or may not be a neighbor to "bead"
						if (bead.are_neighbors(other))
						{
							// This is a neighboring bead: calculate the (attractive) mechanical spring force that
							// will pull this bead towards `other`
							auto direction = other.position - bead.position;
							auto r = glm::length(direction);
							direction = glm::normalize(direction);

							if (abs(r) < params.epsilon)
							{
								continue;
							}

							force += direction * params.h * powf(r, 1.0f + params.beta);
						}
						else
						{
							// This is NOT a neighboring bead: calculate the (repulsive) electrostatic force
							auto direction = bead.position - other.position; // Reversed direction
							auto r = glm::length(direction);
							direction = glm::normalize(direction);

							if (abs(r) < params.epsilon)
							{
								continue;
							}

							force += direction * params.k * powf(r, -(2.0 + params.alpha));
						}
					}
				}

				// Apply anchor force
				if (use_anchors)
				{
					auto direction = anchors.get_vertices()[bead.index] - bead.position;
					auto r = glm::length(direction);
					direction = glm::normalize(direction);

					if (abs(r) > params.epsilon)
					{
						force += (direction * params.h * powf(r, 1.0f + params.beta)) * params.anchor_weight;
					}
				}

				bead.apply_forces(force, params);
			}

			// Update polyline positions for rendering
			rope.set_vertices(gather_position_data());
		}

		/// Resets the physics simulation.
		void reset()
		{
			rope = anchors;

			for (size_t i = 0; i < beads.size(); i++)
			{
				beads[i].position = anchors.get_vertices()[i];
			}
		}

	private:

		std::vector<glm::vec3> gather_position_data() const
		{
			std::vector<glm::vec3> positions;
			std::transform(beads.begin(), beads.end(), std::back_inserter(positions),
				[](Bead bead) -> glm::vec3 { return bead.position; });

			return positions;
		}

		// The "rope" (polygonal line segment) that is knotted and will be animated
		geom::PolygonalCurve rope;

		// Anchor (starting) positions
		geom::PolygonalCurve anchors;

		// All of the "beads" (i.e. points with a position, velocity, and acceleration) that make up this knot
		std::vector<Bead> beads;

		SimulationParams params;

	};

}
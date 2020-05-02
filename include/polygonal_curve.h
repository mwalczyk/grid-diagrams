#pragma once

#define _USE_MATH_DEFINES

#include <cmath>
#include <math.h>
#include <vector>

#include "glm.hpp"
#include "gtx/compatibility.hpp"

namespace geom 
{

	class Segment
	{
		
	public:

		Segment(const glm::vec3& start, const glm::vec3& end) :
			a{ start },
			b{ end }
		{}

		/// Returns the first endpoint of this line segment.
		const glm::vec3& get_start() const { return a; }

		/// Returns the second endpoint of this line segment.
		const glm::vec3& get_end() const { return b; }

		/// Returns the (scalar) length of this line segment.
		float length() const { return glm::distance(a, b); }

		/// Returns the midpoint of this line segment.
		glm::vec3 midpoint() const { return (a + b) * 0.5f; }

		/// Returns the point at `t` along this line segment, where a value
		/// of `0.0` corresponds to `a` and a value of `1.0` corresponds
		/// to `b`.
		glm::vec3 point_at(float t) const
		{
			return glm::lerp(a, b, t);
		}

	private:

		glm::vec3 a;
		glm::vec3 b;

	};

	class PolygonalCurve
	{

	public:

		PolygonalCurve() = default;

		PolygonalCurve(const std::vector<glm::vec3>& vertices) :
			vertices{ vertices }
		{}

		/// Returns the vertices that make up this curve.
		const std::vector<glm::vec3>& get_vertices() const
		{
			return vertices;
		}

		/// Returns the number of vertices that make up this curve.
		size_t get_number_of_vertices() const
		{
			return vertices.size();
		}

		/// Returns a wrapped index. For example, if the curve has 10 vertices,
		/// `get_wrapped_index(11)` would return `0` (i.e. the first vertex).
		size_t get_wrapped_index(size_t index) const
		{
			return index % (get_number_of_vertices() + 1);
		}

		/// Returns the indices of the "left" and "right" neighbors to the vertex at
		/// index `center_index`. The curve is assumed to be "closed" so that the
		/// "left" neighbor of the vertex at index `0` is the index of the last vertex
		/// in this polyline, etc.
		std::pair<size_t, size_t> get_neighboring_indices_wrapped(size_t center_index) const
		{
			const auto wrapped_index = get_wrapped_index(center_index);

			const auto neighbor_l_index = wrapped_index == 0 ? get_number_of_vertices() - 1 : wrapped_index - 1;
			const auto neighbor_r_index = wrapped_index == get_number_of_vertices() - 1 ? 0 : wrapped_index + 1;

			return { neighbor_l_index, neighbor_r_index };
		}

		/// Returns the total length of this curve (i.e. the sum of the lengths
		/// of all of its segments).
		float perimeter() const
		{
			float length = 0.0f;

			for (size_t i = 0; i < get_number_of_vertices() + 1; ++i)
			{
				const auto& a = vertices[get_wrapped_index(i + 0)];
				const auto& b = vertices[get_wrapped_index(i + 1)];
				length += glm::distance(a, b);
			}

			return length;
		}

		/// Returns the line segment between vertex `index` and `index + 1`.
		Segment get_segment(size_t index) const
		{
			return Segment{ vertices[index + 0], vertices[index + 1] };
		}

		/// Returns the point at `t` along this curve, where a value of `0.0`
		/// corresponds to the first vertex and a value of `1.0` corresponds
		/// to the last vertex.
		glm::vec3 point_at(float t) const
		{
			// Short-cut: is this the first or last vertex of the curve?
			if (t == 0.0f)
			{
				return vertices[0];
			}
			else if (t == 1.0f)
			{
				return vertices.back();
			}

			const auto desired_length = perimeter() * t;
			auto traversed = 0.0f;
			auto point = glm::vec3{ 10.0f, 0.0f, 0.0f };

			for (size_t i = 0; i < get_number_of_vertices(); ++i)
			{
				const auto segment = get_segment(i);
				traversed += segment.length();

				if (traversed >= desired_length)
				{
					// We know that the point lies on this segment somewhere...
					float along_segment = traversed - desired_length;
					point = segment.point_at((segment.length() - along_segment) / segment.length());
					break;
				}
			}

			return point;
		}

		PolygonalCurve refine(float minimum_segment_length, bool keep_existing_points = true) const
		{
			auto refined = PolygonalCurve{};

			float total_length = perimeter();
			for (float f = 0; f <= total_length; f += minimum_segment_length)
			{
				float percent = f / total_length;
				percent = fminf(percent, 1.0f);
				//std::cout << percent << std::endl;
				refined.push_vertex(point_at(percent));
			}

			//refined.push_vertex(vertices.back());

			//for (size_t i = 0; i < get_number_of_vertices() - 1; ++i)
			//{
			//	const auto segment = get_segment(i);

			//	// Add the first point
			//	refined.push_vertex(segment.get_start());

			//	// Calculate how many vertices we will need to add between the
			//	// start and end points of the original, unrefined segment
			//	size_t number_of_subdivisions = (segment.length() / minimum_segment_length);
			//	
			//	// Add extra vertices
			//	for (size_t division = 0; division < number_of_subdivisions; division++)
			//	{
			//		float t = static_cast<float>(division) / number_of_subdivisions;
			//		refined.push_vertex(segment.point_at(t));
			//	}

			//	// Finally, add the second point
			//	refined.push_vertex(segment.get_end());
			//}

			return refined;
		}

		/// Deletes all of the vertices that make up this curve.
		void clear()
		{
			vertices.clear();
		}

		/// Adds a new vertex `vertex` to the end of the curve.
		void push_vertex(const glm::vec3& vertex)
		{
			vertices.push_back(vertex);
		}

		/// Removes the last vertex from the curve.
		void pop_vertex()
		{
			vertices.pop_back();
		}

		/// Effectively "clears" this curve and sets its vertices to `other`.
		void set_vertices(const std::vector<glm::vec3>& other)
		{
			vertices = other;
		}
		
	private:

		std::vector<glm::vec3> vertices;

	};

	std::vector<glm::vec3> generate_tube(const PolygonalCurve& curve, float radius = 0.5f, size_t number_of_segments = 10)
	{
		const auto circle_normal = glm::vec3{ 0.0f, 1.0f, 0.0f };
		const auto circle_center = glm::vec3{};
		std::vector<glm::vec3> tube_vertices;

		auto v_prev = glm::vec3{ 0.0f, 0.0f, 0.0f };

		// Loop over all of the indices plus the last one to form a closed loop
		for (size_t i = 0; i < curve.get_number_of_vertices() + 1; ++i)
		{	
			size_t center_index = i;
			if (i == curve.get_number_of_vertices())
			{
				center_index = 0;
			}

			auto [neighbor_l_index, neighbor_r_index] = curve.get_neighboring_indices_wrapped(center_index);

			// Grab the current vertex plus its two neighbors
			auto center = curve.get_vertices()[center_index];
			auto neighbor_l = curve.get_vertices()[neighbor_l_index];
			auto neighbor_r = curve.get_vertices()[neighbor_r_index];

			auto towards_l = glm::normalize(neighbor_l - center); // Vector that points towards the left neighbor
			auto towards_r = glm::normalize(neighbor_r - center); // Vector that points towards the right neighbor
			

			// Calculate the tangent vector at the current point along the polyline
			auto diff = towards_r - towards_l;
			float l2 = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z; // glm::length(towards_r - towards_l)* glm::length(towards_r - towards_l);
			
			auto t = l2 > 0.0f ? glm::normalize(towards_r - towards_l) : -towards_l;

			// Calculate the next `u` basis vector: find an arbitrary vector perpendicular to the first tangent vector
			auto u = i == 0 ? glm::normalize(glm::cross(glm::vec3{ 0.0f, 0.0f, 1.0f }, t)) : glm::normalize(glm::cross(t, v_prev));


			// Calculate the next `v` basis vector
			auto v = glm::normalize(glm::cross(u, t));

			for (size_t segment = 0; segment < number_of_segments; segment++)
			{
				float theta = 2.0f * M_PI * (segment / static_cast<float>(number_of_segments));
				float x = radius * cosf(theta);
				float y = radius * sinf(theta);
				tube_vertices.push_back(u * x + v * y + center);
			}

			// Set the previous `v` vector to the current `v` vector (parallel transport)
			v_prev = v;
		}

		// Generate the final array of vertices, which are the triangles that enclose the
		// tube extrusion: for now, we don't use indexed rendering
		std::vector<glm::vec3> triangles;

		// The number of "rings" (i.e. circular cross-sections) that form the "skeleton" of the tube
		auto number_of_rings = tube_vertices.size() / number_of_segments;

		for (size_t ring_index = 0; ring_index < number_of_rings - 1; ring_index++)
		{
			auto next_ring_index = (ring_index + 1) % number_of_rings;

			for (size_t local_index = 0; local_index < number_of_segments; local_index++)
			{
				// Vertices are laid out in "rings" of `number_of_segments` vertices like
				// so (for `number_of_segments = 6`):
				//
				// 6  7  8  9  ...
				//
				// 0  1  2  3  4  5
				auto next_local_index = (local_index + 1) % number_of_segments;

				// First triangle: 0 -> 6 -> 7
				triangles.push_back(tube_vertices[ring_index * number_of_segments + local_index]);
				triangles.push_back(tube_vertices[next_ring_index * number_of_segments + local_index]); // The next ring
				triangles.push_back(tube_vertices[next_ring_index * number_of_segments + next_local_index]); // The next ring

				// Second triangle: 0 -> 7 -> 1
				triangles.push_back(tube_vertices[ring_index * number_of_segments + local_index]);
				triangles.push_back(tube_vertices[next_ring_index * number_of_segments + next_local_index]); // The next ring
				triangles.push_back(tube_vertices[ring_index * number_of_segments + next_local_index]);
			}
		}

		
		
		return triangles;
	}


}

#pragma once

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "polygonal_curve.h"

namespace knot
{

	// Abstract base class representing any type that can generate / build a knotted curve
	class Generator
	{

	public:

		virtual geom::PolygonalCurve generate_curve() const = 0;

	};

	// A direction (up, down, left, or right)
	enum class Direction
	{
		U,
		D,
		L,
		R
	};

	// An axial direction (row or column)
	enum class Axis
	{
		ROW,
		COL
	};

	// A cardinal direction (used for Cromwell moves)
	enum class Cardinal
	{
		NW,
		SW,
		NE,
		SE
	};

	// A grid cell entry (either x, o, or blank)
	enum class Entry
	{
		X,
		O,
		BLANK
	};

	/// Reference: http://peterforgacs.github.io/2017/06/25/Custom-C-Exceptions-For-Beginners/
	class CromwellException : std::exception
	{

	public:

		CromwellException(std::string message) :
			message{ message }
		{}

		const std::string& get_message() const
		{
			return message;
		}

	private:

		std::string message;

	};

	/// A struct representing a grid diagram corresponding to a particular knot (or the unknot)
	class Diagram: Generator
	{

	public:

		Diagram(std::vector<std::vector<Entry>> from_data) :
			data{ from_data }
		{
			validate();
		}

		Diagram(const std::string& from_csv)
		{
			std::cout << "Loading diagram from file:" << from_csv << std::endl;

			std::ifstream file;
			file.open(from_csv);

			// Count the number of lines in the file
			const auto number_of_lines = std::count(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), '\n') + 1;
			std::cout << "File contains " << number_of_lines << " lines" << std::endl;

			file.seekg(0, file.beg);

			// Read the file, line by line
			std::string line;
			while (std::getline(file, line))
			{
				// Read the line, splitting on commas
				std::istringstream split(line);
				std::string field;

				std::vector<Entry> row;

				while (getline(split, field, ','))
				{
					if (field == "x")
					{
						row.push_back(Entry::X);
						std::cout << field << ",";
					}
					else if (field == "o")
					{
						row.push_back(Entry::O);
						std::cout << field << ",";
					}
					else if (field == " ")
					{
						row.push_back(Entry::BLANK);
						std::cout << "_,";
					}
					else
					{
						throw std::runtime_error("Unknown entry - all entries should be 'x', 'o', or ' ' (blank)");
					}
				}
				std::cout << std::endl;

				data.push_back(row);
			}

			validate();
		}

		/// A move that cyclically translates a row or column in one of four directions: up, down, left, or right
		void apply_translation(Direction direction)
		{
			switch (direction)
			{
			case Direction::U:
			{
				// Move the first row to the end, push everything else up
				auto row = get_row(0);
				data.erase(data.begin());
				data.push_back(row);
				break;
			}
			case Direction::D:
			{
				// Move the last row to the start, push everything else down
				auto row = get_row(data.size() - 1);
				data.pop_back();
				data.insert(data.begin(), row);
				break;
			}
			case Direction::L:
			{
				// Remove the first element from each row, then push it onto the back
				for (auto& row : data)
				{
					auto entry = row[0];
					row.erase(row.begin());
					row.push_back(entry);
				}
				break;
			}
			case Direction::R:
			{
				// Remove the last element from each row, then push it onto the front
				for (auto& row : data)
				{
					auto entry = row[data.size() - 1];
					row.pop_back();
					row.insert(row.begin(), entry);
				}
				break;
			}
			}
		}

		/// A move that exchanges to adjacent, non-interleaved rows or columns
		void apply_commutation(Axis axis, size_t start_index)
		{
			// The last row (or column) doesn't have any adjacent row (or column) to swap with
			if (start_index == data.size() - 1)
			{
				throw CromwellException("Cannot exchange row or column with non-existing adjacent row or column");
			}

			// Commutation is only valid if the two rows (or columns) are not interleaved
			if (!are_interleaved(axis, start_index + 0, start_index + 1))
			{
				if (axis == Axis::ROW)
				{
					exchange_rows(start_index + 0, start_index + 1);
				}
				else
				{
					exchange_cols(start_index + 0, start_index + 1);
				}
			}
			else
			{
				throw CromwellException("The specified rows (or columns) are interleaved and cannot be exchanged");
			}
		}

		/// A move that replaces a non-"blank" entry with a 2x2 sub-grid
		void apply_stabilization(Cardinal cardinal, size_t i, size_t j)
		{
			auto original_entry = data[i][j];

			if (data[i][j] == Entry::BLANK)
			{
				throw CromwellException("There is no `x` or `o` at the specified grid position: stabilization cannot be performed");
			}

			// The cardinal directions below designate the corner of the new 2x2 sub-grid
			// that contains a "blank" cell (i.e. where the original entry resided)
			if (cardinal == Cardinal::NW || cardinal == Cardinal::SW)
			{
				// Insert a blank column to the R of the column in question
				for (auto& row : data)
				{
					row.insert(row.begin() + j + 1, Entry::BLANK);
				}
			}
			else // NE or SE
			{
				// Insert a blank column to the L of the column in question
				for (auto& row : data)
				{
					row.insert(row.begin() + j + 0, Entry::BLANK);
				}
			}

			std::vector<Entry> extra_row(data[0].size(), Entry::BLANK);

			// Stabilization replaces the original entry with a 2x2 sub-grid that will have 
			// 2 entries of one type, a single entry of the opposite type, and a single blank
			// cell
			//
			// For example, a NW:O stabilization would look like:
			//
			//		x---o   x----o
			//		-----   ----ox
			//		----x   ------
			//				----x-
			//
			const auto entry_double = original_entry;
			const auto entry_single = original_entry == Entry::X ? Entry::O : Entry::X;

			switch (cardinal)
			{
			case Cardinal::NW:
			{
				data[i][j + 0] = Entry::BLANK;
				data[i][j + 1] = entry_double;
				extra_row[j + 0] = entry_double;
				extra_row[j + 1] = entry_single;
				data.insert(data.begin() + i + 1, extra_row);
				break;
			}
			case Cardinal::SW:
			{
				data[i][j + 0] = Entry::BLANK;
				data[i][j + 1] = entry_double;
				extra_row[j + 0] = entry_double;
				extra_row[j + 1] = entry_single;
				data.insert(data.begin() + i + 0, extra_row);
				break;
			}
			case Cardinal::NE:
			{
				data[i][j + 0] = entry_double; // Technically, this is unnecessary
				data[i][j + 1] = Entry::BLANK;
				extra_row[j + 0] = entry_single;
				extra_row[j + 1] = entry_double;
				data.insert(data.begin() + i + 1, extra_row);
				break;
			}
			case Cardinal::SE:
			{
				data[i][j + 0] = entry_double; // Technically, this is unnecessary
				data[i][j + 1] = Entry::BLANK;
				extra_row[j + 0] = entry_single;
				extra_row[j + 1] = entry_double;
				data.insert(data.begin() + i + 0, extra_row);
				break;
			}
			}
		}

		/// A move that removes ("flattens") a 2x2 sub-grid
		void apply_destabilization(size_t i, size_t j)
		{
			if (i >= data.size() - 1 || j >= data.size() - 1)
			{
				throw CromwellException("Cannot destabilize at the specified grid position: out of bounds");
			}

			std::pair<size_t, size_t> index_of_blank;

			const auto ul = data[i + 0][j + 0];
			const auto ur = data[i + 0][j + 1];
			const auto ll = data[i + 1][j + 0];
			const auto lr = data[i + 1][j + 1];

			// Count the number of x's, o's, and blanks in the subgrid whose upper-left 
			// corner is <i, j>
			size_t number_of_xs = 0;
			size_t number_of_os = 0;
			size_t number_of_blanks = 0;
			auto cardinal = Cardinal::NW;
			for (size_t subgrid_i = 0; subgrid_i < 2; ++subgrid_i)
			{	
				for (size_t subgrid_j = 0; subgrid_j < 2; ++subgrid_j)
				{
					auto entry = data[i + subgrid_i][j + subgrid_j];

					if (entry == Entry::X)
					{
						number_of_xs++;
					}
					else if (entry == Entry::O)
					{
						number_of_os++;
					}
					else
					{
						number_of_blanks++;

						// Which "corner" of the subgrid does this blank entry reside in?
						if (subgrid_i == 0 && subgrid_j == 0) cardinal = Cardinal::NW;
						else if (subgrid_i == 1 && subgrid_j == 0) cardinal = Cardinal::SW;
						else if (subgrid_i == 0 && subgrid_j == 1) cardinal = Cardinal::NE;
						else if (subgrid_i == 1 && subgrid_j == 1) cardinal = Cardinal::SE;
					}
				}
			}

			auto entry_double = Entry::X;
			auto entry_single = Entry::O;

			if (number_of_xs == 2 && number_of_os == 1 && number_of_blanks == 1)
			{
				// Placeholder
			}
			else if (number_of_xs == 1 && number_of_os == 2 && number_of_blanks == 1)
			{
				entry_double = Entry::O;
				entry_single = Entry::X;
			}
			else
			{
				throw CromwellException("Trying to destabilize subgrid that doesn't have the appropriate number of x's, o's, and/or blank cells");
			}

			// Based on the cardinality found above, remove a full col + row from the grid and insert
			// the new entry at the old "blank" spot
			switch (cardinal)
			{
			case Cardinal::NW:
				data[i + 0][j + 0] = entry_double;
				data.erase(data.begin() + i + 1);
				for (auto& row : data)
				{
					row.erase(row.begin() + j + 1);
				}
				break;
			case Cardinal::SW:
				data[i + 1][j + 0] = entry_double;
				data.erase(data.begin() + i + 0);			
				for (auto& row : data)
				{
					row.erase(row.begin() + j + 1);
				}
				break;
			case Cardinal::NE:
				data[i + 0][j + 1] = entry_double;
				data.erase(data.begin() + i + 1);
				for (auto& row : data)
				{
					row.erase(row.begin() + j + 0);
				}
				break;
			case Cardinal::SE:
				data[i + 1][j + 1] = entry_double;
				data.erase(data.begin() + i + 0);
				for (auto& row : data)
				{
					row.erase(row.begin() + j + 0);
				}
				break;
			};
		}

		/// Returns a reference to this diagram's underlying data store
		const std::vector<std::vector<Entry>>& get_data() const
		{
			return data;
		}

		/// Returns the size (i.e. number of rows or number of cols) in this grid
		size_t get_size() const
		{
			return data.size();
		}

		/// Convenience function (the grid will always be square)
		size_t get_number_of_rows() const
		{
			return get_size();
		}

		/// Convenience function (the grid will always be square)
		size_t get_number_of_cols() const
		{
			return get_size();
		}

		/// Returns the entries in the row at index `row_index`
		std::vector<Entry> get_row(size_t row_index) const
		{
			validate_index(row_index);

			return data[row_index];
		}

		/// Returns the entries in the col at index `col_index`
		std::vector<Entry> get_col(size_t col_index) const
		{
			validate_index(col_index);

			std::vector<Entry> col;
			col.reserve(data.size());

			for (const auto& row : data)
			{
				col.push_back(row[col_index]);
			}

			return col;
		}

		/// Finds the indices of the `x` / `o` that occur in the specified row (or col)
		std::pair<size_t, size_t> find_indices_of_xo(Axis axis, size_t index) const
		{
			const auto x = find_index_of_first(axis, index, Entry::X);
			const auto o = find_index_of_first(axis, index, Entry::O);
			return { x, o };
		}

		/// Finds the index of the first occurence of `entry` in the specified row (or col)
		size_t find_index_of_first(Axis axis, size_t index, Entry entry) const
		{
			if (axis == Axis::ROW)
			{
				const auto row = get_row(index);
				return std::distance(row.begin(), std::find(row.begin(), row.end(), entry));
			}
			else
			{
				const auto col = get_col(index);
				return std::distance(col.begin(), std::find(col.begin(), col.end(), entry));
			}
		}

		/// Checks whether two rows (or cols) are interleaved, i.e. their projections onto the x-axis (or y-axis, respectively) overlap
		bool are_interleaved(Axis axis, size_t a, size_t b)
		{
			// Figure out the indices of x and o in each row (or col)
			auto [a_start, a_end] = find_indices_of_xo(axis, a);
			if (a_start > a_end)
			{
				std::swap(a_start, a_end);
			}

			auto [b_start, b_end] = find_indices_of_xo(axis, b);
			if (b_start > b_end)
			{
				std::swap(b_start, b_end);
			}

			if (a_start > b_start && a_end < b_end)
			{
				// `a` is completely contained in `b`
				return false;
			}
			else if (b_start > a_start && b_end < a_end)
			{
				// `b` is completely contained in `a`
				return false;
			}
			else if (a_end < b_start)
			{
				// `a` is totally "above" `b`
				return false;
			}
			else if (a_start > b_end)
			{
				// `a` is totally "below" `b`
				return false;
			}
			else if (b_end < a_start) {
				// `b` is totally "above" `a`
				return false;
			}
			else if (b_start > a_end) {
				// `b` is totally "below" `a`
				return false;
			}

			return true;
		}

		/// Generates a polygonal curve (polyline) that represents the topological structure of this grid diagram
		geom::PolygonalCurve generate_curve() const override
		{
			// First, get the row or column corresponding to the index where the last
			// row or column ended
			//
			// Note that:
			//
			// Cols are connected: x -> o
			// Rows are connected: o -> x
			//
			// We use the convention that vertical strands always cross OVER horizontal
			// strands
			auto [s, e] = find_indices_of_xo(Axis::COL, 0);
			auto tie = s;

			// Absolute indices of all of the grid cells that form the "path" of this knot
			std::vector<size_t> indices = 
			{
				convert_to_absolute_index(s, 0),
				convert_to_absolute_index(e, 0)
			};

			bool keep_going = true;
			bool traverse_horizontal = true;

			while (keep_going)
			{
				// There are two scenarios to consider:
				// - We just found an `o` (in the last column), so find the `x` in this row
				// - We just found an `x` (in the last row), so find the `o` in this column
				const size_t next_index = traverse_horizontal ? find_index_of_first(Axis::ROW, e, Entry::X) : find_index_of_first(Axis::COL, e, Entry::O);

				// Convert the above index to absolute indices that range from `0..data.size()^2`,
				// taking care to modify the function parameters based on the current orientation (horizontal / vertical)
				const size_t absolute_index = traverse_horizontal ? convert_to_absolute_index(e, next_index) : convert_to_absolute_index(next_index, e);

				// Push back the new endpoint and check to see whether we have finished traversing the entire knot
				if (!std::count(indices.begin(), indices.end(), absolute_index))
				{
					indices.push_back(absolute_index);
				}
				else
				{
					// We are at the end
					indices.push_back(tie);
					keep_going = false;
				}

				// Set "start" and "end"
				s = e;
				e = next_index;

				// Switch directions
				traverse_horizontal = !traverse_horizontal;
			}

			std::cout << "Indices (before inserting crossings): ";
			for (size_t i = 0; i < indices.size(); ++i)
			{
				std::cout << indices[i];
				if (i != indices.size() - 1)
				{
					std::cout << ", ";
				}
			}
			std::cout << std::endl;

			// If we want to traverse just rows or just columns, we can simply use the underlying knot
			// topology and ignore either the first or last element
			auto rows = indices;
			auto cols = indices;
			rows.erase(rows.begin());
			cols.pop_back();

			// This should always be true, i.e. for a 6x6 grid there should be 6 pairs of x's and o's (12
			// indices total)...note that we perform this check before checking for any crossings, which
			// will necessarily add more indices to the knot topology
			if (indices.size() != data.size() * 2 + 1)
			{
				throw std::runtime_error("Error when constructing curve");
			}

			// Find crossings: rows pass under any columns that they intersect, so we will
			// add additional vertex (or vertices) to any column that contains a intersection(s)
			// and "lift" this vertex (or vertices) along the z-axis
			std::vector<size_t> lifted;

			const auto num_col_chunks = static_cast<size_t>(cols.size() / 2);
			const auto num_row_chunks = static_cast<size_t>(rows.size() / 2);

			for (size_t i = 0; i < num_col_chunks; ++i)
			{
				size_t col_s = cols[i * 2 + 0];
				size_t col_e = cols[i * 2 + 1];

				bool oriented_upwards = false;

				// If this condition is `true`, then the column is oriented from bottom to
				// top (i.e. "upwards") - we do this so that it is "easier" to tell whether
				// or not a row intersects a column (see below)
				if (col_s > col_e)
				{
					std::swap(col_s, col_e);
					oriented_upwards = true;
				}

				const auto [cs_i, cs_j] = convert_to_grid_indices(col_s);
				const auto [ce_i, ce_j] = convert_to_grid_indices(col_e);

				// A list of all intersections along this column
				std::vector<std::pair<size_t, size_t>> intersections;

				for (size_t j = 0; j < num_row_chunks; ++j)
				{
					size_t row_s = rows[j * 2 + 0];
					size_t row_e = rows[j * 2 + 1];

					if (row_s > row_e)
					{
						std::swap(row_s, row_e);
					}

					const auto [rs_i, rs_j] = convert_to_grid_indices(row_s);
					const auto [re_i, re_j] = convert_to_grid_indices(row_e);

					if (cs_j > rs_j&& cs_j < re_j && cs_i < rs_i && ce_i > rs_i)
					{
						const size_t intersect = convert_to_absolute_index(rs_i, cs_j);
						intersections.push_back({ rs_i, intersect });

						lifted.push_back(intersect);
					}
				}

				// Sort on the row `i` index (i.e. sort vertically, from top to bottom of the table grid)
				std::sort(intersections.begin(), intersections.end());

				// If the start / end indices of this column were flipped before, we have to reverse the
				// order in which we insert the crossings here as well
				if (!oriented_upwards)
				{
					std::reverse(intersections.begin(), intersections.end());
				}

				for (size_t put = 0; put < indices.size(); ++put)
				{
					size_t node = indices[put];

					if (node == col_s || node == col_e)
					{
						for (const auto& intersect : intersections)
						{
							indices.insert(indices.begin() + put + 1, intersect.second);
						}
						break;
					}
				}
			}

			std::cout << "Indices (after inserting crossings): ";
			for (size_t i = 0; i < indices.size(); ++i)
			{
				std::cout << indices[i];
				if (i != indices.size() - 1)
				{
					std::cout << ", ";
				}
			}
			std::cout << std::endl;

			// Ex: old topology vs. new topology (after crossings are inserted)
			//
			// `[1, 4, 28, __, 26, 8, _, 6, 18, __, 21, 33, 35, 17, __, __, 13, 1]`
			// `[1, 4, 28, 27, 26, 8, 7, 6, 18, 20, 21, 33, 35, 17, 16, 14, 13, 1]`

			// Convert indices to actual 3D positions so that we can
			// (eventually) draw a polyline corresponding to this knot: the
			// world-space width and height of the 3D grid are automatically
			// set to the resolution of the diagram so that each grid "cell"
			// is unit width / height
			const float w = static_cast<float>(data.size());
			const float h = static_cast<float>(data.size());

			static const float lift_amount = 1.0f;

			// World-space position of the vertex corresponding to this grid index:
			// make sure that the center of the grid lies at the origin
			auto get_coordinate = [&](size_t i, size_t j, bool lift)
			{
				const float x = (j / static_cast<float>(data.size()))* w - 0.5f * w;
				const float y = h - (i / static_cast<float>(data.size())) * h - 0.5f * h;
				const float z = lift ? lift_amount : 0.0f;

				return glm::vec3{ x, y, z };
			};

			std::pair<size_t, size_t> prev_indices = { -1, -1 };
			std::vector<glm::vec3> points;

			for (const auto& absolute_index : indices)
			{
				// Remember:
				// `i` is the row, ranging from `[0..data.size()]`
				// `j` is the col, ranging from `[0..data.size()]`
				const auto [i, j] = convert_to_grid_indices(absolute_index);
				std::cout << "Processing grid cell " << i << ", " << j << std::endl;

				if (prev_indices.first != -1 && prev_indices.second != -1)
				{
					if (prev_indices.first == i)
					{
						std::cout << "\tAdding new points along row\n";
						// Curr and prev are part of the same row - add "filler" points along row (`j`)

						if (prev_indices.second > j)
						{
							// The prev cell is D from curr cell
							for (size_t filler_index = prev_indices.second - 1; filler_index > j; filler_index--)
							{
								points.push_back(get_coordinate(i, filler_index, false));
							}
						}
						else
						{
							// The prev cell is U from curr cell
							for (size_t filler_index = prev_indices.second + 1; filler_index < j; filler_index++)
							{
								points.push_back(get_coordinate(i, filler_index, false));
							}
						}
					}
					else
					{
						std::cout << "\tAdding new points along col\n";
						// Curr and prev are part of the same col - add "filler" points along col (`i`)

						if (prev_indices.first > i)
						{
							// The prev cell is to the R of the curr cell
							for (size_t filler_index = prev_indices.first - 1; filler_index > i; filler_index--)
							{
								points.push_back(get_coordinate(filler_index, j, false));
							}
						}
						else
						{
							// The prev cell is to the L of the curr cell
							for (size_t filler_index = prev_indices.first + 1; filler_index < i; filler_index++)
							{
								points.push_back(get_coordinate(filler_index, j, false));
							}
						}
					}
				}

				// World-space position of the vertex corresponding to this grid index:
				// make sure that the center of the grid lies at the origin
				const bool lift = std::count(lifted.begin(), lifted.end(), absolute_index);
				const auto coord = get_coordinate(i, j, lift);

				points.push_back(coord);

				prev_indices = { i, j };
			}

			points.pop_back();

			return geom::PolygonalCurve{ points };
		}

	private:

		void set_row(size_t row_index, const std::vector<Entry>& row)
		{
			validate_index(row_index);

			if (row.size() != data.size())
			{
				throw std::runtime_error("Invalid row size");
			}

			data[row_index] = row;
		}

		void set_col(size_t col_index, const std::vector<Entry>& col)
		{
			validate_index(col_index);

			if (col.size() != data.size())
			{
				throw std::runtime_error("Invalid col size");
			}

			for (size_t row_index = 0; row_index < data.size(); ++row_index)
			{
				data[row_index][col_index] = col[row_index];
			}
		}

		void exchange_rows(size_t a, size_t b)
		{
			validate_index(a);
			validate_index(b);

			// Swap entire vectors (each of which is a "row") in one go
			data[a].swap(data[b]);
		}

		void exchange_cols(size_t a, size_t b)
		{
			validate_index(a);
			validate_index(b);

			for (size_t row_index = 0; row_index < data.size(); ++row_index)
			{
				std::swap(data[row_index][a], data[row_index][b]);
			}
		}

		size_t convert_to_absolute_index(size_t i, size_t j) const
		{
			return i + j * data.size();
		}

		std::pair<size_t, size_t> convert_to_grid_indices(size_t absolute_index) const
		{
			return { absolute_index % data.size(), absolute_index / data.size() };
		}

		void validate() const
		{
			for (size_t i = 0; i < data.size(); ++i)
			{
				const auto row = get_row(i);
				const auto col = get_col(i);

				if (row.size() != data.size() ||
					col.size() != data.size() ||
					std::count(row.begin(), row.end(), Entry::X) != 1 ||
					std::count(row.begin(), row.end(), Entry::O) != 1 ||
					std::count(col.begin(), col.end(), Entry::X) != 1 ||
					std::count(col.begin(), col.end(), Entry::O) != 1)
				{
					throw std::runtime_error("Invalid grid diagram - check that each row and each column contain exactly one 'x' and one 'o' entry");
				}
			}
		}

		void validate_index(size_t index) const
		{
			if (index < 0 || index > data.size())
			{
				throw std::runtime_error("Invalid index");
			}
		}

		// The grid data (i.e. a 2D array of x's, o's, and blank cells)
		std::vector<std::vector<Entry>> data;

	};

}
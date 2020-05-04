#pragma once

#include "diagram.h"

namespace utils
{
	using namespace knot;

	inline const char* to_string(Direction direction)
	{
		switch (direction)
		{
		case Direction::U: return "up";
		case Direction::D: return "down";
		case Direction::L: return "left";
		case Direction::R: return "right";
		default: return "uknown";
		}

	}
	inline const char* to_string(Entry entry)
	{
		switch (entry)
		{
		case Entry::X: return "x";
		case Entry::O: return "o";
		case Entry::BLANK: return " ";
		default: return "uknown";
		}
	}

	inline const char* to_string(Axis axis)
	{
		switch (axis)
		{
		case Axis::ROW: return "row";
		case Axis::COL: return "col";
		default: return "uknown";
		}
	}

	inline const char* to_string(Cardinal cardinal)
	{
		switch (cardinal)
		{
		case Cardinal::NW: return "NW";
		case Cardinal::SW: return "SE";
		case Cardinal::SE: return "SE";
		case Cardinal::NE: return "NE";
		default: return "uknown";
		}
	}

	inline const char* to_string(MessageType entry)
	{
		switch (entry)
		{
		case MessageType::WARNING: return "[Warning]";
		case MessageType::ERROR: return "[Error]";
		case MessageType::INFO: return "[Info]";
		default: return "uknown";
		}
	}

}
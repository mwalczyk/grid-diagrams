#pragma once

#include <string>
#include <vector>

namespace utils
{

	enum class MessageType
	{
		WARNING,
		ERROR,
		INFO
	};

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


	class History
	{

	public:

		History(size_t maximum_messages_to_retain = 10) :
			maximum_messages_to_retain{ maximum_messages_to_retain }
		{
		}

		void push(const std::string& message, MessageType message_type)
		{
			messages.push_back({ message, message_type });

			// Erase the first message
			if (messages.size() > maximum_messages_to_retain)
			{
				messages.erase(messages.begin());
			}
		}

		void clear()
		{
			messages.clear();
		}

		const std::vector<std::pair<std::string, MessageType>>& get_messages() const
		{
			return messages;
		}

	private:

		size_t maximum_messages_to_retain;
		std::vector<std::pair<std::string, MessageType>> messages;

	};

}
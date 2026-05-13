#pragma once

struct CheatCommandParams
{
	std::vector<std::string> stringParams;
	std::vector<int64_t> numericParams;
};

template<class T>
class CheatCommandProcessor
{
public:
	using Handler = std::function<Result(T&, CheatCommandParams&)>;

	CheatCommandProcessor()
	{

	}

	virtual ~CheatCommandProcessor()
	{

	}

	bool Add(const std::string& _command, Handler&& _handler, std::string&& _explain)
	{
		auto ret = m_handlers.emplace(_command, Command{ std::move(_handler), std::move(_explain) });
		return ret.second;
	}

	Result Process(T& _session, const std::string& _command, CheatCommandParams& _params)
	{
		auto found = m_handlers.find(_command);
		if (found == m_handlers.end())
		{
			EError::UnknownCommand;
		}

		auto result = found->second.handler(_session, _params);
		if (!result && result.message.empty())
		{
			result.message = found->second.explain;
		}
		return std::move(result);
	}

protected:
	struct Command
	{
		Handler handler;
		std::string explain;
	};

	std::unordered_map<std::string, Command> m_handlers;
};
#pragma once

enum class EShutdownMode : uint8_t
{
	None = 0,
	RightNow,
	CurrentJob,
	EmptyJob
};

class UseShutdown
{
public:
	UseShutdown() = default;
	virtual ~UseShutdown()
	{
		m_beforeShutdown = nullptr;
		m_afterShutdown = nullptr;
	}

	virtual void Shutdown(const EShutdownMode _shutdownMode, const char* _msg = nullptr)
	{
		if (IsShutdown())
		{
			return;
		}

		if (m_beforeShutdown && not m_beforeShutdown(_shutdownMode))
		{
			return;
		}

		m_shutdownMode = _shutdownMode;
		if (_msg)
		{
			Log("Shutdown : {}", _msg);
		}

		if (m_afterShutdown)
		{
			m_afterShutdown();
		}
	}
	inline bool IsShutdown() const { return (EShutdownMode::None != m_shutdownMode.load(std::memory_order_relaxed)); }

protected:
	std::atomic<EShutdownMode> m_shutdownMode{ EShutdownMode::None };

	std::function<Result(const EShutdownMode)> m_beforeShutdown;
	std::function<void()> m_afterShutdown;
};
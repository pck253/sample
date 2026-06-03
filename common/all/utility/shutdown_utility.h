#pragma once

class UseShutdown
{
public:
	UseShutdown() = default;
	virtual ~UseShutdown()
	{
		m_beforeShutdown = nullptr;
		m_afterShutdown = nullptr;
	}

	virtual void Shutdown(const char* _msg = nullptr)
	{
		if (IsShutdown())
		{
			return;
		}

		if (m_beforeShutdown)
		{
			m_beforeShutdown();
		}

		m_isShutdown.store(true, std::memory_order_relaxed);
		if (_msg)
		{
			Log("Shutdown : {}", _msg);
		}

		if (m_afterShutdown)
		{
			m_afterShutdown();
		}
	}
	inline bool IsShutdown() const { return m_isShutdown; }

protected:
	std::atomic<bool> m_isShutdown{ false };

	std::function<void()> m_beforeShutdown;
	std::function<void()> m_afterShutdown;
};
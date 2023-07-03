#pragma once

class FinalJob
{
public:
	FinalJob(std::function<void()>&& _job) : m_job(std::move(_job)) {};
	virtual ~FinalJob() { if (m_job) m_job(); }

private:
	std::function<void()> m_job;
};

class JobBase
{
public:
	JobBase() = default;
	JobBase(const JobBase& _other) = default;
	JobBase(JobBase&& _other) = default;
	virtual ~JobBase() = default;

	JobBase& operator=(const JobBase& _other) = default;
	JobBase& operator=(JobBase&& _other) = default;

	virtual void Process() = 0;
};
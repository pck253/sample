#pragma once

static_assert(WEB_MODULE == 1);

class Web : public Module
{
    friend Module;
public:
    Web(const std::string& _configFilePath);
    ~Web();

    virtual bool IsBusinessModule() final { return false; }
    virtual EModule GetModuleType() final { return EModule::Web; }

    virtual void Shutdown() final;

    Restful& GetRestful() { return m_restful; }

private:
    virtual Result InitImpl() final;

private:
    Restful m_restful;
};
#pragma once

namespace HTTP
{
  class Request
  {
  public:
    Request(std::string &&url) : m_URL(std::move(url)) {}
    Request(std::string &&url, const std::string &payload) : m_URL(std::move(url)), m_Payload(payload) {}

    bool GET();
    bool POST();
    bool Stream();

    const std::string &GetURL() { return m_URL; }
    const std::string &GetPayload() { return m_Payload; }
    const std::string &GetResponse() { return m_Response; }

  private:
    std::string m_URL, m_Payload, m_Response;
    int m_Status;
  };

  void Init();

  void StartServer();
  void StopServer();

  bool StatusOK(int);
  bool IsRedirect(int);
};

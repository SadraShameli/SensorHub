#pragma once

namespace HTTP
{
  class Request
  {
  public:
    Request(const char *url) : m_URL(url) { RemoveSlash(); }
    Request(const std::string &url) : m_URL(url) { RemoveSlash(); }
    Request(std::string &&url) : m_URL(std::move(url)) { RemoveSlash(); }

    bool GET();
    bool POST(const std::string &);
    bool Stream(const char *);

    const std::string &GetURL() { return m_URL; }
    const std::string &GetResponse() { return m_Response; }

  private:
    void RemoveSlash()
    {
      if (m_URL.back() == '/')
        m_URL.pop_back();
    }

  private:
    std::string m_URL, m_Response;
    int m_Status;
  };

  void Init();

  void StartServer();
  void StopServer();

  bool StatusOK(int);
  bool IsRedirect(int);
};

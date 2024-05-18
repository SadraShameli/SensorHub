#pragma once

class HTTP
{
public:
  static void Init();
  static void StartServer();
  static void StopServer();
};

class Request
{
public:
  Request(const std::string &url) { m_URL = url; }
  Request(const std::string &url, const std::string &payload) : m_URL(url), m_Payload(payload) {}

  bool GET();
  bool POST();
  bool Stream();

  const std::string &GetURL() { return m_URL; }
  const std::string &GetPayload() { return m_Payload; }
  const std::string &GetResponse() { return m_Response; }

  static bool StatusOK(int);
  static bool IsRedirect(int);

private:
  std::string m_URL, m_Payload, m_Response;
  int m_Status;
};
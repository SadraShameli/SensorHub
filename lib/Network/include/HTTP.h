#pragma once

class HTTP
{
public:
  static bool Init();
  static bool StartServer();
  static bool StopServer();
  static bool GET(const char *, std::string &);
  static bool POST(const char *, std::string &);
  static bool Stream(const char *, const char *);
};
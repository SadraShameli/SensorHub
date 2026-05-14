#pragma once

namespace HTTP {

namespace Status {

enum class StatusCode {

    Continue = 100,
    SwitchingProtocols = 101,
    Processing = 102,

    OK = 200,
    Created = 201,
    Accepted = 202,
    NonAuthoritativeInformation = 203,
    NoContent = 204,
    ResetContent = 205,
    PartialContent = 206,

    MultipleChoices = 300,
    MovedPermanently = 301,
    Found = 302,
    SeeOther = 303,
    NotModified = 304,
    TemporaryRedirect = 307,
    PermanentRedirect = 308,

    BadRequest = 400,
    Unauthorized = 401,
    PaymentRequired = 402,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    NotAcceptable = 406,
    ProxyAuthenticationRequired = 407,
    RequestTimeout = 408,
    Conflict = 409,
    Gone = 410,

    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503,
    GatewayTimeout = 504,
    HTTPVersionNotSupported = 505
};

inline bool IsInformational(StatusCode code) {
    return code >= StatusCode::Continue && code <= StatusCode::Processing;
}

inline bool IsSuccess(StatusCode code) {
    return code >= StatusCode::OK && code <= StatusCode::PartialContent;
}

inline bool IsRedirection(StatusCode code) {
    return code >= StatusCode::MultipleChoices &&
           code <= StatusCode::PermanentRedirect;
}

inline bool IsClientError(StatusCode code) {
    return code >= StatusCode::BadRequest && code <= StatusCode::Gone;
}

inline bool IsServerError(StatusCode code) {
    return code >= StatusCode::InternalServerError &&
           code <= StatusCode::HTTPVersionNotSupported;
}

}

class Request {
   public:
    Request(const char* url) : m_URL(url) { RemoveSlash(); }

    Request(const std::string& url) : m_URL(url) { RemoveSlash(); }

    Request(std::string&& url) : m_URL(std::move(url)) { RemoveSlash(); }

    bool GET();
    bool POST(const std::string&);
    bool Stream(const char*);

    const std::string& GetURL();
    const std::string& GetResponse();

   private:
    void RemoveSlash();

   private:
    std::string m_URL, m_Response;
    int m_Status = 0;
};

void Init();

void StartServer();
void StopServer();

};

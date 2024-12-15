#pragma once

/**
 * @namespace
 * @brief A namespace for managing HTTP related functionality.
 */
namespace HTTP {

/**
 * @namespace
 * @brief A namespace for managing HTTP status codes.
 */
namespace Status {

/**
 * @enum
 * @brief Represents HTTP status codes.
 */
enum class StatusCode {
    // Informational responses
    Continue = 100,
    SwitchingProtocols = 101,
    Processing = 102,

    // Success
    OK = 200,
    Created = 201,
    Accepted = 202,
    NonAuthoritativeInformation = 203,
    NoContent = 204,
    ResetContent = 205,
    PartialContent = 206,

    // Redirection
    MultipleChoices = 300,
    MovedPermanently = 301,
    Found = 302,
    SeeOther = 303,
    NotModified = 304,
    TemporaryRedirect = 307,
    PermanentRedirect = 308,

    // Client Errors
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

    // Server Errors
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503,
    GatewayTimeout = 504,
    HTTPVersionNotSupported = 505
};

/**
 * @brief Checks if the given HTTP status code is informational.
 *
 * Informational status codes are in the range `100-199`.
 *
 * @param code The HTTP status code to check.
 * @return `true` if the status code is informational, `false` otherwise.
 */
inline bool IsInformational(StatusCode code) {
    return code >= StatusCode::Continue && code <= StatusCode::Processing;
}

/**
 * @brief Checks if the given HTTP status code is a success.
 *
 * Success status codes are in the range `200-299`.
 *
 * @param code The HTTP status code to check.
 * @return `true` if the status code is a success, `false` otherwise.
 */
inline bool IsSuccess(StatusCode code) {
    return code >= StatusCode::OK && code <= StatusCode::PartialContent;
}

/**
 * @brief Checks if the given HTTP status code is a redirection.
 *
 * Redirection status codes are in the range `300-399`.
 *
 * @param code The HTTP status code to check.
 * @return `true` if the status code is a redirection, `false` otherwise.
 */
inline bool IsRedirection(StatusCode code) {
    return code >= StatusCode::MultipleChoices &&
           code <= StatusCode::PermanentRedirect;
}

/**
 * @brief Checks if the given HTTP status code is a client error.
 *
 * Client error status codes are in the range `400-499`.
 *
 * @param code The HTTP status code to check.
 * @return `true` if the status code is a client error, `false` otherwise.
 */
inline bool IsClientError(StatusCode code) {
    return code >= StatusCode::BadRequest && code <= StatusCode::Gone;
}

/**
 * @brief Checks if the given HTTP status code is a server error.
 *
 * Server error status codes are in the range `500-599`.
 *
 * @param code The HTTP status code to check.
 * @return `true` if the status code is a server error, `false` otherwise.
 */
inline bool IsServerError(StatusCode code) {
    return code >= StatusCode::InternalServerError &&
           code <= StatusCode::HTTPVersionNotSupported;
}

}  // namespace Status

/**
 * @class
 * @brief Represents an HTTP request.
 */
class Request {
   public:
    /**
     * @brief Constructs a new Request object with the specified URL.
     *
     * This constructor initializes the Request object with the given URL as a
     * `const char *` and calls the `RemoveSlash()` method to process the URL.
     *
     * @param url The URL to be used for the request.
     */
    Request(const char *url) : m_URL(url) { RemoveSlash(); }

    /**
     * @brief Constructs a new Request object with the specified URL.
     *
     * This constructor initializes the Request object with the given URL as a
     * `const std::string&` and calls the `RemoveSlash()` method to process the
     * URL.
     *
     * @param url The URL to be used for the request.
     */
    Request(const std::string &url) : m_URL(url) { RemoveSlash(); }

    /**
     * @brief Constructs a new Request object with the specified URL.
     *
     * This constructor initializes the Request object with the given URL as a
     * `std::string &&` and calls the `RemoveSlash()` method to process the URL.
     *
     * @param url The URL to be used for the request.
     */
    Request(std::string &&url) : m_URL(std::move(url)) { RemoveSlash(); }

    bool GET();
    bool POST(const std::string &);
    bool Stream(const char *);

    const std::string &GetURL();
    const std::string &GetResponse();

   private:
    void RemoveSlash();

   private:
    std::string m_URL, m_Response;
    int m_Status;
};

void Init();

void StartServer();
void StopServer();

};  // namespace HTTP

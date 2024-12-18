#include <dirent.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "Backend.h"
#include "Failsafe.h"
#include "HTTP.h"
#include "Storage.h"
#include "WiFi.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"

namespace HTTP {

#define UNIT_DISABLE_FAVICON
#define PARTITION_NAME "web"
#define FOLDER_PATH "/" PARTITION_NAME
#define FILE_PATH "_binary_"

#define FILE_PATH_MAX (15 + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define MAX_FILE_SIZE (512 * 1024)
#define MAX_FILE_SIZE_STR "512KB"
#define SCRATCH_BUFSIZE 8192

struct file_server_data {
    char base_path[16];
    char scratch[SCRATCH_BUFSIZE];
};

static const char* TAG = "HTTP Server";
static httpd_handle_t s_Server = nullptr;

/**
 * @brief Extracts the path from a given URI and appends it to a base path.
 *
 * This function takes a URI and extracts the path component, excluding any
 * query parameters or fragment identifiers. It then appends this path to a
 * specified base path and stores the result in the destination buffer.
 *
 * @param dest The destination buffer where the resulting path will be stored.
 * @param base_path The base path to which the extracted path will be appended.
 * @param uri The URI from which the path component will be extracted.
 * @param destsize The size of the destination buffer.
 * @return A pointer to the end of the base path in the destination buffer, or
 * `nullptr` if the resulting path exceeds the size of the destination buffer.
 */
static const char* get_path_from_uri(char* dest, const char* base_path,
                                     const char* uri, size_t destsize) {
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char* quest = strchr(uri, '?');
    if (quest != nullptr) {
        pathlen = MIN(pathlen, quest - uri);
    }

    const char* hash = strchr(uri, '#');
    if (hash != nullptr) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        return nullptr;
    }

    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    return dest + base_pathlen;
}

/**
 * @brief Sends an HTML response containing the contents of a directory.
 *
 * This function generates an HTML response that lists the contents of the
 * specified directory. It first opens the directory, then iterates over each
 * entry, appending its name and size to the response. If the directory does not
 * exist, it sends a `404 Not Found` error response.
 *
 * @param req The HTTP request object.
 * @param dirpath The path to the directory to be listed.
 * @return `ESP_OK` on success, `ESP_FAIL` on failure.
 */
static esp_err_t http_resp_dir_html(httpd_req_t* req, const char* dirpath) {
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];

    struct dirent* entry;
    struct stat entry_stat;

    DIR* dir = opendir(dirpath);
    if (dir == nullptr) {
        httpd_resp_send_err(req,
                            HTTPD_404_NOT_FOUND,
                            "Directory does not exist");

        return ESP_FAIL;
    }

    const size_t dirpath_len = strlen(dirpath);
    strlcpy(entrypath, dirpath, sizeof(entrypath));

    extern const char upload_script_start[] asm(FILE_PATH "index_html_start");
    extern const char upload_script_end[] asm(FILE_PATH "index_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);

    httpd_resp_send(req, upload_script_start, upload_script_size);

    while ((entry = readdir(dir)) != nullptr) {
        strlcpy(entrypath + dirpath_len,
                entry->d_name,
                sizeof(entrypath) - dirpath_len);

        if (stat(entrypath, &entry_stat) == -1) {
            continue;
        }

        sprintf(entrysize, "%ld", entry_stat.st_size);
    }

    closedir(dir);

    return ESP_OK;
}

/**
 * @brief HTTP GET handler for the index.html page.
 *
 * This function handles HTTP GET requests for the `index.html` page by
 * sending a `307 Temporary Redirect` response to the client, redirecting
 * them to the root URL `"/"`.
 *
 * @param req Pointer to the HTTP request.
 * @return `ESP_OK` on success.
 */
static esp_err_t index_html_get_handler(httpd_req_t* req) {
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, nullptr, 0);

    return ESP_OK;
}

/**
 * @brief Checks if a filename has a specific extension.
 *
 * This function compares the extension of a filename with a specified extension
 * string. It does this by comparing the last characters of the filename with
 * the extension string, ignoring case.
 *
 * @param filename The filename to check.
 * @param ext The extension to compare.
 * @return `true` if the filename has the specified extension, `false`
 * otherwise.
 */
#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/**
 * @brief Sets the content type of an HTTP response based on the file extension.
 *
 * This function sets the content type of an HTTP response based on the file
 * extension of the specified filename. It supports the following file types:
 *
 * - `PDF` application/pdf
 *
 * - `HTML` text/html
 *
 * - `JPEG` image/jpeg
 *
 * - `PNG` image/png
 *
 * - `ICO` image/x-icon
 *
 * If the file extension is not recognized, it sets the content type to
 * `text/plain`.
 *
 * @param req Pointer to the HTTP request.
 * @param filename The filename to check.
 * @return `ESP_OK` on success, `ESP_FAIL` on failure.
 */
static esp_err_t set_content_type_from_file(httpd_req_t* req,
                                            const char* filename) {
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    }

    else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    }

    else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    }

    else if (IS_FILE_EXT(filename, ".png")) {
        return httpd_resp_set_type(req, "image/png");
    }

    else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }

    return httpd_resp_set_type(req, "text/plain");
}

#ifndef UNIT_DISABLE_FAVICON
/**
 * @brief HTTP GET handler for the `favicon.ico` file.
 *
 * This function handles HTTP GET requests for the `favicon.ico` file by
 * sending the contents of the file to the client with the content type
 * `image/x-icon`.
 *
 * @param req Pointer to the HTTP request.
 * @return `ESP_OK` on success.
 */
static esp_err_t favicon_get_handler(httpd_req_t* req) {
    extern const unsigned char favicon_ico_start[] asm(FILE_PATH
                                                       "favicon_ico_start");
    extern const unsigned char favicon_ico_end[] asm(FILE_PATH
                                                     "favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);

    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char*)favicon_ico_start, favicon_ico_size);

    return ESP_OK;
}
#endif

/**
 * @brief HTTP GET handler for downloading files.
 *
 * This function handles HTTP GET requests for downloading files by sending the
 * contents of the specified file to the client. It first checks if the filename
 * is a directory and sends an HTML response listing the contents of the
 * directory. If the filename is a file, it sends the contents of the file to
 * the client in chunks.
 *
 * @param req Pointer to the HTTP request.
 * @return `ESP_OK` on success, `ESP_FAIL` on failure.
 */
static esp_err_t download_get_handler(httpd_req_t* req) {
    char filepath[FILE_PATH_MAX];
    FILE* fd = nullptr;
    struct stat file_stat;

    const char* filename =
        get_path_from_uri(filepath,
                          ((struct file_server_data*)req->user_ctx)->base_path,
                          req->uri,
                          sizeof(filepath));

    if (filename == nullptr) {
        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Filename too long");
        return ESP_FAIL;
    }

    if (filename[strlen(filename) - 1] == '/') {
        return http_resp_dir_html(req, filepath);
    }

    if (stat(filepath, &file_stat) == -1) {
        if (strcmp(filename, "/index.html") == 0) {
            return index_html_get_handler(req);
        }
#ifndef UNIT_DISABLE_FAVICON
        else if (strcmp(filename, "/favicon.ico") == 0) {
            return favicon_get_handler(req);
        }
#endif
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");

        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (fd == nullptr) {
        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Reading existing file failed");

        return ESP_FAIL;
    }

    set_content_type_from_file(req, filename);

    char* chunk = ((struct file_server_data*)req->user_ctx)->scratch;
    size_t chunksize;

    do {
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                httpd_resp_sendstr_chunk(req, nullptr);
                httpd_resp_send_err(req,
                                    HTTPD_500_INTERNAL_SERVER_ERROR,
                                    "Sending file failed");

                return ESP_FAIL;
            }
        }

    } while (chunksize != 0);

    fclose(fd);
    httpd_resp_send_chunk(req, nullptr, 0);

    return ESP_OK;
}

/**
 * @brief HTTP POST handler for setting up the device configuration.
 *
 * This function handles HTTP POST requests for setting up the device
 * configuration. It reads the JSON payload from the request and passes it to
 * the `Backend::SetupConfiguration` function for processing. If the
 * configuration is successfully set up, it sends an empty response to the
 * client with status `200 OK`. If the configuration setup fails, it sends an
 * empty response with status `400 Bad Request`.
 *
 * @param req Pointer to the HTTP request.
 * @return `ESP_OK` on success, `ESP_FAIL` on failure.
 */
static esp_err_t config_handler(httpd_req_t* req) {
    int ret = 0, remaining = req->content_len;
    std::string http_payload(remaining, 0);

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, http_payload.data(), remaining)) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }

            return ESP_FAIL;
        }

        remaining -= ret;
    }

    httpd_resp_send_chunk(req, nullptr, 0);
    Backend::SetupConfiguration(http_payload);

    return ESP_OK;
}

/**
 * @brief Starts the HTTP server.
 *
 * This function starts the HTTP server on the IP address of the device. It
 * registers the URI handlers for downloading files and setting up the device
 * configuration.
 */
void StartServer() {
    ESP_LOGI(TAG, "Starting HTTP server on IP %s", WiFi::GetIPAP().c_str());

    static struct file_server_data* server_data = nullptr;
    if (server_data == nullptr) {
        Storage::Mount(FOLDER_PATH, PARTITION_NAME);
        server_data = (file_server_data*)calloc(1, sizeof(file_server_data));
        strlcpy(server_data->base_path,
                FOLDER_PATH,
                sizeof(server_data->base_path));
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_ERROR_CHECK(httpd_start(&s_Server, &config));

    httpd_uri_t file_download = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = download_get_handler,
        .user_ctx = server_data,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_Server, &file_download));

    httpd_uri_t config_download = {
        .uri = "/config",
        .method = HTTP_POST,
        .handler = config_handler,
        .user_ctx = server_data,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_Server, &config_download));
}

/**
 * @brief Stops the HTTP server.
 *
 * This function stops the HTTP server if it is running.
 */
void StopServer() {
    if (s_Server == nullptr) {
        return;
    }

    httpd_stop(s_Server);
}

}  // namespace HTTP
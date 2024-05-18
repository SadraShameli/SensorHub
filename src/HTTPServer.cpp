#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "Backend.h"
#include "Failsafe.h"
#include "Storage.h"
#include "WiFi.h"
#include "HTTP.h"

#define UNIT_DISABLE_FAVICON
#define PARTITION_NAME "web"
#define FOLDER_PATH "/" PARTITION_NAME
#define FILE_PATH "_binary_"

#define FILE_PATH_MAX (15 + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define MAX_FILE_SIZE (512 * 1024)
#define MAX_FILE_SIZE_STR "512KB"
#define SCRATCH_BUFSIZE 8192

struct file_server_data
{
    char base_path[16];
    char scratch[SCRATCH_BUFSIZE];
};

static const char *TAG = "HTTP Server";
static httpd_handle_t s_Server = nullptr;

static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');

    if (quest)
    {
        pathlen = MIN(pathlen, quest - uri);
    }

    const char *hash = strchr(uri, '#');

    if (hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize)
    {
        return nullptr;
    }

    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    return dest + base_pathlen;
}

static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];

    struct dirent *entry;
    struct stat entry_stat;

    DIR *dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);

    strlcpy(entrypath, dirpath, sizeof(entrypath));

    if (!dir)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }

    extern const char upload_script_start[] asm(FILE_PATH "index_html_start");
    extern const char upload_script_end[] asm(FILE_PATH "index_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);

    httpd_resp_send(req, upload_script_start, upload_script_size);

    while ((entry = readdir(dir)) != nullptr)
    {
        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);

        if (stat(entrypath, &entry_stat) == -1)
        {
            continue;
        }

        sprintf(entrysize, "%ld", entry_stat.st_size);
    }

    closedir(dir);
    return ESP_OK;
}

static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, nullptr, 0);

    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf"))
    {
        return httpd_resp_set_type(req, "application/pdf");
    }

    else if (IS_FILE_EXT(filename, ".html"))
    {
        return httpd_resp_set_type(req, "text/html");
    }

    else if (IS_FILE_EXT(filename, ".jpeg"))
    {
        return httpd_resp_set_type(req, "image/jpeg");
    }

    else if (IS_FILE_EXT(filename, ".png"))
    {
        return httpd_resp_set_type(req, "image/png");
    }

    else if (IS_FILE_EXT(filename, ".ico"))
    {
        return httpd_resp_set_type(req, "image/x-icon");
    }

    return httpd_resp_set_type(req, "text/plain");
}

#ifndef UNIT_DISABLE_FAVICON
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm(FILE_PATH "favicon_ico_start");
    extern const unsigned char favicon_ico_end[] asm(FILE_PATH "favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);

    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);

    return ESP_OK;
}
#endif

static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = nullptr;
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path, req->uri, sizeof(filepath));

    if (!filename)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    if (filename[strlen(filename) - 1] == '/')
    {
        return http_resp_dir_html(req, filepath);
    }

    if (stat(filepath, &file_stat) == -1)
    {
        if (strcmp(filename, "/index.html") == 0)
        {
            return index_html_get_handler(req);
        }

#ifndef UNIT_DISABLE_FAVICON
        else if (strcmp(filename, "/favicon.ico") == 0)
        {
            return favicon_get_handler(req);
        }
#endif
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");

        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");

    if (!fd)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Reading existing file failed");

        return ESP_FAIL;
    }

    set_content_type_from_file(req, filename);

    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;

    do
    {
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0)
        {
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK)
            {
                fclose(fd);
                httpd_resp_sendstr_chunk(req, nullptr);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Sending file failed");

                return ESP_FAIL;
            }
        }

    } while (chunksize != 0);

    fclose(fd);

    httpd_resp_send_chunk(req, nullptr, 0);
    return ESP_OK;
}

static esp_err_t config_handler(httpd_req_t *req)
{
    int ret = 0, remaining = req->content_len;
    std::string http_payload(remaining, 0);

    while (remaining > 0)
    {
        if ((ret = httpd_req_recv(req, http_payload.data(), remaining)) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
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

void HTTP::StartServer()
{
    ESP_LOGI(TAG, "Starting HTTP server on IP %s", WiFi::GetIPAP().c_str());

    static struct file_server_data *server_data = nullptr;

    if (!server_data)
    {
        Storage::MountSPIFFS(FOLDER_PATH, PARTITION_NAME);

        server_data = (file_server_data *)calloc(1, sizeof(file_server_data));
        strlcpy(server_data->base_path, FOLDER_PATH, sizeof(server_data->base_path));
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

    httpd_register_uri_handler(s_Server, &file_download);

    httpd_uri_t config_download = {
        .uri = "/*",
        .method = HTTP_POST,
        .handler = config_handler,
        .user_ctx = server_data,
    };

    httpd_register_uri_handler(s_Server, &config_download);
}

void HTTP::StopServer()
{
    if (!s_Server)
    {
        return;
    }

    httpd_stop(s_Server);
}
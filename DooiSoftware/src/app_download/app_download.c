#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/logging/log.h>
#include <ssp/string.h>
#include <sys/_stdint.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "app_download.h"
#include "csk_malloc.h"

LOG_MODULE_REGISTER(app_download, LOG_LEVEL_INF);

#define DEFAULT_HTTP_PORT (80)
#define DEFAULT_HTTPS_PORT (443)
#define DEFAULT_DNS_QURRY_CNT (5)

#define MAX_IAMGE_SIZE (240 * 1024)
#define PNG_HEADER_SIZE (8)
#define PNG_HEADER (0xA1A0A0D474E5089)

K_SEM_DEFINE(http_rsp_sem, 0, 1);

typedef struct {
	char *scheme;
	char *host;
	char *path;
	int port;
	uint8_t *recv_addr;
	int recv_len;
} http_client_handle_t;

#define HTTP_DOWNLOAD_RECV_BUFFER_MAX_SIZE (10 * 1024)
#define HTTP_DOWNLOAD_RECV_TIMEOUT_MS (60 * 1000)

static http_client_handle_t http_client;

static char *str_strdup(const char *in_str)
{
	if (in_str == NULL) {
		return NULL;
	}
	int len = strlen(in_str) + 1;
	char *out_str = csk_malloc(len);
	if (!out_str) {
		return NULL;
	}
	memcpy(out_str, in_str, len);
	return out_str;
}

static char *url_get_scheme(const char *uri)
{
	char *split_ptr = strstr(uri, "://");
	int scheme_size = split_ptr - uri;
	char scheme_temp[scheme_size];
	memcpy(scheme_temp, uri, scheme_size);
	scheme_temp[scheme_size] = '\0';
	return str_strdup(scheme_temp);
}

static char *url_get_host(const char *uri)
{
	const char *host_head = strstr(uri, "://") + strlen("://");
	int slash_cnt = 0;
	for (int idx = 0; idx < strlen(host_head); idx++) {
		if (host_head[idx] == '/') {
			slash_cnt++;
		}
	}

	char match_char = (slash_cnt == 0) ? '\0' : '/';
	char *temp_ptr = strchr(host_head, match_char);
	int host_size = (int)(temp_ptr - host_head);
	char host_temp[host_size];
	memcpy(host_temp, host_head, host_size);
	host_temp[host_size] = '\0';
	return str_strdup(host_temp);
}

static char *url_get_path(const char *uri)
{
	int slash_cnt = 0;
	for (int idx = 0; idx < strlen(uri); idx++) {
		if (uri[idx] == '/') {
			slash_cnt++;
			if (slash_cnt == 3) {
				return str_strdup(&uri[idx]);
			}
		}
	}
	return NULL;
}

static int http_parse_url(http_client_handle_t *http_client, const char *url)
{
	if (url == NULL) {
		LOG_ERR("%s %d", __FUNCTION__, __LINE__);
		return -1;
	}

	http_client->scheme = url_get_scheme(url);
	http_client->host = url_get_host(url);
	http_client->path = url_get_path(url);
	http_client->port = DEFAULT_HTTP_PORT;

	printk("scheme: %s\r\nhost: %s\r\npath: %s\r\nport: %d\r\n", http_client->scheme, http_client->host,
			http_client->path, http_client->port);
	return 0;
}

static int resolve_dns(const char *host, struct sockaddr_in *ip)
{
	const struct addrinfo hints = {
			.ai_family = AF_INET,
			.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *res;

	int err = getaddrinfo(host, NULL, &hints, &res);
	if (err != 0 || res == NULL) {
		LOG_ERR("DNS lookup failed err=%d res=%p", err, res);
		return err;
	}
	ip->sin_family = AF_INET;
	memcpy(&ip->sin_addr, &((struct sockaddr_in *)(res->ai_addr))->sin_addr, sizeof(ip->sin_addr));
	freeaddrinfo(res);
	return 0;
}

static int connect_socket(const char *server, int port)
{
	int ret = 0;
	int sock = -1;
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, server, &addr.sin_addr) != 1) {
		for (int i = 0; i < DEFAULT_DNS_QURRY_CNT; i++) {
			ret = resolve_dns(server, &addr);
			if (ret == 0) {
				LOG_INF("host: %s", server);
				break;
			}
		}

		LOG_INF("ret: %d", ret);
		if (ret < 0) {
			LOG_ERR("%s get dns address failed", __FUNCTION__);
			return -1;
		}
	}

	char buf[NET_IPV4_ADDR_LEN];
	LOG_INF("host ip address: %s", net_addr_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf)));

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sock < 0) {
		LOG_ERR("Failed to create IPv4 HTTP socket (%d)", -errno);
	} else {
		LOG_INF("Create socket successfully");
	}

	ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		LOG_ERR("Cannot connect to remote (%d)", -errno);
		ret = -errno;
		goto fail;
	} else {
		LOG_INF("connect to server successfully: %d", ret);
	}

	return sock;

fail:
	if (sock >= 0) {
		close(sock);
	}

	return ret;
}

static void response_cb(struct http_response *rsp, enum http_final_call final_data, void *user_data)
{
	if (final_data == HTTP_DATA_MORE) {
		LOG_INF("Partial data received (%zd bytes)", rsp->data_len);
	} else if (final_data == HTTP_DATA_FINAL) {
		LOG_INF("All the data received (%zd bytes), body_addr: %p, body_len:%zd, content_len: %zd", rsp->data_len,
				(void *)rsp->body_frag_start, rsp->body_frag_len, rsp->content_length);
		if (rsp->content_length == rsp->body_frag_len) {
			http_client.recv_addr = rsp->body_frag_start;
			http_client.recv_len = rsp->body_frag_len;
			k_sem_give(&http_rsp_sem);
		} else {
			LOG_ERR("not receive all data: content_length: %zd, body_len: %zd", rsp->content_length,
					rsp->body_frag_len);
		}
	}

	LOG_INF("Response to %s", (const char *)user_data);
	LOG_INF("Response status %s", rsp->http_status);
}

int app_download(const char *url, uint8_t **out_data, int *out_len)
{
	int ret = 0;
	int client_socket = -1;
	if (url == NULL || out_len == NULL) {
		LOG_ERR("%s: %d", __FUNCTION__, __LINE__);
		return -1;
	}

	ret = http_parse_url(&http_client, url);
	if (ret < 0) {
		LOG_ERR("%s: %d", __FUNCTION__, __LINE__);
		return ret;
	}

	client_socket = connect_socket(http_client.host, http_client.port);
	if (client_socket < 0) {
		LOG_ERR("%s: %d", __FUNCTION__, __LINE__);
		ret = client_socket;
		goto failed;
	}

	char *req_buf = csk_malloc(MAX_IAMGE_SIZE);
	if (!req_buf) {
		LOG_ERR("%s: %d", __FUNCTION__, __LINE__);
		close(client_socket);
		ret = -1;
		goto failed;
	}

	LOG_INF("Start download");

	struct http_request req = {0};
	req.method = HTTP_GET;
	req.url = http_client.path;
	req.host = http_client.host;
	req.protocol = "HTTP/1.1";
	req.response = response_cb;
	req.recv_buf = req_buf;
	req.recv_buf_len = MAX_IAMGE_SIZE;

	ret = http_client_req(client_socket, &req, 5000, "IPv4 GET");
	if (ret < 0) {
		LOG_ERR("%s: %d", __FUNCTION__, __LINE__);
	}
	close(client_socket);

	ret = k_sem_take(&http_rsp_sem, K_SECONDS(3));
	if (ret != 0) {
		LOG_ERR("get image timeout: %d", ret);
		goto closed;
	}

	uint8_t *image_buf = csk_malloc(http_client.recv_len);
	if (!image_buf) {
		LOG_ERR("%s: %d", __FUNCTION__, __LINE__);
		goto closed;
	}

	memcpy(image_buf, http_client.recv_addr, http_client.recv_len);
	*out_data = image_buf;
	*out_len = http_client.recv_len;

	LOG_INF("download finished");

closed:
	if (req_buf)
		csk_free(req_buf);
failed:
	if (http_client.scheme)
		csk_free(http_client.scheme);
	if (http_client.host)
		csk_free(http_client.host);
	if (http_client.path)
		csk_free(http_client.path);
	memset(&http_client, 0, sizeof(http_client_handle_t));

	return ret;
}

int app_download_free(void *ptr)
{
	if (ptr != NULL) {
		csk_free(ptr);
	}
	return 0;
}

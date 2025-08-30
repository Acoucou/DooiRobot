/* http_server.c */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <zephyr/net/sntp.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include "http_server.h"
#include <zephyr/posix/unistd.h>
#include "csk_malloc.h"
#include "wifi_mgr.h"
#include "web_interface.h"
#include "cJSON.h"
#include "led.h"
#include "motor.h"
#include "servo.h"
#include "screen.h"
#include "ls_cmd.h"
#include "app_connect.h"
#include <gcs_wakeup_service.h>
#include "app_camera.h"
#include "tiny_jpeg.h"
#include <zephyr/net/dns_sd.h>

LOG_MODULE_REGISTER(http_server, LOG_LEVEL_DBG);

int client = 0;

static bool web_frame_start = false;
struct jpeg_write_ctx {
	uint8_t *buf;
	uint32_t total_size;
	uint32_t wrote;
};

static const uint16_t http_port = htons(8080);
/* 静态声明HTTP服务记录 */
STRUCT_SECTION_ITERABLE(dns_sd_rec, http_service) = {
    .instance = "My HTTP Server",  // 服务实例名
    .service = "_http._tcp",       // 服务类型
    .port = &http_port,            // 服务端口
    .text = "path=/",              // 可选TXT记录
    .text_size = sizeof("path=/") - 1,
};

/* HTTP请求解析 */
static int parse_request(const char *buf, char *method, char *path)
{
    return sscanf(buf, "%6s %127s", method, path);
}

/* HTTP响应生成 */
static void send_response(int sock, int code, const char *body)
{
    char header[128];
    int len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n",code);
    
    zsock_send(sock, header, len, 0);

    if (body) {
        int body_len = strlen(body);
        int sent = 0;
        while (sent < body_len) {
            int ret = zsock_send(sock, body + sent, body_len - sent, 0);
            if (ret <= 0) break;
            sent += ret;
        }
    }
}


void set_pro_id_text(char *pro)
{
	int ret = ls_cmd_write_product_id(pro);
	if (ret != 0) {
		LOG_ERR("Failed to set product id ,ret = %d\n", ret);
		return;
	}
}

void set_sec_id_text(char *sec)
{
	int ret = ls_cmd_write_secret_id(sec);
	if (ret != 0) {
		LOG_ERR("Failed to set secret id ,ret = %d\n", ret);
		return;
	}
}

void set_dev_text(char *pro, char *sec)
{
	set_pro_id_text(pro);
	set_sec_id_text(sec);
}

static void handle_device_commands(cJSON *json, cJSON *resp)
{
    static uint8_t clock_mode = 0;
    static uint8_t eye_mode = 0, eye = 0;

    /* ===== LED 控制（原样） ===== */
    cJSON *led = cJSON_GetObjectItem(json, "led");
    if (led) {
        cJSON *rgb = cJSON_GetObjectItem(led, "rgb");
        if (rgb) {
            int left = cJSON_GetObjectItem(rgb, "left")->valueint;
            int right = cJSON_GetObjectItem(rgb, "right")->valueint;
            led_set_rgb(left, right);
        }

        cJSON *breathing = cJSON_GetObjectItem(led, "breathing");
        if (breathing) {
            int color = cJSON_GetObjectItem(breathing, "color")->valueint;
            int period = cJSON_GetObjectItem(breathing, "period")->valueint;
            bool symmetric = cJSON_GetObjectItem(breathing, "symmetric")->valueint;
            led_set_breathing(color, period, symmetric);
        }

        cJSON *rainbow = cJSON_GetObjectItem(led, "rainbow");
        if (rainbow) {
            int period = cJSON_GetObjectItem(rainbow, "period")->valueint;
            bool symmetric = cJSON_GetObjectItem(rainbow, "symmetric")->valueint;
            led_set_rainbow(period, symmetric);
        }

        cJSON *blink = cJSON_GetObjectItem(led, "blink");
        if (blink) {
            int color = cJSON_GetObjectItem(blink, "color")->valueint;
            int period = cJSON_GetObjectItem(blink, "period")->valueint;
            bool symmetric = cJSON_GetObjectItem(blink, "symmetric")->valueint;
            led_set_blink(color, period, symmetric);
        }

        if (cJSON_GetObjectItem(led, "off")) {
            led_off();
        }
    }

    /* ===== 麦克风增益 ===== */
    cJSON *mic = cJSON_GetObjectItem(json, "mic");
    if (mic) {
        /* 约束范围：模拟[-12,36]，数字[-83,42] */
        int ag = 0, dg = 0;
        cJSON *ag_item = cJSON_GetObjectItem(mic, "analog_gain");
        cJSON *dg_item = cJSON_GetObjectItem(mic, "digital_gain");
        if (ag_item) ag = ag_item->valueint;
        if (dg_item) dg = dg_item->valueint;

        if (ag < -12) ag = -12; 
        if (ag > 36) ag = 36;
        if (dg < -83) dg = -83; 
        if (dg > 42) dg = 42;

        wakeup_service_gcs_set_mic_a_gain(ag);
        wakeup_service_gcs_set_mic_d_gain(dg);
        LOG_INF("MIC gain set: analog=%d dB, digital=%d dB", ag, dg);

        if (resp) {
            cJSON_AddStringToObject(resp, "status", "ok");
            cJSON *micr = cJSON_CreateObject();
            cJSON_AddItemToObject(resp, "mic", micr);
            cJSON_AddNumberToObject(micr, "analog_gain", ag);
            cJSON_AddNumberToObject(micr, "digital_gain", dg);
        }
    }

    /* ===== 电机控制 ===== */
    cJSON *motor = cJSON_GetObjectItem(json, "motor");
    if (motor) {
        cJSON *smooth = cJSON_GetObjectItem(motor, "smooth");
        if (smooth) {
            int left = cJSON_GetObjectItem(smooth, "left")->valueint;
            int right = cJSON_GetObjectItem(smooth, "right")->valueint;
            int duration = cJSON_GetObjectItem(smooth, "duration")->valueint;
            motor_move_smooth(left, right, duration);
        }

        cJSON *motion = cJSON_GetObjectItem(motor, "motion");
        if (motion) {
            float x = (float)cJSON_GetObjectItem(motion, "x")->valuedouble;
            float y = (float)cJSON_GetObjectItem(motion, "y")->valuedouble * -1;
            int duration = cJSON_GetObjectItem(motion, "duration")->valueint;
            motor_motion_control(x, y, (uint16_t)duration);
        }
        if (cJSON_GetObjectItem(motor, "stop")) {
            motor_emergency_stop();
        }
        if (resp && !cJSON_HasObjectItem(resp, "status")) {
            cJSON_AddStringToObject(resp, "status", "ok");
        }
    }

    /* ===== 摄像头 ===== */
    cJSON *camera = cJSON_GetObjectItem(json, "camera");
    if (camera) {
        if (cJSON_GetObjectItem(camera, "enable")) {
            camera_start();
            if (resp) { cJSON_AddStringToObject(resp, "camera", "enabled"); }
            web_frame_start = true;
        }
        if (cJSON_GetObjectItem(camera, "disable")) {
            camera_stop();
            if (resp) { cJSON_AddStringToObject(resp, "camera", "disabled"); }
            web_frame_start = false;
        }
        if (resp && !cJSON_HasObjectItem(resp, "status")) {
            cJSON_AddStringToObject(resp, "status", "ok");
        }
    }

    /* ===== 舵机偏移 / 角度（原样） ===== */
    extern int servo1_offset;
    extern int servo2_offset;
    cJSON *servo_offset = cJSON_GetObjectItem(json, "servo_offset");
    if (servo_offset) {
        cJSON *offset1 = cJSON_GetObjectItem(servo_offset, "servo1");
        cJSON *offset2 = cJSON_GetObjectItem(servo_offset, "servo2");
        if (offset1) servo1_offset = offset1->valueint * 1000;
        if (offset2) servo2_offset = offset2->valueint * 1000;
        LOG_INF("servo1_offset=%d, servo2_offset=%d", servo1_offset, servo2_offset);
        if (resp) {
            cJSON_AddStringToObject(resp, "status", "ok");
            cJSON *so = cJSON_CreateObject();
            cJSON_AddItemToObject(resp, "servo_offset", so);
            cJSON_AddNumberToObject(so, "servo1", servo1_offset/1000);
            cJSON_AddNumberToObject(so, "servo2", servo2_offset/1000);
        }
        return;
    }
    cJSON *servo1 = cJSON_GetObjectItem(json, "servo1");
    if (servo1) {
        cJSON *angle = cJSON_GetObjectItem(servo1, "angle");
        if (angle) servo_set_angle(SERVO_LEFT, angle->valueint);
    }
    cJSON *servo2 = cJSON_GetObjectItem(json, "servo2");
    if (servo2) {
        cJSON *angle = cJSON_GetObjectItem(servo2, "angle");
        if (angle) servo_set_angle(SERVO_RIGHT, angle->valueint);
    }

    /* ===== 屏幕 ===== */
    cJSON *screen = cJSON_GetObjectItem(json, "screen");
    if (screen) {
        cJSON *type = cJSON_GetObjectItem(screen, "type");
        if (type) {
            screen_type_t screen_type = (screen_type_t)type->valueint;
            if (screen_type >= SCREEN_DIGITAL_CLOCK && screen_type < SCREEN_COUNT) {
                if(screen_type == SCREEN_DIGITAL_CLOCK){ clock_mode = !clock_mode; digital_clock_mode_set(clock_mode); }
                if(screen_type == SCREEN_EYES){ eye++; if(eye>=17) eye_emotion_selete(eye); }
                if(screen_type == SCREEN_EMOJI_GIF){ eye_mode++; if(eye_mode>8) eye_mode=0; switch_gif(eye_mode); }
                screen_set_current(screen_type);
                if (resp) { cJSON_AddStringToObject(resp, "status", "ok"); }
            } else {
                LOG_ERR("Invalid screen type: %d", screen_type);
                if (resp) { cJSON_AddStringToObject(resp, "error", "invalid_screen_type"); }
            }
        }
    }

    /* ===== WiFi ===== */
    cJSON *wifi = cJSON_GetObjectItem(json, "wifi");
    if (wifi) {
        cJSON *ssid = cJSON_GetObjectItem(wifi, "ssid");
        cJSON *pass = cJSON_GetObjectItem(wifi, "pass");
        if (ssid && pass) {
            const char *ssid_str = ssid->valuestring;
            const char *pass_str = pass->valuestring;
            LOG_INF("WiFi SSID=%s, PASS=%s", ssid_str, pass_str);
            extern int wifi_connect_to_specified_ap_sync_by_ssid(const char *ssid,const char *password);
            wifi_connect_to_specified_ap_sync_by_ssid(ssid_str, pass_str);
            if (resp) { cJSON_AddStringToObject(resp, "status", "ok"); }
        }
    }

    /* ===== 平台链路管理 ===== */
    cJSON *link = cJSON_GetObjectItem(json, "link");
    if (link) {
        cJSON *product = cJSON_GetObjectItem(link, "product");
        cJSON *secret  = cJSON_GetObjectItem(link, "secret");
        cJSON *preset  = cJSON_GetObjectItem(link, "preset");
        cJSON *query   = cJSON_GetObjectItem(link, "query");

        /* 写入 */
        if (product && secret) {
            set_dev_text(product->valuestring, secret->valuestring);
            LOG_INF("Link credentials updated: product=%s", product->valuestring);
            app_reconnect();
        }

        if (preset) {
            LOG_INF("Link preset applied: %s", preset->valuestring);
            
            if(strcmp(preset->valuestring, "default") == 0)
            {
                set_dev_text("75471b7a-ae97-4048-bdc8-529c97cc713b", "56d6c961-725d-40d9-8028-2cc0158c7e58");
                app_reconnect();
            }
            else if(strcmp(preset->valuestring, "new") == 0)
            {
                set_dev_text("0b09ac67-67fa-4f4c-afc5-57b8229d6765", "b074f485-f6a2-4046-8427-f748768bed6c");
                app_reconnect();
            }
        }

        if (query) {
            device_info_t info;
	        device_info_read(&info);

            /* 统一读取当前生效配置并回填 */
            if (resp) {
                cJSON_AddStringToObject(resp, "status", "ok");
                cJSON_AddStringToObject(resp, "device_id", info.chip_id);

                cJSON *link_obj = cJSON_CreateObject();
                cJSON_AddItemToObject(resp, "link", link_obj);
                cJSON_AddStringToObject(link_obj, "product", info.product_id);
                cJSON_AddStringToObject(link_obj, "secret",  info.secret_id);
            }
        }
    }
}

static void rgb565_to_rgb888_le(uint16_t *src, uint8_t *dst, size_t pixel_count)
{
	for (size_t i = 0; i < pixel_count; i++) {
		uint16_t pixel = src[i];
		uint8_t r = (pixel >> 11) & 0x1F;
		uint8_t g = (pixel >> 5) & 0x3F;
		uint8_t b = pixel & 0x1F;

		dst[i * 3 + 0] = (r * 255) / 31;
		dst[i * 3 + 1] = (g * 255) / 63;
		dst[i * 3 + 2] = (b * 255) / 31;
	}
}

static void jpeg_write(void *context, void *data, int size)
{
	struct jpeg_write_ctx *ctx = (struct jpeg_write_ctx *)context;
	if (ctx == NULL) {
		return;
	}

	uint32_t can_write = ctx->total_size - ctx->wrote;
	can_write = can_write >= size ? size : can_write;

	memcpy(ctx->buf + ctx->wrote, data, can_write);

	ctx->wrote += can_write;
}

/**
 * @note the picture must be formatted as rgb565
 */
static int send_frame(void *pic, uint32_t w, uint32_t h)
{
	uint8_t *rgb888_buf;
	uint32_t rgb888_buf_size;
	uint8_t *jpeg_buf;
	uint32_t jpeg_buf_size;
	int err;

	rgb888_buf_size = w * h * 3;
	jpeg_buf_size = rgb888_buf_size / 5;

	/* convert picture to rgb888 */
	rgb888_buf = csk_malloc(rgb888_buf_size);
	if (rgb888_buf == NULL) {
		LOG_ERR("no more mem to alloc rgb888 buffer, size:%d", rgb888_buf_size);
		return -ENOMEM;
	}
	jpeg_buf = csk_malloc(jpeg_buf_size);
	if (jpeg_buf == NULL) {
		LOG_ERR("no more mem to alloc jpeg_buf buffer, size:%d", rgb888_buf_size);
		csk_free(rgb888_buf);
		return -ENOMEM;
	}
	rgb565_to_rgb888_le(pic, rgb888_buf, w * h);

	/* compress the picture to jpg format */
	struct jpeg_write_ctx jpeg_write_ctx = {
		.buf = jpeg_buf,
		.total_size = jpeg_buf_size,
		.wrote = 0,
	};

	err = tje_encode_with_func(jpeg_write, &jpeg_write_ctx, 1, w, h, 3, rgb888_buf);
	if (err == 0) {
		LOG_ERR("jpeg compression failed");
        csk_free(rgb888_buf);
        csk_free(jpeg_buf);
		return -EIO;
	} else {
		LOG_INF("jpeg compression done, size:%d/%d", jpeg_write_ctx.wrote, rgb888_buf_size);
	}
	/* rgb888_buf no longer needs to be used */
	csk_free(rgb888_buf);

    char part_hdr[128] = {0};
    int hlen = snprintf(part_hdr, sizeof(part_hdr),
                        "--frame\r\n"
                        "Content-Type: image/jpeg\r\n"
                        "Content-Length: %zu\r\n\r\n", jpeg_write_ctx.wrote);

    zsock_send(client, part_hdr, hlen, 0);

    int body_len = jpeg_write_ctx.wrote;
    int sent = 0;
    while (sent < body_len) {
        int ret = zsock_send(client, jpeg_write_ctx.buf, body_len - sent, 0);
        if (ret <= 0) break;
        sent += ret;
    }

    zsock_send(client, "\r\n", 2, 0);

	csk_free(jpeg_buf);

	return err;
}

static void camera_thread(void *p1, void *p2, void *p3)
{
	struct video_buffer *vbuf;
    int w, h, ret;

	while (1) 
    {
        if(!web_frame_start)
        {
		    k_msleep(10000);
            continue;
        }

        ret = app_camera_vbuf_capture(&vbuf, &w, &h);
        if (ret) {
            LOG_INF("Camera capture failed: %d\n", ret);
            continue;
        }

        send_frame(vbuf, 240, 240);

        app_camera_vbuf_unref(vbuf);   // 释放视频缓冲区

		k_msleep(200);
	}
}

static void handle_video_stream(int sock)
{
    // 设置视频流头
    const char *hdr = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
                      "Connection: keep-alive\r\n\r\n";
    
    if (zsock_send(sock, hdr, strlen(hdr), 0) < 0) {
        LOG_ERR("Failed to send stream header");
        zsock_close(sock);
        return;
    }
}

/* 客户端处理线程 */
static void client_handler(int sock)
{
    /* 设置接收超时（5秒）*/
    struct timeval tv = {
        .tv_sec = 5,
        .tv_usec = 0
    };
    zsock_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char buf[RECV_BUF_SIZE], method[7], path[128];
    int received = zsock_recv(sock, buf, sizeof(buf)-1, 0);
    
    if (received <= 0) {
        zsock_close(sock);
        return;
    }
    
    buf[received] = '\0';
    LOG_INF("Received request: %s", buf);
    
    parse_request(buf, method, path);
    
    if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
        send_response(sock, 200, html_content);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(path, "/stream") == 0) {
        // 视频流请求处理
        handle_video_stream(sock);
        return; // 保持连接打开
    }
    else if (strcmp(method, "POST") == 0) {
        char *json_start = strstr(buf, "\r\n\r\n");
        if (json_start) {
            json_start += 4;  // Skip headers
            
            // Parse JSON
            cJSON *json = cJSON_Parse(json_start);
            if (json) {
                cJSON *response = cJSON_CreateObject();

                // Handle device commands & fill response
                handle_device_commands(json, response);

                // Fallback: if no field written, at least return success
                if (cJSON_GetArraySize(response) == 0) {
                    cJSON_AddStringToObject(response, "status", "success");
                }

                char *response_str = cJSON_PrintUnformatted(response);
                send_response(sock, 200, response_str);

                cJSON_Delete(response);
                cJSON_Delete(json);
                cJSON_free(response_str);
            } else {
                send_response(sock, 400, "{\"error\":\"Invalid JSON\"}");
            }
        } else {
            send_response(sock, 400, "{\"error\":\"No JSON data found\"}");
        }
    } else {
        send_response(sock, 404, "{\"error\":\"Not found\"}");
    }
    
    zsock_close(sock);
}


#define UTC8_SEC            (8 * 3600)

struct tm utc8_time_start = {0};     // UTC8 time start
time_t utc8_seconds_start = 0;       // UTC8 seconds start

static int get_ntp_time(struct tm *utc8_time, time_t *utc8_seconds)
{ // 获取网络时间
    int rc;
    struct sntp_time sntp_time;
    char time_str[sizeof("1970-01-01 00:00:00")];
    char *servers[] = {CONFIG_SNTP_SERVER1, CONFIG_SNTP_SERVER2, CONFIG_SNTP_SERVER3, CONFIG_SNTP_SERVER4, CONFIG_SNTP_SERVER5};
    int server_count = sizeof(servers) / sizeof(servers[0]);

    for (int i = 0; i < server_count; i++) {
        int cnt = 0;
        do {
            LOG_INF("Sending NTP request to %s:", servers[i]);
            rc = sntp_simple(servers[i], 200, &sntp_time);
            if (rc == 0) {
                time_t now = sntp_time.seconds + UTC8_SEC;
                // struct tm now_tm;

                gmtime_r(&now, utc8_time);
                strftime(time_str, sizeof(time_str), "%FT%T", utc8_time);
                LOG_INF("  Acquired time: %s", time_str);

                *utc8_seconds = now;

                // *hour = now_tm.tm_hour;
                // *min = now_tm.tm_min;
                LOG_INF("  Acquired time: year-%d, mon-%d, day-%d, hour-%d, min-%d", 
                                utc8_time->tm_year,
                                utc8_time->tm_mon,
                                utc8_time->tm_mday,
                                utc8_time->tm_hour,
                                utc8_time->tm_min
                                );
                LOG_INF("  sec: %lld", now);
                return 0;
            } else {
                LOG_ERR("  Failed to acquire SNTP from %s, code %d, attempt %d", servers[i], rc, cnt);
            }
            cnt++;
        } while (cnt < 10);
    }
    return 1;
}

/* 主服务线程 */
static void http_server_thread(void *p1, void *p2, void *p3)
{
    /* 等待网络就绪 */
    LOG_INF("Waiting for network connection...");
    while (!wifi_is_connected()) {
        k_msleep(1000);
    }

    get_ntp_time(&utc8_time_start, &utc8_seconds_start);

    STRUCT_SECTION_FOREACH(dns_sd_rec, it);
    
    /* 创建监听socket */
    int sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        LOG_ERR("Socket creation failed: %d", -errno);
        return;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(HTTP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (zsock_bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERR("Bind failed: %d", -errno);
        goto error;
    }

    if (zsock_listen(sock, MAX_CLIENTS) < 0) {
        LOG_ERR("Listen failed: %d", -errno);
        goto error;
    }

    LOG_INF("HTTP server running on port %d", HTTP_PORT);

    while (1) {
        client = zsock_accept(sock, NULL, NULL);
        if (client >= 0) {
            client_handler(client);
        }
        k_msleep(20);
    }

error:
    zsock_close(sock);
}

static int http_server_sys_init(void)
{
    k_thread_stack_t *stack;
    k_tid_t tid;
    const int stack_size = 8 * 1024;
    const int thread_prio = 6;

    stack = csk_aligned_alloc(8, stack_size);
    if (stack == NULL) {
        LOG_ERR("HTTP server stack malloc failed");
        return -ENOMEM;
    }

    struct k_thread *new_thread = csk_malloc(sizeof(struct k_thread));
    if (new_thread == NULL) {
        csk_free(stack);
        return -ENOMEM;
    }

    tid = k_thread_create(new_thread, stack, stack_size, http_server_thread, 
                         NULL, NULL, NULL, thread_prio, 0, K_NO_WAIT);
    if (tid != new_thread) {
        csk_free(new_thread);
        csk_free(stack);
        LOG_ERR("HTTP server thread creation failed");
        return -EFAULT;
    }

    k_thread_name_set(tid, "http_server");
    return 0;
}

SYS_INIT(http_server_sys_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

// K_THREAD_DEFINE(camera_thread_id, 10240, camera_thread, NULL, NULL, NULL,
// 		10, 0, 0);

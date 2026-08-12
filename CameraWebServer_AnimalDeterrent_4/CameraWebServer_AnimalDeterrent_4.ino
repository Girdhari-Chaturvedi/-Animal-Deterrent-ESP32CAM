// ============================================
// ANIMAL DETERRENT - ESP32-CAM (AI-Thinker)
// Single-file version - NO external files needed
// (no board_config.h / camera_pins.h / app_httpd.cpp required)
// ============================================

// ---- Blynk credentials: must stay at the very top ----
#define BLYNK_TEMPLATE_ID "_____________________"
#define BLYNK_TEMPLATE_NAME "___________________"
#define BLYNK_AUTH_TOKEN "______________________"

#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "img_converters.h"
#include <BlynkSimpleEsp32.h>

// ---- WiFi credentials ----
const char *ssid     = "__________";
const char *password = "__________";

// ---- AI-Thinker ESP32-CAM pin map ----
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ---- Deterrent hardware ----
#define BUZZER_PIN 13   // External buzzer

httpd_handle_t stream_httpd = NULL;

// Forward declaration (defined further below, used by trigger_handler)
void triggerDeterrent();

// ---- MJPEG stream handler ----
// Camera captures RGB565 (this sensor doesn't init reliably in JPEG mode),
// so each frame is converted to JPEG in software before sending.
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char part_buf[64];

  static const char *STREAM_BOUNDARY = "\r\n--123456789000000000000987654321\r\n";
  static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    bool jpg_needs_free = false;

    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else if (fb->format == PIXFORMAT_JPEG) {
      _jpg_buf = fb->buf;
      _jpg_buf_len = fb->len;
    } else {
      bool converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
      esp_camera_fb_return(fb);
      fb = NULL;
      if (!converted) {
        Serial.println("JPEG conversion failed");
        res = ESP_FAIL;
      } else {
        jpg_needs_free = true;
      }
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, 64, STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    }

    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
    }
    if (jpg_needs_free && _jpg_buf) {
      free(_jpg_buf);
    }
    _jpg_buf = NULL;

    if (res != ESP_OK) break;
  }
  return res;
}

// ---- Single JPEG snapshot endpoint (/capture) ----
static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  esp_err_t res = ESP_OK;
  uint8_t *jpg_buf = NULL;
  size_t jpg_len = 0;
  bool needs_free = false;

  if (fb->format == PIXFORMAT_JPEG) {
    jpg_buf = fb->buf;
    jpg_len = fb->len;
  } else {
    bool converted = frame2jpg(fb, 90, &jpg_buf, &jpg_len);
    esp_camera_fb_return(fb);
    fb = NULL;
    if (!converted) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    needs_free = true;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  res = httpd_resp_send(req, (const char *)jpg_buf, jpg_len);

  if (fb) esp_camera_fb_return(fb);
  if (needs_free && jpg_buf) free(jpg_buf);

  return res;
}

// ---- Manual test-alert endpoint (/trigger) ----
static esp_err_t trigger_handler(httpd_req_t *req) {
  triggerDeterrent();
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "Alert triggered", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// ---- Root page: full-size stream + Capture/Test Alert buttons ----
static esp_err_t index_handler(httpd_req_t *req) {
  static const char PAGE[] =
    "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Animal Deterrent Cam</title>"
    "<style>"
    "body{margin:0;background:#111;font-family:sans-serif;color:#fff;text-align:center}"
    "img{display:block;width:100vw;height:80vh;object-fit:contain;background:#000;image-rendering:pixelated}"
    "button{padding:14px 22px;margin:10px;font-size:16px;border:none;border-radius:8px;cursor:pointer}"
    ".cap{background:#2b8aef;color:#fff}"
    ".alert{background:#e74c3c;color:#fff}"
    "#msg{min-height:20px}"
    "</style></head><body>"
    "<h2>ESP32-CAM Animal Deterrent</h2>"
    "<img src='/stream'>"
    "<div>"
    "<button class='cap' onclick=\"window.open('/capture','_blank')\">Capture Photo</button>"
    "<button class='alert' onclick=\"fetch('/trigger').then(()=>{document.getElementById('msg').innerText='Alert triggered!';})\">Test Alert</button>"
    "</div>"
    "<div id='msg'></div>"
    "</body></html>";

  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN);
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;
  config.max_uri_handlers = 8;

  httpd_uri_t stream_uri = {
    .uri      = "/stream",
    .method   = HTTP_GET,
    .handler  = stream_handler,
    .user_ctx = NULL
  };
  httpd_uri_t capture_uri = {
    .uri      = "/capture",
    .method   = HTTP_GET,
    .handler  = capture_handler,
    .user_ctx = NULL
  };
  httpd_uri_t trigger_uri = {
    .uri      = "/trigger",
    .method   = HTTP_GET,
    .handler  = trigger_handler,
    .user_ctx = NULL
  };
  httpd_uri_t index_uri = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = index_handler,
    .user_ctx = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    httpd_register_uri_handler(stream_httpd, &capture_uri);
    httpd_register_uri_handler(stream_httpd, &trigger_uri);
  }
}

// ---- Sound buzzer + send Blynk alert ----
void triggerDeterrent() {
  digitalWrite(BUZZER_PIN, HIGH);
  if (Blynk.connected()) {
    Blynk.logEvent("animal_alert", "Alert! Target detected via ESP32-CAM.");
  }
  delay(3000);
  digitalWrite(BUZZER_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;

  // Proven-working settings for this sensor (JPEG mode failed with 0x106,
  // RGB565 initializes reliably; we convert to JPEG in software for the stream)
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size   = FRAMESIZE_QQVGA;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location  = CAMERA_FB_IN_DRAM;
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1);
      s->set_brightness(s, 1);
      s->set_saturation(s, -2);
    }
    s->set_framesize(s, FRAMESIZE_QQVGA);
  }

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Connect Blynk using the WiFi connection we already have
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println(":81/stream' to view");
}

void loop() {
  Blynk.run();

  // Type TRIGGER_ALERT in Serial Monitor to test the deterrent manually.
  // Later this can be replaced with a PIR sensor condition instead.
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "TRIGGER_ALERT") {
      triggerDeterrent();
    }
  }
}

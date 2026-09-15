#include <LovyanGFX.hpp>
#include "esp_camera.h"

class LGFX : public lgfx::LGFX_Device {
    lgfx::Bus_SPI _bus_instance;
    lgfx::Panel_ILI9488 _panel_instance;
    lgfx::Light_PWM _light_instance;
public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI3_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.pin_sclk = 20; cfg.pin_mosi = 1; cfg.pin_miso = 41; cfg.pin_dc = 2;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = 19; cfg.pin_rst = 7;
            cfg.panel_width = 320; cfg.panel_height = 480;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 21; cfg.freq = 12000; cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        setPanel(&_panel_instance);
    }
};

static LGFX tft;

#define BTN_SELECT 40 
#define BTN_NEXT   39
bool isStreaming = true;

// Цвета интерфейса
#define COLOR_ACCENT 0x07E0 // Зеленый
#define COLOR_BG     0x0000 // Черный
#define COLOR_WARN   0xF800 // Красный

void setupCamera(pixformat_t format) {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 11; config.pin_d1 = 9; config.pin_d2 = 8; config.pin_d3 = 10;
  config.pin_d4 = 12; config.pin_d5 = 13; config.pin_d6 = 45; config.pin_d7 = 3;
  config.pin_xclk = 0; config.pin_pclk = 4; config.pin_vsync = 16; config.pin_href = 15;
  config.pin_sscb_sda = 18; config.pin_sscb_scl = 17;
  config.pin_pwdn = 5; config.pin_reset = 14;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_HVGA;
  config.pixel_format = format;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.fb_count = 1;
  esp_camera_init(&config);
}

// Рисуем рамку и статус
void drawOverlay(String status, uint16_t color) {
  tft.drawRect(40, 40, 400, 240, color); // Рамка фокуса
  tft.fillRect(0, 0, 480, 30, color);    // Верхняя плашка
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(10, 5);
  tft.print(status);
}

void captureAndSendSerial() {
  isStreaming = false;
  tft.fillScreen(COLOR_BG);
  tft.fillRect(0, 140, 480, 40, 0x001F); // Синяя полоса загрузки
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(120, 150);
  tft.print("ИДЕТ АНАЛИЗ...");

  esp_camera_deinit();
  setupCamera(PIXFORMAT_JPEG);
  sensor_t * s = esp_camera_sensor_get();
  s->set_quality(s, 10);

  camera_fb_t * fb = esp_camera_fb_get();
  if (fb) {
    Serial.println("START_IMAGE");
    Serial.println(fb->len);
    Serial.write(fb->buf, fb->len);
    Serial.flush();
    esp_camera_fb_return(fb);
  }

  String response = "";
  unsigned long start = millis();
  while (millis() - start < 15000) {
    if (Serial.available()) { response = Serial.readStringUntil('\n'); break; }
  }

tft.fillScreen(COLOR_BG);
  if (response != "") {
    // 1. Рисуем красивую серую карточку на весь экран с закругленными углами
    // Оставляем аккуратные отступы от краев дисплея (по 10 пикселей)
    tft.fillRoundRect(10, 10, 460, 290, 10, 0x18E3); // Темно-серый фон карточки
    tft.drawRoundRect(10, 10, 460, 290, 10, COLOR_ACCENT); // Зеленая (или синяя) граница

    // 2. Настраиваем безопасную зону для текста внутри карточки
    // Сдвигаем курсор глубже внутрь рамки, чтобы буквы не назазили на линии
    tft.setCursor(25, 25); 
    tft.setTextColor(COLOR_ACCENT);
    tft.println("СОВЕТ ВРАЧА:");
    
    // Переносим курсор на новую строку с отступом слева
    tft.setCursor(25, 60); 
    tft.setTextColor(TFT_WHITE);
    
    // Включаем перенос строк и выводим само описание лекарства
    tft.setTextWrap(true);
    tft.println(response);

    // 3. Блок для интерактивных кнопок (Да/Нет)
    if (response.indexOf('?') != -1) {
      // Рисуем нижнюю плашку для подсказок кнопок
      tft.fillRect(10, 250, 460, 45, 0x3186); 
      tft.setCursor(35, 262);
      tft.setTextColor(TFT_WHITE);
      tft.print("ДА [SELECT] | НЕТ [NEXT]");
      
      // Ожидание нажатия физических кнопок
      while(true) {
        if (digitalRead(BTN_SELECT) == LOW) { Serial.println("USER_ANSWER: YES"); break; }
        if (digitalRead(BTN_NEXT) == LOW) { Serial.println("USER_ANSWER: NO"); break; }
        delay(50);
      }

      // Экран ожидания финального ответа от Python
      tft.fillScreen(COLOR_BG);
      tft.setCursor(120, 150);
      tft.print("ЖДЕМ ВЕРДИКТ...");
      
      String finalVerdict = "";
      unsigned long w = millis();
      while (millis() - w < 5000) {
        if (Serial.available()) { finalVerdict = Serial.readStringUntil('\n'); break; }
      }
      
      // Вывод финального вердикта в красивой рамке
      tft.fillScreen(COLOR_BG);
      uint16_t boxColor = (finalVerdict.indexOf("Не") != -1) ? COLOR_ACCENT : COLOR_WARN;
      tft.fillRoundRect(20, 80, 440, 160, 15, boxColor);
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(40, 120);
      tft.println(finalVerdict);
    }
  }

  tft.setCursor(120, 400);
  tft.setTextColor(0x7BEF);
  tft.print("Нажмите [NEXT] для выхода");
  while(digitalRead(BTN_NEXT) == HIGH) { delay(10); }
  
  esp_camera_deinit();
  setupCamera(PIXFORMAT_RGB565);
  isStreaming = true;
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  
  tft.init();
  tft.setRotation(3);
  tft.setFont(&fonts::efontCN_16);
  tft.setTextWrap(true);
  
  setupCamera(PIXFORMAT_RGB565);
  
  // Начальный экран
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_ACCENT);
  tft.setCursor(100, 140);
  tft.print("AI MEDICINE READY");
  delay(2000);
}

void loop() {
  if (isStreaming) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) {
      tft.pushImage(0, 0, 480, 320, (uint16_t*)fb->buf);
      esp_camera_fb_return(fb);
    }
  }
  if (digitalRead(BTN_SELECT) == LOW) captureAndSendSerial();
}

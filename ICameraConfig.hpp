#pragma once

#include <esp_camera.h>

class ICameraConfig {
  public:
    virtual ~ICameraConfig(void) {}
    virtual const char* GetName(void) const = 0;
    virtual bool Initialize(void) = 0;
  protected:
    ICameraConfig(void) {}
};

class UnitCamS3Config : public ICameraConfig  {
  public:
    UnitCamS3Config(void) {}
    virtual~UnitCamS3Config(void) {}

    virtual const char* GetName(void) const override final {
      return "UnitCamS3Config";
    }

    virtual bool Initialize(void) override final {
      constexpr int PWDN_GPIO_NUM = -1;
      constexpr int RESET_GPIO_NUM = -1;
      constexpr int XCLK_GPIO_NUM = 11;
      constexpr int SIOD_GPIO_NUM = 17;
      constexpr int SIOC_GPIO_NUM = 41;
      constexpr int Y9_GPIO_NUM = 13;
      constexpr int Y8_GPIO_NUM = 4;
      constexpr int Y7_GPIO_NUM = 10;
      constexpr int Y6_GPIO_NUM = 5;
      constexpr int Y5_GPIO_NUM = 7;
      constexpr int Y4_GPIO_NUM = 16;
      constexpr int Y3_GPIO_NUM = 15;
      constexpr int Y2_GPIO_NUM = 6;
      constexpr int VSYNC_GPIO_NUM = 42;
      constexpr int HREF_GPIO_NUM = 18;
      constexpr int PCLK_GPIO_NUM = 12;

      camera_config_t camCfg {};
      camCfg.ledc_channel = LEDC_CHANNEL_0;
      camCfg.ledc_timer = LEDC_TIMER_0;
      camCfg.pin_d0 = Y2_GPIO_NUM;
      camCfg.pin_d1 = Y3_GPIO_NUM;
      camCfg.pin_d2 = Y4_GPIO_NUM;
      camCfg.pin_d3 = Y5_GPIO_NUM;
      camCfg.pin_d4 = Y6_GPIO_NUM;
      camCfg.pin_d5 = Y7_GPIO_NUM;
      camCfg.pin_d6 = Y8_GPIO_NUM;
      camCfg.pin_d7 = Y9_GPIO_NUM;
      camCfg.pin_xclk = XCLK_GPIO_NUM;
      camCfg.pin_pclk = PCLK_GPIO_NUM;
      camCfg.pin_vsync = VSYNC_GPIO_NUM;
      camCfg.pin_href = HREF_GPIO_NUM;
      camCfg.pin_sccb_sda = SIOD_GPIO_NUM;
      camCfg.pin_sccb_scl = SIOC_GPIO_NUM;
      camCfg.pin_pwdn = PWDN_GPIO_NUM;
      camCfg.pin_reset = RESET_GPIO_NUM;
      camCfg.xclk_freq_hz = 20000000;
      camCfg.frame_size = FRAMESIZE_HD;
      camCfg.pixel_format = PIXFORMAT_JPEG;
      camCfg.grab_mode = CAMERA_GRAB_LATEST;
      camCfg.fb_location = CAMERA_FB_IN_PSRAM;
      camCfg.jpeg_quality = 16;
      camCfg.fb_count = 2;

      // camera init
      const auto isOK = (esp_camera_init(&camCfg) == ESP_OK);
      if (isOK) {
        auto pSensor = esp_camera_sensor_get();
        pSensor->set_vflip(pSensor, 1);
        pSensor->set_hmirror(pSensor, 1);
        delay(100);
      }
      return isOK;
    }
};

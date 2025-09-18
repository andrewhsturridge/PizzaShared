#include "PizzaAudio.h"
#ifdef PIZZA_ENABLE_AUDIO_MODULE
  #include <driver/i2s.h>
#endif

namespace PizzaAudio {
#ifdef PIZZA_ENABLE_AUDIO_MODULE
  static bool s_init=false;
  bool beginI2S() {
    if (s_init) return true;
    i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 22050,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_MSB,
      .intr_alloc_flags = 0,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0
    };
    i2s_pin_config_t pins = {
      .bck_io_num = 43,
      .ws_io_num  = 44,
      .data_out_num = 12,
      .data_in_num = I2S_PIN_NO_CHANGE
    };
    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;
    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;
    s_init = true; return true;
  }

  void playClip(const int16_t* pcm, size_t samples, uint8_t vol) {
    if (!s_init || !pcm || samples==0) return;
    // simple software volume
    static int16_t buf[512];
    size_t i = 0;
    while (i < samples) {
      size_t chunk = min((size_t)512, samples - i);
      for (size_t j=0;j<chunk;j++) buf[j] = (int16_t)((int32_t)pcm[i+j]*vol/255);
      size_t wrote=0; i2s_write(I2S_NUM_0, (const char*)buf, chunk*2, &wrote, portMAX_DELAY);
      i += chunk;
    }
  }
#else
  bool beginI2S(){ return true; }
  void playClip(const int16_t*, size_t, uint8_t){}
#endif
}

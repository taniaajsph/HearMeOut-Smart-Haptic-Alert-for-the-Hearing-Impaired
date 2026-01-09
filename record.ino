#include <driver/i2s.h>

#define I2S_WS 19
#define I2S_SD 33
#define I2S_SCK 18
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 8000

void setup() {
  // CHANGED: Increased speed to handle audio data
  Serial.begin(460800); 
  
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, 
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };
  i2s_set_pin(I2S_PORT, &pin_config);
}

void loop() {
  int32_t sample = 0;
  size_t bytes_read;
  
  i2s_read(I2S_PORT, &sample, 4, &bytes_read, portMAX_DELAY);
  
  if (bytes_read > 0) {
    // >> 11 makes it louder (16x boost)
    // >> 14 is standard volume
    // was: int16_t sample16 = (int16_t)(sample >> 11);
int16_t sample16 = (int16_t)(sample >> 13); 
    Serial.write((uint8_t*)&sample16, 2); 
  }
}
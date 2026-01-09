/* * FINAL INTEGRATION: HearMeOut (Corrected Channel)
 * HARDWARE: 
 * - Mic: INMP441 (SCK=18, WS=19, SD=33, L/R to VDD -> RIGHT CHANNEL)
 * - Motor: GPIO 23 (via Transistor)
 */

#include <siren_inferencing.h> 
#include <driver/i2s.h>

// --- PINS ---
#define I2S_WS 19
#define I2S_SD 33
#define I2S_SCK 18
#define I2S_PORT I2S_NUM_0
#define MOTOR_PIN 23 

// --- TUNING ---
#define SAMPLE_RATE 8000   
#define NOISE_GATE_THRESHOLD 500 // Increased slightly to be safe
#define BIT_SHIFT 13       
#define CONFIDENCE_THRESHOLD 0.7 

// DYNAMIC BUFFERS
int16_t *sampleBuffer = NULL;
int32_t *raw32Buffer = NULL;

// --- HAPTIC PATTERNS ---
void triggerSirenPattern() {
    Serial.println(">>> 🚨 SIREN DETECTED: VIBRATING FAST");
    for(int i=0; i<5; i++){
        digitalWrite(MOTOR_PIN, HIGH);
        delay(100); 
        digitalWrite(MOTOR_PIN, LOW);
        delay(100); 
    }
}

void triggerDoorbellPattern() {
    Serial.println(">>> 🔔 DOORBELL DETECTED: VIBRATING LONG");
    for(int i=0; i<2; i++){
        digitalWrite(MOTOR_PIN, HIGH);
        delay(600); 
        digitalWrite(MOTOR_PIN, LOW);
        delay(300); 
    }
}

// --- AUDIO FEEDER ---
int raw_audio_get_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (float)sampleBuffer[offset + i];
    }
    return 0;
}

void setup() {
    Serial.begin(115200);
    
    // SETUP MOTOR
    pinMode(MOTOR_PIN, OUTPUT);
    digitalWrite(MOTOR_PIN, LOW);
    
    // Quick "Startup Buzz"
    digitalWrite(MOTOR_PIN, HIGH); delay(150); digitalWrite(MOTOR_PIN, LOW);
    
    delay(1000); 
    Serial.println("=== SYSTEM READY ===");

    // ALLOCATE BUFFERS
    sampleBuffer = (int16_t*)malloc(EI_CLASSIFIER_RAW_SAMPLE_COUNT * sizeof(int16_t));
    raw32Buffer = (int32_t*)malloc(EI_CLASSIFIER_RAW_SAMPLE_COUNT * sizeof(int32_t));
    
    // I2S CONFIG
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        
        // *** FIX 1: CHANGED TO RIGHT CHANNEL ***
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, 
        
        .communication_format = I2S_COMM_FORMAT_I2S, 
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,    
        .dma_buf_len = 32,     
        .use_apll = false 
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = -1,
        .data_in_num = I2S_SD
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_set_clk(I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_MONO);
}

void loop() {
    size_t bytesIn = 0;
    
    esp_err_t result = i2s_read(I2S_PORT, raw32Buffer, 
                               EI_CLASSIFIER_RAW_SAMPLE_COUNT * sizeof(int32_t), 
                               &bytesIn, portMAX_DELAY);
    
    if (result == ESP_OK && bytesIn > 0) {
        
        // *** FIX 2: DC OFFSET REMOVAL (Auto-Calibration) ***
        long dc_offset_sum = 0;
        for (int i = 0; i < EI_CLASSIFIER_RAW_SAMPLE_COUNT; i++) {
             int32_t sample = raw32Buffer[i] >> BIT_SHIFT;
             dc_offset_sum += sample;
        }
        int16_t dc_offset = dc_offset_sum / EI_CLASSIFIER_RAW_SAMPLE_COUNT;

        long energy_sum = 0;
        
        for (int i = 0; i < EI_CLASSIFIER_RAW_SAMPLE_COUNT; i++) {
            int32_t sample = raw32Buffer[i] >> BIT_SHIFT; 
            
            // Subtract the offset to center the signal at 0
            sample = sample - dc_offset; 

            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            
            // Noise Gate
            if (abs(sample) < NOISE_GATE_THRESHOLD) sample = 0;
            
            sampleBuffer[i] = (int16_t)sample;
            energy_sum += abs(sampleBuffer[i]);
        }
        
        float avgVol = energy_sum / (float)EI_CLASSIFIER_RAW_SAMPLE_COUNT;

        // TRIGGER AI
        if (avgVol > 10) { 
            signal_t signal;
            signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
            signal.get_data = &raw_audio_get_data;
            
            ei_impulse_result_t r = { 0 };
            run_classifier(&signal, &r, false);
            
            for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                if (r.classification[ix].value > CONFIDENCE_THRESHOLD) {
                    String winner = r.classification[ix].label;
                    Serial.print("Winner: "); Serial.println(winner);
                    
                    if (winner == "siren") triggerSirenPattern();
                    else if (winner == "doorbell") triggerDoorbellPattern();
                }
            }
        } 
    }
}
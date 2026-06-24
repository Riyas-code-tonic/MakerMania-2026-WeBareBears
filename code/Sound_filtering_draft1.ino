#include <driver/i2s.h>
#include <math.h>

// ─── I2S Pins ────────────────────────────────────────────
#define I2S_WS   25
#define I2S_SCK  33
#define I2S_SD   32

// ─── Settings ────────────────────────────────────────────
#define SAMPLE_RATE  16000
#define BUFFER_SIZE  256

// ─── DC Offset Removal ───────────────────────────────────
// Tracks and removes the slow-moving DC bias of the mic
float dcOffset = 0.0f;
const float DC_ALPHA = 0.999f;  // Very slow tracking — only removes true DC

float removeDC(float x) {
  dcOffset = DC_ALPHA * dcOffset + (1.0f - DC_ALPHA) * x;
  return x - dcOffset;
}

// ─── High-Pass Filter (~150 Hz cutoff) ───────────────────
// Removes engine rumble, road vibration, wind drone
float hpf_prev_in  = 0.0f;
float hpf_prev_out = 0.0f;
const float HPF_ALPHA = 0.988f;

float highPass(float x) {
  float y = HPF_ALPHA * (hpf_prev_out + x - hpf_prev_in);
  hpf_prev_in  = x;
  hpf_prev_out = y;
  return y;
}

// ─── Low-Pass Filter (~3400 Hz cutoff) ───────────────────
// Removes high-frequency hiss and electrical interference
float lpf_prev = 0.0f;
const float LPF_ALPHA = 0.572f;

float lowPass(float x) {
  lpf_prev = lpf_prev + LPF_ALPHA * (x - lpf_prev);
  return lpf_prev;
}

// ─── Adaptive Noise Floor ────────────────────────────────
// Starts with a calibration period instead of a cold single-buffer init
double noiseFloor       = 0.0;
int    calibrationCount = 0;
double calibrationSum   = 0.0;
const int CALIBRATION_BUFFERS = 20;   // ~1.3 seconds of silence at startup
bool calibrated = false;

// ─── VAD Holdover ────────────────────────────────────────
// Keeps speech flag alive for N buffers after signal drops
// Prevents choppy cuts between words
int holdoverCount = 0;
const int HOLDOVER_BUFFERS = 8;   // ~0.5 seconds of holdover

// ─── AGC State ───────────────────────────────────────────
// Tracks the recent peak and applies a gain to normalize output level
float agcGain        = 1.0f;
const float AGC_MAX  = 8.0f;    // Maximum gain (avoid over-amplifying silence)
const float AGC_MIN  = 0.5f;    // Minimum gain (avoid crushing loud input)
const float AGC_ATTACK  = 0.01f;  // How fast gain reduces when signal is loud
const float AGC_RELEASE = 0.001f; // How slowly gain recovers when signal is quiet

float applyAGC(float x) {
  float absVal = fabsf(x);

  // If signal level is high, reduce gain quickly (attack)
  // If signal level is low, recover gain slowly (release)
  if (absVal * agcGain > 0.9f) {
    agcGain -= AGC_ATTACK * agcGain;
  } else {
    agcGain += AGC_RELEASE;
  }

  // Clamp gain within safe range
  if (agcGain > AGC_MAX) agcGain = AGC_MAX;
  if (agcGain < AGC_MIN) agcGain = AGC_MIN;

  // Apply gain and hard-clip to prevent overflow
  float out = x * agcGain;
  if (out >  1.0f) out =  1.0f;
  if (out < -1.0f) out = -1.0f;
  return out;
}

void setup() {
  Serial.begin(115200);

  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = BUFFER_SIZE,
    .use_apll             = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num   = I2S_SCK,
    .ws_io_num    = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  Serial.println("Calibrating noise floor — stay quiet...");
}

void loop() {
  int32_t buffer[BUFFER_SIZE];
  size_t bytesRead = 0;

  i2s_read(I2S_NUM_0,
           buffer,
           sizeof(buffer),
           &bytesRead,
           portMAX_DELAY);

  int samples = bytesRead / sizeof(int32_t);
  double sumSquares = 0.0;

  for (int i = 0; i < samples; i++) {

    // Step 1: Extract 24-bit sample from 32-bit I2S frame
    int32_t raw = buffer[i] >> 8;

    // Step 2: Normalize to float -1.0 to +1.0
    float x = (float)raw / 8388608.0f;

    // Step 3: Remove DC offset
    x = removeDC(x);

    // Step 4: High-pass filter (removes rumble below ~150 Hz)
    x = highPass(x);

    // Step 5: Low-pass filter (removes hiss above ~3400 Hz)
    // Steps 4+5 together = band-pass keeping voice frequencies only
    x = lowPass(x);

    // Step 6: AGC — normalize output level
    x = applyAGC(x);

    // Accumulate RMS
    sumSquares += (double)x * (double)x;

    // Write processed sample back to buffer
    buffer[i] = (int32_t)(x * 8388608.0f);
  }

  // ─── RMS of this buffer ──────────────────────────────
  double rms = sqrt(sumSquares / samples);

  // ─── Calibration Phase ───────────────────────────────
  // Collect several quiet buffers at startup before trusting the noise floor
  if (!calibrated) {
    calibrationSum += rms;
    calibrationCount++;

    if (calibrationCount >= CALIBRATION_BUFFERS) {
      noiseFloor = calibrationSum / calibrationCount;
      calibrated = true;
      Serial.print("Noise floor calibrated: ");
      Serial.println((long)noiseFloor);
    } else {
      Serial.print("Calibrating... buffer ");
      Serial.print(calibrationCount);
      Serial.print(" / ");
      Serial.println(CALIBRATION_BUFFERS);
    }
    return;  // Don't transmit during calibration
  }

  // ─── Adaptive Noise Floor Update ─────────────────────
  // Only update floor when signal is close to background level
  // Tighter condition (1.2×) prevents loud noise from raising the floor
  if (rms < noiseFloor * 1.2) {
    noiseFloor = 0.995 * noiseFloor + 0.005 * rms;
  }

  // ─── Voice Activity Detection (VAD) ──────────────────
  bool speechDetected = (rms > noiseFloor * 3.0);

  // Holdover: keep speech flag alive for a few buffers after signal drops
  // Prevents choppy cuts between words during natural pauses
  if (speechDetected) {
    holdoverCount = HOLDOVER_BUFFERS;
  } else if (holdoverCount > 0) {
    holdoverCount--;
    speechDetected = true;  // Keep flag alive during holdover
  }

  // ─── Output ──────────────────────────────────────────
  // If no speech detected, zero out the buffer (mute)
  // This is where you will later pass buffer[] to A2DP transmitter
  if (!speechDetected) {
    memset(buffer, 0, samples * sizeof(int32_t));
  }

  // ─── Serial Monitor ──────────────────────────────────
  Serial.print("RMS: ");
  Serial.print((long)rms);
  Serial.print("  Floor: ");
  Serial.print((long)noiseFloor);
  Serial.print("  Ratio: ");
  Serial.print(rms / (noiseFloor + 1.0), 2);
  Serial.print("  Gain: ");
  Serial.print(agcGain, 2);
  Serial.print("  Status: ");
  Serial.println(speechDetected ? "SPEECH" : "NOISE");
}

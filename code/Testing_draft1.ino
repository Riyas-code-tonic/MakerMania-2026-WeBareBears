#include <driver/i2s.h>
#include <math.h>
#include "BluetoothA2DPSource.h"   // ← ADDED

// ─── I2S Pins (unchanged) ─────────────────────────────────
#define I2S_WS   25
#define I2S_SCK  33
#define I2S_SD   32

// ─── Audio Settings ───────────────────────────────────────
// CHANGED: 16000 → 44100 because A2DP requires 44100 Hz
// All filter coefficients below are recalculated for 44100 Hz
#define SAMPLE_RATE  44100
#define BUFFER_SIZE  256
#define NUM_BANDS    4

// ─── Bluetooth A2DP ───────────────────────────────────────
// ADDED: A2DP object and ring buffer
// The ring buffer passes audio between your DSP loop
// and the BT callback which runs on a separate core
BluetoothA2DPSource a2dp_source;

#define RING_SIZE 4096
volatile int16_t ringBuf[RING_SIZE];
volatile int     ringHead = 0;
volatile int     ringTail = 0;

// This function is called automatically by the BT stack
// whenever it needs audio to send to your earphones
// You never call this yourself — the library calls it
int32_t provideAudio(Frame* frame, int32_t frame_count) {
  for (int i = 0; i < frame_count; i++) {
    int16_t sample = 0;
    if (ringHead != ringTail) {
      sample   = ringBuf[ringTail];
      ringTail = (ringTail + 1) % RING_SIZE;
    }
    frame[i].channel1 = sample;   // Left
    frame[i].channel2 = sample;   // Right (same mono audio)
  }
  return frame_count;
}

// ─── DC Offset Removal (unchanged) ───────────────────────
float dcOffset = 0.0f;
const float DC_ALPHA = 0.999f;

float removeDC(float x) {
  dcOffset = DC_ALPHA * dcOffset + (1.0f - DC_ALPHA) * x;
  return x - dcOffset;
}

// ─── High-Pass Filter (~280 Hz cutoff) ────────────────────
// CHANGED: alpha recalculated for 44100 Hz
// RC = 1/(2*pi*280) = 5.684e-4
// dt = 1/44100     = 2.268e-5
// alpha = RC/(RC+dt) = 0.962
float hpf_prev_in  = 0.0f;
float hpf_prev_out = 0.0f;
const float HPF_ALPHA = 0.962f;   // was 0.975 at 16000 Hz

float highPass(float x) {
  float y = HPF_ALPHA * (hpf_prev_out + x - hpf_prev_in);
  hpf_prev_in  = x;
  hpf_prev_out = y;
  return y;
}

// ─── Low-Pass Filter (~3400 Hz cutoff) ────────────────────
// CHANGED: alpha recalculated for 44100 Hz
// RC = 1/(2*pi*3400) = 4.682e-5
// dt = 1/44100       = 2.268e-5
// alpha = dt/(RC+dt) = 0.326
float lpf_prev = 0.0f;
const float LPF_ALPHA = 0.326f;   // was 0.572 at 16000 Hz

float lowPass(float x) {
  lpf_prev += LPF_ALPHA * (x - lpf_prev);
  return lpf_prev;
}

// ─── 4-Band Splitter ──────────────────────────────────────
// CHANGED: all alphas recalculated for 44100 Hz
// formula: alpha = 1 - exp(-2*pi*fc/fs)
// 600  Hz: 1 - exp(-2*pi*600/44100)  = 0.082
// 1200 Hz: 1 - exp(-2*pi*1200/44100) = 0.157
// 2200 Hz: 1 - exp(-2*pi*2200/44100) = 0.269
float bsf_600_prev  = 0.0f;
float bsf_1200_prev = 0.0f;
float bsf_2200_prev = 0.0f;

const float BSF_ALPHA_600  = 0.082f;   // was 0.210 at 16000 Hz
const float BSF_ALPHA_1200 = 0.157f;   // was 0.376 at 16000 Hz
const float BSF_ALPHA_2200 = 0.269f;   // was 0.578 at 16000 Hz

void bandSplit(float x, float bands[4]) {
  bsf_600_prev  += BSF_ALPHA_600  * (x - bsf_600_prev);
  bsf_1200_prev += BSF_ALPHA_1200 * (x - bsf_1200_prev);
  bsf_2200_prev += BSF_ALPHA_2200 * (x - bsf_2200_prev);

  bands[0] = bsf_600_prev;
  bands[1] = bsf_1200_prev - bsf_600_prev;
  bands[2] = bsf_2200_prev - bsf_1200_prev;
  bands[3] = x - bsf_2200_prev;
}

// ─── AGC (unchanged) ──────────────────────────────────────
float agcGain = 1.0f;
const float AGC_MAX     = 8.0f;
const float AGC_MIN     = 0.5f;
const float AGC_ATTACK  = 0.01f;
const float AGC_RELEASE = 0.002f;

float applyAGC(float x, bool active) {
  if (!active) return x;
  float absVal = fabsf(x);
  if (absVal * agcGain > 0.9f)
    agcGain -= AGC_ATTACK * agcGain;
  else
    agcGain += AGC_RELEASE;
  agcGain = fminf(fmaxf(agcGain, AGC_MIN), AGC_MAX);
  float out = x * agcGain;
  return fminf(fmaxf(out, -1.0f), 1.0f);
}

// ─── Noise Estimation ─────────────────────────────────────
double noiseFloor = 0.0;
double noiseBands[NUM_BANDS] = {0.0, 0.0, 0.0, 0.0};

int    calibrationCount = 0;
double calibrationSum   = 0.0;
double calibBandSums[NUM_BANDS] = {0.0, 0.0, 0.0, 0.0};

// CHANGED: 30 → 50 frames
// At 44100 Hz, each 512-sample frame = 11.6 ms
// 50 frames = ~580 ms of calibration (same real-time as before)
const int CALIBRATION_FRAMES = 50;
bool calibrated = false;

// ─── Band Suppression Gains (unchanged) ───────────────────
float bandGains[NUM_BANDS] = {0.02f, 0.02f, 0.02f, 0.02f};

// ─── Temporal Modulation Tracker ──────────────────────────
// CHANGED: 20 → 30 frames
// At 44100 Hz, 30 frames = ~350 ms (same real-time as before)
#define MOD_HISTORY_LEN 30
float rmsHistory[MOD_HISTORY_LEN] = {0};
int   rmsHistIdx  = 0;
bool  rmsHistFull = false;

float computeModulationIndex() {
  int count = rmsHistFull ? MOD_HISTORY_LEN : rmsHistIdx;
  if (count < 6) return 0.0f;

  float mean = 0.0f;
  for (int i = 0; i < count; i++) mean += rmsHistory[i];
  mean /= (float)count;

  if (mean < 1e-10f) return 0.0f;

  float var = 0.0f;
  for (int i = 0; i < count; i++) {
    float d = rmsHistory[i] - mean;
    var += d * d;
  }
  return sqrtf(var / (float)count) / mean;
}

// ─── VAD State (unchanged) ────────────────────────────────
int  confirmCount  = 0;
int  holdoverCount = 0;
bool speechActive  = false;

// ─── Filtered Sample Buffer (unchanged) ───────────────────
static float filteredBuf[BUFFER_SIZE];

void setup() {
  Serial.begin(115200);

  // ─── I2S Microphone (unchanged except sample rate) ────
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

  // ─── ADDED: Start Bluetooth A2DP ──────────────────────
  // Replace the text below with your earphone's exact BT name
  // Find this name by scanning for BT devices on your phone
  // Example: "boAt Airdopes 141" or "WI-C100" or "JBL GO 3"
  // Capitalization must match exactly
  a2dp_source.start("boAt Rockerz 255 Pro+", provideAudio);

  Serial.println("BT connecting — put earphones in pairing mode now");
  Serial.println("Calibrating — stay quiet for ~1 second...");
}

void loop() {
  int32_t inBuffer[BUFFER_SIZE];
  size_t  bytesRead = 0;

  i2s_read(I2S_NUM_0,
           inBuffer,
           sizeof(inBuffer),
           &bytesRead,
           portMAX_DELAY);

  int samples = bytesRead / sizeof(int32_t);

  double sumSquares = 0.0;
  double bandEnergy[NUM_BANDS] = {0.0, 0.0, 0.0, 0.0};

  float saved_bsf_600  = bsf_600_prev;
  float saved_bsf_1200 = bsf_1200_prev;
  float saved_bsf_2200 = bsf_2200_prev;

  // ═══════════════════════════════════════════════════════
  //  PASS 1 — Filter and compute metrics (unchanged)
  // ═══════════════════════════════════════════════════════

  for (int i = 0; i < samples; i++) {
    int32_t raw = inBuffer[i] >> 8;
    float x = (float)raw / 8388608.0f;

    x = removeDC(x);
    x = highPass(x);
    x = lowPass(x);

    filteredBuf[i] = x;

    float bands[4];
    bandSplit(x, bands);
    for (int b = 0; b < NUM_BANDS; b++) {
      bandEnergy[b] += (double)bands[b] * (double)bands[b];
    }
    sumSquares += (double)x * (double)x;
  }

  double rms = sqrt(sumSquares / samples);

  double bandRms[NUM_BANDS];
  for (int b = 0; b < NUM_BANDS; b++) {
    bandRms[b] = sqrt(bandEnergy[b] / samples);
  }

  // ═══════════════════════════════════════════════════════
  //  CALIBRATION (unchanged)
  // ═══════════════════════════════════════════════════════

  if (!calibrated) {
    calibrationSum += rms;
    for (int b = 0; b < NUM_BANDS; b++) {
      calibBandSums[b] += bandRms[b];
    }
    calibrationCount++;

    if (calibrationCount >= CALIBRATION_FRAMES) {
      noiseFloor = calibrationSum / calibrationCount;
      for (int b = 0; b < NUM_BANDS; b++) {
        noiseBands[b] = calibBandSums[b] / calibrationCount;
      }
      calibrated = true;
      Serial.println("--- Calibration Complete. Listening... ---");
    } else {
      Serial.print("Calibrating... ");
      Serial.print(calibrationCount);
      Serial.print(" / ");
      Serial.println(CALIBRATION_FRAMES);
    }
    return;
  }

  // ═══════════════════════════════════════════════════════
  //  FEATURE COMPUTATION (unchanged)
  // ═══════════════════════════════════════════════════════

  const float eps = 1e-10f;

  float logSum = 0.0f;
  float linSum = 0.0f;
  for (int b = 0; b < NUM_BANDS; b++) {
    logSum += logf((float)bandRms[b] + eps);
    linSum += (float)bandRms[b];
  }
  float geoMean          = expf(logSum / NUM_BANDS);
  float ariMean          = linSum / NUM_BANDS;
  float spectralFlatness = geoMean / (ariMean + eps);

  float midEnergy  = (float)(bandRms[1] + bandRms[2]);
  float edgeEnergy = (float)(bandRms[0] + bandRms[3]);
  float bandRatio  = midEnergy / (edgeEnergy + eps);

  rmsHistory[rmsHistIdx] = (float)rms;
  rmsHistIdx = (rmsHistIdx + 1) % MOD_HISTORY_LEN;
  if (rmsHistIdx == 0) rmsHistFull = true;
  float modulationIdx = computeModulationIndex();

  // ═══════════════════════════════════════════════════════
  //  VAD DECISION (unchanged)
  // ═══════════════════════════════════════════════════════

  bool energyOK    = (rms > noiseFloor * 3.0);
  bool modulationOK = (modulationIdx > 0.20f);
  bool flatnessOK  = (spectralFlatness < 0.70f);
  bool bandRatioOK = (bandRatio > 0.8f);

  bool rawVAD = energyOK && modulationOK && (flatnessOK || bandRatioOK);

  if (rawVAD) {
    confirmCount++;
    holdoverCount = 20;   // CHANGED: 12 → 20 frames (~232 ms at 44100 Hz)
    if (confirmCount >= 2) speechActive = true;
  } else {
    confirmCount = 0;
    if (holdoverCount > 0) holdoverCount--;
    else speechActive = false;
  }

  // ═══════════════════════════════════════════════════════
  //  NOISE FLOOR UPDATE (unchanged)
  // ═══════════════════════════════════════════════════════

  if (!speechActive) {
    for (int b = 0; b < NUM_BANDS; b++) {
      noiseBands[b] = 0.95 * noiseBands[b] + 0.05 * bandRms[b];
    }
    if (rms < noiseFloor * 2.0)
      noiseFloor = 0.98 * noiseFloor + 0.02 * rms;
  }
  if (rms < noiseFloor * 1.5)
    noiseFloor = 0.9995 * noiseFloor + 0.0005 * rms;

  // ═══════════════════════════════════════════════════════
  //  NOISE SUPPRESSION GAINS (unchanged)
  // ═══════════════════════════════════════════════════════

  for (int b = 0; b < NUM_BANDS; b++) {
    float newGain;
    if (bandRms[b] > eps) {
      float snr  = (float)(bandRms[b] / (noiseBands[b] + eps));
      newGain    = snr / (snr+1.0f);
      float flr  = speechActive ? 0.08f : 0.02f;
      newGain    = fmaxf(newGain, flr);
    } else {
      newGain = 0.02f;
    }
    bandGains[b] = 0.7f * bandGains[b] + 0.3f * newGain;
  }
    Serial.print("Gains: ");
    for (int b = 0; b < NUM_BANDS; b++) {
    Serial.print(bandGains[b], 2);
    Serial.print(" ");
}
Serial.println();

  // ═══════════════════════════════════════════════════════
  //  PASS 2 — Noise suppression + AGC (unchanged)
  // ═══════════════════════════════════════════════════════

  bsf_600_prev  = saved_bsf_600;
  bsf_1200_prev = saved_bsf_1200;
  bsf_2200_prev = saved_bsf_2200;

  for (int i = 0; i < samples; i++) {
    float x = filteredBuf[i];

    float bands[4];
    bandSplit(x, bands);

    float output = 0.0f;
    for (int b = 0; b < NUM_BANDS; b++) output += bands[b] * bandGains[b];

    output *= 4.0f;
    if (!speechActive) output *= 0.10f;
    output = fminf(fmaxf(output, -1.0f), 1.0f);

    inBuffer[i] = (int32_t)(output * 8388608.0f);
  }

  // ═══════════════════════════════════════════════════════
  //  ADDED: Push processed audio into ring buffer for BT
  // Converts 24-bit samples → 16-bit and writes to ring buffer
  // The provideAudio callback above drains this buffer
  // ═══════════════════════════════════════════════════════

  for (int i = 0; i < samples; i++) {
    int16_t s    = (int16_t)(inBuffer[i] >> 8);
    int     next = (ringHead + 1) % RING_SIZE;
    if (next == ringTail)
    ringTail = (ringTail + 1) % RING_SIZE;

    ringBuf[ringHead] = s;
    ringHead = next;
  }

  // ═══════════════════════════════════════════════════════
  //  SERIAL MONITOR (unchanged)
  // ═══════════════════════════════════════════════════════

  // Serial.print("RMS:");
  // Serial.print(rms, 6);
  // Serial.print("AGC = ");
  // Serial.println(agcGain);
  // Serial.print("  Floor:");
  // Serial.print(noiseFloor, 6);
  // Serial.print("  Flat:");
  // Serial.print(spectralFlatness, 2);
  // Serial.print("  BR:");
  // Serial.print(bandRatio, 2);
  // Serial.print("  Mod:");
  // Serial.print(modulationIdx, 2);
  // Serial.print("  |E:");
  // Serial.print(energyOK     ? "Y" : "N");
  // Serial.print(" M:");
  // Serial.print(modulationOK ? "Y" : "N");
  // Serial.print(" F:");
  // Serial.print(flatnessOK   ? "Y" : "N");
  // Serial.print(" B:");
  // Serial.print(bandRatioOK  ? "Y" : "N");
  // Serial.print("|  >> ");
  // Serial.println(speechActive ? "SPEECH" : "NOISE");


}

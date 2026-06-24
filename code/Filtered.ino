#include <driver/i2s.h>
#include <math.h>

// ─── I2S Pins ─────────────────────────────────────────────
#define I2S_WS   25
#define I2S_SCK  33
#define I2S_SD   32

// ─── Audio Settings ───────────────────────────────────────
#define SAMPLE_RATE  16000
#define BUFFER_SIZE  512
#define NUM_BANDS    4

// ─── DC Offset Removal ────────────────────────────────────
float dcOffset = 0.0f;
const float DC_ALPHA = 0.999f;

float removeDC(float x) {
  dcOffset = DC_ALPHA * dcOffset + (1.0f - DC_ALPHA) * x;
  return x - dcOffset;
}

// ─── High-Pass Filter (~280 Hz cutoff) ────────────────────
float hpf_prev_in  = 0.0f;
float hpf_prev_out = 0.0f;
const float HPF_ALPHA = 0.975f;

float highPass(float x) {
  float y = HPF_ALPHA * (hpf_prev_out + x - hpf_prev_in);
  hpf_prev_in  = x;
  hpf_prev_out = y;
  return y;
}

// ─── Low-Pass Filter (~3400 Hz cutoff) ────────────────────
float lpf_prev = 0.0f;
const float LPF_ALPHA = 0.572f;

float lowPass(float x) {
  lpf_prev += LPF_ALPHA * (x - lpf_prev);
  return lpf_prev;
}

// ─── 4-Band Splitter ──────────────────────────────────────
// Band 0: 280–600 Hz   (low formants + wind)
// Band 1: 600–1200 Hz  (core speech energy)
// Band 2: 1200–2200 Hz (upper formants)
// Band 3: 2200–3400 Hz (fricatives, sibilants)
float bsf_600_prev  = 0.0f;
float bsf_1200_prev = 0.0f;
float bsf_2200_prev = 0.0f;

const float BSF_ALPHA_600  = 0.210f;
const float BSF_ALPHA_1200 = 0.376f;
const float BSF_ALPHA_2200 = 0.578f;

void bandSplit(float x, float bands[4]) {
  bsf_600_prev  += BSF_ALPHA_600  * (x - bsf_600_prev);
  bsf_1200_prev += BSF_ALPHA_1200 * (x - bsf_1200_prev);
  bsf_2200_prev += BSF_ALPHA_2200 * (x - bsf_2200_prev);

  bands[0] = bsf_600_prev;
  bands[1] = bsf_1200_prev - bsf_600_prev;
  bands[2] = bsf_2200_prev - bsf_1200_prev;
  bands[3] = x - bsf_2200_prev;
}

// ─── AGC ──────────────────────────────────────────────────
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
const int CALIBRATION_FRAMES = 30;
bool calibrated = false;

// ─── Band Suppression Gains ───────────────────────────────
float bandGains[NUM_BANDS] = {0.02f, 0.02f, 0.02f, 0.02f};

// ─── Temporal Modulation Tracker ──────────────────────────
// Longer history = better at distinguishing steady noise from
// syllabic speech rhythm. 20 frames = ~640 ms of context.
#define MOD_HISTORY_LEN 20
float rmsHistory[MOD_HISTORY_LEN] = {0};
int   rmsHistIdx  = 0;
bool  rmsHistFull = false;

float computeModulationIndex() {
  int count = rmsHistFull ? MOD_HISTORY_LEN : rmsHistIdx;

  // Need enough history to detect syllabic rhythm
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

// ─── VAD State ────────────────────────────────────────────
int  confirmCount  = 0;
int  holdoverCount = 0;
bool speechActive  = false;

// ─── Filtered Sample Buffer ───────────────────────────────
static float filteredBuf[BUFFER_SIZE];

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
  //  PASS 1 — Filter and compute metrics
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
  //  CALIBRATION
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
      Serial.println("--- Calibration Complete ---");
      Serial.print("  Noise floor: ");
      Serial.println(noiseFloor, 8);
      for (int b = 0; b < NUM_BANDS; b++) {
        Serial.print("  Band ");
        Serial.print(b);
        Serial.print(" noise: ");
        Serial.println(noiseBands[b], 8);
      }
      Serial.println("--- Listening... ---");
    } else {
      Serial.print("Calibrating... ");
      Serial.print(calibrationCount);
      Serial.print(" / ");
      Serial.println(CALIBRATION_FRAMES);
    }
    return;
  }

  // ═══════════════════════════════════════════════════════
  //  FEATURE COMPUTATION
  // ═══════════════════════════════════════════════════════

  const float eps = 1e-10f;

  // Spectral flatness
  // Speech has a peaked spectrum (low flatness value)
  // Traffic/wind has a flat spectrum (high flatness value)
  float logSum = 0.0f;
  float linSum = 0.0f;
  for (int b = 0; b < NUM_BANDS; b++) {
    logSum += logf((float)bandRms[b] + eps);
    linSum += (float)bandRms[b];
  }
  float geoMean          = expf(logSum / NUM_BANDS);
  float ariMean          = linSum / NUM_BANDS;
  float spectralFlatness = geoMean / (ariMean + eps);

  // Band ratio
  // Speech concentrates energy in mid bands (ratio > 1)
  // Wind concentrates in low band (ratio < 1)
  // Traffic is broadband (ratio ≈ 1)
  float midEnergy  = (float)(bandRms[1] + bandRms[2]);
  float edgeEnergy = (float)(bandRms[0] + bandRms[3]);
  float bandRatio  = midEnergy / (edgeEnergy + eps);

  // Temporal modulation index
  // THIS IS THE KEY DISCRIMINATOR
  // Speech has syllabic rhythm — RMS rises and falls with each word
  // Traffic and wind are STEADY — RMS barely changes over time
  // A high modulation index means the signal is turning on and off
  // which is exactly what speech does and noise does not
  rmsHistory[rmsHistIdx] = (float)rms;
  rmsHistIdx = (rmsHistIdx + 1) % MOD_HISTORY_LEN;
  if (rmsHistIdx == 0) rmsHistFull = true;
  float modulationIdx = computeModulationIndex();

  // ═══════════════════════════════════════════════════════
  //  VAD DECISION — redesigned with clear priority
  // ═══════════════════════════════════════════════════════

  // Gate 1 — Energy (mandatory)
  // Signal must be meaningfully above noise floor
  // 3.0x is a middle ground between too strict (4.0) and too loose (2.5)
  bool energyOK = (rms > noiseFloor * 3.0);

  // Gate 2 — Modulation (mandatory)
  // THIS IS NOW REQUIRED, NOT OPTIONAL
  // Traffic and wind are steady — they will always fail this check
  // Speech is rhythmic — it will always pass this check
  // Threshold of 0.20 means RMS must vary by at least 20% across
  // recent frames — speech does this naturally, noise does not
  bool modulationOK = (modulationIdx > 0.20f);

  // Gate 3 — Spectral shape (one of two must pass)
  // These two together catch edge cases but neither alone decides
  bool flatnessOK  = (spectralFlatness < 0.70f);
  bool bandRatioOK = (bandRatio > 0.8f);

  // Final VAD logic:
  // Energy AND modulation are BOTH mandatory
  // Plus at least one spectral shape check must agree
  // This means:
  //   Traffic noise → fails modulation (steady signal) → NOISE
  //   Wind noise    → fails modulation AND band ratio   → NOISE
  //   Human speech  → passes all four                   → SPEECH
  bool rawVAD = energyOK
             && modulationOK
             && (flatnessOK || bandRatioOK);

  // State machine: require 2 consecutive positive frames
  // This prevents a single loud transient from triggering speech
  // 2 frames = ~64 ms which is shorter than any real syllable
  if (rawVAD) {
    confirmCount++;
    holdoverCount = 12;
    if (confirmCount >= 2) speechActive = true;
  } else {
    confirmCount = 0;
    if (holdoverCount > 0) {
      holdoverCount--;
    } else {
      speechActive = false;
    }
  }

  // ═══════════════════════════════════════════════════════
  //  NOISE FLOOR UPDATE
  // ═══════════════════════════════════════════════════════

  if (!speechActive) {
    for (int b = 0; b < NUM_BANDS; b++) {
      noiseBands[b] = 0.95 * noiseBands[b] + 0.05 * bandRms[b];
    }
    if (rms < noiseFloor * 2.0) {
      noiseFloor = 0.98 * noiseFloor + 0.02 * rms;
    }
  }

  if (rms < noiseFloor * 1.5) {
    noiseFloor = 0.9995 * noiseFloor + 0.0005 * rms;
  }

  // ═══════════════════════════════════════════════════════
  //  NOISE SUPPRESSION GAINS
  // ═══════════════════════════════════════════════════════

  for (int b = 0; b < NUM_BANDS; b++) {
    float newGain;
    if (bandRms[b] > eps) {
      float snr  = (float)(bandRms[b] / (noiseBands[b] + eps));
      newGain    = 1.0f - 1.0f / snr;
      float flr  = speechActive ? 0.08f : 0.02f;
      newGain    = fmaxf(newGain, flr);
    } else {
      newGain = 0.02f;
    }
    bandGains[b] = 0.7f * bandGains[b] + 0.3f * newGain;
  }

  // ═══════════════════════════════════════════════════════
  //  PASS 2 — Noise suppression + AGC
  // ═══════════════════════════════════════════════════════

  bsf_600_prev  = saved_bsf_600;
  bsf_1200_prev = saved_bsf_1200;
  bsf_2200_prev = saved_bsf_2200;

  for (int i = 0; i < samples; i++) {
    float x = filteredBuf[i];

    float bands[4];
    bandSplit(x, bands);

    float output = 0.0f;
    for (int b = 0; b < NUM_BANDS; b++) {
      output += bands[b] * bandGains[b];
    }

    output = applyAGC(output, speechActive);

    if (!speechActive) output *= 0.05f;

    output = fminf(fmaxf(output, -1.0f), 1.0f);

    inBuffer[i] = (int32_t)(output * 8388608.0f);
  }

  // ═══════════════════════════════════════════════════════
  //  SERIAL MONITOR
  // ═══════════════════════════════════════════════════════

  Serial.print("RMS:");
  Serial.print(rms, 6);
  Serial.print("  Floor:");
  Serial.print(noiseFloor, 6);
  Serial.print("  Flat:");
  Serial.print(spectralFlatness, 2);
  Serial.print("  BR:");
  Serial.print(bandRatio, 2);
  Serial.print("  Mod:");
  Serial.print(modulationIdx, 2);
  Serial.print("  |E:");
  Serial.print(energyOK     ? "Y" : "N");
  Serial.print(" M:");
  Serial.print(modulationOK ? "Y" : "N");
  Serial.print(" F:");
  Serial.print(flatnessOK   ? "Y" : "N");
  Serial.print(" B:");
  Serial.print(bandRatioOK  ? "Y" : "N");
  Serial.print("|  >> ");
  Serial.println(speechActive ? "SPEECH" : "NOISE");
}

// DSPEngine.cpp — Speech Enhancer for Helmet Communication
// Philosophy: enhance speech, reduce noise. Never kill speech.

#include <driver/i2s.h>
#include <math.h>
#include <string.h>
#include <Arduino.h>
#include "DSPEngine.h"

#define I2S_WS       25
#define I2S_SCK      33
#define I2S_SD       32
#define SAMPLE_RATE  44100
#define I2S_BUF_LEN  256
#define FFT_N        256
#define FFT_HALF     128
#define NUM_BINS     129
#define BIN_HZ       ((float)SAMPLE_RATE / FFT_N)
#define CALIB_FRAMES 200      // ~0.58s calibration
#define PRE_EMPH     0.97f    // pre-emphasis coefficient
#define WIENER_FLOOR 0.22f    // minimum Wiener gain — speech survives
#define VAD_HOLD     50       // ~145ms holdover

volatile int16_t ringBuf[RING_SIZE];
volatile int     ringHead = 0;
volatile int     ringTail = 0;

// ── Biquad ──

struct Biquad { float b0, b1, b2, a1, a2, x1, x2, y1, y2; };
static Biquad hpf, lpf;

static void bqInitHPF(Biquad &f, float fc, float fs) {
    float K = tanf((float)M_PI * fc / fs), K2 = K * K, s = 1.41421356f;
    float n = 1.0f / (1.0f + s * K + K2);
    f.b0 = n;  f.b1 = -2.0f * n;  f.b2 = n;
    f.a1 = 2.0f * (K2 - 1.0f) * n;
    f.a2 = (1.0f - s * K + K2) * n;
    f.x1 = f.x2 = f.y1 = f.y2 = 0.0f;
}

static void bqInitLPF(Biquad &f, float fc, float fs) {
    float K = tanf((float)M_PI * fc / fs), K2 = K * K, s = 1.41421356f;
    float n = 1.0f / (1.0f + s * K + K2);
    f.b0 = K2 * n;  f.b1 = 2.0f * K2 * n;  f.b2 = K2 * n;
    f.a1 = 2.0f * (K2 - 1.0f) * n;
    f.a2 = (1.0f - s * K + K2) * n;
    f.x1 = f.x2 = f.y1 = f.y2 = 0.0f;
}

static inline float bqTick(Biquad &f, float x) {
    float y = f.b0 * x + f.b1 * f.x1 + f.b2 * f.x2
                        - f.a1 * f.y1 - f.a2 * f.y2;
    f.x2 = f.x1;  f.x1 = x;
    f.y2 = f.y1;  f.y1 = y;
    return y;
}

// ── DC removal ──

static float dcEst = 0.0f;
static inline float removeDC(float x) {
    dcEst = 0.999f * dcEst + 0.001f * x;
    return x - dcEst;
}

// ── Pre/De-emphasis state ──

static float preEmphZ = 0.0f;
static float deEmphZ  = 0.0f;

// ── Hann window ──

static float hann[FFT_N];
static void initHann() {
    for (int i = 0; i < FFT_N; i++)
        hann[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i / (float)FFT_N));
}

// ── Radix-2 FFT ──

static void bitReverse(float *d, int n) {
    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        if (i < j) {
            float tr = d[2*i], ti = d[2*i+1];
            d[2*i] = d[2*j];      d[2*i+1] = d[2*j+1];
            d[2*j] = tr;          d[2*j+1] = ti;
        }
        int m = n >> 1;
        while (m >= 1 && j >= m) { j -= m; m >>= 1; }
        j += m;
    }
}

static void fftCompute(float *d, int n, bool inv) {
    bitReverse(d, n);
    float sgn = inv ? 1.0f : -1.0f;
    for (int step = 1; step < n; step <<= 1) {
        int   jump = step << 1;
        float delta = sgn * (float)M_PI / (float)step;
        float wR = cosf(delta), wI = sinf(delta);
        for (int grp = 0; grp < n; grp += jump) {
            float tR = 1.0f, tI = 0.0f;
            for (int k = 0; k < step; k++) {
                int i1 = 2 * (grp + k), i2 = 2 * (grp + k + step);
                float oR = tR * d[i2] - tI * d[i2+1];
                float oI = tR * d[i2+1] + tI * d[i2];
                d[i2]   = d[i1]   - oR;  d[i2+1] = d[i1+1] - oI;
                d[i1]  += oR;             d[i1+1] += oI;
                float nR = tR * wR - tI * wI;
                tI = tR * wI + tI * wR;   tR = nR;
            }
        }
    }
    if (inv) {
        float s = 1.0f / (float)n;
        for (int i = 0; i < 2 * n; i++) d[i] *= s;
    }
}

// ── Buffers & State ──

static float fftBuf[FFT_N * 2];
static float prevIn[FFT_HALF];
static float olaBuf[FFT_HALF];
static float inAcc[512];
static int   inAccN = 0;

static float nMag[NUM_BINS];
static float pGain[NUM_BINS];
static float cSum[NUM_BINS];
static float cRmsSum = 0.0f;
static int   cN = 0;
static bool  cDone = false;

static bool  spOn = false;
static int   vConf = 0, vHold = 0;
static float nRMS = 0.0f;
static float smoothRMS = 0.0f;
static float spp = 0.0f;           // speech presence probability (0-1)
static float prevWindConf = 0.0f;

#define MOD_N 40
static float mHist[MOD_N];
static int   mIdx = 0;
static bool  mFull = false;

static float modulation() {
    int c = mFull ? MOD_N : mIdx;
    if (c < 6) return 0.0f;
    float mn = 0.0f;
    for (int i = 0; i < c; i++) mn += mHist[i];
    mn /= (float)c;
    if (mn < 1e-10f) return 0.0f;
    float v = 0.0f;
    for (int i = 0; i < c; i++) { float d = mHist[i] - mn; v += d * d; }
    return sqrtf(v / (float)c) / mn;
}

static float agcG = 1.0f;
static int dbgCnt = 0;

// ── Core frame processor ──

static void processFrame(const float *newIn, float *out) {
    const float eps = 1e-10f;

    // ZCR from input (speech: 5-45 per 128 samples at 44.1kHz)
    int zcr = 0;
    for (int i = 1; i < FFT_HALF; i++)
        if ((newIn[i] >= 0.0f) != (newIn[i-1] >= 0.0f)) zcr++;

    // Assemble frame + Hann window
    for (int i = 0; i < FFT_HALF; i++) {
        fftBuf[2*i]   = prevIn[i] * hann[i];
        fftBuf[2*i+1] = 0.0f;
    }
    for (int i = 0; i < FFT_HALF; i++) {
        fftBuf[2*(FFT_HALF+i)]   = newIn[i] * hann[FFT_HALF+i];
        fftBuf[2*(FFT_HALF+i)+1] = 0.0f;
    }

    fftCompute(fftBuf, FFT_N, false);

    float mag[NUM_BINS];
    for (int k = 0; k < NUM_BINS; k++) {
        float r = fftBuf[2*k], im = fftBuf[2*k+1];
        mag[k] = sqrtf(r * r + im * im);
    }

    // ── Calibration ──
    if (!cDone) {
        for (int k = 0; k < NUM_BINS; k++) cSum[k] += mag[k];
        float te = 0.0f;
        for (int k = 0; k < NUM_BINS; k++) te += mag[k] * mag[k];
        cRmsSum += sqrtf(te / (float)FFT_N);
        cN++;
        if (cN >= CALIB_FRAMES) {
            for (int k = 0; k < NUM_BINS; k++) {
                nMag[k] = fmaxf((cSum[k] / (float)cN) * 1.25f, 0.0005f);
                pGain[k] = WIENER_FLOOR;
            }
            nRMS = fmaxf((cRmsSum / (float)cN) * 1.25f, 0.0008f);
            smoothRMS = nRMS;
            cDone = true;
            agcG = 1.0f;
            spp = 0.0f;
            memset(olaBuf, 0, sizeof(olaBuf));
            Serial.println("--- Calibration complete ---");
            Serial.printf("Noise RMS: %.6f\n", nRMS);
        } else if (cN % 50 == 0) {
            Serial.printf("Calibrating %d/%d\n", cN, CALIB_FRAMES);
        }
        memset(out, 0, FFT_HALF * sizeof(float));
        return;
    }

    // ── Feature extraction ──
    float spE = 0, lowE = 0, totE = 0;
    float wfS = 0, mS = 0, lnS = 0;
    float maxSp = 0;
    int nSB = 0;
    float spNoiseE = 0;

    for (int k = 0; k < NUM_BINS; k++) {
        float m2 = mag[k] * mag[k];
        totE += m2;
        if (k >= 2 && k <= 21) {
            spE += m2;
            wfS += (float)k * BIN_HZ * mag[k];
            mS  += mag[k];
            lnS += logf(mag[k] + eps);
            if (mag[k] > maxSp) maxSp = mag[k];
            spNoiseE += nMag[k] * nMag[k];
            nSB++;
        }
        if (k <= 4) lowE += m2;
    }

    float centroid = (mS > eps) ? wfS / mS : 0.0f;
    float flatness = (mS / (float)nSB > eps)
                   ? expf(lnS / (float)nSB) / (mS / (float)nSB) : 1.0f;
    float fRMS  = sqrtf(totE / (float)FFT_N);
    float lowR  = (totE > eps) ? lowE / totE : 0.0f;
    float avgSp = mS / (float)nSB;
    float crest = (avgSp > eps) ? maxSp / avgSp : 0.0f;

    mHist[mIdx] = fRMS;
    mIdx = (mIdx + 1) % MOD_N;
    if (mIdx == 0) mFull = true;

    smoothRMS = 0.70f * smoothRMS + 0.30f * fRMS;

    // ── Score-based VAD (total 9 points) ──
    float vadScore = 0.0f;

    if (smoothRMS > nRMS * 3.0f)                vadScore += 2.0f;  // energy
    if (spE > spNoiseE * 4.0f)                  vadScore += 2.0f;  // speech band SNR
    if (centroid > 400.0f && centroid < 3000.0f) vadScore += 1.0f;  // centroid
    if (flatness < 0.60f)                       vadScore += 1.0f;  // tonality
    if (crest > 2.0f)                           vadScore += 2.0f;  // harmonic peaks
    if (zcr >= 5 && zcr <= 45)                  vadScore += 1.0f;  // zero-crossing

    // Decision with hysteresis at score=4
    if (vadScore >= 5.0f) {
        vConf++;
        vHold = VAD_HOLD;
        if (vConf >= 2) spOn = true;
    } else {
        vConf = 0;
        if (vHold > 0) vHold--;
        else if (vadScore < 4.0f) spOn = false;
        // score 4 after holdover expires: keep previous state
    }

    // Speech Presence Probability — continuous, never binary
    float rawSPP = vadScore / 9.0f;
    spp = 0.88f * spp + 0.12f * rawSPP;
    float sppFactor = 0.25f + 0.75f * spp;  // range 0.25 (noise) to 1.0 (speech)

    // ── Wind confidence (graduated, not binary) ──
    float wc = 0.0f;
    if (lowR > 0.40f) wc += fminf((lowR - 0.40f) / 0.40f, 1.0f);
    if (centroid < 800.0f) wc += fminf((800.0f - centroid) / 800.0f, 1.0f);
    if (flatness > 0.35f) wc += fminf((flatness - 0.35f) / 0.50f, 1.0f);
    wc = fminf(wc / 3.0f, 1.0f);
    prevWindConf = 0.85f * prevWindConf + 0.15f * wc;

    // ── Noise update (ZERO update during speech) ──
    if (!spOn) {
        for (int k = 0; k < NUM_BINS; k++)
            nMag[k] = 0.95f * nMag[k] + 0.05f * mag[k];
        nRMS = 0.95f * nRMS + 0.05f * fRMS;
    }
    if (fRMS < nRMS * 1.5f)
        nRMS = 0.9995f * nRMS + 0.0005f * fRMS;
    nRMS = fmaxf(nRMS, 0.0008f);

    // ── Adaptive Wiener filter ──
    // Alpha adapts with SPP: gentle during speech, firmer during noise
    float alpha = 1.5f + (1.0f - spp) * 2.0f;

    float gain[NUM_BINS];
    for (int k = 0; k < NUM_BINS; k++) {
        float nr = nMag[k] / (mag[k] + eps);
        gain[k] = fmaxf(1.0f - alpha * nr * nr, WIENER_FLOOR);

        // Graduated wind attenuation (preserves speech fundamentals)
        if (prevWindConf > 0.05f) {
            if      (k == 0) gain[k] *= 1.0f - 0.70f * prevWindConf;  // 0-172 Hz: heavy
            else if (k == 1) gain[k] *= 1.0f - 0.40f * prevWindConf;  // 172-344 Hz: moderate
            else if (k == 2) gain[k] *= 1.0f - 0.15f * prevWindConf;  // 344-517 Hz: light
        }

        // Temporal gain smoothing (frame-to-frame correlation)
        gain[k] = 0.60f * pGain[k] + 0.40f * gain[k];
        pGain[k] = gain[k];
    }

    // Frequency smoothing
    float sg[NUM_BINS];
    sg[0] = 0.6f * gain[0] + 0.4f * gain[1];
    for (int k = 1; k < NUM_BINS - 1; k++)
        sg[k] = 0.25f * gain[k-1] + 0.50f * gain[k] + 0.25f * gain[k+1];
    sg[NUM_BINS - 1] = 0.4f * gain[NUM_BINS - 2] + 0.6f * gain[NUM_BINS - 1];

    // Apply gains to spectrum (magnitude scaling, phase preserved)
    for (int k = 0; k < NUM_BINS; k++) {
        fftBuf[2*k]   *= sg[k];
        fftBuf[2*k+1] *= sg[k];
    }
    for (int k = 1; k < FFT_HALF; k++) {
        fftBuf[2*(FFT_N - k)]     =  fftBuf[2*k];
        fftBuf[2*(FFT_N - k) + 1] = -fftBuf[2*k + 1];
    }

    fftCompute(fftBuf, FFT_N, true);

    // Overlap-add
    for (int i = 0; i < FFT_HALF; i++)
        out[i] = fftBuf[2*i] + olaBuf[i];
    for (int i = 0; i < FFT_HALF; i++)
        olaBuf[i] = fftBuf[2*(FFT_HALF + i)];

    // Apply SPP factor (continuous soft scaling, never a hard gate)
    for (int i = 0; i < FFT_HALF; i++)
        out[i] *= sppFactor;

    if (++dbgCnt % 350 == 0)
        Serial.printf("V:%d SPP:%.2f SC:%.1f R:%.5f N:%.5f C:%.0f F:%.2f W:%.2f Z:%d\n",
                      spOn ? 1 : 0, spp, vadScore, fRMS, nRMS,
                      centroid, flatness, prevWindConf, zcr);
}

// ── Setup ──

void dspSetup() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = I2S_BUF_LEN,
        .use_apll             = false
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_SCK,
        .ws_io_num    = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_SD
    };
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);

    bqInitHPF(hpf, 280.0f, (float)SAMPLE_RATE);
    bqInitLPF(lpf, 3600.0f, (float)SAMPLE_RATE);
    initHann();

    memset(prevIn, 0, sizeof(prevIn));
    memset(olaBuf, 0, sizeof(olaBuf));
    memset(inAcc,  0, sizeof(inAcc));
    memset(nMag,   0, sizeof(nMag));
    memset(pGain,  0, sizeof(pGain));
    memset(cSum,   0, sizeof(cSum));
    memset(mHist,  0, sizeof(mHist));
    memset(fftBuf, 0, sizeof(fftBuf));
    inAccN = 0;  cN = 0;  cRmsSum = 0.0f;  cDone = false;
    spOn = false;  vConf = 0;  vHold = 0;
    nRMS = 0.0f;  smoothRMS = 0.0f;  spp = 0.0f;
    prevWindConf = 0.0f;  mIdx = 0;  mFull = false;
    agcG = 1.0f;  dcEst = 0.0f;  preEmphZ = 0.0f;  deEmphZ = 0.0f;
    dbgCnt = 0;

    Serial.println("DSP v4.0 | Speech Enhancer");
    Serial.println("Stay quiet ~0.6s...");
}

// ── Main loop ──

void dspLoop() {
    int32_t raw[I2S_BUF_LEN];
    size_t bytesRead = 0;
    i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytesRead, portMAX_DELAY);
    int nRaw = bytesRead / sizeof(int32_t);

    // Per-sample: DC → pre-emphasis → HPF → LPF → accumulate
    for (int i = 0; i < nRaw; i++) {
        float x = (float)(raw[i] >> 8) / 8388608.0f;
        x = removeDC(x);
        float pe = x - PRE_EMPH * preEmphZ;
        preEmphZ = x;
        x = pe;
        x = bqTick(hpf, x);
        x = bqTick(lpf, x);
        inAcc[inAccN++] = x;
    }

    while (inAccN >= FFT_HALF) {
        float fout[FFT_HALF];
        processFrame(inAcc, fout);
        memcpy(prevIn, inAcc, FFT_HALF * sizeof(float));
        inAccN -= FFT_HALF;
        if (inAccN > 0)
            memmove(inAcc, inAcc + FFT_HALF, inAccN * sizeof(float));

        if (cDone) {
            // Per-sample output: de-emphasis → compressor → AGC → limiter → ring
            float frameRms = 0.0f;

            for (int i = 0; i < FFT_HALF; i++) {
                float s = fout[i];

                // De-emphasis (undo pre-emphasis, restore natural spectrum)
                s = s + PRE_EMPH * deEmphZ;
                deEmphZ = s;

                // Soft compressor (threshold 0.4, ratio 3:1)
                float a = fabsf(s);
                if (a > 0.4f) {
                    float c = 0.4f + (a - 0.4f) / 3.0f;
                    s = (s >= 0.0f) ? c : -c;
                }

                frameRms += s * s;

                // AGC (applied after compressor)
                s *= agcG;

                // Hard limiter
                if (s > 0.95f)       s =  0.95f;
                else if (s < -0.95f) s = -0.95f;

                int16_t sam = (int16_t)(s * 32700.0f);
                int nxt = (ringHead + 1) % RING_SIZE;
                if (nxt == ringTail)
                    ringTail = (ringTail + 1) % RING_SIZE;
                ringBuf[ringHead] = sam;
                ringHead = nxt;
            }

            // Slow AGC update (only during speech, conservative max 2.5×)
            frameRms = sqrtf(frameRms / (float)FFT_HALF);
            if (spp > 0.5f && frameRms > 0.003f) {
                float want = fminf(fmaxf(0.18f / frameRms, 0.5f), 2.5f);
                if (want < agcG)
                    agcG = 0.95f  * agcG + 0.05f  * want;   // fast attack
                else
                    agcG = 0.998f * agcG + 0.002f * want;   // very slow release
            }

        } else {
            for (int i = 0; i < FFT_HALF; i++) {
                int nxt = (ringHead + 1) % RING_SIZE;
                if (nxt == ringTail)
                    ringTail = (ringTail + 1) % RING_SIZE;
                ringBuf[ringHead] = 0;
                ringHead = nxt;
            }
        }
    }
}
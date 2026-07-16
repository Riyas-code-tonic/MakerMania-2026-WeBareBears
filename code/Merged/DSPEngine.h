#ifndef DSPENGINE_H
#define DSPENGINE_H

#include <stdint.h>

// ═══════════════════════════════════════════════════════════
//  Ring Buffer — shared between DSP (producer) and
//  A2DPSourceManager::getAudioData (consumer)
//  Sized for ~93ms of audio at 44100 Hz (4096 samples)
// ═══════════════════════════════════════════════════════════
#define RING_SIZE 4096
extern volatile int16_t ringBuf[RING_SIZE];
extern volatile int     ringHead;
extern volatile int     ringTail;

// ═══════════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════════

// Initialize I2S microphone (INMP441) — call once after BT connects
void dspSetup();

// Process one audio frame (256 samples) — call every loop() iteration
void dspLoop();

#endif

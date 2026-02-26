#pragma once
#include "Types.h"
#include "Sequencer.h" // Нужен для generateEuclidean()

inline void saveToEEPROM() {
    data.bpm = bpm;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        data.n[i] = channels[i].n;
        data.k[i] = channels[i].k;
        data.r[i] = channels[i].r;
        data.isMuted[i] = channels[i].isMuted;
    }
    EEPROM.put(0, data);
    EEPROM.commit(); 
}

inline void applyData() {
    bpm = constrain(data.bpm, 1, 300);
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channels[i].n = constrain(data.n[i], 2, 32);
        channels[i].k = constrain(data.k[i], 0, 32);
        channels[i].r = constrain(data.r[i], 0, 31);
        channels[i].isMuted = data.isMuted[i];
        generateEuclidean(i);
    }
}

inline void factoryReset() {
    data.magic = 0xABCD1234; 
    data.bpm = 120;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        data.n[i] = 16;
        data.k[i] = 4;
        data.r[i] = 0;
        data.isMuted[i] = false;
    }
    applyData();
    saveToEEPROM();
    
    globalStepCounter = 0;
    needRedraw = true;
}
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
        
        data.shuffle[i] = channels[i].shuffle;
        data.paramA[i] = channels[i].paramA;
        data.paramB[i] = channels[i].paramB;
        data.paramC[i] = channels[i].paramC;
        data.paramD[i] = channels[i].paramD;
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
        
        channels[i].shuffle = constrain(data.shuffle[i], -50, 50);
        channels[i].paramA = constrain(data.paramA[i], 0, 127);
        channels[i].paramB = constrain(data.paramB[i], 0, 127);
        channels[i].paramC = constrain(data.paramC[i], 0, 127);
        channels[i].paramD = constrain(data.paramD[i], 0, 127);
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
        
        data.shuffle[i] = 0;
        data.paramA[i] = 0;
        data.paramB[i] = 0;
        data.paramC[i] = 0;
        data.paramD[i] = 0;
    }
    applyData();
    saveToEEPROM();
    
    globalStepCounter = 0;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channels[i].lastStepTime = 0;
        channels[i].absoluteStep = 0; // Сброс глобальной фазы
    }
    needRedraw = true;
}
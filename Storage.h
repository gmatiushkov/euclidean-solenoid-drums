#pragma once
#include "Types.h"
#include "Sequencer.h"

inline void saveToEEPROM() {
    data.bpm = bpm;
    data.viewMode = viewMode;
    data.midiState = (midiState == 2) ? 1 : midiState; // Сохраняем "set" как "on"
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        data.n[i] = channels[i].n;
        data.k[i] = channels[i].k;
        data.r[i] = channels[i].r;
        data.isMuted[i] = channels[i].isMuted;
        
        data.velo[i] = channels[i].velo;
        data.human[i] = channels[i].human;
        data.shuffle[i] = channels[i].shuffle;
        data.pulse[i] = channels[i].pulse;
        data.base[i] = channels[i].base;
        data.midiVelo[i] = channels[i].midiVelo;
        
        data.midiPitch[i] = channels[i].midiPitch;
        data.midiChannel[i] = channels[i].midiChannel;
    }
    EEPROM.put(0, data);
    EEPROM.commit(); 
}

inline void applyData() {
    bpm = constrain(data.bpm, 1, 300);
    viewMode = constrain(data.viewMode, 0, 1);
    midiState = constrain(data.midiState, 0, 1);
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channels[i].n = constrain(data.n[i], 2, 32);
        channels[i].k = constrain(data.k[i], 0, 32);
        channels[i].r = constrain(data.r[i], 0, 31);
        channels[i].isMuted = data.isMuted[i];
        
        channels[i].velo = constrain(data.velo[i], 0, 127);
        channels[i].human = constrain(data.human[i], 0, 127);
        channels[i].shuffle = constrain(data.shuffle[i], -50, 50);
        channels[i].pulse = constrain(data.pulse[i], 1, 200);
        channels[i].base = constrain(data.base[i], 0, 255);
        channels[i].midiVelo = constrain(data.midiVelo[i], 0, 1);
        
        channels[i].midiPitch = data.midiPitch[i];
        channels[i].midiChannel = data.midiChannel[i];
        generateEuclidean(i);
    }
}

inline void factoryReset() {
    data.magic = 0xABCD123B; 
    data.bpm = 120;
    data.viewMode = 0; 
    data.midiState = 0;
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        data.n[i] = 16;
        data.k[i] = 4;
        data.r[i] = 0;
        data.isMuted[i] = false;
        
        data.velo[i] = 127;
        data.human[i] = 0;
        data.shuffle[i] = 0;
        data.pulse[i] = 30;
        data.base[i] = 150;
        data.midiVelo[i] = 1;
        
        data.midiPitch[i] = 48 + i; // По умолчанию: C3, C#3, D3, D#3
        data.midiChannel[i] = 1;
    }
    applyData();
    saveToEEPROM();
    
    globalStepCounter = 0;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channels[i].lastStepTime = 0;
        channels[i].absoluteStep = 0; 
    }
    needRedraw = true;
}
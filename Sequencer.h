#pragma once
#include "Types.h"

inline void generateEuclidean(int ch) {
    if (channels[ch].k > channels[ch].n) channels[ch].k = channels[ch].n;
    for (int i = 0; i < 32; i++) channels[ch].pattern[i] = false;
    
    for (int i = 0; i < channels[ch].n; i++) {
        int index = (i + channels[ch].r) % channels[ch].n; 
        channels[ch].pattern[index] = ((i * channels[ch].k) % channels[ch].n < channels[ch].k);
    }
}

inline void triggerSave() {
    needSave = true;
    lastChangeTime = millis();
    needRedraw = true;
}

inline void selectChannel(int ch) {
    activeChannel = ch;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        digitalWrite(channels[i].ledPin, (i == activeChannel) ? HIGH : LOW);
    }
    needRedraw = true;
}

inline void toggleMute(int ch) {
    channels[ch].isMuted = !channels[ch].isMuted;
    triggerSave();
}
#pragma once
#include "Types.h"
#include <stdio.h>
#include <string.h>

inline void drawRadialMainScreen() {
    Channel &ch = channels[activeChannel];

    float angleStep = 2.0 * M_PI / ch.n;
    float arcLen = (2.0 * M_PI * 23.0) / ch.n;
    float thickness = constrain(arcLen * 0.4, 1.0, 6.0);
    float halfW = thickness / 2.0;

    for (int i = 0; i < ch.n; i++) {
        float angle = i * angleStep - M_PI / 2.0;
        float cosAng = cos(angle);
        float sinAng = sin(angle);

        if (ch.pattern[i]) {
            float cosPerp = -sinAng; 
            float sinPerp = cosAng;

            for (float d = -halfW; d <= halfW; d += 0.5) {
                int x0 = centerX + round(hitInRadius * cosAng + d * cosPerp);
                int y0 = centerY + round(hitInRadius * sinAng + d * sinPerp);
                int x1 = centerX + round(hitOutRadius * cosAng + d * cosPerp);
                int y1 = centerY + round(hitOutRadius * sinAng + d * sinPerp);
                oled.line(x0, y0, x1, y1);
            }
        } else {
            int x0 = centerX + round(stepInRadius * cosAng);
            int y0 = centerY + round(stepInRadius * sinAng);
            int x1 = centerX + round(stepOutRadius * cosAng);
            int y1 = centerY + round(stepOutRadius * sinAng);
            oled.line(x0, y0, x1, y1);
        }
    }

    float playheadAngle = ch.currentStep * angleStep - M_PI / 2.0;
    int px = centerX + round((hitOutRadius + 4) * cos(playheadAngle));
    int py = centerY + round((hitOutRadius + 4) * sin(playheadAngle));
    oled.line(centerX, centerY, px, py);

    if (ch.isMuted) {
        oled.rect(centerX - 20, centerY - 9, centerX + 20, centerY + 9, OLED_CLEAR);        
        oled.setScale(2);
        oled.setCursor(12, 3); 
        oled.print("OFF");
    }

    oled.setScale(2);
    int textX = 73; 

    oled.setCursor(textX, 0);
    if (currentMode == MODE_K) oled.invertText(true);
    oled.print("k:"); oled.print(ch.k); 
    if (ch.k < 10) oled.print(" ");
    oled.invertText(false);

    oled.setCursor(textX, 3);
    if (currentMode == MODE_N) oled.invertText(true);
    oled.print("n:"); oled.print(ch.n); 
    if (ch.n < 10) oled.print(" ");
    oled.invertText(false);

    oled.setCursor(textX, 6);
    if (currentMode == MODE_R) oled.invertText(true);
    oled.print("r:"); oled.print(ch.r); 
    if (ch.r < 10) oled.print(" ");
    oled.invertText(false);
}

inline void drawLinearMainScreen() {
    Channel &chActive = channels[activeChannel];
    
    oled.setScale(2);
    oled.setCursor(0, 0);
    if (currentMode == MODE_K) oled.invertText(true);
    oled.print("k"); oled.print(chActive.k); 
    oled.invertText(false);

    oled.setCursor(44, 0);
    if (currentMode == MODE_N) oled.invertText(true);
    oled.print("n"); oled.print(chActive.n); 
    oled.invertText(false);

    oled.setCursor(88, 0);
    if (currentMode == MODE_R) oled.invertText(true);
    oled.print("r"); oled.print(chActive.r); 
    oled.invertText(false);
    
    oled.setScale(1); 
    
    for (int c = 0; c < NUM_CHANNELS; c++) {
        Channel &ch = channels[c];
        
        int y_top = 17 + c * 11;
        int y_bot = y_top + 9;
        
        if (c == activeChannel) {
            for (int dx = 0; dx <= 4; dx++) {
                oled.line(dx, y_top + 1 + dx, dx, y_bot - 1 - dx);
            }
        }
        
        if (ch.isMuted) {
            oled.line(8, y_top + 4, 127, y_top + 4);
            continue;
        }
        
        for (int i = 0; i < ch.n; i++) {
            int startX = 8 + (i * 118) / ch.n;
            int endX = 8 + ((i + 1) * 118) / ch.n - 2; 
            if (endX < startX) endX = startX;
            
            if (ch.pattern[i]) {
                oled.rect(startX, y_top, endX, y_bot, OLED_FILL); 
            } else {
                oled.line(startX, y_top + 2, startX, y_bot - 2);  
            }
            
            if (i == ch.currentStep) {
                if (ch.pattern[i]) {
                    int innerStartX = startX + 1;
                    int innerEndX = endX - 1;
                    if (innerEndX < innerStartX) innerEndX = innerStartX; 
                    oled.rect(innerStartX, y_top + 1, innerEndX, y_bot - 1, OLED_CLEAR);
                } else {
                    oled.rect(startX, y_top, startX + 2, y_bot, OLED_FILL);
                }
            }
        }
    }
}

inline void drawMenuList(const char* items[], int values[], int totalItems, int plusIndex) {
    int startIdx = menuIndex > 2 ? menuIndex - 2 : 0;
    if (startIdx > totalItems - 3) startIdx = totalItems - 3; 
    if (startIdx < 0) startIdx = 0;

    for (int i = 0; i < 3; i++) {
        int itemIdx = startIdx + i;
        if (itemIdx >= totalItems) break;

        int row = 2 + (i * 2); 
        bool isSelected = (itemIdx == menuIndex);
        
        char tempName[8];
        strncpy(tempName, items[itemIdx], 6); 
        tempName[6] = '\0';
        strcat(tempName, ":");
        
        char leftBuf[8];
        snprintf(leftBuf, sizeof(leftBuf), "%-6s", tempName);
        
        oled.setCursor(12, row); 
        bool nameInvert = (isSelected && !menuEditMode);
        oled.invertText(nameInvert);
        oled.print(leftBuf);
        oled.invertText(false);
        
        char valBuf[6];
        if (strcmp(items[itemIdx], "view") == 0) {
            snprintf(valBuf, sizeof(valBuf), "%s", values[itemIdx] == 0 ? "rad" : "lin");
        } else if (strcmp(items[itemIdx], "midi") == 0) {
            if (values[itemIdx] == 0) snprintf(valBuf, sizeof(valBuf), "off");
            else if (values[itemIdx] == 1) snprintf(valBuf, sizeof(valBuf), "on");
            else snprintf(valBuf, sizeof(valBuf), "set");
        } else if (itemIdx == plusIndex && values[itemIdx] > 0) {
            snprintf(valBuf, sizeof(valBuf), "+%d", values[itemIdx]);
        } else {
            snprintf(valBuf, sizeof(valBuf), "%d", values[itemIdx]);
        }
        
        char rightBuf[4];
        snprintf(rightBuf, sizeof(rightBuf), "%3s", valBuf);
        
        oled.setCursor(84, row); 
        bool valueInvert = false;
        if (isSelected) {
            if (!menuEditMode) valueInvert = true; 
            else if (millis() % 500 < 250) valueInvert = true; 
        }
        oled.invertText(valueInvert);
        oled.print(rightBuf);
        oled.invertText(false);
    }
    
    if (startIdx > 0) {
        oled.line(4, 23, 4, 23);
        oled.line(3, 24, 5, 24);
        oled.line(2, 25, 6, 25);
        oled.line(1, 26, 7, 26);
    }
    if (startIdx + 3 < totalItems) {
        oled.line(1, 58, 7, 58);
        oled.line(2, 59, 6, 59);
        oled.line(3, 60, 5, 60);
        oled.line(4, 61, 4, 61);
    }
}

inline void drawChannelMenu() {
    Channel &ch = channels[activeChannel];
    
    oled.setScale(2);
    oled.setCursor(0, 0);
    oled.print("ch "); oled.print(activeChannel + 1); oled.print(" edit");

    const char* items[] = {"velo", "human", "shuff", "pulse", "base"};
    int values[] = {ch.velo, ch.human, ch.shuffle, ch.pulse, ch.base};
    
    drawMenuList(items, values, 5, 2);
}

inline void drawGlobalMenu() {
    oled.setScale(2);
    oled.setCursor(0, 0);
    oled.print("settings");

    // Убрали плейсхолдеры. Оставили только 3 пункта.
    const char* items[] = {"bpm", "view", "midi"};
    int values[] = {bpm, viewMode, midiState};

    drawMenuList(items, values, 3, -1);
}

inline void drawMidiLearnScreen() {
    oled.setScale(2);
    if (midiDoneTimer > 0) {
        oled.setCursor(30, 3);
        oled.print("done!");
    } else {
        oled.setCursor(0, 1);
        oled.print("press key");
        
        oled.setCursor(0, 4);
        oled.print("for ch ");
        oled.print(midiLearnChannel + 1);
    }
}

inline void drawInterface() {
    oled.clear();
    
    if (currentScreen == SCREEN_MAIN) {
        if (viewMode == 0) drawRadialMainScreen();
        else drawLinearMainScreen();
    }
    else if (currentScreen == SCREEN_CH_SETTINGS) drawChannelMenu();
    else if (currentScreen == SCREEN_GLOBAL) drawGlobalMenu();
    else if (currentScreen == SCREEN_MIDI_LEARN) drawMidiLearnScreen();
    
    oled.update();
}
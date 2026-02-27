#pragma once
#include "Types.h"
#include <stdio.h>
#include <string.h>

inline void drawMainScreen() {
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
    int textX = 64; 

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

// --- Универсальная функция отрисовки списков меню ---
inline void drawMenuList(const char* items[], int values[], int totalItems, bool showPlusOnFirst) {
    int startIdx = menuIndex > 2 ? menuIndex - 2 : 0;
    if (startIdx > totalItems - 3) startIdx = totalItems - 3; 

    for (int i = 0; i < 3; i++) {
        int itemIdx = startIdx + i;
        if (itemIdx >= totalItems) break;

        int row = 2 + (i * 2); 
        bool isSelected = (itemIdx == menuIndex);
        
        // 1. Формируем левую часть
        char tempName[8];
        strncpy(tempName, items[itemIdx], 5);
        tempName[5] = '\0';
        strcat(tempName, ":");
        
        // Выравниваем строго до 6 символов
        char leftBuf[8];
        snprintf(leftBuf, sizeof(leftBuf), "%-6s", tempName);
        
        oled.setCursor(12, row); 
        bool nameInvert = (isSelected && !menuEditMode);
        oled.invertText(nameInvert);
        oled.print(leftBuf);
        oled.invertText(false);
        
        // 2. Форматируем правую часть (строго 3 символа)
        char valBuf[6];
        if (itemIdx == 0 && showPlusOnFirst && values[itemIdx] > 0) {
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
    
    // Скроллбар
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
    
    oled.setCursor(0, 0);
    oled.print("ch "); oled.print(activeChannel + 1); oled.print(" edit");

    const char* items[] = {"shuffle", "param a", "param b", "param c", "param d"};
    int values[] = {ch.shuffle, ch.paramA, ch.paramB, ch.paramC, ch.paramD};
    
    // Передаем true, чтобы у shuffle рисовался плюс, если значение больше нуля
    drawMenuList(items, values, 5, true);
}


inline void drawGlobalMenu() {
    oled.setCursor(0, 0);
    oled.print("settings");

    const char* items[] = {"bpm", "global a", "global b"};
    int values[] = {bpm, 0, 0};

    // Передаем false, так как плюсы перед значениями (вроде BPM) здесь не нужны
    drawMenuList(items, values, 3, false);
}

inline void drawInterface() {
    oled.clear();
    
    if (currentScreen == SCREEN_MAIN) drawMainScreen();
    else if (currentScreen == SCREEN_CH_SETTINGS) drawChannelMenu();
    else if (currentScreen == SCREEN_GLOBAL) drawGlobalMenu();
    
    oled.update();
}
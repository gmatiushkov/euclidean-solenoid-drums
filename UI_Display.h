#pragma once
#include "Types.h"

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

    if (inBpmMode) {
        oled.setCursor(textX, 1); oled.print("BPM:");
        oled.setCursor(textX, 4);
        oled.invertText(true);
        if (bpm < 100) oled.print(" "); 
        oled.print(bpm); oled.print(" ");
        oled.invertText(false);
    } else {
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
}

// Главная функция отрисовки (будет расширяться)
inline void drawInterface() {
    oled.clear();
    
    if (currentScreen == SCREEN_MAIN) {
        drawMainScreen();
    }
    
    oled.update();
}
#include <Wire.h>
#include <GyverOLED.h>
#include <EncButton.h>
#include <EEPROM.h>
#include <math.h>

// ==========================================
// --- НАСТРОЙКИ УПРАВЛЕНИЯ И ЭНКОДЕРА ---
// ==========================================
const int HOLD_TIME = 500;             // Время (мс) для долгого нажатия кнопок (Mute, переход в BPM)
const int RESET_HOLD_TIME = 3000;      // Время (мс) удержания кнопки энкодера для сброса до заводских

const int ENC_DEBOUNCE_MS = 15;        // Защита от дребезга (мс): игнорируем слишком быстрые ложные тики
const int ENC_ACCEL_THRESHOLD = 80;    // Порог скорости (мс): тики быстрее этого времени включают ускорение

const int ENC_FAST_STEP = 2;           // Шаг изменения параметров (N, K, R) при быстром вращении
const int BPM_FAST_STEP = 10;          // Шаг изменения BPM при быстром вращении
// ==========================================

GyverOLED<SSD1306_128x64> oled;

// --- ПИНЫ ЭНКОДЕРА ---
const int pinDT = 10;
const int pinCLK = 11;
const int pinSW = 12;

EncButton eb(pinDT, pinCLK); 
VirtButton btnEb;            

Button btnCh1(16);
Button btnCh2(17);

const int NUM_CHANNELS = 2;

struct Channel {
    int n = 16;      
    int k = 4;       
    int r = 0;       
    bool pattern[32] = {false};
    
    int currentStep = 0;
    
    int solPin;      
    int ledPin;      
    
    bool solActive = false;
    unsigned long solTurnOffTime = 0;
    
    bool isMuted = false; 
};
Channel channels[NUM_CHANNELS];

struct SaveData {
    uint32_t magic; 
    int bpm;
    int n[NUM_CHANNELS];
    int k[NUM_CHANNELS];
    int r[NUM_CHANNELS];
    bool isMuted[NUM_CHANNELS];
};
SaveData data;

bool needSave = false;
unsigned long lastChangeTime = 0;

int activeChannel = 0; 
int bpm = 120;
unsigned long lastStepTime = 0;
const int pulseDuration = 30;

// Глобальный счетчик фазы (Global Phase)
unsigned long globalStepCounter = 0;

const int centerX = 30; 
const int centerY = 32;
const int hitInRadius = 18; const int hitOutRadius = 28;
const int stepInRadius = 22; const int stepOutRadius = 26;

enum Mode { MODE_K, MODE_N, MODE_R, MODES_COUNT };
int currentMode = MODE_K;
bool inBpmMode = false; 

volatile bool needRedraw = true;
unsigned long encHoldTimer = 0;

void generateEuclidean(int ch) {
    if (channels[ch].k > channels[ch].n) channels[ch].k = channels[ch].n;
    for (int i = 0; i < 32; i++) channels[ch].pattern[i] = false;
    
    for (int i = 0; i < channels[ch].n; i++) {
        int index = (i + channels[ch].r) % channels[ch].n; 
        channels[ch].pattern[index] = ((i * channels[ch].k) % channels[ch].n < channels[ch].k);
    }
}

void triggerSave() {
    needSave = true;
    lastChangeTime = millis();
    needRedraw = true;
}

void saveToEEPROM() {
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

void applyData() {
    bpm = constrain(data.bpm, 1, 300);
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channels[i].n = constrain(data.n[i], 2, 32);
        channels[i].k = constrain(data.k[i], 0, 32);
        channels[i].r = constrain(data.r[i], 0, 31);
        channels[i].isMuted = data.isMuted[i];
        generateEuclidean(i);
    }
}

void factoryReset() {
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
    
    // При сбросе обнуляем и глобальную фазу
    globalStepCounter = 0;
    needRedraw = true;
}

void selectChannel(int ch) {
    activeChannel = ch;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        digitalWrite(channels[i].ledPin, (i == activeChannel) ? HIGH : LOW);
    }
    needRedraw = true;
}

void toggleMute(int ch) {
    channels[ch].isMuted = !channels[ch].isMuted;
    triggerSave();
}

// ==========================================
// --- ЯДРО 0: ЛОГИКА, КНОПКИ, СЕКВЕНСОР ---
// ==========================================
void setup() {
    pinMode(pinSW, INPUT_PULLUP);
    pinMode(pinDT, INPUT_PULLUP);
    pinMode(pinCLK, INPUT_PULLUP);

    EEPROM.begin(512);
    EEPROM.get(0, data);
    
    if (data.magic != 0xABCD1234) {
        factoryReset(); 
    } else {
        applyData(); 
    }
    
    btnEb.setHoldTimeout(HOLD_TIME); 
    btnCh1.setHoldTimeout(HOLD_TIME);
    btnCh2.setHoldTimeout(HOLD_TIME);

    channels[0].solPin = 15;
    channels[0].ledPin = 18;
    channels[1].solPin = 14;
    channels[1].ledPin = 19;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        pinMode(channels[i].solPin, OUTPUT);
        digitalWrite(channels[i].solPin, LOW);
        pinMode(channels[i].ledPin, OUTPUT);
    }

    selectChannel(0); 
}

void loop() {
    eb.tick();
    btnCh1.tick();
    btnCh2.tick();

    bool isPhysicallyPressed = (digitalRead(pinSW) == LOW);
    bool isAtDetent = (digitalRead(pinDT) == HIGH && digitalRead(pinCLK) == HIGH);
    
    bool filteredPress = isPhysicallyPressed && isAtDetent;
    btnEb.tick(filteredPress);

    if (filteredPress) {
        if (encHoldTimer == 0) encHoldTimer = millis();
        if (millis() - encHoldTimer > RESET_HOLD_TIME) {
            factoryReset();
            encHoldTimer = millis(); 
        }
    } else {
        encHoldTimer = 0;
    }

    if (btnCh1.click()) selectChannel(0);
    if (btnCh2.click()) selectChannel(1);
    if (btnCh1.hold()) toggleMute(0);
    if (btnCh2.hold()) toggleMute(1);

    if (btnEb.hold()) {
        if (!inBpmMode) {
            inBpmMode = true;
            needRedraw = true;
        }
    }

    if (btnEb.click()) {
        if (inBpmMode) {
            inBpmMode = false;
        } else {
            currentMode = (currentMode + 1) % MODES_COUNT; 
        }
        triggerSave();
    }

    if (eb.turn()) {
        int dir = eb.dir();
        
        static unsigned long lastTurnTime = 0;
        unsigned long currentTurnTime = millis();
        int stepNKR = 1;
        int stepBPM = 1;

        if (currentTurnTime - lastTurnTime > ENC_DEBOUNCE_MS) {
            if (currentTurnTime - lastTurnTime < ENC_ACCEL_THRESHOLD) {
                stepNKR = ENC_FAST_STEP;       
                stepBPM = BPM_FAST_STEP;   
            }
            
            if (inBpmMode) {
                bpm = constrain(bpm + dir * stepBPM, 1, 300); 
            } else {
                Channel &ch = channels[activeChannel]; 

                if (currentMode == MODE_N) {
                    ch.n = constrain(ch.n + dir * stepNKR, 2, 32);
                    if (ch.k > ch.n) ch.k = ch.n;
                    if (ch.r >= ch.n) ch.r = ch.n - 1; 
                } else if (currentMode == MODE_K) {
                    ch.k = constrain(ch.k + dir * stepNKR, 0, ch.n);
                } else if (currentMode == MODE_R) {
                    ch.r = constrain(ch.r + dir * stepNKR, 0, ch.n - 1); 
                }
                generateEuclidean(activeChannel); 
            }
            triggerSave();
        }
        lastTurnTime = currentTurnTime;
    }

    if (needSave && (millis() - lastChangeTime > 3000)) {
        saveToEEPROM();
        needSave = false;
    }

    // --- ГЛОБАЛЬНЫЙ СЕКВЕНСОР ---
    unsigned long currentTime = millis();
    unsigned long stepInterval = 60000 / (bpm * 4);
    
    if (currentTime - lastStepTime >= stepInterval) {
        lastStepTime = currentTime;
        
        // Увеличиваем единый глобальный счетчик
        globalStepCounter++;
        
        for (int i = 0; i < NUM_CHANNELS; i++) {
            // Вычисляем текущий шаг канала на основе глобальной фазы
            channels[i].currentStep = globalStepCounter % channels[i].n;
            
            if (channels[i].pattern[channels[i].currentStep] && !channels[i].isMuted) {
                digitalWrite(channels[i].solPin, HIGH);
                channels[i].solActive = true;
                channels[i].solTurnOffTime = currentTime + pulseDuration;
            }
        }
        needRedraw = true; 
    }

    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (channels[i].solActive && (currentTime >= channels[i].solTurnOffTime)) {
            digitalWrite(channels[i].solPin, LOW);
            channels[i].solActive = false;
        }
    }
}


// ==========================================
// --- ЯДРО 1: ИНТЕРФЕЙС И ЭКРАН (I2C) ---
// ==========================================
void setup1() {
    Wire.setSDA(4); Wire.setSCL(5);
    Wire.begin(); 
    Wire.setClock(400000); 
    oled.init();
}

void drawInterface() {
    oled.clear();

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

    oled.update();
}

void loop1() {
    if (needRedraw) {
        drawInterface();
        needRedraw = false;
    }
    delay(20); 
}
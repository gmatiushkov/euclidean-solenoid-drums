#include "Types.h"
#include "Sequencer.h"
#include "Storage.h"
#include "UI_Display.h"

// ==========================================
// --- ИНИЦИАЛИЗАЦИЯ ГЛОБАЛЬНЫХ ПЕРЕМЕННЫХ ---
// ==========================================
GyverOLED<SSD1306_128x64> oled;
EncButton eb(pinDT, pinCLK); 
VirtButton btnEb;            
Button btnCh1(16);
Button btnCh2(17);

Channel channels[NUM_CHANNELS];
SaveData data;

bool needSave = false;
unsigned long lastChangeTime = 0;
int activeChannel = 0; 
int bpm = 120;
unsigned long lastStepTime = 0;
unsigned long globalStepCounter = 0;
int currentMode = MODE_K;
bool inBpmMode = false; 
volatile bool needRedraw = true;
unsigned long encHoldTimer = 0;

// Стартуем с главного экрана
UI_State currentScreen = SCREEN_MAIN; 

// ==========================================
// --- ЯДРО 0: ЛОГИКА ---
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

    // Сброс настроек
    if (filteredPress) {
        if (encHoldTimer == 0) encHoldTimer = millis();
        if (millis() - encHoldTimer > RESET_HOLD_TIME) {
            factoryReset();
            encHoldTimer = millis(); 
        }
    } else {
        encHoldTimer = 0;
    }

    // Управление каналами
    if (btnCh1.click()) selectChannel(0);
    if (btnCh2.click()) selectChannel(1);
    if (btnCh1.hold()) toggleMute(0);
    if (btnCh2.hold()) toggleMute(1);

    // Временная заглушка для входа в BPM
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

    // Обработка энкодера (пока только для главного экрана)
    if (eb.turn() && currentScreen == SCREEN_MAIN) {
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

    // Фоновое сохранение
    if (needSave && (millis() - lastChangeTime > 3000)) {
        saveToEEPROM();
        needSave = false;
    }

    // Секвенсор
    unsigned long currentTime = millis();
    unsigned long stepInterval = 60000 / (bpm * 4);
    
    if (currentTime - lastStepTime >= stepInterval) {
        lastStepTime = currentTime;
        globalStepCounter++;
        
        for (int i = 0; i < NUM_CHANNELS; i++) {
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
// --- ЯДРО 1: ЭКРАН ---
// ==========================================
void setup1() {
    Wire.setSDA(4); Wire.setSCL(5);
    Wire.begin(); 
    Wire.setClock(400000); 
    oled.init();
}

void loop1() {
    if (needRedraw) {
        drawInterface();
        needRedraw = false;
    }
    delay(20); 
}
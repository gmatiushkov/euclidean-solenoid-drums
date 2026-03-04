#include "Types.h"
#include "Sequencer.h"
#include "Storage.h"
#include "UI_Display.h"

// Создаем два MIDI интерфейса
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI_USB);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI_HW);

// ==========================================
// --- ИНИЦИАЛИЗАЦИЯ ГЛОБАЛЬНЫХ ПЕРЕМЕННЫХ ---
// ==========================================
GyverOLED<SSD1306_128x64> oled;
EncButton eb(pinDT, pinCLK); 
VirtButton btnEb;            
Button btnCh1(16);
Button btnCh2(18);
Button btnCh3(20);
Button btnCh4(22);

Channel channels[NUM_CHANNELS];
SaveData data;

bool needSave = false;
unsigned long lastChangeTime = 0;
int activeChannel = 0; 
int bpm = 120;
unsigned long globalStepCounter = 0;
int currentMode = MODE_K;
volatile bool needRedraw = true;
unsigned long encHoldTimer = 0;

UI_State currentScreen = SCREEN_MAIN; 
int menuIndex = 0;
bool menuEditMode = false;
unsigned long menuBlinkTimer = 0;

int viewMode = 0;
int midiState = 0; 
int midiLearnChannel = 0;
unsigned long midiDoneTimer = 0;

// ==========================================
// --- ФУНКЦИИ СОЛЕНОИДОВ И MIDI ---
// ==========================================
void fireSolenoidDirect(int i, int pwm, unsigned long t) {
    if (pwm >= 255) {
        digitalWrite(channels[i].solPin, HIGH);
    } else if (pwm == 0) {
        digitalWrite(channels[i].solPin, LOW);
    } else {
        analogWrite(channels[i].solPin, pwm);
    }
    channels[i].solActive = true;
    channels[i].solTurnOffTime = t + channels[i].pulse;
}

void fireSolenoidMidi(int i, int velo, unsigned long t) {
    int pwm = (velo >= 127) ? 255 : map(velo, 1, 126, channels[i].base, 254);
    fireSolenoidDirect(i, pwm, t);
}

void handleNoteOn(byte channel, byte pitch, byte velocity) {
    if (velocity == 0) return;
    
    if (currentScreen == SCREEN_MIDI_LEARN) {
        if (midiDoneTimer == 0 && midiLearnChannel < NUM_CHANNELS) {
            channels[midiLearnChannel].midiChannel = channel;
            channels[midiLearnChannel].midiPitch = pitch;
            midiLearnChannel++;
            needRedraw = true;
            
            if (midiLearnChannel >= NUM_CHANNELS) {
                midiDoneTimer = millis();
                midiState = 1; // Автоматически включаем MIDI после настройки
                triggerSave();
            }
        }
    } else if (midiState == 1) { 
        unsigned long t = millis();
        for (int i = 0; i < NUM_CHANNELS; i++) {
            if (channels[i].midiPitch == pitch && (channels[i].midiChannel == channel || channels[i].midiChannel == 0)) {
                if (!channels[i].isMuted) {
                    
                    // НОВЫЙ БЛОК ЛОГИКИ MIDI VELOCITY
                    int actual_velo = velocity;
                    if (channels[i].midiVelo == 0) { 
                        // Игнорируем входящий velocity, применяем параметры канала
                        int min_v = max(0, channels[i].velo - channels[i].human);
                        int max_v = min(127, channels[i].velo + channels[i].human);
                        actual_velo = random(min_v, max_v + 1);
                    }
                    
                    fireSolenoidMidi(i, actual_velo, t);
                }
            }
        }
    }
}

void handleNoteOff(byte channel, byte pitch, byte velocity) { }

// ==========================================
// --- ЯДРО 0: ЛОГИКА ---
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n--- Euclidean Drum Machine Boot (4 Channels + MIDI) ---");

    pinMode(pinSW, INPUT_PULLUP);
    pinMode(pinDT, INPUT_PULLUP);
    pinMode(pinCLK, INPUT_PULLUP);

    EEPROM.begin(512);
    EEPROM.get(0, data);
    if (data.magic != 0xABCD123B) {
        Serial.println("[EEPROM] Formatting for 4 channels...");
        factoryReset();
    } else {
        Serial.println("[EEPROM] Loaded.");
        applyData();
    }
    
    btnEb.setHoldTimeout(HOLD_TIME); 
    btnCh1.setHoldTimeout(HOLD_TIME);
    btnCh2.setHoldTimeout(HOLD_TIME);
    btnCh3.setHoldTimeout(HOLD_TIME);
    btnCh4.setHoldTimeout(HOLD_TIME);

    channels[0].solPin = 3; channels[0].ledPin = 17;
    channels[1].solPin = 2; channels[1].ledPin = 19;
    channels[2].solPin = 1; channels[2].ledPin = 21;
    channels[3].solPin = 0; channels[3].ledPin = 26;

    unsigned long t = millis();
    for (int i = 0; i < NUM_CHANNELS; i++) {
        pinMode(channels[i].solPin, OUTPUT);
        digitalWrite(channels[i].solPin, LOW); 
        pinMode(channels[i].ledPin, OUTPUT);
        channels[i].lastStepTime = t;
        channels[i].absoluteStep = 0;
    }

    // --- MIDI SETUP ---
    Serial2.setRX(9);
    Serial2.setTX(8);
    
    MIDI_HW.turnThruOff();
    MIDI_HW.setHandleNoteOn(handleNoteOn);
    MIDI_HW.setHandleNoteOff(handleNoteOff);
    MIDI_HW.begin(MIDI_CHANNEL_OMNI);

    MIDI_USB.turnThruOff();
    MIDI_USB.setHandleNoteOn(handleNoteOn);
    MIDI_USB.setHandleNoteOff(handleNoteOff);
    MIDI_USB.begin(MIDI_CHANNEL_OMNI);

    selectChannel(0); 
    Serial.println("--- Setup Complete ---");
}

void loop() {
    MIDI_HW.read();
    MIDI_USB.read();
    
    // Таймер выхода из режима обучения MIDI
    if (currentScreen == SCREEN_MIDI_LEARN && midiDoneTimer > 0) {
        if (millis() - midiDoneTimer > 1000) {
            midiDoneTimer = 0;
            currentScreen = SCREEN_GLOBAL;
            needRedraw = true;
        }
    }

    eb.tick();
    btnCh1.tick();
    btnCh2.tick();
    btnCh3.tick();
    btnCh4.tick();

    bool isPhysicallyPressed = (digitalRead(pinSW) == LOW);
    bool isAtDetent = (digitalRead(pinDT) == HIGH && digitalRead(pinCLK) == HIGH);
    bool filteredPress = isPhysicallyPressed && isAtDetent;
    btnEb.tick(filteredPress);

    if (filteredPress) {
        if (encHoldTimer == 0) encHoldTimer = millis();
        if (millis() - encHoldTimer > RESET_HOLD_TIME) {
            factoryReset();
            encHoldTimer = millis(); 
            Serial.println("[SYSTEM] Factory Reset!");
        }
    } else {
        encHoldTimer = 0;
    }

    // --- УПРАВЛЕНИЕ КНОПКАМИ КАНАЛОВ ---
    static bool ch1WasHeld = false;
    if (btnCh1.press()) { selectChannel(0); }
    if (btnCh1.hold()) { toggleMute(0); ch1WasHeld = true; }
    if (btnCh1.release()) {
        if (!ch1WasHeld && currentScreen != SCREEN_MAIN) { currentScreen = SCREEN_MAIN; needRedraw = true; }
        ch1WasHeld = false;
    }
    if (btnCh1.hasClicks(2)) { currentScreen = SCREEN_CH_SETTINGS; activeChannel = 0; menuIndex = 0; menuEditMode = false; needRedraw = true; }

    static bool ch2WasHeld = false;
    if (btnCh2.press()) { selectChannel(1); }
    if (btnCh2.hold()) { toggleMute(1); ch2WasHeld = true; }
    if (btnCh2.release()) {
        if (!ch2WasHeld && currentScreen != SCREEN_MAIN) { currentScreen = SCREEN_MAIN; needRedraw = true; }
        ch2WasHeld = false;
    }
    if (btnCh2.hasClicks(2)) { currentScreen = SCREEN_CH_SETTINGS; activeChannel = 1; menuIndex = 0; menuEditMode = false; needRedraw = true; }

    static bool ch3WasHeld = false;
    if (btnCh3.press()) { selectChannel(2); }
    if (btnCh3.hold()) { toggleMute(2); ch3WasHeld = true; }
    if (btnCh3.release()) {
        if (!ch3WasHeld && currentScreen != SCREEN_MAIN) { currentScreen = SCREEN_MAIN; needRedraw = true; }
        ch3WasHeld = false;
    }
    if (btnCh3.hasClicks(2)) { currentScreen = SCREEN_CH_SETTINGS; activeChannel = 2; menuIndex = 0; menuEditMode = false; needRedraw = true; }

    static bool ch4WasHeld = false;
    if (btnCh4.press()) { selectChannel(3); }
    if (btnCh4.hold()) { toggleMute(3); ch4WasHeld = true; }
    if (btnCh4.release()) {
        if (!ch4WasHeld && currentScreen != SCREEN_MAIN) { currentScreen = SCREEN_MAIN; needRedraw = true; }
        ch4WasHeld = false;
    }
    if (btnCh4.hasClicks(2)) { currentScreen = SCREEN_CH_SETTINGS; activeChannel = 3; menuIndex = 0; menuEditMode = false; needRedraw = true; }

    // --- УПРАВЛЕНИЕ ЭНКОДЕРОМ ---
    if (btnEb.hold()) {
        if (currentScreen == SCREEN_MAIN) {
            currentScreen = SCREEN_GLOBAL;
            menuIndex = 0; menuEditMode = false; needRedraw = true;
        } 
        else if (currentScreen == SCREEN_CH_SETTINGS) {
            Channel &ch = channels[activeChannel];
            if (menuIndex == 0) ch.velo = 127;
            else if (menuIndex == 1) ch.human = 0;
            else if (menuIndex == 2) ch.shuffle = 0;
            else if (menuIndex == 3) ch.midiVelo = 1; // НОВОЕ (включаем по дефолту)
            else if (menuIndex == 4) ch.pulse = 30;   // Сдвинуто
            else if (menuIndex == 5) ch.base = 150;   // Сдвинуто
            
            triggerSave();
            needRedraw = true;
        }
        else if (currentScreen == SCREEN_GLOBAL) {
            if (menuIndex == 0) bpm = 120;
            else if (menuIndex == 1) viewMode = 0; 
            else if (menuIndex == 2) midiState = 0;
            triggerSave();
            needRedraw = true;
        }
    }

    if (btnEb.click()) {
        if (currentScreen == SCREEN_MAIN) {
            currentMode = (currentMode + 1) % MODES_COUNT;
            needRedraw = true; triggerSave();
        } 
        else if (currentScreen == SCREEN_GLOBAL) {
            // Перехватываем клик по 'set' для запуска обучения
            if (menuEditMode && menuIndex == 2 && midiState == 2) {
                menuEditMode = false;
                currentScreen = SCREEN_MIDI_LEARN;
                midiLearnChannel = 0;
                midiDoneTimer = 0;
                needRedraw = true;
            } else {
                menuEditMode = !menuEditMode;
                menuBlinkTimer = millis();
                needRedraw = true;
            }
        }
        else {
            menuEditMode = !menuEditMode;
            menuBlinkTimer = millis();
            needRedraw = true;
        }
    }

    if (eb.turn()) {
        int dir = eb.dir();
        static unsigned long lastTurnTime = 0;
        unsigned long currentTurnTime = millis();
        int stepNKR = 1, stepBPM = 1, stepMenu = 1;

        if (currentTurnTime - lastTurnTime > ENC_DEBOUNCE_MS) {
            if (currentTurnTime - lastTurnTime < ENC_ACCEL_THRESHOLD) {
                stepNKR = ENC_FAST_STEP;
                stepBPM = BPM_FAST_STEP; 
                stepMenu = 5; 
            }
            
            if (currentScreen == SCREEN_MAIN) {
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
                triggerSave();
            } 
            else if (currentScreen == SCREEN_CH_SETTINGS) {
                if (!menuEditMode) {
                    menuIndex = constrain(menuIndex + dir, 0, 5); // ИЗМЕНЕНО: макс. индекс теперь 5
                } else {
                    Channel &ch = channels[activeChannel];
                    if (menuIndex == 0) ch.velo = constrain(ch.velo + dir * stepMenu, 0, 127);
                    else if (menuIndex == 1) ch.human = constrain(ch.human + dir * stepMenu, 0, 127);
                    else if (menuIndex == 2) ch.shuffle = constrain(ch.shuffle + dir * stepMenu, -50, 50);
                    else if (menuIndex == 3) ch.midiVelo = constrain(ch.midiVelo + dir, 0, 1); // НОВОЕ
                    else if (menuIndex == 4) ch.pulse = constrain(ch.pulse + dir * stepMenu, 1, 200); // Сдвинуто
                    else if (menuIndex == 5) ch.base = constrain(ch.base + dir * stepMenu, 0, 255);   // Сдвинуто
                    triggerSave();
                }
            }
            else if (currentScreen == SCREEN_GLOBAL) {
                if (!menuEditMode) {
                    // ИЗМЕНЕНИЕ ЗДЕСЬ: Максимальный индекс теперь 2 (так как пунктов 3)
                    menuIndex = constrain(menuIndex + dir, 0, 2); 
                } else {
                    if (menuIndex == 0) bpm = constrain(bpm + dir * stepBPM, 1, 300);
                    else if (menuIndex == 1) viewMode = constrain(viewMode + dir, 0, 1);
                    else if (menuIndex == 2) midiState = constrain(midiState + dir, 0, 2);
                    triggerSave();
                }
            }
            needRedraw = true;
        }
        lastTurnTime = currentTurnTime;
    }

    if (needSave && (millis() - lastChangeTime > 3000)) {
        saveToEEPROM();
        needSave = false;
        Serial.println("[SYSTEM] EEPROM Saved");
    }

    // --- ГЛОБАЛЬНЫЙ СЕКВЕНСОР ---
    unsigned long currentTime = millis();
    unsigned long baseInterval = 60000 / (bpm * 4);
    bool stepChanged = false;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        long safeShuffle = constrain(channels[i].shuffle, -50, 50);
        long baseInt = (long)baseInterval;
        long delta = (baseInt * safeShuffle) / 100;
        
        unsigned long dynamicInterval = (unsigned long)((channels[i].absoluteStep % 2 == 0) ? (baseInt + delta) : (baseInt - delta));
        unsigned long elapsed = currentTime - channels[i].lastStepTime;
        
        if (elapsed >= dynamicInterval) {
            if (elapsed > baseInterval * 3) {
                channels[i].lastStepTime = currentTime;
            } else {
                channels[i].lastStepTime += dynamicInterval;
            }
            
            channels[i].absoluteStep++;
            channels[i].currentStep = channels[i].absoluteStep % channels[i].n; 
            
            if (i == 0) globalStepCounter++;
            
            if (channels[i].pattern[channels[i].currentStep] && !channels[i].isMuted) {
                
                // ИЗМЕНЕНО: menuIndex == 4 заменен на menuIndex == 5
                bool isEditingBase = (currentScreen == SCREEN_CH_SETTINGS && menuIndex == 5 && menuEditMode && activeChannel == i);
                if (isEditingBase) {
                    fireSolenoidDirect(i, channels[i].base, currentTime);
                } else {
                    int min_v = max(0, channels[i].velo - channels[i].human);
                    int max_v = min(127, channels[i].velo + channels[i].human);
                    int actual_velo = random(min_v, max_v + 1);
                    
                    int pwm = 0;
                    if (actual_velo == 0) {
                        pwm = 0;
                    } else if (actual_velo >= 127) {
                        pwm = 255;
                    } else {
                        pwm = map(actual_velo, 1, 126, channels[i].base, 254);
                    }
                    fireSolenoidDirect(i, pwm, currentTime);
                }
            }
            stepChanged = true;
        }
    }

    if (stepChanged && currentScreen == SCREEN_MAIN) needRedraw = true;
    
    // --- ВЫКЛЮЧЕНИЕ СОЛЕНОИДОВ ПО ТАЙМЕРУ ---
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (channels[i].solActive && (currentTime >= channels[i].solTurnOffTime)) {
            analogWrite(channels[i].solPin, 0);
            digitalWrite(channels[i].solPin, LOW); 
            channels[i].solActive = false;
        }
    }
}

// ==========================================
// --- ЯДРО 1: ЭКРАН ---
// ==========================================
void setup1() {
    Wire.setSDA(4);
    Wire.setSCL(5);
    Wire.begin(); 
    Wire.setClock(400000); 
    oled.init();
}

void loop1() {
    if (menuEditMode) needRedraw = true;
    if (needRedraw) {
        needRedraw = false; 
        drawInterface();
    }
    delay(20); 
}
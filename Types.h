#pragma once
#include <Wire.h>
#include <GyverOLED.h>
#include <EncButton.h>
#include <EEPROM.h>
#include <math.h>

// ==========================================
// --- КОНСТАНТЫ И ПИНЫ ---
// ==========================================
const int HOLD_TIME = 500;
const int RESET_HOLD_TIME = 3000;
const int ENC_DEBOUNCE_MS = 15;
const int ENC_ACCEL_THRESHOLD = 80;
const int ENC_FAST_STEP = 2;
const int BPM_FAST_STEP = 10;
const int pulseDuration = 30;

const int pinDT = 10;
const int pinCLK = 11;
const int pinSW = 12;

const int NUM_CHANNELS = 2;

// Константы отрисовки
const int centerX = 30; 
const int centerY = 32;
const int hitInRadius = 18; const int hitOutRadius = 28;
const int stepInRadius = 22; const int stepOutRadius = 26;

// ==========================================
// --- СТРУКТУРЫ И СОСТОЯНИЯ ---
// ==========================================
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

struct SaveData {
    uint32_t magic; 
    int bpm;
    int n[NUM_CHANNELS];
    int k[NUM_CHANNELS];
    int r[NUM_CHANNELS];
    bool isMuted[NUM_CHANNELS];
};

enum Mode { MODE_K, MODE_N, MODE_R, MODES_COUNT };

// НОВОЕ: Состояния интерфейса
enum UI_State {
    SCREEN_MAIN,
    SCREEN_CH_SETTINGS,
    SCREEN_GLOBAL
};

// ==========================================
// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (EXTERN) ---
// ==========================================
extern GyverOLED<SSD1306_128x64> oled;
extern EncButton eb; 
extern VirtButton btnEb;            
extern Button btnCh1;
extern Button btnCh2;

extern Channel channels[NUM_CHANNELS];
extern SaveData data;

extern bool needSave;
extern unsigned long lastChangeTime;
extern int activeChannel; 
extern int bpm;
extern unsigned long lastStepTime;
extern unsigned long globalStepCounter;
extern int currentMode;
extern bool inBpmMode; 
extern volatile bool needRedraw;
extern unsigned long encHoldTimer;
extern UI_State currentScreen;
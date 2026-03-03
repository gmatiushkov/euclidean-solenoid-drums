#pragma once
#include <Wire.h>
#include <GyverOLED.h>
#include <EncButton.h>
#include <EEPROM.h>
#include <math.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

// ==========================================
// --- КОНСТАНТЫ И ПИНЫ ---
// ==========================================
const int HOLD_TIME = 500;
const int RESET_HOLD_TIME = 3000;
const int ENC_DEBOUNCE_MS = 15;
const int ENC_ACCEL_THRESHOLD = 80;
const int ENC_FAST_STEP = 2;
const int BPM_FAST_STEP = 10;

const int pinDT = 10;
const int pinCLK = 11;
const int pinSW = 12;

const int NUM_CHANNELS = 4;

const int centerX = 32; 
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
    
    unsigned long lastStepTime = 0;
    unsigned long absoluteStep = 0; 
    bool isMuted = false; 
    
    int velo = 127;     
    int human = 0;      
    int shuffle = 0;    
    int pulse = 30;     
    int base = 150;     
    
    byte midiPitch = 0;
    byte midiChannel = 1;
};

struct SaveData {
    uint32_t magic; 
    int bpm;
    int n[NUM_CHANNELS];
    int k[NUM_CHANNELS];
    int r[NUM_CHANNELS];
    bool isMuted[NUM_CHANNELS];
    
    int velo[NUM_CHANNELS];
    int human[NUM_CHANNELS];
    int shuffle[NUM_CHANNELS];
    int pulse[NUM_CHANNELS];
    int base[NUM_CHANNELS];
    
    int viewMode; // 0 = Radial, 1 = Linear
    
    int midiState; 
    byte midiPitch[NUM_CHANNELS];
    byte midiChannel[NUM_CHANNELS];
};

enum Mode { MODE_K, MODE_N, MODE_R, MODES_COUNT };

enum UI_State {
    SCREEN_MAIN,
    SCREEN_CH_SETTINGS,
    SCREEN_GLOBAL,
    SCREEN_MIDI_LEARN
};

// ==========================================
// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (EXTERN) ---
// ==========================================
extern GyverOLED<SSD1306_128x64> oled;
extern EncButton eb; 
extern VirtButton btnEb;            
extern Button btnCh1;
extern Button btnCh2;
extern Button btnCh3;
extern Button btnCh4;

extern Channel channels[NUM_CHANNELS];
extern SaveData data;

extern bool needSave;
extern unsigned long lastChangeTime;
extern int activeChannel; 
extern int bpm;
extern unsigned long lastStepTime;
extern unsigned long globalStepCounter;
extern int currentMode;
extern volatile bool needRedraw;
extern unsigned long encHoldTimer;
extern UI_State currentScreen;
extern int menuIndex;
extern bool menuEditMode;
extern unsigned long menuBlinkTimer;

extern int viewMode; 
extern int midiState; // 0=off, 1=on, 2=set
extern int midiLearnChannel;
extern unsigned long midiDoneTimer;
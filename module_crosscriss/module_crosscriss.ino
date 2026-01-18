// -- MODULE: template ----//
#include <Wire.h>

// --- COMMUNICATIONS --- //
const int PACKET_SIZE = 16;
byte info[PACKET_SIZE];
byte command[PACKET_SIZE];

bool info_read = false;
bool incoming_data = false;

enum Register_Mapping {
  SOLVE_STATUS = 0,
  SELECT_PIN = 1
};

const int addr = 11;
const int pin_slotSelect = 2;
const int pin_indicator[2] = {12,13};

void requestEvent();
void receiveEvent(int bytes);
void proc_infoHandler();
void proc_setIndicatorLed(bool status);
void rgbWrite(int r, int g, int b);
void setColourFromLedColour();
void updateRgbBlink();


// --- PROTOCOL VARIABLES --- //
bool mistake = false;
uint32_t flash_time_start;
bool flash_state = false;

int expectedWire();
int expectedPos();
void scanConnections(); 

// --- GAME VARIABLES --- //
int ledColour;
int ledBlinks;
const int startPins[4] = {3,4,5,6}; //where the wires go
const int endPins[4] = {7,8,9,10}; // where the wires end
int lastWireToPos[4] = {-1, -1, -1, -1};
bool completedPuzzle = false;
int wireToPos[4] = {-1, -1, -1, -1};
const bool RGB_COMMON_ANODE = false;  


unsigned long blinkTimer = 0;
int blinkCount = 0;
bool blinkOn = false;

const unsigned long BLINK_ON_MS = 200;
const unsigned long BLINK_OFF_MS = 200;
const unsigned long BLINK_GAP_MS = 600;

// --- Pins (to change) --- //

const int pin_rgbR = A2;
const int pin_rgbG = A1;
const int pin_rgbB = A0;


void setup() {
  Serial.begin(9600);
  Serial.println("Brain Up");

  Wire.begin(addr);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  for (int i = 0; i < 16; ++i) {
  info[i] = 0xFF;
  command[i] = 0xFF;
}

// start pins (wire identity): OUTPUT, idle HIGH
for (int i = 0; i < 4; i++) {
  pinMode(startPins[i], OUTPUT);
  digitalWrite(startPins[i], HIGH);
}

// end pins (positions): INPUT_PULLUP, active LOW when connected to a LOW start pin
for (int i = 0; i < 4; i++) {
  pinMode(endPins[i], INPUT_PULLUP);
}

pinMode(pin_rgbR, OUTPUT);
pinMode(pin_rgbG, OUTPUT);
pinMode(pin_rgbB, OUTPUT);

randomSeed(analogRead(A5));  
ledColour = random(4);      
ledBlinks = random(4);     

Serial.print("LED colour number: ");
Serial.println(ledColour);

Serial.print("LED blinks number: ");
Serial.println(ledBlinks);

Serial.print("Expected wire (1-4): ");
Serial.println(expectedWire() + 1);

Serial.print("Expected position (1-4): ");
Serial.println(expectedPos() + 1);

}

/*
1. game state requirement
2. if player makes a mistake, proc_mistakeMade();

*/

void loop() {
  //update slot select
  proc_infoHandler();

  // check if game finished
  if (completedPuzzle) { //TODO GAME STATE REQUIREMENT
    Serial.println("MODULE SOLVED");
    info[SOLVE_STATUS] = 0x01;
    proc_setIndicatorLed(true);

    mistake = false;
  return   ;                   
}
  //pause input and flash wrong for 2 seconds
  else if (mistake) {
    Serial.println("mistake");
    if (millis() - flash_time_start > 2000) {
      mistake = false;

    } else {
      bool flash = ((millis() - flash_time_start) / 100 % 2 == 0);
      proc_setIndicatorLed(flash);
    }
  }
  else {
    proc_setIndicatorLed(false);
    updateRgbBlink();

    scanConnections();

    int wire = expectedWire();
    int pos = expectedPos();
for (int i = 0; i < 4; i++) {
    if (lastWireToPos[i] == -1 && wireToPos[i] != -1) {

    Serial.print("New connection: wire ");
    Serial.print(i + 1);
    Serial.print(" -> position ");
    Serial.println(wireToPos[i] + 1);

    // If it matches exactly, solve
    if (i == wire && wireToPos[i] == pos) {
      completedPuzzle = true;
      Serial.println("SOLVED: correct wire in correct position");
    }
    else {
      mistake = true;
      proc_mistakeMade();
    }
  }
}

// update history
for (int i = 0; i < 4; i++) {
  lastWireToPos[i] = wireToPos[i];
}

}




  delay(10);
}

void requestEvent() {
  info_read = true;
  Wire.write(info, 16); // respond with message of 6 bytes
  // as expected by master
}

void receiveEvent(int bytes) {
  incoming_data = true;
  int i = 0;
   while (i < PACKET_SIZE) {
    command[i] = Wire.read();
    i++;
  }
}

void proc_infoHandler(){ 
  // Serial.println(digitalRead(pin_slotSelect));
  if (digitalRead(pin_slotSelect) == HIGH) {
    info[SELECT_PIN] = 0x01;
    // Serial.println("yup hit");
  }

  //reset info when read --> slot select, and mistake made
  if (info_read) {
    info[SELECT_PIN] = 0x00;
    if (info[SOLVE_STATUS] == 0x02) {
      info[SOLVE_STATUS] = 0x00;
    }
    info_read = false;
  }
}

void proc_mistakeMade(){
  info[SOLVE_STATUS] = 0x02; //MUST DO ONE TIME ONLY, will register to the main
  mistake = true;
  flash_time_start = millis();
  flash_state = true;
}

void proc_setIndicatorLed(bool status){
  if (status){
    digitalWrite(pin_indicator[0], LOW);
    digitalWrite(pin_indicator[1], HIGH);
  }
  else {
    digitalWrite(pin_indicator[0], HIGH);
    digitalWrite(pin_indicator[1], LOW);
  }
}

int expectedWire() {
  // Assumes: 0=Red, 1=Green, 2=Blue, 3=Yellow
  if (ledColour == 0) return 0;      // Red -> first wire (top)
  else if (ledColour == 1) return 1; // Green -> second wire
  else if (ledColour == 2) return 3; // Blue -> fourth wire
  else return 2;                     // Yellow -> third wire
}

int expectedPos() {
  // ledBlinks: 0..3 means 1..4 blinks
  if (ledBlinks == 0) return 3;      // 1 blink -> last position (bottom)
  else if (ledBlinks == 1) return 1; // 2 blinks -> second position
  else if (ledBlinks == 2) return 0; // 3 blinks -> first position (top)
  else return 2;                     // 4 blinks -> third position
}

void scanConnections() {
  // clear old scan
  for (int i = 0; i < 4; i++) wireToPos[i] = -1;

  for (int i = 0; i < 4; i++) {
      digitalWrite(startPins[0], HIGH);
      digitalWrite(startPins[1], HIGH);
      digitalWrite(startPins[2], HIGH);
      digitalWrite(startPins[3], HIGH);

    // pull ONE wire low to test
    digitalWrite(startPins[i], LOW);
    delayMicroseconds(200);

    // read which end pin went low (active-low)
    if (digitalRead(endPins[0]) == LOW) wireToPos[i] = 0;
    else if (digitalRead(endPins[1]) == LOW) wireToPos[i] = 1;
    else if (digitalRead(endPins[2]) == LOW) wireToPos[i] = 2;
    else if (digitalRead(endPins[3]) == LOW) wireToPos[i] = 3;
    else wireToPos[i] = -1;
  }

  // release everything
  digitalWrite(startPins[0], HIGH);
  digitalWrite(startPins[1], HIGH);
  digitalWrite(startPins[2], HIGH);
  digitalWrite(startPins[3], HIGH);
}

void rgbWrite(int r, int g, int b) {
  // treat any value > 0 as "ON"
  bool rOn = (r > 0);
  bool gOn = (g > 0);
  bool bOn = (b > 0);

  if (RGB_COMMON_ANODE) {
    digitalWrite(pin_rgbR, rOn ? LOW : HIGH);
    digitalWrite(pin_rgbG, gOn ? LOW : HIGH);
    digitalWrite(pin_rgbB, bOn ? LOW : HIGH);
  } else {
    digitalWrite(pin_rgbR, rOn ? HIGH : LOW);
    digitalWrite(pin_rgbG, gOn ? HIGH : LOW);
    digitalWrite(pin_rgbB, bOn ? HIGH : LOW);
  }
}

void setColourFromLedColour() {
  // ledColour: 0..3
  // 0=Red, 1=Green, 2=Blue, 3=Yellow  (edit if your numbering differs)
  if (ledColour == 0) rgbWrite(255, 0, 0);         // Red
  else if (ledColour == 1) rgbWrite(0, 255, 0);    // Green
  else if (ledColour == 2) rgbWrite(0, 0, 255);    // Blue
  else rgbWrite(255, 255, 0);                      // Yellow
}

void updateRgbBlink() {
  int targetBlinks = ledBlinks + 1; // because ledBlinks is 0..3 representing 1..4

  unsigned long now = millis();

  // starting a new blink set
  if (blinkCount == 0 && !blinkOn && (now - blinkTimer >= BLINK_GAP_MS)) {
    blinkOn = true;
    blinkTimer = now;
    setColourFromLedColour();
    blinkCount = 1;
    return;
  }

  if (blinkOn) {
    if (now - blinkTimer >= BLINK_ON_MS) {
      blinkOn = false;
      blinkTimer = now;
      rgbWrite(0,0,0);
    }
  } else {
    // LED is off between blinks
    if (blinkCount > 0 && now - blinkTimer >= BLINK_OFF_MS) {
      if (blinkCount < targetBlinks) {
        // next blink
        blinkOn = true;
        blinkTimer = now;
        setColourFromLedColour();
        blinkCount++;
      } else {
        // finished the set, reset and wait gap
        blinkCount = 0;
        blinkTimer = now;
      }
    }
  }
}


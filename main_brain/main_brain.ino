#include <SevSeg.h>
#include <Wire.h>

// -------------- VARIABLES -------------- //

// --- Module Set Up --- //
enum Module_Types {
  MODULE_SIMONSAYS,
  MODULE_CUTTHEWIRE,
  MODULE_PATTERNDRAWINGS,
};

const int MODULE_SLOTS = 1;
const int MODULE_LIST[MODULE_SLOTS] = {MODULE_SIMONSAYS};
const int MODULE_ADDR[MODULE_SLOTS] = {8}; //choose from range 8 - 25

// --- Pins --- //
const int SLOT_SELECT[MODULE_SLOTS] = {4}; //to   change, maps to modules not slots
byte digitPins[] = {22,24};
byte segmentPins[] = {26, 28, 30, 32, 34, 36, 38};
const int pin_speaker_toggle = 42;
const int pin_speaker_pitch = 7; 
const int pin_motor = 46;
const int pin_mistake_one = 31;
const int pin_mistake_two = 33;

// --- Communication Protocol Setup --- //
const int PACKET_SIZE = 16;
struct Module {
  Module_Types type;
  int addr;
  int slot;
  int state;
};
enum Register_Mapping {
  SOLVE_STATUS = 0,
  SELECT_PIN = 1
};

// --- Globals --- //
SevSeg sevseg; //Instantiate a seven segment object
int modules_solved = 0;
int mistakes_strikes = 0;
bool buzzing = false;
uint32_t buzz_start_time;
uint32_t watchdog_start_time;
uint32_t game_start_time;

byte new_packet[PACKET_SIZE];
Module modules[MODULE_SLOTS];

const uint32_t GAME_LENGTH = 30000; 
const uint32_t BUZZING_LENGTH = 1000;




// -------------- FUNCTIONS -------------- //

// --- processes ---/
void sense_modules(bool debug = false);
void game_over(bool win);

// --- wire --- //
bool wire_checkConnection(byte addr);
void wire_sendMessage(int addr, const int size, byte* message);
void wire_requestInfoFrom(int addr);
void wire_getPacket(bool debug = false);
void wire_clearStream();

// --- watchdog --- //
bool watchdog_setStart();
bool watchdog_timeout(int time);

// --- debugging --- //
void debug_exception(const String& msg);






void setup() {
  Wire.begin();        // join I2C bus (address optional for master)
  Serial.begin(9600);  // start serial for output
  Serial.println("Brain Up");

  //set up pins
  sevseg.begin(COMMON_CATHODE, 2, digitPins, segmentPins, false, false, false, true);
  pinMode(pin_speaker_toggle, OUTPUT);
  pinMode(pin_speaker_pitch, OUTPUT);
  pinMode(pin_motor, OUTPUT);
  pinMode(pin_mistake_one, OUTPUT);
  pinMode(pin_mistake_two, OUTPUT);

  digitalWrite(pin_motor, HIGH);

  //set up game time
  game_start_time = millis();

  //set up module array
  sense_modules(true);
  Serial.println("Brain Spinning...");
}

void loop(){
  //wins
  if (modules_solved >= MODULE_SLOTS) {
    game_over(true);
  }

  //strikes
  if (mistakes_strikes > 2) {
    game_over(false);
  } 
  if (mistakes_strikes > 1) {
    digitalWrite(pin_mistake_two, HIGH);
  }
  if (mistakes_strikes > 0) {
    digitalWrite(pin_mistake_one, HIGH);
  }

  //time keeping
  int32_t time_left = game_start_time + GAME_LENGTH - millis();
  if (time_left < 0) {
    sevseg.blank();
    game_over(false);
  }
  else if (time_left < 60000) {
    sevseg.setNumber(time_left/1000,0); //show secs
  }
  else {
    sevseg.setNumber(time_left/60000,0);
  }

  //buzzing 
  if (buzzing) {
    //check time
    if (millis() - buzz_start_time > BUZZING_LENGTH) {
      digitalWrite(pin_speaker_toggle, LOW);
      buzzing = false;
    }
    else {
      bool flash = ((millis() - buzz_start_time) / 100 % 2 == 0);
      digitalWrite(pin_speaker_toggle, flash);
      analogWrite(pin_speaker_pitch, 50);
    }
  }
  else {
    bool flash = ((millis() - buzz_start_time) / 200 % 5 == 0);
    digitalWrite(pin_speaker_toggle, flash);
    analogWrite(pin_speaker_pitch, 5);
  }

  for (int i = 0; i < MODULE_SLOTS; ++i){
    //per module logic
    if (modules[i].state != 0x01) { // solved
      wire_requestInfoFrom(modules[i].addr);
      wire_getPacket();

      //see if module is solved
      if (new_packet[SOLVE_STATUS] == 0x01) {
        modules[i].state = 0x01;
        ++modules_solved;
      }
      else if (new_packet[SOLVE_STATUS] == 0x02) {
        Serial.println(modules[i].state);
        if (modules[i].state != 0x02) {
          Serial.println("ggeaffd");
          modules[i].state = 0x02;
          buzzing = true;
          buzz_start_time = millis();
          ++mistakes_strikes;
        }
      }
      else if (new_packet[SOLVE_STATUS] == 0x00){
        modules[i].state = 0x00;
      }
    }
  }

  //implement score keeping / case stuff here
  sevseg.refreshDisplay(); // Must run repeatedly
}


void sense_modules(bool debug){
  if (debug) Serial.println("sense_modules__starting");
  //loop through each module
  for (int j = 0; j < MODULE_SLOTS; ++j) {
    Module_Types modType = MODULE_LIST[j];
    int modAddress = MODULE_ADDR[j];

    if (debug) Serial.println("sense_modules__searching for address " + String(modAddress));
    // check connection
    bool result = wire_checkConnection(modAddress);
    if (!result){
      debug_exception("sense_modules__address " + String(modAddress) + "offline.\n Stopping...");
    }
    
    //loop through each slot
    bool found = false;
    for (int i = 0; i < MODULE_SLOTS; ++i) {
      //pull select pin high
      int pin = SLOT_SELECT[i];
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);

      //delay a little bit to let the electrons flow, very needed
      delay(500);

      //ask for info packet
      wire_requestInfoFrom(modAddress);

      //recive the info packet
      wire_getPacket();
           
      //pull select pin low
      digitalWrite(pin, LOW);

      //check result
      if (debug) Serial.println("sense_modules__Slot " + String(i) + " result: " + String(new_packet[SELECT_PIN]));
      if (new_packet[SELECT_PIN] == 0x01) {
        //create module struct
        Module mod;
        mod.type = modType;
        mod.addr = modAddress;
        mod.slot = i;

        modules[j] = mod;
        if (debug) Serial.println("sense_modules__Module " + String(modType) + " found at slot" + String(i));
        found = true;
        break;
      }
    
    }

    if (!found) debug_exception("Module " + String(j) + "Slot not found.");
  }
}

void game_over(bool win){
  if (win) {
    digitalWrite(pin_speaker_toggle, HIGH);
    analogWrite(pin_speaker_pitch, 50);
    delay(200);
    digitalWrite(pin_speaker_toggle, LOW);
    delay(200);
    digitalWrite(pin_speaker_toggle, HIGH);
    analogWrite(pin_speaker_pitch, 50);
    delay(200);
    digitalWrite(pin_speaker_toggle, LOW);
  }
  else {
    digitalWrite(pin_motor, LOW);
    digitalWrite(pin_speaker_toggle, HIGH);
    analogWrite(pin_speaker_pitch, 50);
    delay(1000);
    digitalWrite(pin_speaker_toggle, LOW);
  }
  while (true) {
    if (win) {
      Serial.println("GAME OVER, YOU WIN");
    }
    else {
      Serial.println("GAME OVER, YOU LOST");
      digitalWrite(pin_motor, LOW);
    }
    delay(10);
  }
}

// --- wire --- //

bool wire_checkConnection(byte addr) {
  Wire.beginTransmission(addr);
  byte error = Wire.endTransmission();
  if (error == 0) {
    return true;
  }
  return false;
}

void wire_sendMessage(int addr, const int size, byte* message) {
  Wire.beginTransmission(addr);
  Wire.write(message, size);
  Wire.endTransmission(addr);
}

void wire_requestInfoFrom(int addr){
  Wire.requestFrom(addr, PACKET_SIZE);
}

void wire_getPacket(bool debug){
  int bytesRecieved = 0;
  watchdog_setStart();
  while (bytesRecieved < PACKET_SIZE) {
    if (Wire.available()) {
      new_packet[bytesRecieved] = Wire.read();
      if (debug) Serial.println("recived " + String(new_packet[bytesRecieved] ));
      ++bytesRecieved;
    }
    if (watchdog_timeout(1000)) break;
  }
  wire_clearStream();
}

void wire_clearStream() {
  // TODO to be implemented, be careful with ts, send error messeages
}

// --- watchdog --- //

bool watchdog_setStart(){
  watchdog_start_time = millis();
}

bool watchdog_timeout(int time){
  uint32_t now = millis();
  if (now - watchdog_start_time > time) {
    return true;
  }
  return false;
}


// --- debugging --- //
void debug_exception(const String& msg) {
  Serial.println(msg);
  while (true) {
    delay(10);
  }
}
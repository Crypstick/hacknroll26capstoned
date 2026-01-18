// -- MODULE: CUT WIRE ----//
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

const int addr = 8;
const int pin_slotSelect = 2;
const int pin_indicator[2] = {3,4};

void requestEvent();
void receiveEvent(int bytes);
void proc_infoHandler();
void proc_setIndicatorLed(bool status);

// --- PROTOCOL VARIABLES --- //
bool mistake = false;
uint32_t flash_time_start;
bool flash_state = false;

// --- GAME VARIABLES --- //

String wireArrangement[4];
bool cutWires[4] = {false, false, false, false};
int redCount = 0;
int blueCount = 0;
int yellowCount = 0;
int lastRed;
int correctWire;
bool completedPuzzle = false;


/*
Voltages:
Black —> ~0.7V
yellow —> ~3.5V
red —> ~0.5V
blue —> ~2.6V
*/

// --- Pins (to change) --- //

const byte wirePins[4] = {A0, A1, A2, A3};

void setup() {
  //set up comms
  Serial.begin(9600);
  Serial.println("Brain Up");
  Wire.begin(addr);             // join I2C bus with address
  Wire.onRequest(requestEvent); // pub info
  Wire.onReceive(receiveEvent); // sub command
  //set up data arrays
  for (int i = 0; i < 16; ++i) {
    info[i] == 0xFF;  
    command[i] == 0xFF; 
  }
  //info defaults
  info[SOLVE_STATUS] = 0x00;
  info[SELECT_PIN] = 0x00;

  //set pin modes (to change)
  pinMode(pin_slotSelect, INPUT);
  pinMode(pin_indicator[0], OUTPUT);
  pinMode(pin_indicator[1], OUTPUT);
  //TODO

  //set up shuffle (to change)
  flash_time_start = millis();
  flash_state = 0;
  //TODO

  for (int i = 0; i < 4; i++) {
    int raw = analogRead(wirePins[i]);
    float voltage = raw * (5.0 / 1023.0);

    if (voltage > 0.3 && voltage < 0.7) {
      wireArrangement[i] = "red";
      redCount++;
      if (redCount > 1) {
        lastRed = i;
      };
    }
    else if (voltage > 0.5 && voltage < 1.0) {
      // black (~0.7V)
      wireArrangement[i] = "black";
    }
    else if (voltage > 2.4 && voltage < 2.8) {
      // blue  (~2.6V)
      wireArrangement[i] = "blue";
      blueCount++;
    }
    else if (voltage > 3.3 && voltage < 3.7) {
      // yellow (~3.5V)
      wireArrangement[i] = "yellow";
      yellowCount++;
    }
    else {
      // lowk no wire there
      Serial.println();
    }


  };


  if (redCount > 1) {
    correctWire = lastRed;
  }
  else if (wireArrangement[3] == "yellow" && redCount == 0) {
    correctWire = 0;
  }
  else if (blueCount == 1) {
    correctWire = 0;
  }
  else if (yellowCount > 1) {
    correctWire = 3;
  }
  else {
    correctWire = 1;
  }


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
  }
  //pause input and flash wrong for 2 seconds
  else if (mistake) {
    if (millis() - flash_time_start > 2000) {
      mistake = false;
    } else {
      bool flash = ((millis() - flash_time_start) / 100 % 2 == 0);
      proc_setIndicatorLed(flash);
    }
  }
  else {
    proc_setIndicatorLed(false);
    //GAMEPLAY HERE
    for (int i = 0; i < 4; i++) {
      if (digitalRead(wirePins[i]) == LOW) {
        cutWires[i] = true;
      }
      if (cutWires[i] == true && i == correctWire) {
        completedPuzzle = true;
      }
      else if (cutWires[i] == true && i != correctWire) {
        mistake = true;
      }

      }
    }


  delay(10);
};

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
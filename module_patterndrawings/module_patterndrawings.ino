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

const int addr = 10;
const int pin_slotSelect = 3;
const int pin_indicator[2] = {A2,A3};

void requestEvent();
void receiveEvent(int bytes);
void proc_infoHandler();
void proc_setIndicatorLed(bool status);

// --- PROTOCOL VARIABLES --- //
bool mistake = false;
uint32_t flash_time_start;
bool flash_state = false;

// --- GAME VARIABLES --- //
int x_pos = 1;
int y_pos = 1;
struct Joystick {
  int x;
  int y;
};
int flipped = 0;
int target = 5;
bool latched = true;
//simon says, cut wire, criss cross, pattern drawings
const int truth[3][3] = { //imsorry for scamming
  {1, 1, 0},
  {0, 1, 1},
  {0, 0, 1}
};
int current[3][3] = {
  {0,0,0},
  {0,0,0},
  {0,0,0}
};

// --- Pins (to change) --- //
const int grid[3][3] = {
  {4,5,6},
  {7,8,9},
  {10,11,13}
};
const int pin_x = A1;
const int pin_y = A0;
const int pin_sw = 13;


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
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      pinMode(grid[i][j], OUTPUT);
      // digitalWrite(grid[i][j], HIGH);
      // delay(300);
      digitalWrite(grid[i][j], LOW);
    }
  }
  pinMode(pin_x, INPUT);
  pinMode(pin_y, INPUT);
  pinMode(pin_sw, INPUT);

  //set up shuffle (to change)
  flash_time_start = millis();
  flash_state = 0;
  //TODO
  

}

/*
1. game state requirement
2. if player makes a mistake, proc_mistakeMade();

*/

void loop() {
  //update slot select
  proc_infoHandler();

  // check if game finished
  if (flipped >= target) { //TODO GAME STATE REQUIREMENT
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
    if (current[x_pos][y_pos] == 0) {
      if (truth[x_pos][y_pos] == 0) {
        x_pos = 1;
        y_pos = 1;
        flipped = 0;
        for (int i = 0; i < 3; i++) {
          for (int j = 0; j < 3; j++) {
            current[i][j] = 0;
            digitalWrite(grid[i][j], LOW);
          }
        }
        proc_mistakeMade();
      }
      else {
        flipped++;
        current[x_pos][y_pos] = 1;
        digitalWrite(grid[x_pos][y_pos], HIGH);
      }
    }

    Joystick joy = readJoystick();
    if (latched) {
      if (joy.x == 0 && joy.y == 0) {
        // Serial.println("unlactced");
        // Serial.println(joy.x);
        // Serial.println(joy.y);
        latched = false;
      }
    }
    else {
      if (joy.x != 0 or joy.y != 0) latched = true;
      x_pos += joy.x;
      y_pos += joy.y;
      x_pos = constrain(x_pos, 0, 2);
      y_pos = constrain(y_pos, 0, 2);
    }
    
    // Serial.println(String(joy.x) + " " + String(joy.y) + " " + String(latched));
    // Serial.println(String(x_pos) + " " + String(y_pos) + " " + String(latched));
    

  }




  delay(10);
}

// Reads a 2-axis analog joystick and maps values to -1 → 1
Joystick readJoystick() {
  int rawX = analogRead(pin_x);
  int rawY = analogRead(pin_y);

  // Map raw value (0-1023) to -1 → 1
  float x = ((rawX * 5.0 / 1023.0) - 2.5) / 2.5;
  float y = ((rawY * 5.0 / 1023.0) - 2.5) / 2.5;

  // Clip to ensure range stays -1 → 1
  x = constrain(x, -1.0, 1.0);
  y = constrain(y, -1.0, 1.0);

  // Serial.print(String(x) + "  " + String(y) + " ---- " );

  if (x > 0.3) x = 1;
  else if (x < -0.3) x = -1;
  else x = 0;

  if (y > 0.3) y = -1;
  else if (y < -0.3) y = 1;
  else y = 0;

  // Serial.println(String(x) + "  " + String(y) );
  return {int(x), int(y)};
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
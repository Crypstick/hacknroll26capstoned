// -- MODULE: SIMONSAYS ----//
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
void proc_setIndicatorLed();




// --- GAME VARIABLES --- //
//yellow, green, red, blue
//red, yellow, blue, green
enum buttons {
  RED,
  YELLOW,
  BLUE,
  GREEN,
  NONE
};
int mappings[4] = {BLUE, GREEN, RED, YELLOW};
bool wait_for_release = false;
bool mistake = false;
int game_sequence[4] = {0, 1, 2, 3};
int correct_buttons[4];
int game_state = 0;
uint32_t flash_time_start;
bool flash_state = false;




// --- Pins (to change) --- //
const int pin_buttons[4] = {5,6,7,8};
const int pin_buttonLeds[4] = {9,10,11,12};




void setup() {
  //set up comms
  Serial.begin(9600);
  Serial.println("Brain Up");
  Wire.begin(addr);             // join I2C bus with address
  Wire.onRequest(requestEvent); // pub info
  Wire.onReceive(receiveEvent); // sub command
  //set up data arrays
  for (int i = 0; i < 16; ++i) {
    info[i] = 0xFF;  
    command[i] = 0xFF;
  }
  //info defaults
  info[SOLVE_STATUS] = 0x00;
  info[SELECT_PIN] = 0x00;




  //set pin modes (to change)
  pinMode(pin_slotSelect, INPUT);
  pinMode(pin_indicator[0], OUTPUT);
  pinMode(pin_indicator[1], OUTPUT);
  for (int i = 0; i < 4; ++i) {
    pinMode(pin_buttons[i], INPUT);
    pinMode(pin_buttonLeds[i], OUTPUT);
  }




  //set up shuffle (to change)
  randomSeed(analogRead(A0));
  flash_time_start = millis();
  flash_state = 0;
  for (int i = 0; i < 4; i++) {
    int j = random(4);
    int t = game_sequence[i];
    game_sequence[i] = game_sequence[j];
    game_sequence[j] = t;
  }
  Serial.print("Sequence is ");
  for (int i = 0; i < 4; i++) {
    // Serial.print(String(game_sequence[i]) + " ");
    // digitalWrite(pin_buttonLeds[i], LOW);
    correct_buttons[i] = mappings[game_sequence[i]];
  };
  Serial.println();
  digitalWrite(13, HIGH);






}




void loop() {
  //update slot select
  proc_infoHandler();




  // check if game finished
  if (game_state >= 4) {
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
    // check button pressed
    buttons pressed = NONE;
    for (int i = 0; i<4; ++i) {
      if (digitalRead(pin_buttons[i]) == HIGH){ //pull up resistor, active low
        pressed = i;
        break;
      }
    }
    if (wait_for_release) {
      if (pressed != NONE) pressed = NONE;
      else {
        wait_for_release = false;
        Serial.println("Released");
      }
    }

    //check correct or wrong
    if (pressed != NONE){
      if (pressed == correct_buttons[game_state]) {
        Serial.println("correct");
        /*
        for (int i = 0; i < 4; i++) {
          digitalWrite(pin_buttonLeds[i], LOW);
        };
        */
        game_state++;
      }
      else{
        Serial.println("wrong");
        info[SOLVE_STATUS] = 0x02; //MUST DO ONE TIME ONLY, will register to the main
        mistake = true;
        flash_time_start = millis();
        flash_state = true;
        game_state = 0;
      }
      Serial.println("current state: " + String(game_state) + ", button to press: " + String(game_sequence[game_state]));
      wait_for_release = true;
    }

    //deal with flashing
    uint32_t now = millis();
    if (now - flash_time_start > 500) {
      flash_state = !flash_state;
      flash_time_start = now;
      digitalWrite(pin_buttonLeds[game_sequence[game_state]], flash_state);
    }

  }

  delay(10);
}



void requestEvent() {
  Wire.write(info, 16); // respond with message of 6 bytes
  // as expected by master
  info_read = true;
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
    Serial.println("yup hit");
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




void proc_setIndicatorLed(bool status){
  if (status){
    digitalWrite(pin_indicator[0], HIGH);
    digitalWrite(pin_indicator[1], LOW);
  }
  else {
    digitalWrite(pin_indicator[0], LOW);
    digitalWrite(pin_indicator[1], HIGH);
  }
}






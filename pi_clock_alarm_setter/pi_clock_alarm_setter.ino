#include <RCSwitch.h>
#include <Arduino.h>
#include <TM1637Display.h>


//7_SEGMENT_CLOCK
#define CLK_CLOCK 8
#define DIO_CLOCK 9

TM1637Display display(CLK_CLOCK, DIO_CLOCK);

uint8_t digit_0 = 0x3f;
uint8_t digit_1 = 0x06;
uint8_t digit_2 = 0x5b;
uint8_t digit_3 = 0x4f;
uint8_t digit_4 = 0x66;
uint8_t digit_5 = 0x6d;
uint8_t digit_6 = 0x7d;
uint8_t digit_7 = 0x07;
uint8_t digit_8 = 0x7f;
uint8_t digit_9 = 0x6f;
uint8_t segment_code_line = digit_8 - digit_0;

uint8_t points = 0x40;
uint8_t no_points = 0x80;

uint8_t clearDisplay[] = {0x00, 0x00, 0x00, 0x00};

//433MHz SENDER
RCSwitch rcSwitch = RCSwitch();
#define SEND_CODE_SET_ALARM 42//+ set time
#define SEND_CODE_SHOW_NEXT 33
#define SEND_CODE_DELETE_ALL 31
#define SEND_BITS 16
#define PIN_DATA 10

//ROTARY ENCODER
//pins
#define DT_ROTARY_ENCODER 6
#define CLK_ROTARY_ENCODER 5
#define SW_ROTARY_ENCODER 7
//variables
int rotaryEncoderPos = 0;
int lastPos = LOW;
int n = LOW;
int button = LOW;
int lastButton = LOW;

//BUTTONS
#define PIN_BUTTON_SEND 4
#define PIN_BUTTON_SHOW_NEXT 3
#define PIN_BUTTON_DELETE_ALL 2

void setup() {
  //set the rotary encoder inputs
  pinMode(CLK_ROTARY_ENCODER, INPUT_PULLUP);
  pinMode(DT_ROTARY_ENCODER, INPUT_PULLUP);
  pinMode(SW_ROTARY_ENCODER, INPUT_PULLUP);

  //set the button pins to input
  pinMode(PIN_BUTTON_SEND, INPUT);
  pinMode(PIN_BUTTON_SHOW_NEXT, INPUT);
  pinMode(PIN_BUTTON_DELETE_ALL, INPUT);

  //start with 0700 clock code
  rotaryEncoderPos = 7 * 60 / 5 + 1;

  //set the clock display brightness to (about) 50%
  display.setBrightness(3);
  
  //rc-switch sending on digital 10
  rcSwitch.enableTransmit(PIN_DATA);

  //start the serial connection (for debugging)
  Serial.begin(9600);
}

void loop() {
  //time encoded by rotary encoder
  getTimeSetterSignal();
  //button inputs for SEND, SHOW, DELETE
  getButtonInputs();
  
  //low delay for the rotary encoder (greater delays won't work good)
  delay(1);
}

void getTimeSetterSignal() {
  n = digitalRead(CLK_ROTARY_ENCODER);
  button = !digitalRead(SW_ROTARY_ENCODER);
  
  if (button != lastButton) {
    //Serial.print(rotaryEncoderPos);
    //Serial.print("|");
    //Serial.println(button);
    //delay(10);
    lastButton = button;
  }

  boolean updateClock = false;
  if ((lastPos == LOW) && (n == HIGH)) {
    if (digitalRead(DT_ROTARY_ENCODER) == LOW) {
      rotaryEncoderPos++;
      updateClock = true;
    }
    else {
      rotaryEncoderPos--;
      updateClock = true;
    }
    //Serial.print (rotaryEncoderPos);
    //Serial.print ("|");
    //Serial.println (button);
  }
  lastPos = n;
  
  if (updateClock) {
    updateClock = false;
    updateClockDisplay();
  }
}

void getButtonInputs() {
  if (digitalRead(PIN_BUTTON_SEND) == HIGH) {
    delay(50);//unbounce button
    if (digitalRead(PIN_BUTTON_SEND) == HIGH) {
      sendAlarmTime();
    }
  }
  if (digitalRead(PIN_BUTTON_SHOW_NEXT) == HIGH) {
    delay(50);//unbounce button
    if (digitalRead(PIN_BUTTON_SHOW_NEXT) == HIGH) {
      sendShowAlarmSignal();
    }
  }
  if (digitalRead(PIN_BUTTON_DELETE_ALL) == HIGH) {
    delay(50);//unbounce button
    if (digitalRead(PIN_BUTTON_DELETE_ALL) == HIGH) {
      //clear the display
      display.setSegments(clearDisplay);
      
      //load function for deleting
      uint8_t loadSegmentCode[] = {0x00, 0x00, 0x00, 0x00};
      for (int i = 0; i < 4; i++) {
        loadSegmentCode[i] = segment_code_line;
        delay(500);//load delete function
        if (digitalRead(PIN_BUTTON_DELETE_ALL) == HIGH) {
          display.setSegments(loadSegmentCode);
        }
        else {
          break;
        }
      }
      
      //end load and send the signal
      if (digitalRead(PIN_BUTTON_DELETE_ALL) == HIGH) {
        sendDeleteAllSignal();
      }
      updateClockDisplay();
    }
  }
}

//calculate the current time setting by the encoder position
int getTimeSet(int encoderPos) {
  const int maxTime = 24 * 60;
  int timeSet = encoderPos * 5;//5 minute steps
  
  //stay in 24 hour range
  while (timeSet < 0) {
    timeSet += maxTime;
  }
  timeSet = timeSet % maxTime;
  
  int hours = timeSet / 60;
  int minutes = timeSet % 60;

  //convert to string-like clock code
  timeSet = hours * 100 + minutes;

  return timeSet;
}

void updateClockDisplay() {
  int timeSet = getTimeSet(rotaryEncoderPos);
  display.showNumberDecEx(timeSet, points, true);//set the time (with clock points)
}

void sendAlarmTime() {
  int clockCode = getTimeSet(rotaryEncoderPos);
  int sendCode = SEND_CODE_SET_ALARM + clockCode;
  
  //send serval times and hope the 433MHz receiver gets at least one of them
  for (int i = 0; i < 5; i++) {
    rcSwitch.send(sendCode, SEND_BITS);
    delay(10);
  }
}

void sendShowAlarmSignal() {
  //send serval times and hope the 433MHz receiver gets at least one of them
  for (int i = 0; i < 5; i++) {
    rcSwitch.send(SEND_CODE_SHOW_NEXT, SEND_BITS);
    delay(10);
  }
}

void sendDeleteAllSignal() {
  //send serval times and hope the 433MHz receiver gets at least one of them
  for (int i = 0; i < 5; i++) {
    rcSwitch.send(SEND_CODE_DELETE_ALL, SEND_BITS);
    delay(10);
  }
}

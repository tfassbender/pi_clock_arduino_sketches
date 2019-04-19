#include <Arduino.h>
#include <TM1637Display.h>
#include <RCSwitch.h>

// Module connection pins (Digital Pins)
#define CLK 3
#define DIO 9//changed from 2 to 9 because PIN 2 is an interrupt pin (needed for the rc-switch)

// connection pins for infra red receiver and target led 
#define LED 4
#define IR_RECEIVER 5

// connection pin for relays (speaker amplifier and display backlight)
#define SPEAKER_AMP 6
#define DISPLAY_BACKLIGHT 7

// connection pin for the display backlight push-button
#define DISPLAY_BACKLIGHT_BUTTON 8

// analog pin for reading the photoresistor values
#define PHOTO_RESISTOR 0

// connection pins and settings for the 433MHz RC-Switch
RCSwitch rcSwitch = RCSwitch();

#define SEND_CODE_SET_ALARM 42//+ set time
#define SEND_CODE_SHOW_NEXT 33
#define SEND_CODE_DELETE_ALL 31
#define SEND_BITS 16
#define PIN_DATA 0//interrupt 0 (PIN 3 on Pro Micro; PIN 2 on Nano)

//the message codes to send to the alarm clock for remote alarms (need pattern recognition because no callback can be used)
#define REMOTE_ALARM_CODE_SET_ALARM "REMOTE_ALARM_SET "//+ alarm time as string
#define REMOTE_ALARM_CODE_SHOW_NEXT_ALARM "REMOTE_ALARM_SHOW"
#define REMOTE_ALARM_CODE_DELETE_ALL "REMOTE_ALARM_DELETE_ALL"

unsigned long fiveSecondsDisplayStartTime;

// display settings and codes
TM1637Display display(CLK, DIO);

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

uint8_t points = 0x40;
uint8_t no_points = 0x80;

const uint8_t clearDisplay[] = {0x00, 0x00, 0x00, 0x00};
const uint8_t SEG_NONE[] = {
  SEG_C | SEG_E | SEG_G,                           // n
  SEG_C | SEG_D | SEG_E | SEG_G,                   // o
  SEG_C | SEG_E | SEG_G,                           // n
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G            // E
};

String inputText = "";

int lastKnownClockTime;
unsigned long lastBrightnessUpdateTime;

bool displayBacklightOn = true;
//ignore the next command from the serial connection to turn the display off when it was turned on using the button
bool ignoreNextDisplayTurnOff = false;

bool alarmEnabled = false;

void setup()
{
  //beginn the serial connection to the pi
  Serial.begin(9600);
  //set the clock display to max brightness
  display.setBrightness(0xff);
  //enable the pull-up resistor for the display backlight push-button
  pinMode(DISPLAY_BACKLIGHT_BUTTON, INPUT_PULLUP);
  //set all output pins to output mode
  pinMode(SPEAKER_AMP, OUTPUT);
  pinMode(DISPLAY_BACKLIGHT, OUTPUT);
  //enable the display backlight by default
  digitalWrite(DISPLAY_BACKLIGHT, HIGH);
  //enable receiver on interrupt pin 0 (PIN 3 on Pro Micro; PIN 2 on Nano)
  rcSwitch.enableReceive(PIN_DATA);
}

void loop()
{
  //read the serial input
  while (Serial.available() > 0) {
    char inChar = Serial.read();
    if (inChar == ';') {
      //delay(10000);
      //Serial.println("Message received: " + inputText);
      handleInput(inputText);
      inputText = "";
    }
    else {
      inputText += inChar;
    }
  }
  
  //check for hits in the infra red sensor if the alarm is enabled
  if (alarmEnabled) {
    if (digitalRead(IR_RECEIVER) == LOW) {//IR-receiver sends low when hit
      //inform the pi_clock that the alarm was "shot"
      Serial.println("alarm_hit");
      delay(100);//wait for 100 ms for the pi_clock to answer
    }
  }

  //update the clock display brightness
  unsigned long currentTime = millis();
  if (currentTime < 10000) {
    //expect that the programm just stared or the counter reached an overflow (after about 50 days)
    lastBrightnessUpdateTime = 10000;
  }
  if (currentTime - lastBrightnessUpdateTime > 5000) {
    lastBrightnessUpdateTime = currentTime;
    updateDisplayBrightness();
  }

  //check whether the display backlight button is pressed
  int buttonValue = digitalRead(DISPLAY_BACKLIGHT_BUTTON);
  if (buttonValue == LOW) {//pull-up resistor: LOW -> button pressed
    displayBacklightOn = !displayBacklightOn;
    delay(300);
    if (displayBacklightOn) {
      digitalWrite(DISPLAY_BACKLIGHT, HIGH);
      ignoreNextDisplayTurnOff = true;
    }
    else {
      digitalWrite(DISPLAY_BACKLIGHT, LOW);
      ignoreNextDisplayTurnOff = false;
    }
  }

  //timer for the five seconds text (set back to clock time afterwards)
  if (currentTime < 10000) {
    //expect that the programm just stared or the counter reached an overflow (after about 50 days)
    fiveSecondsDisplayStartTime = 10000;
  }
  if (currentTime - fiveSecondsDisplayStartTime > 5000) {
    //set the displayed text back to the last known time
    display.showNumberDecEx(lastKnownClockTime, points, true);
  }
  
  //receive remote alarm settings from the rc-switch
  receiveRemoteAlarm();
}

void handleInput(String inputText) {
  char firstSign = inputText[0];
  switch (firstSign) {
    case 'C'://set clock
      setClockTime(inputText);
      break;
    case 'A'://set alarm enabled
      setAlarmEnabled(inputText);
      break;
    case 'T'://get temperature
      getTemperature();
      break;
    case 'H'://get humidity
      getHumidity();
      break;
    case 'S'://speaker amplifier
      setSpeakerAmplifierEnabled(inputText);
      break;
    case 'B'://display backlight
      setDisplayBacklight(inputText);
      break;
    case 'D'://display a text for 5 seconds
      displayForFiveSeconds(inputText);
      break;
  }
}

void receiveRemoteAlarm() {
  if (rcSwitch.available()) {
    int received = rcSwitch.getReceivedValue();
    int bitLength = rcSwitch.getReceivedBitlength();
    
    //print the raw data
    /*Serial.write("Received: ");
    Serial.write(received);
    Serial.write(" Bit-Length: ");
    Serial.write(bitLength);
    Serial.write("\n");*/
    
    if (bitLength == SEND_BITS) {
      if (received == SEND_CODE_SHOW_NEXT) {
        //Serial.write("Received SHOW_NEXT code\n");
        Serial.println(REMOTE_ALARM_CODE_SHOW_NEXT_ALARM);
      }
      if (received == SEND_CODE_DELETE_ALL) {
        //Serial.write("Received DELETE_ALL code\n");
        Serial.println(REMOTE_ALARM_CODE_DELETE_ALL);
      }
      if (received >= SEND_CODE_SET_ALARM) {
        int receivedClockCode = received - SEND_CODE_SET_ALARM;
        char cstr[16];
        char* timeCode = itoa(receivedClockCode, cstr, 10);
        /*Serial.write("Received SET_ALARM code with clock code: ");
        Serial.write(s);
        Serial.write("\n");*/
        Serial.write(REMOTE_ALARM_CODE_SET_ALARM);
        Serial.println(timeCode);
      }
    }
    
    delay(250);//prevents repetitions
    
    //reset the receiver
    rcSwitch.resetAvailable();
  }
}

void setClockTime(String inputText) {
  //input form: "C HHMM"
  
  String inputNumber = "";
  for (int i = 2; i < 6; i++) {
    inputNumber += inputText[i];
  }
  int clockTime = inputNumber.toInt();
  lastKnownClockTime = clockTime;
  
  display.showNumberDecEx(clockTime, points, true);
}

void setAlarmEnabled(String inputText) {
  //input form: "A [0/1]" (where 1 is enabled and 0 is disabled)

  String inputStateText = "";
  inputStateText += inputText[2];
  int state = inputStateText.toInt();

  if (state == 1) {
    //enable the alarm switch and the LED
    alarmEnabled = true;
    digitalWrite(LED, HIGH);
  }
  else {
    //disable the alarm switch and the LED
    alarmEnabled = false;
    digitalWrite(LED, LOW);
  }
}

void setSpeakerAmplifierEnabled(String inputText) {
  //input form: "S [0/1]" (where 1 is enabled and 0 is disabled)
  
  String inputStateText = "";
  inputStateText += inputText[2];
  int state = inputStateText.toInt();
  
  if (state == 1) {
    //enable the speaker amplifier
    digitalWrite(SPEAKER_AMP, HIGH);
  }
  else {
    //disable the speaker amplifier
    digitalWrite(SPEAKER_AMP, LOW);
  }
}

//the display brightness is also set by a button
void setDisplayBacklight(String inputText) {
  //input form: "B [0/1]" (where 1 is backlight ON and 0 is OFF)

  String inputStateText = "";
  inputStateText += inputText[2];
  int state = inputStateText.toInt();
  
  if (state == 1) {
    //turn the display backlight ON
    digitalWrite(DISPLAY_BACKLIGHT, HIGH);
    displayBacklightOn = true;
  }
  else {
    if (ignoreNextDisplayTurnOff) {
      ignoreNextDisplayTurnOff = false;
    }
    else {
      //turn the display backlight OFF
      digitalWrite(DISPLAY_BACKLIGHT, LOW);
      displayBacklightOn = false;
    }
  }
}

void getTemperature() {
  //just send 42 till the method is implemented
  Serial.println("42");
  //TODO
}

void getHumidity() {
  //just send 42 till the method is implemented
  Serial.println("42");
  //TODO
}

void updateDisplayBrightness() {
  int photoResistorValue = analogRead(PHOTO_RESISTOR);
  float photoResistorVoltage = photoResistorValue * (5.0 / 1023.0);
  //value of the photoresistor (in ohm): R_p = U_in / U_a0 * R_1 - R_1 (U_in is 5V; R_1 was chosen to be 10K Ohm)
  float resistorValue = 5.0 / photoResistorVoltage * 10000 - 10000;

  if (resistorValue < 3000) {
    //the photo resistor has a low value -> it's bright -> set to maximum brightness
    display.setBrightness(7);
  }
  else if (resistorValue < 5000) {
    display.setBrightness(6);
  }
  else if (resistorValue < 7000) {
    display.setBrightness(5);
  }
  else if (resistorValue < 10000) {
    display.setBrightness(4);
  }
  else if (resistorValue < 15000) {
    display.setBrightness(3);
  }
  else if (resistorValue < 30000) {
    display.setBrightness(2);
  }
  else if (resistorValue < 60000) {
    display.setBrightness(1);
  }
  else {
    //realy big value -> it's very dark -> set to minimum brightness
    display.setBrightness(0);
  }
  display.showNumberDecEx(lastKnownClockTime, points, true);
}

void displayForFiveSeconds(String inputText) {
  //input form:   "D HHMM"
  //alternative:  "D NONE"
  
  //store the last known time because it will be set to a different time for 5 seconds
  int prevLastKnownTime = lastKnownClockTime;
  
  //check whether it's the text NONE that shall be displayed or a time
  if (inputText[2] == 'N') {
    display.setSegments(SEG_NONE);
    //display.showNumberDecEx(SEG_NONE, no_points, true);
  }
  else {
    setClockTime(inputText);
    //the last known time was overwritten by the setClockTime() method -> set it back
    lastKnownClockTime = prevLastKnownTime;
  }
  //set the counter for the five seconds display
  fiveSecondsDisplayStartTime = millis();
  //also reset the brightness update time because that causes the last known time to reapear
  lastBrightnessUpdateTime = millis();
}

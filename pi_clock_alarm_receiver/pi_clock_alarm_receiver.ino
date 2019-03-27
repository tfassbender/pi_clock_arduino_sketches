#include <RCSwitch.h>

RCSwitch rcSwitch = RCSwitch();

#define SEND_CODE_SET_ALARM 42//+ set time
#define SEND_CODE_SHOW_NEXT 33
#define SEND_CODE_DELETE_ALL 31
#define SEND_BITS 16
#define PIN_DATA 0//interrupt 0 (PIN 3 on Pro Micro; PIN 2 on Nano)

void setup() {
  //serial connection for printing the received answers
  Serial.begin(9600);
  
  //enable receiver on interrupt pin 0 (PIN 3 on Pro Micro; PIN 2 on Nano)
  rcSwitch.enableReceive(PIN_DATA);
}
 
void loop() {
  if (rcSwitch.available()){
    int received = rcSwitch.getReceivedValue();
    int bitLength = rcSwitch.getReceivedBitlength();
    
    //print the raw data
    Serial.write("Received: ");
    Serial.write(received);
    Serial.write(" Bit-Length: ");
    Serial.write(bitLength);
    Serial.write("\n");
    
    if (bitLength == SEND_BITS) {
      if (received == SEND_CODE_SHOW_NEXT) {
        Serial.write("Received SHOW_NEXT code\n");
        //TODO do something usefull now
      }
      if (received == SEND_CODE_DELETE_ALL) {
        Serial.write("Received DELETE_ALL code\n");
        //TODO do something usefull now
      }
      if (received >= SEND_CODE_SET_ALARM) {
        int receivedClockCode = received - SEND_CODE_SET_ALARM;
        Serial.write("Received SET_ALARM code with clock code: ");
        char cstr[16];
        char* s = itoa(receivedClockCode, cstr, 10);
        Serial.write(s);
        Serial.write("\n");
        //TODO set the alarm
      }
    }
    
    delay(250);//prevents repetitions
    
    //reset the receiver
    rcSwitch.resetAvailable();
  }
}

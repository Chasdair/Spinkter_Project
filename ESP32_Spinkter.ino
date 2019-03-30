
/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE" 
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second. 
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "EEPROM.h"

#define EEPROM_SIZE 1

BLECharacteristic *pCharacteristic;

bool deviceConnected = false;
bool motorLock;
bool pressFlag = 0;
float txValue = 0;
const int FSRPin = 36; 
const int LED = 2; // Could be different depending on the dev board. I used the DOIT ESP32 dev board.
String incomingByte = ""; 
const int stepPin = 25; //3 brown
const int dirPin = 26; //4 white
const int distPin = 14;
const int myDelay = 300;
const int buttonPin = 19;
int steps = 150;

int senderFlag = 0;
int hallValue;
int buttonState;
int forceSensorValue;
int forceSensorRead;
int buttonValue;
char distValBuf [1];
char forceSensorBuf [4];
int buttonCounter = 0;
int prevCount = 0;
int fixFlag = 0;

int idx = 0;
HardwareSerial bt(2);

//std::string rxValue; // Could also make this a global var to access it in loop()

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


void sendData(){

    if (bt.available() > 0) {
        // read the incoming byte:
        incomingByte = bt.readString();

        Serial.print("saturaion test output:  ");
        Serial.println(incomingByte);

    }
    // Fabricate some arbitrary junk for now...
    forceSensorRead = analogRead(FSRPin); // This could be an actual sensor reading!

    hallValue = digitalRead(distPin);
    Serial.print("Hall Value: ");
    Serial.println(hallValue);
    

    // Let's convert the value to a char array:
    char txString[20]; // make sure this is big enuffz
    //dtostrf((float)incomingByte, 1, 2, txString); // float_val, min_width, digits_after_decimal, char_buffer
    
    forceSensorValue = map(forceSensorRead, 0, 1023, 0, 100);
    Serial.print("FSR Value: ");
    Serial.println(forceSensorValue);
     
    sprintf (forceSensorBuf, "%03i", forceSensorValue);
    sprintf (distValBuf, "%01i", hallValue);

    incomingByte.toCharArray(txString,incomingByte.indexOf('%')+1);
    strcat(txString,"$");
    strcat(txString,forceSensorBuf);
    strcat(txString,"$");
    strcat(txString,distValBuf);

    
    pCharacteristic->setValue(txString);
    
    if (deviceConnected){
      pCharacteristic->notify(); // Send the value to the app!
    }

    Serial.print("*** Sent Value: ");
    Serial.println(incomingByte);
    Serial.println(txString);
    Serial.println(" ***");

    strcpy(txString,"");
  
}

void press_unpress(){
  
  if (motorLock == 0){
    pressBladder();
  }
  else{
    unpressBladder();
    motorLock=!motorLock; //create toggle
    fixFlag = 0;
    pressFlag = 0;
  }
  
  EEPROM.write(0, motorLock);
  EEPROM.commit();
  Serial.print("press/unpress");
  Serial.println(motorLock);
  
}


void pressBladder(){

    

  if((forceSensorValue<150) || (forceSensorValue>200)){
    
    Serial.print("Number of steps: ");
    Serial.println(steps);
    Serial.println(forceSensorValue);
    
    if (forceSensorValue<150){ //give steps to press the bladder
        digitalWrite(dirPin,LOW); 
    }
    else if (forceSensorValue>200){ //over pressing! need step back from bladder
        digitalWrite(dirPin,HIGH); 
        steps = steps/2;
    }
    for(int x = 0; x < steps; x++) {
        digitalWrite(stepPin,HIGH); 
        delayMicroseconds(myDelay); 
        digitalWrite(stepPin,LOW); 
        delayMicroseconds(800); 
    }
    if (steps == 0) {
      steps = 150;
      unpressBladder();
    }
  } else {
    motorLock=!motorLock; //create toggle
    pressFlag = 0;
    fixFlag = 1;
    steps = 150;
  }
  
}

void unpressBladder(){

  digitalWrite(dirPin,HIGH);

  for(int x = 0; x < 2000; x++) {
    digitalWrite(stepPin,HIGH); 
    delayMicroseconds(myDelay); 
    digitalWrite(stepPin,LOW); 
    delayMicroseconds(800); 
  }
  
}


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }

};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");

        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        }

        Serial.println();

        // Do stuff based on the command received from the app
        if (rxValue.find("A") != -1) { 
          pressFlag = 1;
          
        }
        Serial.println();
        Serial.println("*********");
      }
    }
};

void setup() {
  Serial.begin(115200);

  pinMode(LED, OUTPUT);
  pinMode(stepPin,OUTPUT); 
  pinMode(dirPin,OUTPUT);
  pinMode(distPin,INPUT);
  pinMode(buttonPin,INPUT_PULLUP);
  bt.begin(115200, SERIAL_8N1, 27,17);


  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("failed to initialise EEPROM");
  }
  
  motorLock = byte(EEPROM.read(0));
  
  // Create the BLE Device
  BLEDevice::init("ESP32_URINE"); // Give it a name

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
                      
  pCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {

  buttonState = digitalRead(buttonPin);
  Serial.print("this is button state: ");
  Serial.println(buttonState);
  if (!buttonState){
    buttonCounter++;
  }
  if (buttonState && (buttonCounter>prevCount)) {
    pressFlag = !pressFlag;
    prevCount = buttonCounter;
  }
  

  sendData();
  if (pressFlag) {
    press_unpress();
  } 
  else if (fixFlag && ((forceSensorValue<150) || (forceSensorValue>200))) {
    steps = 10;
    if (forceSensorValue<150) { //give some steps to press the bladder
        digitalWrite(dirPin,LOW); 
    }
    else if (forceSensorValue>200){ //over pressing! need step back from bladder
        digitalWrite(dirPin,HIGH); 
    }
    for(int x = 0; x < steps; x++) {
        digitalWrite(stepPin,HIGH); 
        delayMicroseconds(myDelay); 
        digitalWrite(stepPin,LOW); 
        delayMicroseconds(800); 
    }
  }


  
  delay(100);
}

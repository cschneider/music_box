#include "Arduino.h"
#include <Streaming.h>
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <avr/sleep.h>
#include <OneButton.h>

#define PLAY_PIN 3
#define NEXT_PIN 2
#define PREV_PIN 4
#define SERIAL_RX_PIN 9
#define SERIAL_TX_PIN 10

#define POWER_EXT A3

#define PULLUP true
#define SLEEP_AFTER_MS 300000
 
TwoWire wire;
PN532_I2C nfci2c = PN532_I2C(wire);
PN532 nfc = PN532(nfci2c);

SoftwareSerial mySoftwareSerial(SERIAL_RX_PIN, SERIAL_TX_PIN);
DFRobotDFPlayerMini myDFPlayer;

const int maxCards = 22;
char *cards[maxCards];
char *cards2[maxCards];
int currentFolder = -1;
int pin2_interrupt_flag = 0;
boolean playPause = true; // Play
long pauseStart = 0;
int volume = 15;

OneButton playB(2, PULLUP);
OneButton nextB(3, PULLUP);
OneButton prevB(4, PULLUP);

// -------------------------------------------------------------------
// Init
// -------------------------------------------------------------------

void setup(void) {
  pinMode(PLAY_PIN, INPUT_PULLUP);
  playB.attachClick(handlePlayPause);
  nextB.attachClick(handleNext);
  nextB.attachDuringLongPress(volumeUp);
  prevB.attachClick(handlePrev);
  prevB.attachDuringLongPress(volumeDown);
  Serial.begin(115200);
  powerExt(true);
  delay(500); // Wait for external devices to be ready
  initCards();
  initNfc();
  initDfPlayer();
  Serial.println("Muscicbox ready");
}

void initCards() {
  cards[0]="341D04C5";
  cards[1]="B41C04C5";
  cards[2]="052304C5";
  cards[3]="EB3F04C5";
  cards[4]="DA2004C5";
  cards[5]="00D45FA8";
  cards[6]="0680ED45";
  cards[7]="51A904C5";
  cards[8]="D01C04C5";
  cards[9]="B6B4ED45";
  cards[10]="A6E4E345";
  cards[11]="36F4E745";
  cards[12]="2669E245";
  cards[13]="264CE845";
  cards[14]="7B4A04C5";
  cards[15]="6651E545";
  cards[16]="86D0E345";
  cards[17]="46C2EB45";
  cards[18]="E6939145";
  cards[19]="BE1204C5";
  cards[20]="B2D404C5";
  cards[21]="EAED04C5";

  cards2[0]="42A77707";
  cards2[1]="E3F4701E";
  cards2[2]="E2BD7F19";
  cards2[3]="1941D114";
  cards2[4]="D9641E15";
  cards2[5]="FAED04C5";
  cards2[6]="352304C5";
  cards2[7]="83D404C5";
  cards2[8]="6E5C04C5";
  cards2[9]="335404C5";
  cards2[10]="195404C5";
  cards2[11]="06EE04C5";
  cards2[12]="A936E64C";
  cards2[13]="49BAD414";
  cards2[14]="799C1815";
  cards2[15]="8982E714";
  cards2[16]="C9B72B15";
  cards2[17]="19EB4700";
  cards2[18]="79D0F214";
  cards2[19]="E9071415";
  cards2[20]="8922D114";
  cards2[21]="19634800";
  
}

void initNfc() {
  Serial.println("Initializing card reader.");
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.println();
  nfc.setPassiveActivationRetries(0x01);
  nfc.SAMConfig();
}

void initDfPlayer() {
  wire.begin();
  mySoftwareSerial.begin(9600);
  Serial.println("Initializing DFPlayer.");
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println("Error communicating with dfplayer");
  } else {
    Serial.println("DFPlayer Mini online.");
  }
  myDFPlayer.volume(volume);
  myDFPlayer.setTimeOut(500); 
}

// -------------------------------------------------------------------
// Loop
// -------------------------------------------------------------------

void loop(void) {
  handleCard();
  handleSerial();
  playB.tick();
  nextB.tick();
  prevB.tick();
  
  if (myDFPlayer.available()) {
    printDetail(myDFPlayer.readType(), myDFPlayer.read()); 
  }
  if (playPause == false) {
    long elapsed = millis() - pauseStart;
    if (elapsed > SLEEP_AFTER_MS) {
      enter_sleep();
    }
  }
  
  //delay(10);
}

void handleSerial() {
  char rc;
  String inSt;
  while (Serial.available() > 0) {
    rc = Serial.read();
    if (rc == '\n') {
      handleInput(inSt);
      inSt = "";
    } else {
      inSt += rc;
    }
  }
}

void handleInput(String inSt) {
  if (inSt.equals("s")) {
    enter_sleep();
  } else {
    int folderNum = inSt.toInt();
    if (folderNum >0) {
      playFolder(folderNum);
    }
  }
}

void handlePlayPause() {
  playPause = !playPause;
  if (playPause) {
    Serial.println("Play");
    myDFPlayer.start();
  } else {
    Serial.println("Pause");
    myDFPlayer.pause();
    pauseStart = millis();
//    enter_sleep();
  }
}

void handleNext() {
  myDFPlayer.next();
  playPause = true;
  Serial.println(myDFPlayer.readCurrentFileNumber());
}

void handlePrev() {
  myDFPlayer.previous();
  playPause = true;
  Serial.println(myDFPlayer.readCurrentFileNumber());
}

void volumeUp() {
  Serial.println("volume up");
  myDFPlayer.volumeUp();
  delay(100);
}

void volumeDown() {
  Serial.println("volume down");
  myDFPlayer.volumeDown();
  delay(100);
}

void handleCard() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  char cardId[10] = "";
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (success) {
    array_to_string(uid, uidLength, cardId);
    Serial << "UID: " << cardId << endl;
    int folderNum = getIndex(cardId) + 1;
    if (folderNum > 0 && currentFolder != folderNum) {
      playFolder(folderNum);
    }
  }

}


void INT_PINisr() {
  detachInterrupt(0);
}

void enter_sleep() {
  Serial.println("sleep");
  powerExt(false);
  delay(200);
  attachInterrupt(0, INT_PINisr, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();

  // Waking up
  sleep_disable();
  Serial.println("wakeup");
  powerExt(true);
  delay(500);
  initNfc();
  playFolder(currentFolder);
}

void powerExt(boolean power) {
  pinMode(POWER_EXT, OUTPUT); 
  digitalWrite(POWER_EXT, power ? HIGH : LOW);
}


void playFolder(int folderNum) {
  currentFolder = folderNum;
  int filesInFolder = myDFPlayer.readFileCountsInFolder(currentFolder);
  Serial << "Playing folder " << folderNum << " with " << filesInFolder << " files." << endl;
  myDFPlayer.loopFolder(folderNum);
  playPause = true;
  Serial.println(myDFPlayer.readCurrentFileNumber());
}

// -------------------------------------------------------------------
// Generic util
// -------------------------------------------------------------------

void array_to_string(byte array[], unsigned int len, char buffer[]) {
    int c1 = 0;
    for (unsigned int i = 0; i < len; i++) {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[c1++] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[c1++] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[c1++] = '\0';
}

int getIndex(char *cardId) {
  for (unsigned int c = 0; c < maxCards; c++) {
    if (strncmp(cards[c], cardId, 10) == 0) {
      return c;
    }
  }
  for (unsigned int c = 0; c < maxCards; c++) {
    if (strncmp(cards2[c], cardId, 10) == 0) {
      return c;
    }
  }
  return -1;
}

//Print the detail message from DFPlayer to handle different errors and states.
void printDetail(uint8_t type, int value){
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerUSBInserted:
      Serial.println("USB Inserted!");
      break;
    case DFPlayerUSBRemoved:
      Serial.println("USB Removed!");
      break;
    case DFPlayerPlayFinished:
      Serial.print(F("Number:"));
      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}


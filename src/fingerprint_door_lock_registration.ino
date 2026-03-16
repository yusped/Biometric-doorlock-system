#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
SoftwareSerial mySerial(2, 3);
#else
#define mySerial Serial1
#endif

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Adjust the I2C address as needed

const int solenoidPin = 4;  // Pin to control the solenoid lock
const int ledPin = 13;      // Onboard LED pin
const int enrollButtonPin = 7; // Pin for enrollment button
const int accessButtonPin = 9; 

int enrollButtonPressCount = 0;
int accessButtonPressCount = 0;
unsigned long lastButtonPressTime = 0;  // Track time for debounce


uint8_t id;
bool lockOpened = false;  // Track if the lock is currently open

void setup() {
  pinMode(solenoidPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(enrollButtonPin, INPUT_PULLUP); // Set button pin as input with pull-up
pinMode(accessButtonPin,  INPUT_PULLUP);
  Serial.begin(9600);
  while (!Serial);
  delay(100);
  Serial.println("\n\nFingerprint Lock System");

  lcd.init();
  lcd.backlight();

  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
    lcd.setCursor(0, 0);
    lcd.print("Sensor: OK");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    lcd.setCursor(0, 0);
    lcd.print("Sensor: FAIL");
    while (1) { delay(1); }
  }

  finger.getTemplateCount();
  if (finger.templateCount == 0) {
    Serial.println("No templates found.");
    lcd.setCursor(0, 1);
    lcd.print("No templates");
  } else {
    Serial.print("Sensor contains "); Serial.print(finger.templateCount); Serial.println(" templates");
    lcd.setCursor(0, 1);
    lcd.print("Templates: ");
    lcd.print(finger.templateCount);
  }
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready for Use");
}
void loop() {
unsigned long currentMillis = millis();
  
  if (digitalRead(enrollButtonPin) == LOW) {
    if (currentMillis - lastButtonPressTime > 200) {  // Debounce delay
      enrollButtonPressCount++;
      lastButtonPressTime = currentMillis;
      Serial.println("Enroll button pressed");
    }
  }

  if (digitalRead(accessButtonPin) == LOW) {
    if (currentMillis - lastButtonPressTime > 200) {  // Debounce delay
      accessButtonPressCount++;
      lastButtonPressTime = currentMillis;
      Serial.println("Access button pressed");
    }
  }

  // Check if both buttons have been pressed twice
  if (enrollButtonPressCount >= 2 && accessButtonPressCount >= 2) {
    Serial.println("Enrollment combination activated!");
    if (getFingerprintID() != -1) {  // Check if a valid fingerprint is detected
      enrollNewFingerprint();
    } else {
      indicateError();  // Indicate error if fingerprint is not valid
    }

    // Reset the button press counts
    enrollButtonPressCount = 0;
    accessButtonPressCount = 0;
  }

  uint8_t fingerprintID = getFingerprintID();
  if (fingerprintID != -1 && !lockOpened) {
    unlockDoor();
    lockOpened = true;
  } else if (fingerprintID == -1 && lockOpened) {
    lockOpened = false;  // Reset state when the finger is removed
  }

  delay(500); // Small delay to avoid rapid re-triggering
}

void enrollNewFingerprint() {
  id = findAvailableID();  // Find the next available ID
  if (id == 0) {
    Serial.println("No available ID slots.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ID slots full!");
    delay(2000);
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enrolling ID: ");
  lcd.print(id);

  enrollFingerprint();
}

uint8_t findAvailableID() {
  for (uint8_t i = 1; i < 128; i++) {  // IDs range from 1 to 127
    if (finger.loadModel(i) != FINGERPRINT_OK) {
      return i;  // Return the first available ID
    }
  }
  return 0;  // Return 0 if no IDs are available
}

void enrollFingerprint() {
  Serial.print("Enrolling ID #");
  Serial.println(id);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enrolling ID: ");
  lcd.print(id);

  while (!getFingerprintEnroll());

  Serial.println("Fingerprint registered successfully!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enroll Success!");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready for Use");
}

uint8_t getFingerprintEnroll() {
  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  lcd.setCursor(0, 1);
  lcd.print("Waiting for finger");

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
      delay(100);
      continue;
    }
    if (p != FINGERPRINT_OK) {
      Serial.println("Error: Imaging error");
       lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("Imaging Error");
      delay(2000);
      return p;
    }
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error: Could not process the fingerprint image.");
    lcd.setCursor(0, 1);
     lcd.clear();
    lcd.print("Process Error");
    delay(2000);
    return p;
  }

  Serial.println("Remove finger");
  lcd.setCursor(0, 1);
   lcd.clear();
  lcd.print("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }

  Serial.println("Place the same finger again");
  lcd.setCursor(0, 1);
   lcd.clear();
  lcd.print("Place again");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    if (p != FINGERPRINT_OK) {
      Serial.println("Error: Imaging error");
      lcd.setCursor(0, 1);
      lcd.print("Imaging Error");
      delay(2000);
      return p;
    }
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error: Could not process the fingerprint image.");
    lcd.setCursor(0, 1);
     lcd.clear();
    lcd.print("Process Error");
    delay(2000);
    return p;
  }

  Serial.print("Creating model for #"); Serial.println(id);
  lcd.setCursor(0, 1);
   lcd.clear();
  lcd.print("Creating model");

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
    lcd.setCursor(0, 1);
     lcd.clear();
    lcd.print("Prints matched!");
  } else {
    Serial.println("Fingerprints did not match.");
    lcd.setCursor(0, 1);
     lcd.clear();
    lcd.print("Match Error");
    delay(2000);
    return p;
  }

  Serial.print("Storing model for ID "); Serial.println(id);
  lcd.setCursor(0, 1);
   lcd.clear();
  lcd.print("Storing model");

  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error: Could not store the fingerprint model.");
    lcd.setCursor(0, 1);
     lcd.clear();
    lcd.print("Store Error");
    delay(2000);
    return p;
  }

  return true;
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");
      return -1;  // Return -1 if no finger detected
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return -1;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return -1;
    default:
      Serial.println("Unknown error");
      return -1;
  }

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return -1;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return -1;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return -1;
    default:
      Serial.println("Unknown error");
      return -1;
  }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return -1;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    indicateError();
    return -1;

  } else {
    Serial.println("Unknown error");
    return -1;
  }

  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Access Granted");
  lcd.setCursor(0, 1);
  lcd.print("ID: ");
  lcd.print(finger.fingerID);
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready for Use");
  unlockDoor();
  return finger.fingerID;

}

void unlockDoor() {
  digitalWrite(solenoidPin, HIGH);  // Activate the solenoid to unlock the door
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Door Unlocked");
  
  // Keep the lock open for 5 seconds (adjustable)
  delay(5000); 
  
  digitalWrite(solenoidPin, LOW);   // Deactivate the solenoid to lock the door again
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Door Locked");
  
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready for Use");
}

void indicateError() {
  digitalWrite(ledPin, HIGH);
  delay(200);
  digitalWrite(ledPin, LOW);
  delay(200);
  digitalWrite(ledPin, HIGH);
  delay(200);
  digitalWrite(ledPin, LOW);
}

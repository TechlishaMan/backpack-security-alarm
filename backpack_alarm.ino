/*
  Backpack Security Alarm - Firmware
  Hardware: Arduino Uno, MPU6050 (I2C), DFPlayer Mini (UART), 74HC32/74HC08 Logic Gates

  Functionality:
  - Reads digital inputs from the logic gates (combined Hall sensors + Arm switch).
  - Reads motion data from MPU6050 via I2C.
  - Triggers DFPlayer audio and LED if the system is armed and a breach is detected.
*/

#include <SoftwareSerial.h>
#include <Wire.h>
#include <MPU6050.h>     
#include <DFPlayer_Mini_Mp3.h> 

// ---------- Pin Definitions ----------
#define LOGIC_TRIGGER_PIN  2   // Output from the 74HC08 AND gate (Armed + Breach)
#define ARMED_STATUS_PIN   3   // Optional: read the arm switch directly (used for status)
#define LED_PIN            13  // Built-in LED (or external)
#define DFPLAYER_TX_PIN    10  // Connect to DFPlayer RX (Software Serial)
#define DFPLAYER_RX_PIN    11  // Connect to DFPlayer TX (Software Serial)

// ---------- Global Objects ----------
SoftwareSerial mySoftwareSerial(DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
MPU6050 mpu;

// ---------- State Variables ----------
bool isArmed = false;
bool alarmTriggered = false;
unsigned long lastMotionCheck = 0;
const unsigned long motionCheckInterval = 100; // Check motion every 100ms

const int MOTION_THRESHOLD = 15000;

void setup() {
  // 1. Initialize Serial for debugging
  Serial.begin(9600);
  
  // 2. Initialize Pins
  pinMode(LOGIC_TRIGGER_PIN, INPUT);
  pinMode(ARMED_STATUS_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // 3. Initialize MPU6050 (I2C)
  Wire.begin();
  mpu.initialize();
  
  // 4. Initialize DFPlayer Mini (UART)
  mySoftwareSerial.begin(9600);
  mp3_set_serial(mySoftwareSerial);
  delay(500); // Give DFPlayer time to boot
  
  mp3_set_volume(20); // Set volume (0-30)
  Serial.println("System Ready. Waiting for arm signal.");
}

void loop() {
  // Step 1: Read the current arming state (from the slide switch + logic)
  // We read the digital output from the AND gate (which already includes the arm switch)
  int logicState = digitalRead(LOGIC_TRIGGER_PIN);
  int armSwitchState = digitalRead(ARMED_STATUS_PIN); // Optional direct read

  // Step 2: Determine if the system is armed
  if (armSwitchState == HIGH) {
    isArmed = true;
  } else {
    isArmed = false;
  }

  // Step 3: Check for Breach via Logic Gates (Hardware path)
  // If the logicState is HIGH, it means the AND gate saw the arm switch ON AND the OR gate saw a zipper open.
  if (isArmed && logicState == HIGH) {
    triggerAlarm("Zipper opened!");
    return; // Skip motion check to avoid duplicate triggers
  }

  // Step 4: Check for Breach via IMU (Software path)
  if (isArmed) {
    // Read accelerometer data
    int16_t ax, ay, az, gx, gy, gz;
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    
    // Calculate the magnitude of movement (squared sum to avoid sqrt)
    long accelMagnitude = (long)ax * ax + (long)ay * ay + (long)az * az;
    
    // If movement exceeds threshold, trigger
    if (accelMagnitude > MOTION_THRESHOLD) {
      triggerAlarm("Motion detected!");
    }
  }

  // Step 5: Safety check - if the system is disarmed, silence everything
  if (!isArmed && alarmTriggered) {
    resetAlarm();
  }

  delay(50); // Small delay to prevent flooding
}

// ---------- Helper Functions ----------
void triggerAlarm(String reason) {
  if (alarmTriggered) return; // Already triggered, don't repeat

  alarmTriggered = true;
  Serial.print("ALARM TRIGGERED: ");
  Serial.println(reason);
  
  // 1. Turn on LED
  digitalWrite(LED_PIN, HIGH);
  
  // 2. Play audio (Track 1 on the microSD card)
  mp3_play(1); // Play the first MP3 file on the SD card
  delay(1000); // Wait for DFPlayer to start playing
  
  // 3. Keep alarm on until disarmed
  while (digitalRead(ARMED_STATUS_PIN) == HIGH) {
    delay(200);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Toggle LED
  }
  
  resetAlarm();
}

void resetAlarm() {
  alarmTriggered = false;
  digitalWrite(LED_PIN, LOW);
  mp3_stop(); // Stop any playing audio
  Serial.println("Alarm reset. System disarmed.");
}

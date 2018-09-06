#include <HID.h>

#define SHIFTER_CLOCK_PIN  0
#define SHIFTER_DATA_PIN   1
#define SHIFTER_MODE_PIN   4
#define SHIFTER_X_PIN      8
#define SHIFTER_Y_PIN      9

// Pines de lectura
#define REVERSE_BUTTON 1
#define BUTTON_BLACK_UP 8
#define BUTTON_BLACK_RIGHT 9
#define BUTTON_BLACK_LEFT 10
#define BUTTON_BLACK_DOWN 11
#define BUTTON_RED_ONE 7
#define BUTTON_RED_TWO 5
#define BUTTON_RED_TREE 4
#define BUTTON_RED_FOUR 6
#define DPAD_RIGHT 12
#define DPAD_LEFT 13
#define DPAD_DOWN 14
#define DPAD_UP 15

#define SIGNAL_SETTLE_DELAY 10

#define G27_REPORT_ID 0x03
#define G27_STATE_SIZE 3

static const uint8_t _hidReportDescriptor[] PROGMEM = {
  // Joystick
  0x05, 0x01,           // USAGE_PAGE (Generic Desktop)
  0x09, 0x04,           // USAGE (Joystick)
  0xa1, 0x01,           // COLLECTION (Application)
  0x85, G27_REPORT_ID,  //   REPORT_ID (3)

  // 15 Buttons
  0x05, 0x09,           //   USAGE_PAGE (Button)
  0x19, 0x01,           //   USAGE_MINIMUM (Button 1)
  0x29, 0x0F,           //   USAGE_MAXIMUM (Button 15)
  0x15, 0x00,           //   LOGICAL_MINIMUM (0)
  0x25, 0x01,           //   LOGICAL_MAXIMUM (1)
  0x75, 0x01,           //   REPORT_SIZE (1)
  0x95, 0x10,           //   REPORT_COUNT (16)
  0x55, 0x00,           //   UNIT_EXPONENT (0)
  0x65, 0x00,           //   UNIT (None)
  0x81, 0x02,           //   INPUT (Data,Var,Abs)
  
  // Hat Switch
  0x05, 0x01,           //   USAGE_PAGE (Generic Desktop)
  0x09, 0x39,           //   USAGE (Hat switch)
  0x09, 0x39,           //   USAGE (Hat switch)
  0x15, 0x01,           //   LOGICAL_MINIMUM (1)
  0x25, 0x08,           //   LOGICAL_MAXIMUM (8)
  0x95, 0x02,           //   REPORT_COUNT (2)
  0x75, 0x04,           //   REPORT_SIZE (4)
  0x81, 0x02,           //   INPUT (Data,Var,Abs)
  
  0xc0                  // END_COLLECTION
};

typedef struct {
  uint8_t gear;
  uint8_t buttons;
  uint8_t dpad;
} ReportData;

ReportData data;

bool canReverse = false;

void setup() {
  Serial.begin(38400);

  // Setup HID report structure
  static HIDSubDescriptor node(_hidReportDescriptor, sizeof(_hidReportDescriptor));
  HID().AppendDescriptor(&node);
  
  pinMode(SHIFTER_MODE_PIN, OUTPUT);
  pinMode(SHIFTER_CLOCK_PIN, OUTPUT);

  digitalWrite(SHIFTER_MODE_PIN, HIGH);
  digitalWrite(SHIFTER_CLOCK_PIN, HIGH);

  // Inicializo los estados
  data.gear = 0;
  data.buttons = 0;
  data.dpad = 0;
}

void loop() {
  setGearStatus();
  setButtonsStatus();
  
  HID().SendReport(G27_REPORT_ID, &data, G27_STATE_SIZE);
}

void setButtonsStatus() {
  bool dpadUp = false;
  bool dpadRight = false;
  bool dpadDown = false;
  bool dpadLeft = false;
  
  data.dpad = 0;
  
  digitalWrite(SHIFTER_MODE_PIN, LOW);    // Switch to parallel mode: digital inputs are read into shift register
  waitForSignalToSettle();
  digitalWrite(SHIFTER_MODE_PIN, HIGH);   // Switch to serial mode: one data bit is output on each clock falling edge
  
  for(int i = 0; i < 16; ++i) {           // Iteration over both 8 bit registers
    digitalWrite(SHIFTER_CLOCK_PIN, LOW);         // Generate clock falling edge
    waitForSignalToSettle();
    
    int value = digitalRead(SHIFTER_DATA_PIN);
    
    switch(i) {
      case REVERSE_BUTTON:
        canReverse = value;
        break;
      case DPAD_UP:
        if(value) dpadUp = true;
        break;
      case DPAD_RIGHT:
        if(value) dpadRight = true;
        break;
      case DPAD_DOWN:
        if(value) dpadDown = true;
        break;
      case DPAD_LEFT:
        if(value) dpadLeft = true;
        break;
      case BUTTON_RED_ONE:
        // Uso el ultimo bit que corresponde al shifter
        setBit(data.gear, 7, value);
        break;
      case BUTTON_RED_TWO:
        setButton(0, value);
        break;
      case BUTTON_RED_TREE:
        setButton(1, value);
        break;
      case BUTTON_RED_FOUR:
        setButton(2, value);
        break;
      case BUTTON_BLACK_UP:
        setButton(3, value);
        break;
      case BUTTON_BLACK_RIGHT:
        setButton(4, value);
        break;
      case BUTTON_BLACK_DOWN:
        setButton(5, value);
        break;
      case BUTTON_BLACK_LEFT:
        setButton(6, value);
        break;
    }
    
    if(dpadUp && dpadRight)
      data.dpad = 2;
    else if(dpadRight && dpadDown)
      data.dpad = 4;
    else if(dpadDown && dpadLeft)
      data.dpad = 6;
    else if(dpadUp && dpadLeft)
      data.dpad = 8;
    else if(dpadUp)
      data.dpad = 1;
    else if(dpadRight)
      data.dpad = 3;
    else if(dpadDown)
      data.dpad = 5;
    else if(dpadLeft)
      data.dpad = 7;
    
    digitalWrite(SHIFTER_CLOCK_PIN, HIGH);        // Generate clock rising edge
    waitForSignalToSettle();
  }
}

void setGearStatus() {
  int posX = analogRead(SHIFTER_X_PIN);
  int posY = analogRead(SHIFTER_Y_PIN);

  bool can12 = false;
  bool can56 = false;
  bool can135 = false;
  bool can246 = false;

  if(posX < 300) can12 = true;
  if(posX > 650) can56 = true;
  
  if(posY > 800) can135 = true;
  if(posY < 220) can246 = true;

  data.gear = 0;
  
  if(!canReverse) {
    if(can12 && can135) setBit(data.gear, 0, HIGH); // 1
    if(can12 && can246) setBit(data.gear, 1, HIGH); // 2
    if(!can12 && !can56 && can135) setBit(data.gear, 2, HIGH); // 3
    if(!can12 && !can56 && can246) setBit(data.gear, 3, HIGH); // 4
    if(can56 && can135) setBit(data.gear, 4, HIGH); // 5
    if(can56 && can246) setBit(data.gear, 5, HIGH); // 6
  } else {
    if(can56 && can246) setBit(data.gear, 6, HIGH); // Reverse
  }
}

// Metodos de ayuda

void waitForSignalToSettle() {
  delayMicroseconds(SIGNAL_SETTLE_DELAY);
}

void setButton(int index, int value) {
  setBit(data.buttons, index, value);
}

void setBit(uint8_t &b, int index, int value) {
  if(value == HIGH) bitSet(b, index);
  if(value == LOW) bitClear(b, index);
}


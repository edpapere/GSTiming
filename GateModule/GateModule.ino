/*
        MCU    MCU      Nano    My
        pin    func     pin    func
        ----------------------------- 
             |         |    |        |
        PD5  | PD5     |  5 |  BTN.2 |>--------> BUTTON 2 (SET AS FINISH GATE BEACON)
        PD4  | PD4     |  4 |  BTN.1 |>--------> BUTTON 1 (SET AS START GATE BEACON)
             |         |    |        | 
        PD3  | OC2B    |  3 | IR.AND |>--------> IR LED ANODE @ 38 kHz
        PD1  | TXD     | TX | IR.CAT |>--------> IR LED CATHODE @ TX
             |         |    |        | 
        PC0  | PC0     | A0 |  G.LED |>--------> START GATE BEACON MODE INDICATOR  -- GREEN LED CATHODE
        PC1  | PC1     | A1 |  Y.LED |>--------> ACTIVE STATUS INDICATOR           -- YELLOW LED CATHODE
        PC2  | PC2     | A2 |  R.LED |>--------> FINISH GATE BEACON MODE INDICATOR -- RED LED CATHODE
             |         |    |        | 
        PC3  | PC3     | A3 |  M.BTN |>--------> GATE BEACON MODE BUTTON -- 
             |         |    |        | 
        PB1  | PB1     |  9 | RF.RST |---------- RFID-RC522 RST
        PB2  | nSS     | 10 |    SDA |---------- RFID-RC522 SDA
        PB3  | MOSI    | 11 |   MOSI |---------- RFID-RC522 MOSI
        PB4  | MISO    | 12 |   MISO |---------- RFID-RC522 MISO
        PB5  | SCK     | 13 |    SCK |---------- RFID-RC522 SCK
             |         |    |        | 
        ----------------------------- 
*/

//#define __USE_MFRC522__

#ifdef __USE_MFRC522__
#include <SPI.h>
#include <MFRC522.h>
#endif

#include <EEPROM.h>

// main CPU Frequency 8 / 16 / 20
#define CPU_FRQ_MHZ 16  

// serial port Frequency 
#define SERIAL_PORT_SPEED       4800


// IR Base Frequency output pin
// 3 -- OC2B (PD3)
// 11 -- OC2A (PB3)
#define IR_LED_PIN 3

//#define STATUS_LED_PIN          LED_BUILTIN
#define STATUS_LED_PIN          17  // A3 -- PC3

#define GATE_MODE_DIP_1_PIN     5  // D4 -- PD4
#define GATE_MODE_DIP_2_PIN     6  // D5 -- PD5
#define GATE_MODE_DIP_3_PIN     7  // D6 -- PD6
#define GATE_MODE_DIP_4_PIN     8  // D7 -- PD7

#define VOLTMETER_PIN           A7  // A7 -- A7
#define VOLTMETER_REF          3.3  // 
#define VOLTMETER_DIV          900  // 220+680 kOhm
#define VOLTMETER_MUL          220  // 220 kOhm
 

#define LEDS_ACTIVITY_TIMEOUT   10  // in loop() cycles // comment out thic line to disable this feature

// IR Base Frequency output ...
// PIN = 11 COM2A0
// PIN =  3 COM2B0
#if IR_LED_PIN == 11
  #define IR_LED_OC2x0  COM2A0  // Toggle OC2A in
#elif IR_LED_PIN == 3
  #define IR_LED_OC2x0  COM2B0  // Toggle OC2B in
#endif

// IR Frequency timer settings = f_cpu / ( 2 * f_ir ) - 1
// 20 MHz - 262: 20 MHz / ( 2 * 38 kHz ) - 1 = 263.158 - 1 ~ 263 => Compare Register=0.0000131500 => Frq=38.022 kHz (+0.022)
// 16 MHz - 210: 16 MHz / ( 2 * 38 kHz ) - 1 = 210.526 - 1 ~ 210 => Compare Register=0.0000131875 => Frq=37.915 kHz (-0.085)
//  8 MHz - 104: 8 MHz / ( 2 * 38 kHz ) - 1 = 105.263 - 1 ~ 104 => Compare Register=0.000013125 => Frq=38.095 kHz (+0.095)
#if CPU_FRQ_MHZ == 16
  #define TIMER_CYCLE 210
#elif CPU_FRQ_MHZ == 20
  #define TIMER_CYCLE 263
#else 
  // assume as 8mHz
  #define TIMER_CYCLE 104
#endif

#ifdef __USE_MFRC522__
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.
#endif

// Run-time global variables:

int iBeaconMode = 4; // finish by default
#define __BEACON_MODE_START   1
#define __BEACON_MODE_INT1    2
#define __BEACON_MODE_INT2    3
#define __BEACON_MODE_FINISH  4
int iBeaconModeAddress = 0;

#define BEACON_ID char[10]
char chBeaconId[10] = "A536C98D";
int iBeaconIdAddress = iBeaconModeAddress + sizeof(iBeaconMode);
String gateID;

#ifdef LEDS_ACTIVITY_TIMEOUT
int ledsActive;
#endif 

int     giSignalLength  = 3;
String  gsGateTp        = "UNKN";
String  gsGateSignal    = "UNKNUNKNUNKN";
String  gateCardUID = "";

String getGateIdStr()
{
  switch(iBeaconMode)
  {
    case __BEACON_MODE_START:
    gsGateTp="STRT";
    break;
    case __BEACON_MODE_INT1:
    gsGateTp="INT1"; 
    break;
    case __BEACON_MODE_INT2:
    gsGateTp="INT2";  
    break;
    case __BEACON_MODE_FINISH:
    gsGateTp="FFFF";//"FNSH";   
    break;
    default:
    gsGateTp="UNKN";   
    break;
  }
  gsGateSignal="";
  for(int i=0; i < giSignalLength; i++)
  {
    gsGateSignal.concat( gsGateTp );
  }
  return(gsGateTp);
}

int readConfig()
{
  // Get current configuration from EEPROM
  EEPROM.get(iBeaconModeAddress,iBeaconMode);
  EEPROM.get(iBeaconIdAddress,chBeaconId);
  gateID = String(chBeaconId);
  
  // Setup control buttons and read their state:
  pinMode(GATE_MODE_DIP_1_PIN, INPUT_PULLUP);
  pinMode(GATE_MODE_DIP_2_PIN, INPUT_PULLUP);
  pinMode(GATE_MODE_DIP_3_PIN, INPUT_PULLUP);
  pinMode(GATE_MODE_DIP_4_PIN, INPUT_PULLUP);
  
  int iDipPin1 = digitalRead(GATE_MODE_DIP_1_PIN);
  int iDipPin2 = digitalRead(GATE_MODE_DIP_2_PIN);
  int iDipPin3 = digitalRead(GATE_MODE_DIP_3_PIN);
  int iDipPin4 = digitalRead(GATE_MODE_DIP_4_PIN);

  // Set and save configuration according to combination of button pressed 
  if ( iDipPin1 == LOW )        // DIP #1 is on 
  {
    // Configure module as Start Gate Beacon 
    iBeaconMode = __BEACON_MODE_START;
  }
  else if ( iDipPin2 == LOW )   // DIP #2 is on
  {
    // Configure module as Finish Gate Beacon 
    iBeaconMode = __BEACON_MODE_INT1;
  }
  else if ( iDipPin3 == LOW )    // DIP #3 is on
  {
    // Configure module as Finish Gate Beacon and set default Beacon ID 
    iBeaconMode = __BEACON_MODE_INT2;
  }
  else if ( iDipPin4 == LOW )    // DIP #4 is on
  {
    // Configure module as Finish Gate Beacon and set default Beacon ID 
    iBeaconMode = __BEACON_MODE_FINISH;
  }
  EEPROM.put(iBeaconModeAddress,iBeaconMode);
  EEPROM.put(iBeaconIdAddress,chBeaconId);
  return(iBeaconMode);
}

void setupIRTransmitter()
{
  // Setup IR output pin
  pinMode (IR_LED_PIN, OUTPUT);
  delay(100);
    
  // Configure 38kHz output to IR base rate output pin
  // http://forum.arduino.cc/index.php?topic=102430.msg769421#msg769421
  // set up Timer 2
  TCCR2A = _BV (IR_LED_OC2x0) | _BV(WGM21); // CTC, toggle register (OC2A or OC2B) on Compare Match
  TCCR2B = _BV (CS20); // No prescaler
  OCR2A = TIMER_CYCLE; // set compare A register value = (TIMER_CYCLE+1) * clock speed to get output ~38kHz
  delay(100);
}

// ====================================================================
// the setup function runs once when you press reset or power the board
// --------------------------------------------------------------------
void setup() 
{
  // --------------------------------
  readConfig(); // get gate mode
  getGateIdStr(); // retreave gate id string
    
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(STATUS_LED_PIN, OUTPUT);
  setupIRTransmitter();

  // Initialise UART 
  // configurate port
  Serial.begin(SERIAL_PORT_SPEED); // should be x2 of Computer serial speed
  delay(100);
}

// ====================================================================
// the loop function runs over and over again forever
// --------------------------------------------------------------------
void loop() 
{
  //int activityLED = digitalRead(STATUS_LED_PIN);
  //digitalWrite(STATUS_LED_PIN, activityLED != HIGH ? HIGH : LOW);

  //digitalWrite(GATE_MODE_PIN_START,  iBeaconMode == __BEACON_MODE_START);
  //digitalWrite(GATE_MODE_PIN_FINISH, iBeaconMode == __BEACON_MODE_FINISH);

  // 1010 0101 0011 0110 1100 1001 1000 1101 
  // $PGSTB -- Giant Slalom Timing System Gate Beacon
  // $PGSTB,<1>,<2>,<3>,<4>,<5>,<6>*<hh>
  // <1> -- Gate Type: START, FINISH, FIRST, SECOND, THIRD Intermediary 
  // <2> -- Gate ID: string of 8 hexadecimal digits
  // <3> -- RFID Card UID of current participant: string of 8 or 16 hexadecimal digits
  // <4> -- Battery voltage X.XX
  // <5> -- Local air pressure
  // <6> -- local millisecons
  // <hh> -- Checksum (two hexadecimal digits)

#ifdef __USE_MFRC522__
  // Read RFID card
  if ( mfrc522.PICC_IsNewCardPresent() ) 
  {
    if ( mfrc522.PICC_ReadCardSerial() ) 
    {
      gateCardUID = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) 
      {
        gateCardUID.concat( String( (mfrc522.uid.uidByte[i] & 0xF0 ) >> 4, HEX) );
        gateCardUID.concat( String( (mfrc522.uid.uidByte[i] & 0x0F ), HEX) );
      }
    }
  }
#endif

  // send signal
  //for(int i=0; i < 100; i++) 
  { 
    Serial.println(gsGateSignal);
    Serial.flush();
    //delay(5);
  }
  delay(1000);
}

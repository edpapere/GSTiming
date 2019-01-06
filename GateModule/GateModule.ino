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

#ifdef F_CPU
  #if F_CPU == 16000000L        //  16 MHz
//  #define TIMER_CYCLE   209   //  f_cpu / ( 2 * f_ir ) - 1 = 16 MHz / ( 2 * 38 kHz ) - 1 = 210.526 - 1 ~ 209 => 38.095 kHz (+0.095)
    #define TIMER_CYCLE   210   //  f_cpu / ( 2 * f_ir ) - 1 = 16 MHz / ( 2 * 38 kHz ) - 1 = 210.526 - 1 ~ 210 => 37.915 kHz (-0.085)
  #elif F_CPU == 8000000L       //  8 MHz
    #define TIMER_CYCLE   104   //  f_cpu / ( 2 * f_ir ) - 1 =  8 MHz / ( 2 * 38 kHz ) - 1 = 105.263 - 1 ~ 104 => 38.095 kHz (+0.095)
  #else
    #error "Unsupported F_CPU macro value"
  #endif
#else
  #error "F_CPU macro is not defined"
#endif





#ifdef __USE_MFRC522__
#include <SPI.h>
#include <MFRC522.h>
#endif

#include <EEPROM.h>

//#define STATUS_LED_PIN          LED_BUILTIN
#define STATUS_LED_PIN          15  // A1 -- PC1

#define IR_LED_PIN               3  //  3 -- OC2B (PD3) // 11 -- OC2A (PB3)

#define GATE_MODE_PIN_START     14  // A0 -- PC0
#define GATE_MODE_PIN_FINISH    16  // A2 -- PC2
#define GATE_MODE_PIN_INTERIM1  15  // A1 -- PC1

#define BUTTON_1_PIN             4  // D4 -- PD4
#define BUTTON_2_PIN             5  // D5 -- PD5

#define VOLTMETER_PIN           A7  // A7 -- A7
#define VOLTMETER_REF          3.3  // 
#define VOLTMETER_DIV            3  // 

#define LEDS_ACTIVITY_TIMEOUT   10  // in loop() cycles // comment out thic line to disable this feature

#if IR_LED_PIN == 11
  #define IR_LED_OC2x0  COM2A0  // Toggle OC2B in
#elif IR_LED_PIN == 3
  #define IR_LED_OC2x0  COM2B0  // Toggle OC2B in
#endif

#ifdef __USE_MFRC522__
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.
#endif

// Run-time global variables:

int beaconMode;
#define __BEACON_MODE_START   1
#define __BEACON_MODE_FINISH  2
int beaconModeAddress = 0;

#define BEACON_ID char[10]
char beaconId[10] = "A536C98D";
int beaconIdAddress = beaconModeAddress + sizeof(beaconMode);
String gateID;

#ifdef LEDS_ACTIVITY_TIMEOUT
int ledsActive;
#endif 

// ====================================================================
// the setup function runs once when you press reset or power the board
// --------------------------------------------------------------------
void setup() 
{
  
  // Setup control buttons and read their state:
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  int button1 = digitalRead(BUTTON_1_PIN);
  int button2 = digitalRead(BUTTON_2_PIN);

  // Set and save configuration according to combination of button pressed 
  if ( button1 == LOW && button2 == HIGH )        // only button #1 is pressed 
  {
    // Configure module as Start Gate Beacon 
    beaconMode = __BEACON_MODE_START;
    EEPROM.put(beaconModeAddress,beaconMode);
  }
  else if ( button1 == HIGH && button2 == LOW )   // only button #2 is pressed
  {
    // Configure module as Finish Gate Beacon 
    beaconMode = __BEACON_MODE_FINISH;
    EEPROM.put(beaconModeAddress,beaconMode);
  }
  else if ( button1 == LOW && button2 == LOW )    // both buttons are pressed
  {
    // Configure module as Finish Gate Beacon and set default Beacon ID 
    beaconMode = __BEACON_MODE_FINISH;
    EEPROM.put(beaconModeAddress,beaconMode);
    EEPROM.put(beaconIdAddress,beaconId);
  }
  
  // Get current configuration from EEPROM
  EEPROM.get(beaconModeAddress,beaconMode);
  EEPROM.get(beaconIdAddress,beaconId);
  gateID = String(beaconId);

  // --------------------------------
  
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(STATUS_LED_PIN, OUTPUT);

#ifdef LEDS_ACTIVITY_TIMEOUT
  ledsActive = LEDS_ACTIVITY_TIMEOUT;
#endif 
  pinMode(GATE_MODE_PIN_START,    OUTPUT);
  pinMode(GATE_MODE_PIN_FINISH,   OUTPUT);
  pinMode(GATE_MODE_PIN_INTERIM1, OUTPUT);
  digitalWrite(GATE_MODE_PIN_START,    LOW);
  digitalWrite(GATE_MODE_PIN_FINISH,   HIGH);
  digitalWrite(GATE_MODE_PIN_INTERIM1, HIGH);
  delay(200);

  // Configure 38 kHz output
  // http://forum.arduino.cc/index.php?topic=102430.msg769421#msg769421
  // Setup IR output
  pinMode (IR_LED_PIN, OUTPUT);
  // set up Timer 2
  TCCR2A = _BV (IR_LED_OC2x0) | _BV(WGM21);   // CTC, toggle OC2A or OC2B on Compare Match
  TCCR2B = _BV (CS20);                        // No prescaler
  OCR2A =  TIMER_CYCLE;                       // compare A register value = f_cpu / ( 2 * f_ir ) - 1

  digitalWrite(GATE_MODE_PIN_START,    HIGH);
  digitalWrite(GATE_MODE_PIN_FINISH,   HIGH);
  digitalWrite(GATE_MODE_PIN_INTERIM1, LOW);
  delay(200);

  // Initialise UART 
  Serial.begin(4800);

  if ( button1 == LOW && button2 == HIGH )        // only button #1 is pressed 
  {
    Serial.println("only button #1 is pressed");
    Serial.flush();
  }
  else if ( button1 == HIGH && button2 == LOW )   // only button #2 is pressed
  {
    Serial.println("only button #2 is pressed");
    Serial.flush();
  }
  else if ( button1 == LOW && button2 == LOW )    // both buttons are pressed
  {
    Serial.println("both buttons are pressed");
    Serial.flush();
  }

  digitalWrite(GATE_MODE_PIN_START,    HIGH);
  digitalWrite(GATE_MODE_PIN_FINISH,   LOW);
  digitalWrite(GATE_MODE_PIN_INTERIM1, HIGH);
  delay(200);

  digitalWrite(GATE_MODE_PIN_START,    HIGH);
  digitalWrite(GATE_MODE_PIN_FINISH,   HIGH);
  digitalWrite(GATE_MODE_PIN_INTERIM1, HIGH);
  delay(200);

#ifdef __USE_MFRC522__
  SPI.begin();      // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
#endif

  digitalWrite(GATE_MODE_PIN_START,    LOW);
  digitalWrite(GATE_MODE_PIN_FINISH,   LOW);
  digitalWrite(GATE_MODE_PIN_INTERIM1, LOW);
  delay(200);

  digitalWrite(GATE_MODE_PIN_START,    HIGH);
  digitalWrite(GATE_MODE_PIN_FINISH,   HIGH);
  digitalWrite(GATE_MODE_PIN_INTERIM1, HIGH);
  delay(200);

}


// ====================================================================
// the loop function runs over and over again forever
// --------------------------------------------------------------------
String gateCardUID = "";
void loop() {

#ifdef LEDS_ACTIVITY_TIMEOUT
  if( ledsActive )
  {
    ledsActive--;
    if( ! ledsActive )
    {
      // turn LEDs off
      pinMode(GATE_MODE_PIN_START,    INPUT);
      pinMode(GATE_MODE_PIN_FINISH,   INPUT);
      pinMode(GATE_MODE_PIN_INTERIM1, INPUT);
    }
  }
  int button1 = digitalRead(BUTTON_1_PIN);
  int button2 = digitalRead(BUTTON_2_PIN);
  if( button1 == LOW || button2 == LOW )
  {
    if( ! ledsActive )
    {
      // turn LEDs on
      pinMode(GATE_MODE_PIN_START,    OUTPUT);
      pinMode(GATE_MODE_PIN_FINISH,   OUTPUT);
      pinMode(GATE_MODE_PIN_INTERIM1, OUTPUT);
    }
    ledsActive = LEDS_ACTIVITY_TIMEOUT;
  }
#endif 

  int activityLED = digitalRead(STATUS_LED_PIN);
  digitalWrite(STATUS_LED_PIN, activityLED != HIGH ? HIGH : LOW);

  digitalWrite(GATE_MODE_PIN_START,  beaconMode == __BEACON_MODE_START);
  digitalWrite(GATE_MODE_PIN_FINISH, beaconMode == __BEACON_MODE_FINISH);

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

  String gateStart  = "START";
  String gateFinish = "FINISH";

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

  float voltage = analogRead(VOLTMETER_PIN) * (VOLTMETER_REF / 1024.0) * (VOLTMETER_DIV);

  // Build NMEA-0183 message string
  String gateString = "$PGSTB,";
  gateString.concat( beaconMode == __BEACON_MODE_START ? gateStart : gateFinish );
  gateString.concat( "," );
  gateString.concat( gateID );
  gateString.concat( "," );
  gateString.concat( gateCardUID );
  gateString.concat( "," );
  gateString.concat( voltage );
  gateString.concat( "," );
  // gateString.concat( "0" );
#ifdef LEDS_ACTIVITY_TIMEOUT
  gateString.concat( ledsActive ); // temporary used to report LED activity count down
#endif
  gateString.concat( "," );
  gateString.concat( millis() );
  gateString.toUpperCase();
  
  // Append NMEA-0183 checksum
  char k = 0;
  for(int l=1; l < gateString.length(); l++)
  {
    k ^= gateString.charAt(l);
  }
  gateString.concat("*");
  gateString.concat( String( (k & 0xF0) >> 4, HEX) );
  gateString.concat( String( (k & 0x0F), HEX) );
  gateString.toUpperCase();

  String gateString2 = "$GSTA";
  for(int ix=0; ix < 5; ix++)
  {
    gateString2.concat( "," + ( beaconMode == __BEACON_MODE_START ? gateStart : gateFinish ) );
  }
  gateString2.toUpperCase();

  for(int i=0; i < 4; i++) 
  { 
    Serial.println(gateString2);
    Serial.flush();
  }
    Serial.println(gateString);
    Serial.flush();
  Serial.println("--==<>==--");
//  Serial.println("-------------------");
  Serial.flush();

  delay(50);

//    Serial.print( "#########$StartGate - " );
//    Serial.print( IntToStrLen(h,2) + ":" + IntToStrLen(m,2) + ":" + IntToStrLen(s,2) + "." + IntToStrLen(ms,3) );
//    Serial.println( ".........1.........2.........3.........4.........5.........6.........7.........8.........9.........A" );
//    Serial.println( "(2)......1.........2.........3.........4.........5.........6.........7.........8.........9.........A" );
//    Serial.println( "(3)......1.........2.........3.........4" );
//    Serial.println();
//    Serial.flush();
//    delay(50);                       // wait for a second

}

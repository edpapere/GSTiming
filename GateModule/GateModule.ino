/*
        MCU    MCU      Nano    My
        pin    func     pin    func
        ----------------------------- 
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
        PC7  | ADC7    | A7 |  BAT.V |>--------> BATERY VOLTMETER -------------------.
             |         |    |        |                                               |            
             |         |    RAW.V.IN | ------------------------------------[R=200K]--+--[R=100K]-----| GND
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

//#define STATUS_LED_PIN          LED_BUILTIN
#define STATUS_LED_PIN          15  // A1 -- PC1

#define IR_LED_PIN               3  //  3 -- OC2B (PD3) // 11 -- OC2A (PB3)

#define GATE_MODE_PIN_START     14  // A0 -- PC0
#define GATE_MODE_PIN_FINISH    16  // A2 -- PC2
#define GATE_MODE_PIN_INTERIM1  15  // A1 -- PC1

#define GATE_MODE_BUTTON_PIN    17  // A3 -- PC3

#define VOLTMETER_PIN           A7  // A7 -- PC7

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

// ====================================================================
// the setup function runs once when you press reset or power the board
// --------------------------------------------------------------------
void setup() 
{
  
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(STATUS_LED_PIN, OUTPUT);

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

  digitalWrite(GATE_MODE_PIN_START,    HIGH);
  digitalWrite(GATE_MODE_PIN_FINISH,   LOW);
  digitalWrite(GATE_MODE_PIN_INTERIM1, HIGH);
  delay(200);

  pinMode(GATE_MODE_BUTTON_PIN, INPUT);

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

  int activityLED = digitalRead(STATUS_LED_PIN);
  digitalWrite(STATUS_LED_PIN, activityLED != HIGH ? HIGH : LOW);

  int beaconMode = digitalRead(GATE_MODE_BUTTON_PIN);
  digitalWrite(GATE_MODE_PIN_START,  beaconMode == HIGH ? HIGH : LOW);
  digitalWrite(GATE_MODE_PIN_FINISH, beaconMode != HIGH ? HIGH : LOW);

  String gateID = "A536C98D";
  // 1010 0101 0011 0110 1100 1001 1000 1101 
  // $PGSTB -- Giant Slalom Timing System Gate Beacon
  // $PGSTB,<1>,<2>,<3>*<hh>
  // <1> -- Gate Type: START, FINISH, FIRST, SECOND, THIRD Intermediary 
  // <2> -- Gate ID: string of 8 hexadecimal digits
  // <3> -- RFID Card UID of current participant: string of 8 or 16 hexadecimal digits
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

  // Build NMEA-0183 message string
  String gateString = "$PGSTB,";
  gateString.concat( beaconMode == LOW ? gateStart : gateFinish );
  gateString.concat( "," );
  gateString.concat( gateID );
  gateString.concat( "," );
  gateString.concat( gateCardUID );
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
  
  for(int i=0; i < 5; i++) 
  { 
   // Serial.println("$STARTGATE,A53CF86D");
    Serial.println(gateString);
    Serial.flush();
  }
// ----------------------------------------------
  float voltage = analogRead(VOLTMETER_PIN) * (5.0 / 1023.0);  
  Serial.print("$PGSTV,");
  Serial.println(voltage);
  Serial.flush();
// ----------------------------------------------
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

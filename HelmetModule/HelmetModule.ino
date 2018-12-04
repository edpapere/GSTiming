/*
        MCU    MCU      Nano    My
        pin    func     pin    func
        ----------------------------- 
             |         |    |        |
        PD2  | INT0    |  2 | IR1.IN |<--------< IR SENSOR 1        (+)
        PD3  | INT1    |  3 | IR2.IN |<--------< IR SENSOR 2        (+)
             |         |    |        | 
        PD0  | RXD     | RX |  IR.RX |<--------< IR MUX OUT         (+)
        PD1  | TXD     | TX |  BT.TX |>--------> BT RXD             (+)
        PB0  | PB0     |  8 |  BT.RX |<--------< BT TXD             (-)
        PB1  | PB1     |  9 | AUX.TX |>--------> (not connected)    (-)
             |         |    |        |
        PD4  | PD4     |  4 | IR1.EN |>--------> Enable RX of IR SENSOR 1 / 1=Enamble
             |         |    |        |
        ----------------------------- 
             |         |    |        |
        PB2  | nSS     | 10 |     SS |---------- (not connected)    (-)
        PB3  | MOSI    | 11 |   MOSI |>--------> (not connected)    (-)
        PB4  | MISO    | 12 |   MISO |<--------< (not connected)    (-)
        PB5  | SCK     | 13 |    SCK |>--------> (not connected)    (-)
             |         |    |        |
        PC4  | SDA     | A4 |    SDA |---------- (not connected)    (-)
        PC5  | SCL     | A5 |    SCL |---------- (not connected)    (-)
             |         |    |        |
        PC0  | PC0     | A0 |  R.LED |>--------> RED LED
        PC2  | PC2     | A2 |  Y.LED |>--------> YELLOW LED
        PC4  | PC4     | A4 |  G.LED |>--------> GREEN LED
             |         |    |        |
        PC0  | PC0     | A0 |  G.LED |>--------> FINISH GATE BEACON MODE INDICATOR -- RED LED CATHODE
        PC1  | PC1     | A1 |  Y.LED |>--------> ACTIVE STATUS INDICATOR           -- YELLOW LED CATHODE
        PC2  | PC2     | A2 |  G.LED |>--------> START GATE BEACON MODE INDICATOR  -- GREEN LED CATHODE
             |         |    |        |
        ----------------------------- 

#include <SoftwareSerial.h>

SoftwareSerial mySerial1(8, 9); // RX, TX

*/

//
const int R_LED = 14;       // A0 - pin 14
const int Y_LED = 16;       // A2 - pin 16
const int G_LED = 18;       // A4 - pin 18

const int IR_SENSOR_1 = 2;      // D2 - (pin 2)
const int IR_SENSOR_2 = 3;      // D3 - (pin 3)
const int IR_1_EN     = 4;      // D4 - (pin 4)
const int IR_STATUS_1 = G_LED;  // A4 - pin 18
const int IR_STATUS_2 = Y_LED;  // A2 - pin 16

int isOutput = 0;

const int flagLogMax = 100;
struct {unsigned long ms; int flags;} flagLog [flagLogMax];
int flagLogBegin = 0;
int flagLogEnd = 0;

const int SYSLOGSIZE = 100;
#define SYSLOG_FLAG_IR_SENSOR_1 1
#define SYSLOG_FLAG_IR_SENSOR_2 2
#define SYSLOG_FLAG_LATCH_DELAY  5000
struct sysLogRecord {unsigned long ms; unsigned int flags;} __attribute__((packed));
struct sysLogRecord sysLog[SYSLOGSIZE];
int sysLogBegin = 0;
int sysLogEnd = 0;
int sysLogPreEnd = 0;
struct sysLogRecord sysLog_Sensor1;
struct sysLogRecord sysLog_Sensor2;



const int     nmeaInMsgBufLen = 128; 
char          nmeaInMsgBuf[nmeaInMsgBufLen];
int           nmeaInMsgLen = 0;   // Length of incomming message
char          nmeaInCrcVal = 0;   // NMEA checksum calculated from the message body (before '*')
int           nmeaInCrcChk = 0;   // NMEA checksum read from the NMEA message
int           nmeaInCrcPos = -1;  // Position of the checksum in the message (position of '*')


unsigned long last_ms = 0;
int           send_ms = 0;
String        k = "$PGSTC,";

int int_pause_length = 2000;
int int_message_frame = 1000;


/*
ISR (PCINT2_vect)
{
  digitalWrite(G_LED, PIND & _BV (IR_SENSOR_1) ? HIGH : LOW);
  digitalWrite(Y_LED, PIND & _BV (IR_SENSOR_2) ? HIGH : LOW);
}  // end of PCINT2_vect
*/


// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  // Setup IR receiver pins
  pinMode(IR_SENSOR_1, INPUT);
  pinMode(IR_STATUS_1, OUTPUT);   digitalWrite(IR_STATUS_1, LOW);

  // pin change interrupt
  attachInterrupt(digitalPinToInterrupt(IR_SENSOR_1), ir_sensor_1_int_v5, CHANGE);

  Serial.begin(9600);
  Serial.println("AT+BAUD3");  // 3=4800, 4=9600, 5=19200
  Serial.flush();
  // Wait for "OK\r\n+BAUD=4800\r\n"
  Serial.begin(4800);
  Serial.println("AT+PIN123456");  // Set PIN to 123456
  Serial.flush();
  // Wait for "OK\r\n+PIN=123456\r\n"
  Serial.println("AT+NAMEGST-Helmet-A55A");  // Set NAME to GST-Helmet-A55A
  Serial.flush();
  // Wait for "OK\r\n+NAME=GST-Helmet-A55A\r\n"
  Serial.println("AT+ROLE0");  // Set to Slave or Periperial
  Serial.flush();
  // Wait for "OK\r\n+ROLE=0\r\n"
  Serial.println("AT+IMME0");  // Set to Immediate connection
  Serial.flush();
  // Wait for "OK\r\n+IMME=0\r\n"

  Serial.begin(4800);
  pinMode(IR_1_EN, OUTPUT);
  digitalWrite(IR_1_EN, HIGH);

}

// the loop function runs over and over again forever
void loop() {

  int activityLED = digitalRead(LED_BUILTIN);
  digitalWrite(LED_BUILTIN, activityLED != HIGH ? HIGH : LOW);

/*
  if ( ( send_ms > 0 ) )
  {
    Serial.println("INT");
    Serial.println("$PGSTZ,"+String(last_ms)+","+String(millis()));
  }
*/
  
  if ( ( send_ms > 0 ) and ( last_ms + int_message_frame > millis() ) )
  {
    if ( send_ms == 5 )
    {
      Serial.println("$PGSTZ,"+String(last_ms)+","+String(millis()));
      k = "$PGSTC," + ulongToStrLen(last_ms,8);
      k.toUpperCase();
      char z = 0;
      for(int l=1; l < k.length(); l++)
      {
        z ^= k.charAt(l);
      }
      k.concat("*");
      k.concat( String( (z & 0xF0) >> 4, HEX) );
      k.concat( String( (z & 0x0F), HEX) );
      k.toUpperCase();
    }
    Serial.println();
    Serial.println(k);
    Serial.println();
    send_ms--;
  }
  else
  {
    send_ms = 0;
  }

  while (Serial.available()) 
  {
    char c = Serial.read();
    
    if ( ( c == '$' ) or ( nmeaInMsgLen+1 >= nmeaInMsgBufLen ) )
    {
      // Reset buffer and CRCs:
      nmeaInMsgLen = 0;
      nmeaInCrcPos = -1;
      nmeaInCrcVal = 0;
      nmeaInCrcChk = 0;
    }
    
    if((c == '\n') || (c == '\r'))
    {
      if(nmeaInMsgLen > 0)
      {

        // Decode CRC in a message:
        if( ( nmeaInCrcPos >= 0 ) and ( nmeaInCrcPos+1 < nmeaInMsgLen ) )
        {
          for(int m=nmeaInCrcPos+1; m < nmeaInMsgLen; m++)
          {
            char a;
            a = nmeaInMsgBuf[m];
            if( ( a < '0' ) or ( a > '9' and a < 'A' ) or ( a > 'F' ) )
            {
              nmeaInCrcChk = 0x1FF;  // incorrect CRC
            }
            else
            {
              nmeaInCrcChk = ( nmeaInCrcChk << 4 ) + a - ( ( a <= '9' ) ? '0' : '7' );
            }
            if( nmeaInCrcChk > 0xFF )
            {
              break;
            }
          }
        }

/*****
        Serial.print( "@ ");
        Serial.println(nmeaInMsgBuf);

        //
        Serial.println("@ Calculated CRC: '" + String(nmeaInCrcVal,HEX) + "'");
        if( nmeaInCrcPos < 0 )
        {
          Serial.println("@ No CRC sign '*' found in message!");
        }
        else if( nmeaInCrcPos+1 >= nmeaInMsgLen )
        {
          Serial.println( "@ No CRC after '*' found!");
        }
        else if( nmeaInCrcChk > 0xFF )
        {
          Serial.println( "@ Incorrect incomming CRC!");
        }
        else 
        {
          Serial.println( "@ Incomming CRC: '" + String(nmeaInCrcChk,HEX) + "'");
          Serial.println( ( nmeaInCrcVal == nmeaInCrcChk ) ? "@ CRC is OK!" : "@ CRC check failed!");
        }
*****/

        if ( ( nmeaInCrcVal == nmeaInCrcChk ) or ( nmeaInCrcPos < 0 ) )
        {
          Serial.println( nmeaInMsgBuf);
        }






/***************************************************************
        Serial.print( "# ");
        Serial.println(nmeaInMsgBuf);

        
        // Check Input Buffer Checksum
        char calculatedCRC = 0;
        int decodedCRC = 0;
        int PositionOfCRC = -1;
        for(int l=1; l < nmeaInMsgLen; l++)
        {
          if( nmeaInMsgBuf[l] == '*' )
          {
            PositionOfCRC = l;
            break;
          }
          calculatedCRC ^= nmeaInMsgBuf[l];
        }
        Serial.println("# Calculated CRC: '" + String(calculatedCRC,HEX) + "'");
        if( PositionOfCRC < 0 )
        {
          Serial.println("# No CRC sign '*' found!");
        }
        else if( PositionOfCRC+1 < nmeaInMsgLen )
        {
          // Serial.print( "# ");
          // Serial.println( &nmeaInMsgBuf[PositionOfCRC+1]);
          for(int m=PositionOfCRC+1; m < nmeaInMsgLen; m++)
          {
            char c;
            c = nmeaInMsgBuf[m];
            if( ( c < '0' ) or ( c > '9' and c < 'A' ) or ( c > 'F' ) )
            {
              decodedCRC = 0x1FF;
            }
            else
            {
              c = c - ( ( c <= '9' ) ? '0' : '7' );
              decodedCRC = ( decodedCRC << 4 ) + c;
            }
            if( decodedCRC > 0xFF )
            {
              break;
            }
          }
          if( decodedCRC > 0xFF )
          {
            Serial.println( "# Incorrect incomming CRC!");
          }
          else
          {
            Serial.println( "# Incomming CRC: " + String(decodedCRC,HEX));
          }

          Serial.println( ( calculatedCRC == decodedCRC ) ? "# CRC is OK!" : "# CRC check failed!");
        }
        else
        Serial.println( "# No CRC after '*' found!");
        if ( ( calculatedCRC == decodedCRC ) or ( PositionOfCRC < 0 ) )
        {
          Serial.println( nmeaInMsgBuf);
        }
***************************************************************/

        
        // -----------------------------
        Serial.flush();
        
        // Reset buffer and CRCs:
        nmeaInMsgLen = 0;
        nmeaInCrcPos = -1;
        nmeaInCrcVal = 0;
        nmeaInCrcChk = 0;
      }
    }
    else
    {
      if(nmeaInMsgLen < nmeaInMsgBufLen)
      {
        if( ( c == '*' ) and ( nmeaInCrcPos < 0 ) )
        {
          nmeaInCrcPos = nmeaInMsgLen;
        }
        if( ( nmeaInCrcPos < 0 ) and ( c != '$' ) )
        {
          nmeaInCrcVal ^= c;
        }
        nmeaInMsgBuf[nmeaInMsgLen++] = c; 
        nmeaInMsgBuf[nmeaInMsgLen] = 0;  // append null terminator
      }
    }
  }

/*
  while( sysLogBegin != sysLogEnd )
  {
    unsigned long ms = sysLog[sysLogBegin].ms;
    int h = ms / 3600000;
    ms %= 3600000;
    int m = ms / 60000; 
    ms %= 60000;
    int s = ms / 1000; 
    ms %= 1000;
    Serial.print( IntToStrLen(h,2) + ":" + IntToStrLen(m,2) + ":" + IntToStrLen(s,2) + "." + IntToStrLen(ms,3) );
    Serial.print( " - " );
    Serial.print( sysLog[sysLogBegin].flags );
    Serial.print( " - " );
    Serial.print( (((sysLog[sysLogBegin].flags & 1) != 0) ? "GREEN" : "green"));
    Serial.print( " - " );
    Serial.print( (((sysLog[sysLogBegin].flags & 2) != 0) ? "YELLOW" : "yellow"));
//    Serial.print( " - " );
//    Serial.print( (flagLogMax + flagLogEnd - flagLogBegin) % flagLogMax);
    Serial.print( " - [" );
    Serial.print( IntToStrLen(sysLogBegin,2));
    Serial.print( "-" );
    Serial.print( IntToStrLen(sysLogEnd,2));
    Serial.print( "]=" );
    Serial.print( (SYSLOGSIZE + sysLogEnd - sysLogBegin) % SYSLOGSIZE);
    sysLogBegin = (sysLogBegin + 1) % SYSLOGSIZE;
    Serial.println(" *");
  }
*/

  delay(15);
//  Serial.println("IT'S ME!");
}

/*
  SerialEvent occurs whenever a new data comes in the hardware serial RX. This
  routine is run between each time loop() runs, so using delay inside loop can
  delay response. Multiple bytes of data may be available.
*/
/*
void serialEvent() 
{
  while (Serial.available()) 
  {
    // Read one byte
    unsigned char inChar = (unsigned char) Serial.read();

    if( inChar == '$' )
    {
      nmeaInputBuffer = inChar;
      nmeaInputCR     = false;
    }
    else if( nmeaInputBuffer.length() > 0 )
    {
      nmeaInputBuffer += inChar;
      if( inChar == 13 )  nmeaInputCR = true;
      if( ( inChar == 10 ) and ( nmeaInputCR ) )
      {
        nmeaInputString    = nmeaInputBuffer;
        nmeaStringComplete = true;
        nmeaInputBuffer    = "";
        nmeaInputCR        = false;
      }
    }
  }  
}
*/
void ir_sensor_interrupt()
{
  int ir1_reading = digitalRead(IR_SENSOR_1);
  int ir2_reading = digitalRead(IR_SENSOR_2);
  digitalWrite(G_LED, ir1_reading == HIGH ? LOW : HIGH);
  digitalWrite(Y_LED, ir2_reading == HIGH ? LOW : HIGH);
//  digitalWrite(G_LED, PIND & _BV (IR_SENSOR_1) ? LOW : HIGH);
  isOutput = (ir1_reading == HIGH ? 0 : 1) | (ir2_reading == HIGH ? 0 : 2) ;
  flagLog[flagLogEnd].ms = millis();
  flagLog[flagLogEnd].flags = isOutput;
  flagLogEnd = (flagLogEnd + 1) % flagLogMax;
}

void ir_sensor_interrupt_v2()
{
  unsigned int ir1_reading = digitalRead(IR_SENSOR_1);
  unsigned int ir2_reading = digitalRead(IR_SENSOR_2);
  unsigned long ms = millis();

  ir1_reading = ir1_reading == HIGH ? 0 : SYSLOG_FLAG_IR_SENSOR_1;
  ir2_reading = ir2_reading == HIGH ? 0 : SYSLOG_FLAG_IR_SENSOR_2;

  if( (ms-sysLog_Sensor1.ms) > SYSLOG_FLAG_LATCH_DELAY )
  {
    if( ir1_reading != sysLog_Sensor1.flags )
    {
      sysLog_Sensor1.flags = ir1_reading;
      sysLog_Sensor1.ms = ms;
    }  
  }
  if( (ms-sysLog_Sensor2.ms) > SYSLOG_FLAG_LATCH_DELAY )
  {
    if( ir2_reading != sysLog_Sensor2.flags )
    {
      sysLog_Sensor2.flags = ir2_reading;
      sysLog_Sensor2.ms = ms;
    }
  }
  if(sysLog_Sensor1.ms == ms or sysLog_Sensor2.ms == ms)
  {
    digitalWrite(G_LED, ir1_reading ? LOW : HIGH);
    digitalWrite(Y_LED, ir2_reading ? LOW : HIGH);
    if( sysLog[sysLogPreEnd].ms != ms )
    {
      sysLogPreEnd = sysLogEnd;
      sysLogEnd = (sysLogEnd + 1) % SYSLOGSIZE;
    }
    sysLog[sysLogPreEnd].ms = ms;
    sysLog[sysLogPreEnd].flags = sysLog_Sensor1.flags | sysLog_Sensor2.flags;
//    sysLogPreEnd = sysLogEnd;
//    sysLogEnd = (sysLogEnd + 1) % SYSLOGSIZE;
  }
}

void ir_sensor_1_int_v5()
{
  unsigned long ms = millis();
  if( ms > ( last_ms + int_pause_length ) )
  {
    last_ms = ms;
    send_ms = 5;
  } 
}

void ir_sensor_1_int()
{
  int ir1_reading = digitalRead(IR_SENSOR_1);
  digitalWrite(G_LED, ir1_reading == HIGH ? LOW : HIGH);
//  digitalWrite(G_LED, PIND & _BV (IR_SENSOR_1) ? LOW : HIGH);
  isOutput |= 1;
  flagLog[flagLogEnd].ms = millis();
  flagLog[flagLogEnd].flags = 1;
  flagLogEnd = (flagLogEnd + 1) % flagLogMax;
}

void ir_sensor_2_int()
{
  int ir2_reading = digitalRead(IR_SENSOR_2);
  digitalWrite(Y_LED, ir2_reading == HIGH ? LOW : HIGH);
//  digitalWrite(Y_LED, PIND & _BV (IR_SENSOR_2) ? LOW : HIGH);
  isOutput |= 2;
  flagLog[flagLogEnd].ms = millis();
  flagLog[flagLogEnd].flags = 2;
  flagLogEnd = (flagLogEnd + 1) % flagLogMax;
}

String IntToStrLen( int i, int l)
{
  String r = "0000000000000000000" + String(i);
//  if( r.length() < l )
//  {
//    r = "000" + r;
    return r.substring(r.length()-l);
//    r.replace("_",c);
//  }
//  return r;
}

String ulongToStrLen( unsigned long i, int l)
{
  String r = "0000000000000000000" + String(i);
//  if( r.length() < l )
//  {
//    r = "000" + r;
    return r.substring(r.length()-l);
//    r.replace("_",c);
//  }
//  return r;
}


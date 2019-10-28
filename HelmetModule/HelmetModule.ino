/*
        MCU    MCU      Nano    My
        pin    func     pin    func
        ----------------------------- 
        RXI  |         |    |        |<--------< Serial input: IR SENSOR 1/2 serial receive
        TXO  |         |    |        |>--------> Serial output: bluetooth serial send
             |         |    |        |
        PD2  | INT0    |  2 | IR1.IN |<--------< IR SENSOR 1: catch signal appeared in IR 1 and switch RXI to IR 1   (+)
        PD3  | INT1    |  3 | IR2.IN |<--------< IR SENSOR 2: catch signal appeared in IR 2 and switch RXI to IR 2   (+)
             |         |    |        | 
        PD0  | RXD     | RX |  IR.RX |<--------< IR MUX OUT         (+)
        PD1  | TXD     | TX |  BT.TX |>--------> BT RXD             (+)
        PB0  | PB0     |  8 |  BT.RX |<--------< BT TXD             (-)
        PB1  | PB1     |  9 | AUX.TX |>--------> (not connected)    (-)
             |         |    |        |
        PD4  | PD4     |  4 | IR1.EN |>--------> Switch ON/OFF RX of IR SENSOR 1/2 / 1=Enable
             |         |    |        |
        PD7  | PD7     |  7 | RST.EN |>--------> Reset button. (To switch App to START mode)
             |         |    |        |

        A7   | A7      |    |        |>--------> Voltmetr input
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

#define STR_CMD_RESET           "CMD.RESET"
#define STR_NTF_HELMET_PWR_LOW  "NTF.HPWLW"
#define STR_NTF_NO_DATA         "NTF.NODTA"
#define STR_DATA_RCV_PREFIX     "DTR:"

String STR_DEVICE_NAME=         "GST-Hlmt-Rcvr";      
//"GST-Helmet-A55A"
String STR_DEVICE_PASSWORD=     "123456";

//
//const int R_LED = 14;           // A0 - pin 14
//const int Y_LED = 16;           // A2 - pin 16

#define PIN_OUT_READY_LED       18     // A4 - pin 18


#define PIN_IN_IR1              2       // D2 - (pin 2) IR 1 signal appearance detector
#define PIN_IN_IR2              3       // D3 - (pin 3) IR 2 signal appearance detector
#define PIN_OUT_CTRL_IR_ONOFF   4       // D4 - (pin 4)

#define PIN_OUT_IR_STATUS_LED   PIN_OUT_READY_LED  // A4 - pin 18

#define SYSLOG_FLAG_PIN_IN_IR1  1
#define SYSLOG_FLAG_PIN_IN_IR2  2
#define SYSLOG_FLAG_LATCH_DELAY 5000


#define PIN_IN_BTN_RESET        7    // D7 - (pin 7) RESET button pressed detector

#define VOLTMETER_PIN           A7  // A7 -- A7
#define VOLTMETER_REF          3.3  // 
#define VOLTMETER_DIV          900  // 220+680 kOhm
#define VOLTMETER_MUL          220  // 220 kOhm

#define POWER_OK_LEVEL         4.5

int isOutput = 0;

// log params
const int SYSLOGSIZE = 100;

const int flagLogMax = 100;
struct {unsigned long ms; int flags;} flagLog [flagLogMax];
int       flagLogBegin = 0;
int       flagLogEnd = 0;


struct  sysLogRecord {unsigned long ms; unsigned int flags;} __attribute__((packed));
struct  sysLogRecord sysLog[SYSLOGSIZE];
int     sysLogBegin = 0;
int     sysLogEnd = 0;
int     sysLogPreEnd = 0;
struct  sysLogRecord sysLog_Sensor1;
struct  sysLogRecord sysLog_Sensor2;

const int     nmeaInMsgBufLen = 128; 
char          nmeaInMsgBuf[nmeaInMsgBufLen];
int           nmeaInMsgLen = 0;   // Length of incomming message
char          nmeaInCrcVal = 0;   // NMEA checksum calculated from the message body (before '*')
int           nmeaInCrcChk = 0;   // NMEA checksum read from the NMEA message
int           nmeaInCrcPos = -1;  // Position of the checksum in the message (position of '*')


unsigned long ulLastTm = 0;   // last 
unsigned long ulSendTm = 0;
//String        k = "$PGSTC,";

unsigned long ulPauseTm = 2000;
int           ulMsgTm = 1000;

/*
void callback_IR1_interrupt()
{
  unsigned long ulTm = millis();
  if( ulTm > ( ulLastTm + ulPauseTm ) )
  {
    ulLastTm = ulTm;
    ulSendTm = 5;  // set send message flag
    Serial.println("#");
    Serial.flush();
  } 
}
*/
/*
void setSerialFq(int iFqId)
{
  String sFqATCmd = "AT+BAUD";
  int     iFq = 0;
  switch(iFqId)
  {
    case 1: iFq=1200; sFqATCmd+="AT+BAUD1"; break;
    case 2: iFq=2400; sFqATCmd+="AT+BAUD2"; break;
    case 3: iFq=4800; sFqATCmd+="AT+BAUD3"; break;
    case 4: iFq=9600; sFqATCmd+="AT+BAUD4"; break;
    case 5: iFq=19200; sFqATCmd+="AT+BAUD5"; break;
    case 6: iFq=38400; sFqATCmd+="AT+BAUD6"; break;
    case 7: iFq=57600; sFqATCmd+="AT+BAUD7"; break;
    case 8: iFq=74880; sFqATCmd+="AT+BAUD8"; break;
  }
  Serial.begin(1200);
  Serial.println(sFqATCmd);
  Serial.flush();
  Serial.begin(2400);
  Serial.println(sFqATCmd);
  Serial.flush();
  Serial.begin(4800);
  Serial.println(sFqATCmd);
  Serial.flush();
  Serial.begin(9600);
  Serial.println(sFqATCmd);
  Serial.flush();
  Serial.begin(19200);
  Serial.println(sFqATCmd);
  Serial.flush();
  Serial.begin(38400);
  Serial.println(sFqATCmd);
  Serial.flush();
  Serial.begin(57600);
  Serial.println(sFqATCmd);
  Serial.flush();
  Serial.begin(iFq);
}
*/
#define SERIAL_PORT_SETUP_SPEED 9600

#define SERIAL_PORT_SPEED       4800
#define SERIAL_PORT_SPEED_CMD   "AT+BAUD3"

unsigned long ulLastBtnResetEventTm = 0;
unsigned long ulBtnResetEventTimeOut = 500;

unsigned long ulLastSerialReadTm = 0;
unsigned long ulSerialReadTimeOut = 200;



// the setup function runs once when you press reset or power the board
void setup() 
{
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  // setup RESET button input
  pinMode(PIN_IN_BTN_RESET, INPUT_PULLUP);

  // setup IR receiver pins
  pinMode(PIN_IN_IR1, INPUT);    // IR sensor 1 input
  //pinMode(PIN_IN_IR2, INPUT);    // IR sensor 2 input
  
  pinMode(PIN_OUT_IR_STATUS_LED, OUTPUT);   // IR sensors status LED
  pinMode(PIN_OUT_CTRL_IR_ONOFF, OUTPUT);   // IR sensors on/off switch 

  digitalWrite(PIN_OUT_IR_STATUS_LED, LOW); // set IR sensor 1 ON???

  // set callback for pin change interrupt
  //attachInterrupt( digitalPinToInterrupt(PIN_IN_IR1), callback_IR1_interrupt, CHANGE);

  // configurate port
  Serial.begin(SERIAL_PORT_SETUP_SPEED); // should be x2 of Computer serial speed
  delay(15);
  // set 4800
  Serial.println(SERIAL_PORT_SPEED_CMD);  // setup port frequency 3=4800, 4=9600, 5=19200
  Serial.flush();
  delay(15);
 
  // Wait for "OK\r\n+BAUD=4800\r\n"
  Serial.begin(SERIAL_PORT_SPEED);
  delay(15);
  
  Serial.println("AT+PIN"+STR_DEVICE_PASSWORD);  // Set BlueTooth device PIN
  Serial.flush();
  delay(15);
  // Wait for "OK\r\n+PIN=123456\r\n"
  Serial.println("AT+NAME"+STR_DEVICE_NAME);  // Set BlueTooth device NAME
  Serial.flush();
  delay(15);
  // Wait for "OK\r\n+NAME=GST-Helmet-A55A\r\n"
  Serial.println("AT+ROLE0");  // Set to Slave or Periperial
  Serial.flush();
  delay(15);
  // Wait for "OK\r\n+ROLE=0\r\n"
  Serial.println("AT+IMME0");  // Set to Immediate connection
  Serial.flush();
  delay(15);
  // Wait for "OK\r\n+IMME=0\r\n"

  Serial.begin(SERIAL_PORT_SPEED);
  delay(15);
  
  // switch on IR sensors
  digitalWrite(PIN_OUT_CTRL_IR_ONOFF, HIGH);
  delay(15);

  ulLastBtnResetEventTm = millis();
  ulLastSerialReadTm = ulLastBtnResetEventTm;
}


void readSerialS()
{
  nmeaInMsgLen=0;
  while (Serial.available()) 
  {
    char c = Serial.read();

/*    
    nmeaInMsgBuf[nmeaInMsgLen++] = c; 
    nmeaInMsgBuf[nmeaInMsgLen] = 0;  // append null terminator
    Serial.println( nmeaInMsgBuf);
    Serial.flush();
*/
    if(c != '\n' && c != '\r')
    {
        nmeaInMsgBuf[nmeaInMsgLen++] = c; 
    }
   else
    {
      if(nmeaInMsgLen>0)
      {
       nmeaInMsgBuf[nmeaInMsgLen++] = 0;  // append null terminator
       Serial.println( nmeaInMsgBuf);
       Serial.flush();
       nmeaInMsgLen=0;
      }
    }
  }
}

/*
void readSerial()
{
  while (Serial.available()) 
  {
    char c = Serial.read();

    nmeaInMsgLen=0;

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

        if ( ( nmeaInCrcVal == nmeaInCrcChk ) or ( nmeaInCrcPos < 0 ) )
        {
          Serial.println( nmeaInMsgBuf);
        }
        
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
}

String calcCRC(String sSrc)
{
    char z = 0;
    for(int l=1; l < sSrc.length(); l++)
    {
      z ^= sSrc.charAt(l);
    }
    String sCRC="";
    sCRC.concat( String( (z & 0xF0) >> 4, HEX) );
    sCRC.concat( String( (z & 0x0F), HEX) );

    return(sCRC);
}
*/
void checkResetBtnState()
{
  int iBtnResetValue = digitalRead(PIN_IN_BTN_RESET);
  if( iBtnResetValue == LOW && ( ulLastBtnResetEventTm + ulBtnResetEventTimeOut < millis()) ) // button RESET is pressed
  { 
    Serial.println(STR_CMD_RESET);
    ulLastBtnResetEventTm = millis();

    int activityLED = digitalRead(LED_BUILTIN);
    digitalWrite(LED_BUILTIN, activityLED != HIGH ? HIGH : LOW);    

    // check & notify if the power is low
    float fVoltage = analogRead(VOLTMETER_PIN) * (VOLTMETER_REF / 1024.0) * (VOLTMETER_DIV) / (VOLTMETER_MUL);
    if(fVoltage < POWER_OK_LEVEL)
    {
      String sMsg="";
      sMsg = STR_NTF_HELMET_PWR_LOW;
      sMsg.concat(fVoltage);
      Serial.println(sMsg);
    }
  }  
}

int   iMsgLen = 0;
int readSerialInput()
{
  char  chInMsgBuf[300];
  char  chInput;
  int   iBytesReceived=0;
  
  while (Serial.available()) 
  {
    while(Serial.available() && iMsgLen<255)
    {
      chInput=Serial.read();
      ulLastSerialReadTm = millis();

      if(chInput=='\n' || chInput=='\r')
      {
        iBytesReceived++;
        break;
      }
      chInMsgBuf[iMsgLen++] = chInput;
    }
    
    if(iMsgLen>0)
    {
      // output in case of CR LF or 
      if( iMsgLen==255 || chInput=='\n' || chInput=='\r' || (ulLastSerialReadTm + ulSerialReadTimeOut < millis()))
      {
        // output to serial
        digitalWrite(LED_BUILTIN, HIGH);
        //Serial.println("#");
        //Serial.flush();

        iBytesReceived+=iMsgLen;
        chInMsgBuf[iMsgLen++] = 0;
        
        Serial.println( chInMsgBuf );
        Serial.flush();
        digitalWrite(LED_BUILTIN, LOW);
        iMsgLen=0;
      }
    }
  }
  return(iBytesReceived); 
}
/*
int routeSerialInput()
{
  int   iCharReceivedCount=0;
  char  chInput;
  while (Serial.available()) 
  {
      chInput=Serial.read();
      iCharReceivedCount++;
      Serial.write(chInput);
      //Serial.println("#");
      Serial.flush();
  }  
  //Serial.println("#");
  //Serial.flush();
  return(iCharReceivedCount);
}
*/
// the loop function runs over and over again forever
void loop() 
{
  String sMsg="";
  
  // check if RESET button is pressed
  checkResetBtnState();

  // read & transmit serial port input
  readSerialInput();
  
  delay(15); //15
}



/*
void PIN_IN_IR2_int()
{
  int ir2_reading = digitalRead(PIN_IN_IR2);
//  digitalWrite(Y_LED, ir2_reading == HIGH ? LOW : HIGH);
//  digitalWrite(Y_LED, PIND & _BV (PIN_IN_IR2) ? LOW : HIGH);
  isOutput |= 2;
  flagLog[flagLogEnd].ms = millis();
  flagLog[flagLogEnd].flags = 2;
  flagLogEnd = (flagLogEnd + 1) % flagLogMax;
}
*/
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

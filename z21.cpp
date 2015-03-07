#include <XpressNet.h> 

#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library
#include <Syslog.h>
#include "z21.h"

byte XBusVer= 0x30;
// buffers for receiving and sending data
unsigned char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
//char debugBuffer[64];
//String debugString;

#define maxIP 10        //Speichergröße für IP-Adressen
#define ActTimeIP 20    //Aktivhaltung einer IP für (sec./2)
#define interval 2000   //interval at milliseconds
struct TypeActIP {
  byte ip0;    // Byte IP
  byte ip1;    // Byte IP
  byte ip2;    // Byte IP
  byte ip3;    // Byte IP
  byte time;  //Zeit
};
TypeActIP ActIP[maxIP];    //Speicherarray für IPs
//long previousMillis = 0;        // will store last time of IP decount updated

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;
unsigned int localPort = 21105;      // local port to listen on

//#include <SoftwareSerial.h>
//SoftwareSerial //debug(6, 5); // RX, TX


/*********************************************************
 * LOGGING
 * *******************************************************/
/*******************************************************
Logger

sevirity:
          0       Emergency
          1       Alert
          2       Critical
          3       Error
          4       Warning
          5       Notice
          6       Informational
          7       debug
          
facility: local0 13
*********************************************************/


void trace(const char* msg)
{
  Syslog.logger(1,1,"z21",msg);
  return ;
}

void debug(const char* tag, char* msg){
  Syslog.logger(13, 7, tag, msg);
}

void debug(const char* msg){
  Syslog.logger(13,7,"z21", msg);
}

void debug(String msg){
    Syslog.logger(13,7,"z21", msg);
}

void info(const char* msg){
  Syslog.logger(13,6,"z21", msg);
}

void warn(const char* msg){
  Syslog.logger(13,4,"z21", msg);
}

void error(const char* msg){
  Syslog.logger(13,3,"z21", msg);
}

/**********************************************************
 * HELPERS
 * ********************************************************/
//--------------------------------------------------------------------------------------------
void clearIPSlots() {
  for (int i = 0; i < maxIP; i++) {
    ActIP[i].ip0 = 0;
    ActIP[i].ip1 = 0;
    ActIP[i].ip2 = 0;
    ActIP[i].ip3 = 0;
    ActIP[i].time = 0;
  }
}

String printIP(byte * ip){
    static String ips;
    ips= String(ip[0]);
    ips+='.';
    ips+=ip[1];
    ips+='.';
    ips+=ip[2];
    ips+='.';
    ips+=ip[3];
    return ips;
}

//--------------------------------------------------------------------------------------------
void addIPToSlot (byte ip0, byte ip1, byte ip2, byte ip3) {
  byte Slot = maxIP;
  for (int i = 0; i < maxIP; i++) {
    if (ActIP[i].ip0 == ip0 && ActIP[i].ip1 == ip1 && ActIP[i].ip2 == ip2 && ActIP[i].ip3 == ip3) {
      ActIP[i].time = ActTimeIP;
      //debug("IP '" + printIP(&ActIP[i].ip0) + "' was already active");
      return;
    }
    else if (ActIP[i].time == 0 && Slot == maxIP)
      Slot = i;
  }
  ActIP[Slot].ip0 = ip0;
  ActIP[Slot].ip1 = ip1;
  ActIP[Slot].ip2 = ip2;
  ActIP[Slot].ip3 = ip3;
  ActIP[Slot].time = ActTimeIP;
  notifyXNetPower(XpressNet.getPower());
}



//--------------------------------------------------------------------------------------------
//Senden von Lokdaten via Ethernet
void EthSendOut (unsigned int DataLen, unsigned int Header, byte Data[], boolean withXOR) {
      Udp.write(DataLen & 0xFF);
      Udp.write(DataLen & 0xFF00);
      Udp.write(Header & 0xFF);
      Udp.write(Header & 0xFF00);
      
      unsigned char XOR = 0;
      byte ldata = DataLen-5;  //Ohne Length und Header und XOR
      if (!withXOR)    //XOR vorhanden?
        ldata++;
      for (int i = 0; i < (ldata); i++) {
        XOR = XOR ^ Data[i];
        Udp.write(Data[i]);
      }
      if (withXOR) 
        Udp.write(XOR);
}

//--------------------------------------------------------------------------------------------
void EthSend (unsigned int DataLen, unsigned int Header, byte Data[], boolean withXOR, boolean BC) {
  if (BC) {
    IPAddress IPout = Udp.remoteIP();
    for (int i = 0; i < maxIP; i++) {
      if (ActIP[i].time > 0) {    //Noch aktiv?
        IPout[0] = ActIP[i].ip0;
        IPout[1] = ActIP[i].ip1;
        IPout[2] = ActIP[i].ip2;
        IPout[3] = ActIP[i].ip3;
        ////debug("z21 sending data to: ", IPout);
        Udp.beginPacket(IPout, Udp.remotePort());    //Broadcast
        EthSendOut (DataLen, Header, Data, withXOR);
        Udp.endPacket();
      }
    }
  }
  else {
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());    //Broadcast
    EthSendOut (DataLen, Header, Data, withXOR);
    ////debug(Udp.remoteIP());
    Udp.endPacket();
  }
}

//--------------------------------------------------------------------------------------------
void notifyXNetPower (uint8_t State)
{
    //debug.print("Power: 0x"); 
    ////debug(State, HEX); 
  byte data[] = {0x61, 0x00};
  switch (State) {
    case csNormal: data[1] = 0x01;
    break;
    case csTrackVoltageOff: data[1] = 0x00;
    break;
    case csServiceMode: data[1] = 0x02;
    break;
    case csEmergencyStop:
            data[0] = 0x81;
            data[1] = 0x00; 
    break;
  }
  EthSend(0x07, 0x40, data, true, true);
}

//--------------------------------------------------------------------------------------------
void notifyLokFunc(uint8_t Adr_High, uint8_t Adr_Low, uint8_t F2, uint8_t F3 ) {
  // //debug.print("Loco Fkt: "); 
  // //debug.print(Adr_Low); 
  // //debug.print(", Fkt2: "); 
  // //debug.print(F2, BIN); 
  // //debug.print("; "); 
  // //debug(F3, BIN); 
  
}

//--------------------------------------------------------------------------------------------
void notifyLokAll(uint8_t Adr_High, uint8_t Adr_Low, boolean Busy, uint8_t Steps, uint8_t Speed, uint8_t Direction, uint8_t F0, uint8_t F1, uint8_t F2, uint8_t F3) {

  byte DB2 = Steps;
  if (DB2 == 3)  //nicht vorhanden!
    DB2 = 4;
  if (Busy) 
    bitWrite(DB2, 3, 1);
  byte DB3 = Speed;
  if (Direction == 1)  
    bitWrite(DB3, 7, 1);
  byte data[9]; 
  data[0] = 0xEF;  //X-HEADER
  data[1] = Adr_High & 0x3F;
  data[2] = Adr_Low;
  data[3] = DB2;
  data[4] = DB3;
  data[5] = F0;    //F0, F4, F3, F2, F1
  data[6] = F1;    //F5 - F12; Funktion F5 ist bit0 (LSB)
  data[7] = F2;  //F13-F20
  data[8] = F3;  //F21-F28
  EthSend (14, 0x40, data, true, true);  //Send Power und Funktions to all active Apps
}

//--------------------------------------------------------------------------------------------
void notifyTrnt(uint8_t Adr_High, uint8_t Adr_Low, uint8_t Pos) {
  // //debug.print("Weiche: "); 
   // //debug.print(word(Adr_High, Adr_Low)); 
   // //debug.print(", Position: "); 
   // //debug(Pos, BIN); 
  //LAN_X_TURNOUT_INFO
  byte data[4];
  data[0] = 0x43;  //HEADER
  data[1] = Adr_High;
  data[2] = Adr_Low;
  data[3] = Pos;
  EthSend (0x09, 0x40, data, true, false);  
}

//--------------------------------------------------------------------------------------------
void notifyCVInfo(uint8_t State ) {
   // //debug.print("CV Prog STATE: "); 
   // //debug(State); 
  if (State == 0x01 || State == 0x02) {  //Busy or No Data
    //LAN_X_CV_NACK
    byte data[2];
    data[0] = 0x61;  //HEADER
    data[1] = 0x13; //DB0
    EthSend (0x07, 0x40, data, true, false);  
  }
}

//--------------------------------------------------------------------------------------------
void notifyCVResult(uint8_t cvAdr, uint8_t cvData ) {
   // //debug.print("CV Prog Read: "); 
   // //debug.print(cvAdr); 
   // //debug.print(", "); 
   // //debug(cvData); 
  //LAN_X_CV_RESULT
  byte data[5];
  data[0] = 0x64; //HEADER
  data[1] = 0x14;  //DB0
  data[2] = 0x00;  //CVAdr_MSB
  data[3] = cvAdr;  //CVAdr_LSB
  data[4] = cvData;  //Value
  EthSend (0x0A, 0x40, data, true, false);
}

//--------------------------------------------------------------------------------------------
void notifyXNetVersion(uint8_t Version, uint8_t ID ) {
  XBusVer = Version;
}

/*
//--------------------------------------------------------------------------------------------
void notifyXNetStatus(uint8_t LedState )
{
}
*/

void xPressNetParse(byte* packetBuffer, byte* data){
  boolean ok = false;
  switch (packetBuffer[4]) { //X-Header
  case LAN_X_GENERAL: 
    switch (packetBuffer[5]) {  //DB0
      case LAN_X_GET_VERSION:
        data[0] = 0x63;
        data[1] = 0x21;
        data[3] = XBusVer;   //X-Bus Version
        data[4] = 0x12;  //ID der Zentrale
        data[5]= 0;
        EthSend (0x09, 0x40, data, true, false);
        debug("LAN_X_GET_VERSION"); 
        break;
      case LAN_X_GET_STATUS:
        data[0] = 0x62;
        data[1] = 0x22;
        data[2] = XpressNet.getPower();
        EthSend (0x08, 0x40, data, true, false);
        //debug("LAN_X_GET_STATUS"); This is asked very often ... 
        break;
      case LAN_X_SET_TRACK_POWER_OFF:
        ok = XpressNet.setPower(csTrackVoltageOff);
        debug("LAN_X_SET_TRACK_POWER_OFF"); 
        if (ok == false) {
          warn("Power Send FEHLER"); 
        }
        break;
      case LAN_X_SET_TRACK_POWER_ON:
        ok= XpressNet.setPower(csNormal);
        debug("LAN_X_SET_TRACK_POWER_ON");
        if (ok == false) {
          warn("Power Send FEHLER"); 
        }
        break;  
    }
    break;
  case LAN_X_CV_READ_0:
    if (packetBuffer[5] == LAN_X_CV_READ_1) {  //DB0
      debug("LAN_X_CV_READ"); 
      byte CV_MSB = packetBuffer[6];
      byte CV_LSB = packetBuffer[7];
      XpressNet.readCVMode(CV_LSB+1);
    }
    break;             
  case LAN_X_CV_WRITE_0:
    if (packetBuffer[5] == LAN_X_CV_WRITE_1) {  //DB0
      debug("LAN_X_CV_WRITE"); 
      byte CV_MSB = packetBuffer[6];
      byte CV_LSB = packetBuffer[7];
      byte value = packetBuffer[8]; 
      XpressNet.writeCVMode(CV_LSB+1, value);
    }
    break;             
  case LAN_X_GET_TURNOUT_INFO:
    debug("LAN_X_GET_TURNOUT_INFO"); 
    XpressNet.getTrntInfo(packetBuffer[5], packetBuffer[6]);
    break;             
  case LAN_X_SET_TURNOUT:
    debug("LAN_X_SET_TURNOUT"); 
    XpressNet.setTrntPos(packetBuffer[5], packetBuffer[6], packetBuffer[7] & 0x0F);
    break;  
  case LAN_X_SET_STOP:
    debug("LAN_X_SET_STOP"); 
    ok= XpressNet.setPower(csEmergencyStop);
    if (ok == false) {
      warn("Power Send FEHLER");             
    }
    break;  
  case LAN_X_GET_LOCO_INFO_0:
    if (packetBuffer[5] == LAN_X_GET_LOCO_INFO_1) {  //DB0
      debug("LAN_X_GET_LOCO_INFO: ");
      //Antwort: LAN_X_LOCO_INFO  Adr_MSB - Adr_LSB
      ////debug(word(packetBuffer[6], packetBuffer[7]));  //mit F1-F12
      XpressNet.getLocoInfo(packetBuffer[6]& 0x3F, packetBuffer[7]);
      XpressNet.getLocoFunc(packetBuffer[6] & 0x3F, packetBuffer[7]);  //F13 bis F28
    }
    break;  
  case LAN_X_SET_LOCO_FUNCTION_0:
      debug("Lok-Adresse");  
    if (packetBuffer[5] == LAN_X_SET_LOCO_FUNCTION_1) {  //DB0
      //LAN_X_SET_LOCO_FUNCTION  Adr_MSB        Adr_LSB            Type (EIN/AUS/UM)      Funktion
      XpressNet.setLocoFunc(packetBuffer[6] & 0x3F, packetBuffer[7], packetBuffer[8] >> 5, packetBuffer[8] & B00011111); 
    }
    else {
      debug("LAN_X_SET_LOCO_DRIVE");
      //LAN_X_SET_LOCO_DRIVE            Adr_MSB          Adr_LSB      DB0          Dir+Speed
      XpressNet.setLocoDrive(packetBuffer[6] & 0x3F, packetBuffer[7], packetBuffer[5] & B11, packetBuffer[8]);       
    }
    break;  
  case LAN_X_CV_POM:
    if (packetBuffer[5] == LAN_X_CV_POM_WRITE) {  //DB0
      byte Option = packetBuffer[8] & B11111100;  //Option DB3
      byte Adr_MSB = packetBuffer[6] & 0x3F;  //DB1
      byte Adr_LSB = packetBuffer[7];    //DB2
      int CVAdr = packetBuffer[9] | ((packetBuffer[8] & B11) << 7);
      if (Option == LAN_X_CV_POM_WRITE_BYTE) {
        debug("LAN_X_CV_POM_WRITE_BYTE"); 
        byte value = packetBuffer[10];  //DB5
      }
      if (Option == LAN_X_CV_POM_WRITE_BIT) {
         warn("LAN_X_CV_POM_WRITE_BIT"); 
        //Nicht von der APP Unterstützt
      }
    }
    break;  
  case LAN_X_GET_FIRMWARE_VERSION:
    debug("LAN_X_GET_FIRMWARE_VERSION"); 
    /*data[0] = 0xf3;
    data[1] = 0x0a;
    data[3] = 0x01;   //V_MSB
    data[4] = 0x23;  //V_LSB*/
    data[0] = 0xf3;
    data[1] = 0x0a;
    data[3] = 0x01;   //V_MSB
    data[4] = 0x23;  //V_LSB

    EthSend (0x09, 0x40, data, true, false);
    break;     
  }
}


/********************************
 * Public Members
 * *****************************/

void z21Setup(){
    Udp.begin(localPort);  //UDP Z21 Port
    clearIPSlots();  //löschen gespeicherter aktiver IP's
}

void z21CheckActiveIP(){
    for (int i = 0; i < maxIP; i++) {
      if (ActIP[i].time > 0) {
        ActIP[i].time--;    //Zeit herrunterrechnen
      }
      else {
        
        //clear IP DATA
        ActIP[i].ip0 = 0;
        ActIP[i].ip1 = 0;
        ActIP[i].ip2 = 0;
        ActIP[i].ip3 = 0;
        ActIP[i].time = 0;
      }
    } 
   
}

//--------------------------------------------------------------------------------------------
void z21Receive() {
  int packetSize = Udp.parsePacket();
  if(packetSize > 0) {
    addIPToSlot(Udp.remoteIP()[0], Udp.remoteIP()[1], Udp.remoteIP()[2], Udp.remoteIP()[3]);
    Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);  // read the packet into packetBufffer
    // send a reply, to the IP address and port that sent us the packet we received
    int header = (packetBuffer[3]<<8) + packetBuffer[2];
//    int datalen = (packetBuffer[1]<<8) + packetBuffer[0];
    byte data[16]; 
    boolean ok = false;
    //packetBuffer[packetSize]= 0;
    //debug("z21 packetBuffer: ", (char*) packetBuffer); 
    switch (header) {
      case LAN_GET_SERIAL_NUMBER:
        data[0] = 0xF5;  //Seriennummer 32 Bit (little endian)
        data[1] = 0x0A;
        data[2] = 0x00; 
        data[3] = 0x00;
        EthSend (0x08, 0x10, data, false, false);
        //debug("z21 Serial Number: ", (char*) data);
        debug("z21 Serial Number: ");
        break; 
      case LAN_GET_CONFIG:
        debug("Z21-Einstellungen"); 
        break;   
      case LAN_GET_HWINFO:
        data[0] = 0x00;  //HwType 32 Bit
        data[1] = 0x00;
        data[2] = 0x02; 
        data[3] = 0x01;
        data[4] = 0x20;  //FW Version 32 Bit
        data[5] = 0x01;
        data[6] = 0x00; 
        data[7] = 0x00;
        EthSend (0x0C, 0x1A, data, false, false);
        debug("LAN_GET_HWINFO"); 
        break;  
      case LAN_LOGOFF:
        debug("LAN_LOGOFF"); 
        //Antwort von Z21: keine
        break; 
      case LAN_XPRESS_NET:
          xPressNetParse(packetBuffer, data);
          break; 
      case LAN_SET_BROADCASTFLAGS:
        warn("!LAN_SET_BROADCASTFLAGS: "); 
        // //debug.print(packetBuffer[4], BIN);  // 1=BC Power, Loco INFO, Trnt INFO; B100=BC Sytemstate Datachanged
        break;
      case LAN_GET_BROADCASTFLAGS:
        warn("!LAN_GET_BROADCASTFLAGS"); 
        break;
      case LAN_GET_LOCOMODE:
        warn("!LAN_GET_LOCOMODE"); 
        break;
      case LAN_SET_LOCOMODE:
        warn("!LAN_SET_LOCOMODE"); 
        break;
      case LAN_GET_TURNOUTMODE:
        warn("!LAN_GET_TURNOUTMODE"); 
        break;
      case LAN_SET_TURNOUTMODE:
        warn("!LAN_SET_TURNOUTMODE"); 
        break;
      case LAN_RMBUS_GETDATA:
        warn("!LAN_RMBUS_GETDATA"); 
        break;
      case LAN_RMBUS_PROGRAMMODULE:
        warn("!LAN_RMBUS_PROGRAMMODULE"); 
        break;
      case LAN_SYSTEMSTATE_GETDATA:
        warn("!LAN_SYSTEMSTATE_GETDATA");  //LAN_SYSTEMSTATE_DATACHANGED
        data[0] = 0x00;  //MainCurrent mA
        data[1] = 0x00;  //MainCurrent mA
        data[2] = 0x00;  //ProgCurrent mA
        data[3] = 0x00;  //ProgCurrent mA        
        data[4] = 0x00;  //FilteredMainCurrent
        data[5] = 0x00;  //FilteredMainCurrent
        data[6] = 0x00;  //Temperature
        data[7] = 0x20;  //Temperature
        data[8] = 0x0F;  //SupplyVoltage
        data[9] = 0x00;  //SupplyVoltage
        data[10] = 0x00;  //VCCVoltage
        data[11] = 0x03;  //VCCVoltage
        data[12] = XpressNet.getPower();  //CentralState
        data[13] = 0x00;  //CentralStateEx
        data[14] = 0x00;  //reserved
        data[15] = 0x00;  //reserved
        EthSend (0x14, 0x84, data, false, false);
        break;
      case LAN_RAILCOM_GETDATA:
         warn("!LAN_RAILCOM_GETDATA"); 
        break;
      case LAN_LOCONET_FROM_LAN:
         warn("!LAN_LOCONET_FROM_LAN"); 
        break;
      case LAN_LOCONET_DISPATCH_ADDR:
         warn("!LAN_LOCONET_DISPATCH_ADDR"); 
        break;
      default:
        error("!LAN_X_UNKNOWN_COMMAND"); 
        data[0] = 0x61;
        data[1] = 0x82;
        EthSend (0x07, 0x40, data, true, false);
    }
  }
  
  
  
  
}

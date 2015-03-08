/*
    Z21 Ethernet Emulation für die App-Steuerung via Smartphone über XpressNet.

  The adress is 10.0.1.111.
  This program will log to a Syslog server at 10.0.1.101.  
  May need to disable firewall (sudo ufw disable) and 
  enable syslog server to receive log messages from outside (enable remote logging)

  Starting:
  Load firmware 
  Connect Arduino to Roco Booster Slave 
  Connect Multimaus to Roco Booster Master
  Connect LAN Plug
  Connect Arduino USB Plug with USB Power Supply

  Should start with the following log:
	Mar  8 21:23:15 z21 Starting XPressNet
	Mar  8 21:23:15 z21 Starting Z21 Emulation
	Mar  8 21:23:15 z21 Setup finished
  
  Connect Z21 App to 10.0.1.111

  Status:
  8.3.2015
  App can control a train, but will not show it on the display (Horn Button is not lid when pressed)

 */
 
#include <XpressNet.h> 
XpressNetClass XpressNet;

#include <EEPROM.h>
#define EEip 10    //Startddress im EEPROM für die IP
#define EEXNet 9   //Adresse im XNet-Bus

#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
//#include <EthernetUdp.h>         // UDP library
#include <Syslog.h>
#include "z21.h"

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xFE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(10, 0, 1, 111);
byte loghost[] = { 10, 0, 1, 101 };

EthernetServer server(80);

#define ResetPin A1  //Reset Pin bei Neustart betätigen um Standard IP zu setzten!

// XpressNet address: must be in range of 1-31; must be unique. Note that some IDs
// are currently used by default, like 2 for a LH90 or LH100 out of the box, or 30
// for PC interface devices like the XnTCP.
byte XNetAddress = 30;    //Adresse im XpressNet

#define interval 2000   //interval at milliseconds

long previousMillis = 0;        // will store last time of IP decount updated

//#include <SoftwareSerial.h>
//SoftwareSerial //debug(6, 5); // RX, TX

//--------------------------------------------------------------------------------------------
void setup() {
 // //debug.begin(115200); 
 // //debug("Z21"); 
   pinMode(ResetPin, INPUT);  
   digitalWrite(ResetPin, HIGH);  //PullUp  
   delay(100);
   
   if (digitalRead(ResetPin) == LOW || EEPROM.read(EEXNet) < 32) {
     XNetAddress = EEPROM.read(EEXNet);
   }
   else {  
      EEPROM.write(EEXNet, XNetAddress);
      EEPROM.write(EEip, ip[0]);
      EEPROM.write(EEip+1, ip[1]);
      EEPROM.write(EEip+2, ip[2]);
      EEPROM.write(EEip+3, ip[3]);
    }
  ip[0] = EEPROM.read(EEip);
  ip[1] = EEPROM.read(EEip+1);
  ip[2] = EEPROM.read(EEip+2);
  ip[3] = EEPROM.read(EEip+3);

//  //debug(ip);
 
  // start the Ethernet and UDP:
  Ethernet.begin(mac,ip);  //IP and MAC Festlegung
  Syslog.setLoghost(loghost);
  //  server.begin();    //HTTP Server
  
  debug("Starting XPressNet");
  XpressNet.start(XNetAddress,3);    //Initialisierung XNet
  debug("Starting Z21 Emulation");
  z21Setup();

  debug("Setup finished");
}


//--------------------------------------------------------------------------------------------
void loop() {
  
  XpressNet.receive();  //Check for XpressNet
 
  z21Receive();    //Read Data on UDP Port
  
  XpressNet.receive();  //Check for XpressNet
  
  Webconfig();    //Webserver for Configuration

  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis; 
    z21CheckActiveIP();
  }
}



//--------------------------------------------------------------------------------------------
void Webconfig() {
  EthernetClient client = server.available();
  if (client) {
    String receivedText = String(50);
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (receivedText.length() < 50) {
          receivedText += c;
        }
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          //client.println("Connection: close");  // the connection will be closed after completion of the response
	  //client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          //Website:
          client.println("<html><head><title>Z21</title></head><body>");
          client.println("<h1>Z21</h1><br />");
//----------------------------------------------------------------------------------------------------          
          int firstPos = receivedText.indexOf("?");
          if (firstPos > -1) {
            client.println("-> accept changes after RESET!");
            byte lastPos = receivedText.indexOf(" ", firstPos);
            String theText = receivedText.substring(firstPos+3, lastPos); // 10 is the length of "?A="
            byte XNetPos = theText.indexOf("&XNet=");
            XNetAddress = theText.substring(XNetPos+6, theText.length()).toInt();
            byte Aip = theText.indexOf("&B=");
            byte Bip = theText.indexOf("&C=", Aip);
            byte Cip = theText.indexOf("&D=", Bip);
            byte Dip = theText.substring(Cip+3, XNetPos).toInt();
            Cip = theText.substring(Bip+3, Cip).toInt();
            Bip = theText.substring(Aip+3, Bip).toInt();
            Aip = theText.substring(0, Aip).toInt();
            ip[0] = Aip;
            ip[1] = Bip;
            ip[2] = Cip;
            ip[3] = Dip;
            if (EEPROM.read(EEXNet) != XNetAddress)
              EEPROM.write(EEXNet, XNetAddress);
            if (EEPROM.read(EEip) != Aip)  
              EEPROM.write(EEip, Aip);
            if (EEPROM.read(EEip+1) != Bip)  
              EEPROM.write(EEip+1, Bip);
            if (EEPROM.read(EEip+2) != Cip)  
              EEPROM.write(EEip+2, Cip);
            if (EEPROM.read(EEip+3) != Dip)  
              EEPROM.write(EEip+3, Dip);
          }
//----------------------------------------------------------------------------------------------------          
          client.print("<form method=get>IP-Adr.: <input type=number min=10 max=254 name=A value=");
          client.println(ip[0]);
          client.print(">.<input type=number min=0 max=254 name=B value=");
          client.println(ip[1]);
          client.print(">.<input type=number min=0 max=254 name=C value=");
          client.println(ip[2]);
          client.print(">.<input type=number min=0 max=254 name=D value=");
          client.println(ip[3]);
          client.print("><br /> XBus Adr.: <input type=number min=1 max=31 name=XNet value=");
          client.print(XNetAddress);
          client.println("><br /><br />");
          client.println("<input type=submit></form>");
          client.println("</body></html>");
          break;
        }
        if (c == '\n') 
          currentLineIsBlank = true; // you're starting a new line
        else if (c != '\r') 
          currentLineIsBlank = false; // you've gotten a character on the current line
      }
    }
    client.stop();  // close the connection:
  }
}




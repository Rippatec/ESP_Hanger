
/*
This runs from 2 alkaline C-cells, no regulator. Starts about 3.2V when new.
Power consumption in deep-sleep (no power LED) is about 20uA

The first time you flash this program, do a FULL-ERASE to clear out old info.
The connect speed will be long for the first time. reboots or power-cycles
after the first will be LOTS quicker.

The figures you will get are very dependent on the router you are connecting to.
I typically get < 250ms from millis-startup (pre-start of sketch) to UDP post completed.

To get the really fast connects, your WiFi router must be on a fixed WiFi channel, not AUTO.
Most routers are AUTO by default.

HTU21D library by enjoyneering79 from: https://github.com/enjoyneering/

Heat index calculation from: https://en.wikipedia.org/wiki/Heat_index

Fast WiFi connect from: https://pokewithastick.net/?page_id=72

This sketch reads temperature and relative-humidity from a HTU21D sensor, then Heat Index
is calculated from the current samples.
It also collects the time taken for the WiFi to connect and reads the battery voltage.
These items are collated into a UDP packet and sent out over the WiFi as a broadcast message.
The ESP then goes into deep-sleep (3-min) till woken up to do it all again.

The sequence [ Wake->read sensor->connect to WiFi->send UDP->enter deep-sleep ] takes
about 1/4 sec. 
Honest... 250ms!

The handy thing about broadcast packets is they can be received by any (and multiple) devices
in the local subnet. No specific destination is targeted.
So it can be picked off the network by a raspberry-pi for long term logging, and displayed on a LCD
display with a ESP UDP listener (fairly trivial). It is not single-source to single-destination, but
single-source to any/all-destinations, in the local subnet.

*/



//#define SHOW_INFO			// uncomment to send the wifi connection details to serial


#include "ZZ-network.h"				// my network settings
#include <ESP8266WiFi.h>			// wifi lib
#include <WiFiUdp.h>				// UDP lib
#include <wire.h>					// I2C lib
#include <HTU21D.h>					// sensor lib


HTU21D	myHTU21D(HTU21D_RES_RH12_TEMP14);		// sensor at max resolution
WiFiUDP udp;

float temperature, humidity;

// i2c pins
#define SDApin 5	// GPIO5
#define SCLpin 4	// GPIO4


// this configures the internal ADC to read VCC. If chip has an external ADC input
// it must be left unconnected.
ADC_MODE(ADC_VCC);			

unsigned long t1, t2, t3;		// for millis() timestamps
int counter;					// used while connecting

float adcfrigg = 0.9900;		// a calibration factor for the internal ADC
float battery = 0;

String payload;					// to build the UDP string

IPAddress local;				// copy of local IP address
IPAddress lb;					// modified to make Local-Broadcast addr

// constants used to calculate heat-index in Celsius
const float c1 = -8.78469475556;
const float c2 = 1.61139411;
const float c3 = 2.33854883889;
const float c4 = -0.14611605;
const float c5 = -0.012308094;
const float c6 = -0.0164248277778;
const float c7 = 0.002211732;
const float c8 = 0.00072546;
const float c9 = -0.000003582;

float Hindex;		// calculated heat-index

// ===================================================================================
// note that there is no WiFi.begin() in the setup.
// It IS accessed in a function the 1st time the sketch is run
// so that the details can be stored, but any restarts after that
// are connected by the saved credentials and it happens MUCH faster.

void setup(void) {
	t1 = millis();							// first timestamp

	WiFi.config(IPA, GATE, MASK, DNS);		// supply ALL the items so no need of DHCP negotiation time.
	
	system_deep_sleep_set_option(2);		// Option 2 is WAKE_NO_RFCAL

	Serial.begin(115200);					// start up serial port. only needed for testing
	Serial.println();

	Wire.begin(SDApin, SCLpin);				// start up I2C bus
	myHTU21D.begin();						// initialise sensor

	// allocate fixed length string space
	payload.reserve(400);
							
// at this time, WiFi is connecting in the background, its a good time to read the sensor so data
// is ready for when connection completed
	humidity = myHTU21D.readCompensatedHumidity();
	temperature = myHTU21D.readTemperature();
	doHeatIndex();

// bit of a sanity check. If sensor not found set values to zero.
// Maybe not post unless you want to see its alive, but sensor bad. Battery voltage is still reported.
	if ((humidity == 255) || (temperature == 255)) {
		humidity =0; 
		temperature =0;
		Hindex=0;
	}

	getVoltage();		// reads VCC so can check battery state

// so even though no WiFi.connect() (so far), check and see if we are connecting.
// The 1st time sketch runs, this will time-out and THEN it accesses WiFi.connect().
// After the first time (and a successful initial connect), next time it connects very fast
	while (WiFi.status() != WL_CONNECTED) {
		delay(5);			// 5 ms
		if (++counter > 1000) break;     // 5 sec timeout (1000 * 5ms)
	}
//if timed-out, connect the slow-way. This happens 1st time after programming
	if (counter > 1000) launchSlowConnect();

	t2 = millis();				// second time-stamp. WiFi is now connected

	make_payload();				// create string
	Serial.println(payload);	// show to serial

	local = WiFi.localIP();		// get the working IP address. 
	lb = local;					// copy it
	lb[3] = 255;				// and adjust to make a broadcast version (assuming a 255.255.255.0 mask)


// this prints out the connection details for the connected wifi network.
// maybe show this stuff to start with, so you can get the router MAC address and wifi channel.
#ifdef SHOW_INFO
//	Serial.println("\n");
	Serial.print("Connected to AP: ");
	Serial.println(ssid);
	Serial.print("AP-MAC: ");
	Serial.println(WiFi.BSSIDstr());
	Serial.print("AP-WiFi channel: ");
	Serial.println(WiFi.channel());
	Serial.print("Gadget IP address: ");
	Serial.println(local);
	Serial.println();
#endif // 


	sendBroadcast();	// UDP send local broadcast. 
	delay(2);			// give the UDP a chance to send the data

	t3 = millis();			// 3rd time-stamp. Network transaction completed
//	Serial.println(t3);		// can only see this one by serial port (if required)
//	delay(2);				// time so serial can send.


	system_deep_sleep_instant(180000000);  // 180 sec

}
// ===================================================================================


// ------------------------------------------------------------------------------------------
void loop(void) {	
	// We never actually get here as it all happens during setup(), then shuts down.
}
// ------------------------------------------------------------------------------------------


// reads the chip VCC, takes 8 samples and averages. End result is modified by a multiplier [adcfrigg]
// When initial testing, compare the reported voltage to a measured voltage (with a multi-meter)
// and work out the multiplier to correct it. The ESP has a pretty awful ADC
void getVoltage() {
	int val = 0;
	for (int x = 0; x < 8; x++) val += ESP.getVcc();
	battery = (((val / 8) / 1024.00f) * adcfrigg);		// esp ADC is poor, apply frigg-factor!
}


// 1st run after flash has been wiped, we land here to do a 'conventional' WiFi-begin.
// this is accessed when the initial run fails to connect because no (or old) WiFi info
// By supplying all the WiFi details, the new connection is much faster.
void launchSlowConnect() {
	Serial.println("No (or wrong) saved WiFi credentials. Doing a fresh connect.");
	counter = 0;
	// persistent and auto-connect should be true by default, but lets make sure.
	if (!WiFi.getAutoConnect()) WiFi.setAutoConnect(true);	// autoconnect from saved credentials
	if (!WiFi.getPersistent()) WiFi.persistent(true);		// save the wifi credentials to flash


// Note the two forms of WiFi.begin() below. If the first version is used
// then no wifi-scan required as the RF channel and the AP mac-address are all provided.
// so next boot, all this info is used for a fast connect.
// If the second version is used, a WiFi scan happens (and for me) adds about 2-seconds
// to my connect time.

	WiFi.begin(ssid, password, channel, target_mac, true);		// this method is the fastest 
	//WiFi.begin(ssid, password);


	// now loop waiting for good connection, or reset
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
		if (++counter > 20) {				// allow up to 10-sec to connect to wifi
			Serial.println("WiFi connection failed...");
			delay(10);	// so the serial message has time to get sent

// since we didn't connect, no point in staying awake, just reboot.
// Another possibility is go back to deep-sleep to try again later
			ESP.reset();		// reboot
		}
	}
	Serial.println("WiFi connected and credentials saved");
}
// ------------------------------------------------------------------------------------------





// Send a UDP broadcast on the local network so all the transaction details and times
// can be viewed on a UDP viewer/terminal. I use an android app or a windows UDP viewer
void sendBroadcast() {
	udp.beginPacket(lb, 8092);		// lb is the broadcast address, port 8092
	udp.print(payload);				// send the data
	udp.endPacket();
}


// This happens to be in InfluxDB line-format but its easilly human readable.
// I post it to a Raspberry-Pi running Influx and Grafana
void make_payload() {

	payload = "hanger temp=";
	payload += temperature;
	payload += "\n";

	payload += "hanger humid=";
	payload += humidity;
	payload += "\n";

	payload += "hanger Ctime=";
	payload += t2;
	payload += "\n";

	payload += "hanger bv=";
	payload += battery;
	payload += "\n";

	payload += "hanger HI=";
	payload += Hindex;
	payload += "\n";

}


void doHeatIndex() {
	float rsq = humidity * humidity;
	float tsq = temperature * temperature;
	float T = temperature;
	float R = humidity;
	Hindex = c1 + (c2 * T) + (c3 * R) + (c4 * T * R) + (c5 * tsq) + (c6 * rsq) + (c7 * tsq * R) + (c8 * T * rsq) + (c9 * tsq * rsq);

}






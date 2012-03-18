/**
 * Bubo - Yaesu ethernet rotor computer control interface
 *
 * Based upon work by Tom Doyle
 */
#include "Arduino.h"
#include <stdlib.h>
#include "SPI/SPI.h"
#include "Ethernet/Ethernet.h"

/** Bubo controller's ethernet shield MAC address */
static byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };

/** The Ethernet Server */
EthernetServer server(23);

/** Server IP */
IPAddress ip(192, 168, 0, 50);
/** Server network gateway */
IPAddress gateway(192, 168, 0, 1);
/** Server network subnet */
IPAddress subnet(255, 255, 0, 0);

// ------------------------------------------------------------
// ---------- you may wish to adjust these values -------------
// ------------------------------------------------------------

// A/D converter parameters
/*
 AFTER you have adjusted your G-5500 control box as per the manual
 adjust the next 4 parameters. The settings interact a bit so you may have
 to go back and forth a few times. Remember the G-5500 rotors are not all that
 accurate (within 4 degrees at best) so try not to get too compulsive when
 making these adjustments.
 */
const long azAdZeroOffset = 325; // adjust to zero out lcd az reading when control box az = 0
const long elAdZeroOffset = 0; // adjust to zero out lcd el reading when control box el = 0

/*
 10 bit A/D converters in the Arduino have a max value of 1023
 for the azimuth the A/D value of 1023 should correspond to 450 degrees
 for the elevation the A/D value of 1023 should correspond to 180 degrees
 integer math is used so the scale value is multiplied by 100 to maintain accuracy
 the scale factor should be 100 * (1023 / 450) for the azimuth
 the scale factor should be 100 * (1023 / 180) for the elevation
 */
const long azScaleFactor = 232; //  adjust as needed
const long elScaleFactor = 568; //  adjust as needed

// pins
const byte G5500UpPin = 8; // elevation rotor up control line
const byte G5500DownPin = 9; // elevation rotor down control line
const byte G5500LeftPin = 10; // azimuth rotor left control line
const byte G5500RightPin = 11; // azimuth rotor right control line
const byte azimuthInputPin = A0; // azimuth analog signal from G5500
const byte elevationInputPin = A1; // elevation analog signal from G5500

// take care if you lower this value -  wear or dirt on the pots in your rotors
// or A/D converter jitter may cause hunting if the value is too low.
long closeEnough = 100; // tolerance for az-el match in rotor move in degrees * 100

// ------------------------------------------------------------
// ------ values from here down should not need adjusting -----
// ------------------------------------------------------------

// rotor
const long maxRotorAzimuth = 45000L; // maximum rotor azimuth in degrees * 100
const long maxRotorElevation = 18000L; // maximum rotor elevation in degrees * 100

long rotorAzimuth = 0L; // current rotor azimuth in degrees * 100
long rotorElevation = 0L; // current rotor azimuth in degrees * 100
long azimuthTemp = 0L; // used for gs232 azimuth decoding
long elevationTemp = 0L; // used for gs232 elevation decoding
long newAzimuth = 0L; // new azimuth for rotor move
long newElevation = 0L; // new elevation for rotor move
long previousRotorAzimuth = 0L; // previous rotor azimuth in degrees * 100
long previousRotorElevation = 0L; // previous rotor azimuth in degrees * 100

unsigned long rtcLastDisplayUpdate = 0UL; // rtc at start of last loop
unsigned long rtcLastRotorUpdate = 0UL; // rtc at start of last loop
unsigned long displayUpdateInterval = 500UL; // display update interval in mS
unsigned long rotorMoveUpdateInterval = 100UL; // rotor move check interval in mS

boolean gs232WActive = false; // gs232 W command in process
int gs232AzElIndex = 0; // position in gs232 Az El sequence
long gs232Azimuth = 0; // gs232 Azimuth value
long gs232Elevation = 0; // gs232 Elevation value

boolean azimuthMove = false; // azimuth move needed
boolean elevationMove = false; // elevation move needed

String azRotorMovement; // string for az rotor move display
String elRotorMovement; // string for el rotor move display

// run once at reset
//
void readAzimuth() {
	long sensorValue = analogRead(azimuthInputPin);
	rotorAzimuth = ((sensorValue * 10000) / azScaleFactor) - azAdZeroOffset;
}

//
// update elevation rotor move
//
void updateElevationMove() {
	// calculate rotor move
	long rotorMoveEl = newElevation - rotorElevation;

	if (rotorMoveEl > 0) {
		elRotorMovement = "  U ";
		elRotorMovement = elRotorMovement + String(newElevation / 100);
		digitalWrite(G5500DownPin, LOW);
		digitalWrite(G5500UpPin, HIGH);
	}
	else {
		if (rotorMoveEl < 0) {
			elRotorMovement = "  D ";
			elRotorMovement = elRotorMovement + String(newElevation / 100);
			digitalWrite(G5500UpPin, LOW);
			digitalWrite(G5500DownPin, HIGH);
		}
	}
}

//
// update azimuth rotor move
//
void updateAzimuthMove() {
	// calculate rotor move
	long rotorMoveAz = newAzimuth - rotorAzimuth;
	// adjust move if necessary
	if (rotorMoveAz > 18000) // adjust move if > 180 degrees
			{
		rotorMoveAz = rotorMoveAz - 180;
	}
	else {
		if (rotorMoveAz < -18000) // adjust move if < -180 degrees
				{
			rotorMoveAz = rotorMoveAz + 18000;
		}
	}

	if (rotorMoveAz > 0) {
		azRotorMovement = "  R ";
		azRotorMovement = azRotorMovement + String(newAzimuth / 100);
		digitalWrite(G5500LeftPin, LOW);
		digitalWrite(G5500RightPin, HIGH);
	}
	else {
		if (rotorMoveAz < 0) {
			azRotorMovement = "  L ";
			azRotorMovement = azRotorMovement + String(newAzimuth / 100);
			digitalWrite(G5500RightPin, LOW);
			digitalWrite(G5500LeftPin, HIGH);
		}
	}
}

//
// read azimuth from G5500
//
void readElevation() {
//	long sensorValue = analogRead(elevationInputPin);
	long sensorValue = 0;
	rotorElevation = (sensorValue * 10000) / elScaleFactor;
}

//
// read azimuth from G5500
//
//
// decode gs232 commands
//
void processAzElNumeric(char character) {
	switch (gs232AzElIndex) {
		case 0: // first azimuth character
		{
			azimuthTemp = (character - 48) * 100;
			gs232AzElIndex++;
			break;
		}

		case 1: {
			azimuthTemp = azimuthTemp + (character - 48) * 10;
			gs232AzElIndex++;
			break;
		}

		case 2: // final azimuth character
		{
			azimuthTemp = azimuthTemp + (character - 48);
			gs232AzElIndex++;

			// check for valid azimuth
			if ((azimuthTemp * 100) > maxRotorAzimuth) {
				gs232WActive = false;
				newAzimuth = 0L;
				newElevation = 0L;
			}
			break;
		}

		case 3: // first elevation character
		{
			elevationTemp = (character - 48) * 100;
			gs232AzElIndex++;
			break;
		}

		case 4: {
			elevationTemp = elevationTemp + (character - 48) * 10;
			gs232AzElIndex++;
			break;
		}

		case 5: // last elevation character
		{
			elevationTemp = elevationTemp + (character - 48);
			gs232AzElIndex++;

			// check for valid elevation
			if ((elevationTemp * 100) > maxRotorElevation) {
				gs232WActive = false;
				newAzimuth = 0L;
				newElevation = 0L;
			}
			else // both azimuth and elevation are ok
			{
				Serial.println(newAzimuth + " : " + newElevation);
				// set up for rotor move
				newAzimuth = azimuthTemp * 100;
				newElevation = elevationTemp * 100;
				azimuthMove = true;
				elevationMove = true;
			}
			break;
		}
		default: {
			// should never get here
			break;
		}
	}
}

//void displayAz(long az) {
//	// clear azimuth line
//	lcdSerial.print(line0, BYTE);
//	lcdSerial.print("                ");
//
//	//  adjust value for display
//	double azFloat = az;
//	azFloat = azFloat / 100.0;
//
//	// position lcd cursor on top line
//	lcdSerial.print(line0, BYTE);
//
//	// display azimuth
//	lcdSerial.print("AZ ");
//	// pad with spaces
//	if (azFloat < 10.0) {
//		lcdSerial.print(" ");
//	}
//	if (azFloat < 100.0) {
//		lcdSerial.print(" ");
//	}
//	lcdSerial.print(azFloat, 1);
//	lcdSerial.print(azRotorMovement);
//}

//void displayEl(long el) {
//	// clear elevation line  lcdSerial
//	lcdSerial.print(line1, BYTE);
//	lcdSerial.print("                ");
//
//	//  adjust value for display
//	double elFloat = el;
//	elFloat = elFloat / 100.0;
//
//	// position lcd cursor on bottom line
//	lcdSerial.print(line1, BYTE);
//
//	// display elevation
//	lcdSerial.print("EL ");
//	// pad with spaces
//	if (elFloat < 10.0) {
//		lcdSerial.print(" ");
//	}
//	if (elFloat < 100.0) {
//		lcdSerial.print(" ");
//	}
//	lcdSerial.print(elFloat, 1);
//	lcdSerial.print(elRotorMovement);
//}

void decodeGS232(char character) {
	Serial.println("Received " + character);
	switch (character) {
		case 'w': // gs232 W command
		case 'W': {
			Serial.println("W command started");
			gs232WActive = true;
			gs232AzElIndex = 0;
			break;
		}

			// numeric - azimuth and elevation digits
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9': {
			if (gs232WActive) {
				processAzElNumeric(character);
			}
			break;
		}

		default: {
			// ignore everything else
			break;
		}
	}
}

//
// process az el numeric characters from gs232 W command
//
//
// display az el on display
//
void displayAzEl(long az, long el) {
	// display azimuth - filter A/D noise
	if (abs(rotorAzimuth - previousRotorAzimuth) > 50) {
		previousRotorAzimuth = rotorAzimuth;
//		displayAz(az);
	}

	// display elevation - filter A/D noise
	if (abs(rotorElevation - previousRotorElevation) > 50) {
		previousRotorElevation = rotorElevation;
//		displayEl(el);
	}

}

//
// display elevation - pad to length 8
//   error message if < 0 or > max
//
//
// display azimuth - pad to length 8
//   error message if < 0 or > max
//
void loop() {
	EthernetClient client = server.available();

	if (client) {
		if(client.available() > 0) {
			byte inByte = client.read();
			decodeGS232(inByte);
		}
	}

	// check for serial data
//	if (Serial.available() > 0) {
//		decodeGS232(Serial.read());
//	}

	// get current elapsed time (since program started)
	unsigned long elapsedTime = millis(); // get current rtc value

	// check for rtc overflow - skip this cycle if overflow
	if (elapsedTime > rtcLastDisplayUpdate) // overflow if not true    _rotorMoveUpdateInterval
			{
		// update rotor movement if necessary
		if (elapsedTime - rtcLastRotorUpdate > rotorMoveUpdateInterval) {
			rtcLastRotorUpdate = elapsedTime; // reset rotor move timer base

			// AZIMUTH
			readAzimuth(); // get current azimuth from G-5500
			// see if azimuth move is required
			if ((abs(rotorAzimuth - newAzimuth) > closeEnough) && azimuthMove) {
				updateAzimuthMove();
			}
			else // no move required - turn off azimuth rotor
			{
				digitalWrite(G5500LeftPin, LOW);
				digitalWrite(G5500RightPin, LOW);
				azimuthMove = false;
				azRotorMovement = "        ";
			}

			// ELEVATION
			readElevation(); // get current elevation from G-5500
			// see if an elevation move is required
			if (abs(rotorElevation - newElevation) > closeEnough && elevationMove) {
				updateElevationMove();
			}
			else {
				digitalWrite(G5500UpPin, LOW);
				digitalWrite(G5500DownPin, LOW);
				elevationMove = false;
				elRotorMovement = "        ";
			}
		} // end of update rotor move

		// update display if necessary
		//		if (elapsedTime - rtcLastDisplayUpdate > displayUpdateInterval) {
		//			// update rtcLast
		//			rtcLastDisplayUpdate = elapsedTime;  // reset display update counter base
		//			displayAzEl(rotorAzimuth, rotorElevation);
		//		}
	}
	else // rtc overflow - just in case
	{
		// update rtcLast
		rtcLastDisplayUpdate = elapsedTime;
	}
}

void disengageAllRotorPins() {
	// set all the rotor control outputs low
	digitalWrite(G5500UpPin, LOW);
	digitalWrite(G5500DownPin, LOW);
	digitalWrite(G5500LeftPin, LOW);
	digitalWrite(G5500RightPin, LOW);
}

bool setupEthernetServer() {
	// start the Ethernet connection:
	Serial.println("Trying to get an IP address using DHCP");
	digitalWrite(G5500DownPin, HIGH);
	digitalWrite(G5500RightPin, HIGH);

	if (Ethernet.begin(mac) == 0) {
		Serial.print("Ethernet unavailable, DHCP failed");
		return false;
	}
	digitalWrite(G5500UpPin, HIGH);
	digitalWrite(G5500LeftPin, HIGH);

	server.begin();

	// print local IP address to LCD screen.
	// TODO change from serial to LCD screen print.
	Serial.print("My IP address: ");
	ip = Ethernet.localIP();
	for (byte thisByte = 0; thisByte < 4; thisByte++) {
		// print the value of each byte of the IP address:
		Serial.print(ip[thisByte], DEC);
		Serial.print(".");
	}

	Serial.println();


	delay(2000);
	disengageAllRotorPins();

	return true;
}


void setup() {
	// initialise rotor control pins as outputs
	pinMode(G5500UpPin, OUTPUT);
	pinMode(G5500DownPin, OUTPUT);
	pinMode(G5500LeftPin, OUTPUT);
	pinMode(G5500RightPin, OUTPUT);

	// set all the rotor control outputs low
	digitalWrite(G5500UpPin, LOW);
	digitalWrite(G5500DownPin, LOW);
	digitalWrite(G5500LeftPin, LOW);
	digitalWrite(G5500RightPin, LOW);

	// initialise serial ports:
	Serial.begin(9600); // control

	// initialise software uart used for lcd display
//	pinMode(LcdTxPin, OUTPUT);
//	lcdSerial.begin(9600);

	// initialise lcd display
//	lcdSerial.print(backLightOn, BYTE); // backlight on
//	lcdSerial.print(cursorOff, BYTE); // cursor off
//	lcdSerial.print(clearScreen, BYTE); // clear screen
//	delay(100); // wait for clear screen

	Serial.println("BUBO");
//	lcdSerial.println("BUBO");
	delay(2000);
//	lcdSerial.print(clearScreen, BYTE); // clear screen

	// set up rotor lcd display values
	readAzimuth(); // get current azimuth from G-5500
	previousRotorAzimuth = rotorAzimuth + 1000;
	readElevation(); // get current elevation from G-5500
	previousRotorElevation = rotorElevation + 1000;


	setupEthernetServer();
}


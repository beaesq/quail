/*	Bea Esquivel!
 *	Temperature and Relative Humidity Reader and Controller v1.0 04-18-16
 *	UPLOAD TO GIZDUINO+ 644 BOARD
 *	
 *	Four DHT22 sensors are used. The DHT22 can only be read from every 2 seconds. 
 *	Serial communication with Galileo is done using serialGalileo. (Keep serial  
 *	comm printed to Galileo under 15 chars, including \n.)
 *
 *	Default Controller Settings: 
 *		CHIE = auto, 
 *		SD saving = on, 
 *		Data collection interval = 30s
 */

//	LIBRARIES
#include "DHT22.h"	// DHT22 sensor library - Developed by Ben Adams - 2011
#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>	// For the built-in watchdog timer of the Atmega

typedef struct {
	
	int errorNum;
	double temp, cTemp;
	double rh, cRh;
	
} dhtdata;

dhtdata sensor[4];

/*	PIN SETTINGS
	0-1	: Serial communication
	2-3	: Galileo serial comm
	4-7	: Relay input
	14-17	: Relay LED power indicators
	A4	: Push button input
	21	: LCD
	23-27	: LCD
	28-31	: DHT22 data
*/
const int DHTpin = 28;	// Sensors are connected to pins DHTpin to DHTpin+3
const int pushPin = 4;	// Push button analog input pin; also change pinMode in setup()
const int coolingPin = 4;	// Cooling mechanism input pin
const int heatingPin = 5;	// Heating mechanism input pin
const int intakePin = 6;	// Intake fan input pin
const int exhaustPin = 7;	// Exhaust fan input pin
const int ledCoolingPin = 14;	// Cooling relay power indicator pin
const int ledHeatingPin = 15;	// Heating relay power indicator pin
const int ledIntakePin = 16;	// Intake relay power indicator pin
const int ledExhaustPin = 17;	// Exhaust relay power indicator pin

// Initialize DHT sensors at pins DHTpin to DHTpin+3
DHT22 dht_0 (DHTpin+0);
DHT22 dht_1 (DHTpin+1);
DHT22 dht_2 (DHTpin+2);
DHT22 dht_3 (DHTpin+3);

// Initialize LCD interface pins
LiquidCrystal lcd (27, 26, 25, 24, 23, 21);

// Initialize soft serial connection to Galileo
SoftwareSerial serialGalileo(2, 3); // TX, RX

//	CONTROLLER SETTINGS
const float minTemp = 18.0;	// Lowest temperature allowed
const float maxTemp = 30.0;	// Highest temperature allowed
const float minAccTemp = 21.0, maxAccTemp = 27.0;	// Acceptable temperature range
const float maxRHum = 80.0;	// Maximum relative humidity 
const float maxAccRHum = 60.0;	// Acceptable relative humidity value

boolean intake = false, exhaust = false, heating = false, cooling = false;	// Controller on/off

unsigned long interval = 30000;	// Time interval (in ms) for data collection

int setting_cooling = 2, setting_heating = 2, setting_intake = 2, setting_exhaust = 2;
boolean setting_SD = true;

// MISC GLOBAL VARIABLES
unsigned long prevTime, curTime = 0;	// For data collection timer
unsigned long startTime, forcedResetInterval = 10800000;	//3hrs
unsigned long scrnChangeTime, scrnTimeout = 300000;

float aveTemp = 0.0, aveRHum = 0.0;	// Average temperature and RH

char action[5];	// Actions string (which relays are turned on); for print purposes

String strDate = "YYYY/MM/DD", strTime = "hh:mm:ss";

int curDay = 0; // For date checking for filename

byte degree[8] = { // Degree character for LCD printing
  B11100,
  B10100,
  B11100,
  B00000,
  B00000,
  B00000,
  B00000,
};

int mainscreen = 0, errorsRead = 0;

void setup () {
	
	wdt_disable();
	
	// Set pins connected to relays
	pinMode(intakePin, OUTPUT);
	pinMode(exhaustPin, OUTPUT);
	pinMode(coolingPin, OUTPUT);
	pinMode(heatingPin, OUTPUT);
	
	// Turn off all relays
	digitalWrite(heatingPin, LOW);
	digitalWrite(coolingPin, LOW);
	digitalWrite(intakePin, LOW);
	digitalWrite(exhaustPin, LOW);
	
	// Set pins connected to relay LED's
	pinMode(ledCoolingPin, OUTPUT);
	pinMode(ledExhaustPin, OUTPUT);
	pinMode(ledHeatingPin, OUTPUT);
	pinMode(ledIntakePin, OUTPUT);
	
	// Turn off relay LED's
	digitalWrite(ledCoolingPin, LOW);
	digitalWrite(ledExhaustPin, LOW);
	digitalWrite(ledHeatingPin, LOW);
	digitalWrite(ledIntakePin, LOW);
	
	// Set push buttons' analog input pin
	pinMode(A4, INPUT_PULLUP);	// Internal pull-up needed to avoid board explosion
	
	// LCD setup
	lcd.begin(16, 2);
	lcd.createChar(0, degree);	// Make degree character for printing
	
	// Print setup sequence on LCD
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Temp & RH Reader");
	lcd.setCursor(0,1);
	lcd.print("0407.01  0407.01");
	delay(2000);
	lcd.setCursor(0,1);
	lcd.print("Wait for Galileo");	// Wait for Galileo to finish booting
	delay(10000);
	
	// Serial comm setup
	Serial.begin(9600); 
	serialGalileo.begin(4800);
	Serial.println("Starting...");
	
	// Reset Galileo
	serialGalileo.println("\nreset please");	//The \n clears Galileo's serial buffer
	
	action[4] = '\0';	// Add null character to end of action string
	
	// The DHT22 requires a minimum 2s warm-up. Added a few more seconds for Galileo reset.
	delay (20000);
	
	lcd.setCursor(0,1);
	lcd.print("Wait for reading");
	
	wdt_enable(WDTO_4S);
	wdt_reset();
	
	Serial.println("Setup finished");
	
	// Print header line
	printToSerial(1);
	
	startTime = millis();
}

void loop () {
	
	wdt_reset();

	int button = 0;
	
	// Check if a button was pressed; change main LCD screen or open controller settings menu
	button = readPushButton();
	
	switch (button) {
		
		case 1:	// Change main LCD screen left
			if (mainscreen == 0) 
				mainscreen = 4;
			else
				mainscreen--;
			
			printToLCD();
			scrnChangeTime = millis();
			break;
		
		case 2:	// Change main LCD screen right
			if (mainscreen == 4) 
				mainscreen = 0;
			else
				mainscreen++;
			
			printToLCD();
			scrnChangeTime = millis();
			break;
			
		case 4:	// Open settings menu
			wdt_reset();
			wdt_disable();
			lcd.begin(16, 2);
			changeSettings();
			lcd.begin(16, 2);
			printToLCD();
			wdt_enable(WDTO_4S);
			break;
	}
	
	// Get current time
	curTime = millis();
	
	// If interval time has passed, get new measurements from sensors and adjust fans accordingly
	if (curTime - prevTime >= interval) {
					
		// Save current time as latest time of measurement
		prevTime = curTime;
		
		// Get current date & time
		getTimeDate();
		
		// Get sensor readings
		getDHTReadings();
		
		// Compute average temperature and RH and set relay booleans
		setRelayBooleans();
		
		// Turn on/off relays
		setRelays();
		
		// Show average measurements/error warning on LCD
		printToLCD();

		// Show measurements/errors on serial monitor
		printToSerial(0);
		
		// Save data to Galileo
		if (setting_SD == true)
			saveToGalileo();
		
		// Force controller to reset every 3 hrs
		if (curTime - startTime >= forcedResetInterval) {
			Serial.println("Forcing reset of box");
			lcd.clear();
			lcd.setCursor(0,0);
			lcd.print("Resetting box");
			while (1) {
				// use the watchdog timer to reset the board
			}
		}
	}
	
	// LCD screen timeout: if screen stays at sensors for more than 5 mins, return to main screen
	if ((millis() - scrnChangeTime >= scrnTimeout) && (mainscreen != 0)) {
		mainscreen = 0;
		printToLCD();
	}
}

void printToLCD () { 
// Display measurements, date, time, actions on LCD screen
	
	lcd.begin(16, 2);

	// Change screen accdg to buttons
	switch (mainscreen) {
		
		case 0: // Main screen: time, date, actions, average measurements
			LCD_main();
			break;
		
		case 1: // Sensor 1
			LCD_sensor(0);
			break;
		
		case 2: // Sensor 2
			LCD_sensor(1);
			break;
		
		case 3: // Sensor 3
			LCD_sensor(2);
			break;
		
		case 4: // Sensor 4
			LCD_sensor(3);
			break;	
	}
}

void LCD_main () { 
// Main LCD screen with the date and time and averages
	
	/*	MM-DD hh:mm CHIE
		Temp:##.##°RH:##
	*/
	
	// Convert floats to string for proper decimal point placement
	char ATemp[] = "00.0", ARH[] = "00";
	if (abs(aveTemp) >= 100) {
		dtostrf(aveTemp, 4, 0, ATemp);
	}
	else if (abs(aveTemp) < 10) {
		dtostrf(aveTemp, 4, 2, ATemp);
	}
	else {
		dtostrf(aveTemp, 4, 1, ATemp);
	}
	
	dtostrf(aveRHum, 2, 0, ARH);
	
	// First row
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print(strDate.substring(5, 10));
	lcd.print(" ");
	lcd.print(strTime.substring(0, 5));
	lcd.print(" ");
	lcd.print(action);
		
	// Print second row
	lcd.setCursor(0,1);
	if (errorsRead == 4) {	// All sensors returned an error
		lcd.print("All snsrs errord");
				
	} else {	// At least one sensor did not error
		lcd.print("Temp:");
		lcd.print(ATemp);
		lcd.write(byte(0));
		lcd.print("RH:");
		if (aveRHum >= 100) {
			lcd.print("100");
		}
		else {
			lcd.print(ARH);
			lcd.print("%");
		}
	}
}

void LCD_sensor (int sensorNum) { 
// Sensor LCD screen: shows temp and RH OR error msg per sensor
	
	lcd.clear();
	lcd.setCursor(0,0);
	
	char strTemp[] = "00.0", strRH[] = "00";
	
	if (sensor[sensorNum].errorNum == 0) {	// No error
		/*	< Sensor #     >
			Temp:##.##°RH:##
		*/
		
		// Convert floats to string for proper decimal point placement
		if (abs(sensor[sensorNum].cTemp) >= 100) {
			dtostrf(sensor[sensorNum].cTemp, 4, 0, strTemp);	
		}
		else if (abs(sensor[sensorNum].cTemp) < 10) {
			dtostrf(sensor[sensorNum].cTemp, 4, 2, strTemp);	
		}
		else {
			dtostrf(sensor[sensorNum].cTemp, 4, 1, strTemp);	
		}
		
		dtostrf(sensor[sensorNum].cRh, 2, 0, strRH);
		
		lcd.print("< Sensor ");
		lcd.print(sensorNum+1);
		lcd.print("     >");
		lcd.setCursor(0,1);
		lcd.print("Temp:");
		lcd.print(strTemp);
		lcd.write(byte(0));
		lcd.print("RH:");
		if (aveRHum >= 100) {
			lcd.print("100");
		}
		else {
			lcd.print(strRH);
			lcd.print("%");
		}
		
	
	} else if (sensor[sensorNum].errorNum == 1) {	// Checksum error
		/*	< Snsr # chksm >
			Temp:##.##°RH:##
		*/
		
		// Convert floats to string for proper decimal point placement
		if (abs(sensor[sensorNum].cTemp) >= 100) {
			dtostrf(sensor[sensorNum].cTemp, 4, 0, strTemp);	
		}
		else if (abs(sensor[sensorNum].cTemp) < 10) {
			dtostrf(sensor[sensorNum].cTemp, 4, 2, strTemp);	
		}
		else {
			dtostrf(sensor[sensorNum].cTemp, 4, 1, strTemp);	
		}
			
		dtostrf(sensor[sensorNum].cRh, 2, 0, strRH);
		
		lcd.print("< Snsr ");
		lcd.print(sensorNum+1);
		lcd.print(" chksm >");
		lcd.setCursor(0,1);
		lcd.print("Temp:");
		lcd.print(strTemp);
		lcd.write(byte(0));
		lcd.print("RH:");
		if (aveRHum >= 100) {	//add rounding fn; lcd.print rounds rh
			lcd.print("100");
		}
		else {
			lcd.print(strRH);
			lcd.print("%");
		}
	
	} else {	// Error num: 2/3/4/5/6/7
		/*	< Snsr # error >
			error message :)
		*/
		lcd.print("< Snsr ");
		lcd.print(sensorNum+1);
		lcd.print(" error >");
		lcd.setCursor(0,1);
		
		// Print error message
		switch (sensor[sensorNum].errorNum) {
			
			case 2:
				lcd.print("Bus Hung");
				break;
			
			case 3:
				lcd.print("Not Present");
				break;
			
			case 4:
				lcd.print("ACK Time Out");
				break;
			
			case 5:
				lcd.print("Sync Timeout");
				break;
			
			case 6:
				lcd.print("Data Timeout");
				break;
			
			case 7:
				lcd.print("Polled too quick");
				break;
		
		}
	}
}

void setRelays () { 
// Turn on/off relays, set indicator LED's, and make action string
	
	if (cooling == true) {
		digitalWrite(coolingPin, HIGH);
		digitalWrite(ledCoolingPin, HIGH);
		action[0] = 'C';
	} else {
		digitalWrite(coolingPin, LOW);
		digitalWrite(ledCoolingPin, LOW);
		action[0] = ' ';
	}
	
	if (heating == true) {
		digitalWrite(heatingPin, HIGH);
		digitalWrite(ledHeatingPin, HIGH);
		action[1] = 'H';
	} else {
		digitalWrite(heatingPin, LOW);
		digitalWrite(ledHeatingPin, LOW);
		action[1] = ' ';
	}
	
	if (intake == true) {
		digitalWrite(intakePin, HIGH);
		digitalWrite(ledIntakePin, HIGH);
		action[2] = 'I';
	} else {
		digitalWrite(intakePin, LOW);
		digitalWrite(ledIntakePin, LOW);
		action[2] = ' ';
	}
	
	if (exhaust == true) {
		digitalWrite(exhaustPin, HIGH);
		digitalWrite(ledExhaustPin, HIGH);
		action[3] = 'E';
	} else {
		digitalWrite(exhaustPin, LOW);
		digitalWrite(ledExhaustPin, LOW);
		action[3] = ' ';
	}
}

void setRelayBooleans () { 
// Compute averages and set relay boolean values


	// If all sensors error'd, turn off all systems
	if (errorsRead == 4) {	
	
		cooling = false;
		heating = false;
		intake = false;
		exhaust = false;
	} else {
	// If at least one sensor returned a reading, set relay booleans accdg to avgs
	
		// Turn on relays if average values reach min/max allowable levels
		
		// If aveRHum is too high, ventilation is needed, so turn on intake and exhaust fans
		if (aveRHum >= maxRHum) {
			intake = true;
			exhaust = true;
		}
		
		// If aveTemp is too low, heating is needed, so turn on heating
		if (aveTemp <= minTemp) {
			cooling = false;
			heating = true;
			intake = false;
			exhaust = false;
		} 
		// If aveTemp is too high, cooling is needed, so turn on cooling and intake and exhaust fans
		else if (aveTemp >= maxTemp) {
			cooling = true;
			heating = false;
			intake = true;
			exhaust = true;
		}
		
		// Turn off relays if average values are within appropriate range
		//Serial.println("turning off?");
		// If aveTemp is within acceptable range, turn off cooling/heating
		if (aveTemp >= minAccTemp) {
			heating = false;
			
			// If aveRHum is also within acceptable range, turn off fans
			if (aveRHum <= maxAccRHum) {
				intake = false;
				exhaust = false;
			}
		}
		if (aveTemp <= maxAccTemp) {
			cooling = false;
			
			// If aveRHum is also within acceptable range, turn off fans
			if (aveRHum <= maxAccRHum) {
				intake = false;
				exhaust = false;
			}
		}
	}
	
	// Check settings; override auto changes
	if (setting_cooling == 0) {
		cooling = true;
	} else if (setting_cooling == 1) {
		cooling = false;
	}
	if (setting_heating == 0) {
		heating = true;
	} else if (setting_heating == 1) {
		heating = false;
	}
	if (setting_intake == 0) {
		intake = true;
	} else if (setting_intake == 1) {
		intake = false;
	}
	if (setting_exhaust == 0) {
		exhaust = true;
	} else if (setting_exhaust == 1) {
		exhaust = false;
	}
	
}

void printToSerial (int opt) { 
// Print measurements, average measurements, and actions to serial monitor
	
	// For printing header line; used in setup() only
	if (opt == 1) {
		Serial.println("Time\t\tTemp1\tRH1\tError1\tTemp2\tRH2\tError2\tTemp3\tRH3\tError3\tTemp4\tRH4\tError4\tAvgTemp\tAvgRH\tActions");
		return;
	}
	
	// Print date/time
	Serial.print("\n");
	Serial.print(strTime);
	Serial.print("\t");
	
	// Print sensor readings (temp and rh) and error codes
	for (int i = 0; i < 4; i++) {
		if (sensor[i].errorNum == 0 || sensor[i].errorNum == 1) {
		// If no error or checksum error
			Serial.print(sensor[i].cTemp);
			Serial.print("\t");
			Serial.print(sensor[i].cRh);
			Serial.print("\t");
		} else {
		// If error
			Serial.print("-\t-\t");		
		}
		
		Serial.print(sensor[i].errorNum);
		Serial.print(" ");
		
		switch (sensor[i].errorNum) {
			case 0:
				Serial.print("none");
				break;
			case 1:
				Serial.print("chksm");
				break;
			case 2:
				Serial.print("bus");
				break;
			case 3:
				Serial.print("n.p.");
				break;
			case 4:
				Serial.print("ackTO");
				break;
			case 5:
				Serial.print("syncTO");
				break;
			case 6:
				Serial.print("dataTO");
				break;
			case 7:
				Serial.print("quick");
				break;
		}
		
		Serial.print("\t");	
	}
	
	if (errorsRead == 4) {
		Serial.print("All error'd\t");
	} else {
		Serial.print(aveTemp);
		Serial.print("\t");
		Serial.print(aveRHum);
		Serial.print("\t");
	}
	
	Serial.print(action);
	Serial.println("\t");
}

void saveToGalileo () { 
// Send DHT readings to Galileo board; tell Galileo to save data to SD

	Serial.println("Asking to save to SD... ");
	serialGalileo.println("save SD");
	serialGalileo.println(action);
}

void getTimeDate () { 
// Gets current time and date from Galileo and saves to global time and date variables	
	
	unsigned long gtdStartTime, gtdTimeout = 3000;
	gtdStartTime = millis();
	
	// Clear buffer first
	while (serialGalileo.available() > 0) {
		char recieved = serialGalileo.read();
		wdt_reset();
	}
	
	serialGalileo.println("time please");
	
	// Wait for Galileo to send, then save string from buffer
	String strTimeDate = "";
	boolean ok = false;
	while (ok == false) {	// wait for galileo
		wdt_reset();
		while (serialGalileo.available() > 0) {	
			wdt_reset();
			char received = serialGalileo.read();
			if (received == '\n') {
				ok = true;
				break;
			}
			else if (strTimeDate.length() >= 21) {
				Serial.println("time/date received is too long, setting to default");
				strTimeDate = "1990/01/01	00:00:00";
				ok = true;
				break;
			}
			else {
				if (isAscii(received)) { 
					strTimeDate += received;
				} else {
					// Discard unrecognizable string
					Serial.print("Discarded invalid input from serialGalileo: ");
					Serial.println(received);
					strTimeDate = "";
				}
			} 
		}
		
		if (millis() - gtdStartTime >= gtdTimeout) { 
			Serial.println("getTimeDate() serialGalileo timeout, setting to default");
			strTimeDate = "1990/01/01	00:00:00";
			ok = true;
			break;
		} 
	}
	
	// Check if date & time are ok, if not, wait 5s and request new date & time
	// Galileo uses "1990/01/01	00:00:00" as strTimeDate when there is an error in getClock()
	if (strTimeDate == "1990/01/01	00:00:00") {
		Serial.println("Error in getTimeDate: Galileo sent default strTimeDate");
		wdt_disable();
		delay(5000);
		wdt_enable(WDTO_4S);
		
		// Ask for new date/time again; if default is sent again, just continue with default date and time
		serialGalileo.println("time please");

		// Wait for Galileo to send, then save string from buffer
		String strTimeDate = "";
		bool ok = false;
		while (ok == false) {	// wait for galileo
			wdt_reset();
			while (serialGalileo.available() > 0) {	
				wdt_reset();
				char received = serialGalileo.read();
				if (received == '\n') {
					ok = true;
					break;
				}
				else if (strTimeDate.length() >= 20) {
					Serial.println("time/date received is too long, setting to default");
					strTimeDate = "1990/01/01	00:00:00";
					ok = true;
					break;
				}
				else {
					if (isAscii(received)) { 
						strTimeDate += received;
					} else {
						// Discard unrecognizable string
						Serial.print("Discarded invalid input from serialGalileo: ");
						Serial.println(received);
						strTimeDate = "";
					}
				} 
			}
			
			if (millis() - gtdStartTime >= gtdTimeout) {
				Serial.println("getTimeDate() serialGalileo timeout, setting to default");
				strTimeDate = "1991/01/01	00:00:00";
				ok = true;
				break;
			}
		}
	} 
	
	// Chop into date and time
	strDate = strTimeDate.substring(0, 10); //"YYYY/MM/DD"
	strTime = strTimeDate.substring(11, 19); //"hh:mm:ss"
}

void getDHTReadings () { 
// Get readings from DHT sensors; save to sensor[]
	
	// Declare error codes for DHT sensors
	DHT22_ERROR_t errorCode0;
	DHT22_ERROR_t errorCode1;
	DHT22_ERROR_t errorCode2;
	DHT22_ERROR_t errorCode3;
	
	// Check sensors for errors
	errorCode0 = dht_0.readData();
	errorCode1 = dht_1.readData();
	errorCode2 = dht_2.readData();
	errorCode3 = dht_3.readData();

	switch(errorCode0) {
	  
		case DHT_ERROR_NONE:	// No error detected
			// Get measurements
			sensor[0].temp = dht_0.getTemperatureC();
			sensor[0].rh = dht_0.getHumidity();	
			sensor[0].errorNum = 0;
			break;	
		case DHT_ERROR_CHECKSUM:
			// Get measurements
			sensor[0].temp = dht_0.getTemperatureC();
			sensor[0].rh = dht_0.getHumidity();	
			sensor[0].errorNum = 1;	
			break;
		case DHT_BUS_HUNG:
			sensor[0].errorNum = 2;
			break;
		case DHT_ERROR_NOT_PRESENT:
			sensor[0].errorNum = 3;
			break;
		case DHT_ERROR_ACK_TOO_LONG:
			sensor[0].errorNum = 4;
			break;
		case DHT_ERROR_SYNC_TIMEOUT:
			sensor[0].errorNum = 5;
			break;
		case DHT_ERROR_DATA_TIMEOUT:
			sensor[0].errorNum = 6;
			break;
		case DHT_ERROR_TOOQUICK:
			sensor[0].errorNum = 7;
			break;		
	}
	
	switch(errorCode1) {
	  
		case DHT_ERROR_NONE:	// No error detected
			// Get measurements
			sensor[1].temp = dht_1.getTemperatureC();
			sensor[1].rh = dht_1.getHumidity();	
			sensor[1].errorNum = 0;
			break;	
		case DHT_ERROR_CHECKSUM:
			// Get measurements
			sensor[1].temp = dht_1.getTemperatureC();
			sensor[1].rh = dht_1.getHumidity();	
			sensor[1].errorNum = 1;	
			break;
		case DHT_BUS_HUNG:
			sensor[1].errorNum = 2;
			break;
		case DHT_ERROR_NOT_PRESENT:
			sensor[1].errorNum = 3;
			break;
		case DHT_ERROR_ACK_TOO_LONG:
			sensor[1].errorNum = 4;
			break;
		case DHT_ERROR_SYNC_TIMEOUT:
			sensor[1].errorNum = 5;
			break;
		case DHT_ERROR_DATA_TIMEOUT:
			sensor[1].errorNum = 6;
			break;
		case DHT_ERROR_TOOQUICK:
			sensor[1].errorNum = 7;
			break;		
	}
	
	switch(errorCode2) {
	  
		case DHT_ERROR_NONE:	// No error detected
			// Get measurements
			sensor[2].temp = dht_2.getTemperatureC();
			sensor[2].rh = dht_2.getHumidity();	
			sensor[2].errorNum = 0;
			break;	
		case DHT_ERROR_CHECKSUM:
			// Get measurements
			sensor[2].temp = dht_2.getTemperatureC();
			sensor[2].rh = dht_2.getHumidity();	
			sensor[2].errorNum = 1;	
			break;
		case DHT_BUS_HUNG:
			sensor[2].errorNum = 2;
			break;
		case DHT_ERROR_NOT_PRESENT:
			sensor[2].errorNum = 3;
			break;
		case DHT_ERROR_ACK_TOO_LONG:
			sensor[2].errorNum = 4;
			break;
		case DHT_ERROR_SYNC_TIMEOUT:
			sensor[2].errorNum = 5;
			break;
		case DHT_ERROR_DATA_TIMEOUT:
			sensor[2].errorNum = 6;
			break;
		case DHT_ERROR_TOOQUICK:
			sensor[2].errorNum = 7;
			break;		
	}
	
	switch(errorCode3) {
	  
		case DHT_ERROR_NONE:	// No error detected
			// Get measurements
			sensor[3].temp = dht_3.getTemperatureC();
			sensor[3].rh = dht_3.getHumidity();	
			sensor[3].errorNum = 0;
			break;	
		case DHT_ERROR_CHECKSUM:
			// Get measurements
			sensor[3].temp = dht_3.getTemperatureC();
			sensor[3].rh = dht_3.getHumidity();	
			sensor[3].errorNum = 1;	
			break;
		case DHT_BUS_HUNG:
			sensor[3].errorNum = 2;
			break;
		case DHT_ERROR_NOT_PRESENT:
			sensor[3].errorNum = 3;
			break;
		case DHT_ERROR_ACK_TOO_LONG:
			sensor[3].errorNum = 4;
			break;
		case DHT_ERROR_SYNC_TIMEOUT:
			sensor[3].errorNum = 5;
			break;
		case DHT_ERROR_DATA_TIMEOUT:
			sensor[3].errorNum = 6;
			break;
		case DHT_ERROR_TOOQUICK:
			sensor[3].errorNum = 7;
			break;		
	}
	
	// Send readings to Galileo for correction
	Serial.println("Sending readings to Galileo...");
	serialGalileo.println("sending readng");
	for (int i = 0; i < 4; i++) {
		
		if (sensor[i].errorNum == 0 || sensor[i].errorNum == 1) {
			serialGalileo.print(sensor[i].temp);
			serialGalileo.print("\t");
			serialGalileo.print(sensor[i].rh);
			serialGalileo.print("\t");
		} else {
			serialGalileo.print("-\t-\t");		
		}
		
		serialGalileo.print(sensor[i].errorNum);
		serialGalileo.print("\t");
		
	}
	serialGalileo.print("\n");
	
	String strCData = "";
	long gdStartTime, gdTimeout = 3000;
	boolean ok = false;
	gdStartTime = millis();
	
	// Get corrected data and averages from Galileo
	while (ok == false) {	// wait for galileo
		wdt_reset();
		while (serialGalileo.available() > 0) {	
			wdt_reset();
			char received = serialGalileo.read();
			if (received == '\n') {
				ok = true;
				break;
			}
			else if (strCData.length() >= 75) {
				Serial.println("data received is too long");
				strCData = "";
				aveTemp = 25.0;
				aveRHum = 10.0;
				ok = false;
				break;
			}
			else {
				if (isAscii(received)) { 
					strCData += received;
				} else {
					// Discard unrecognizable string
					Serial.print("Discarded invalid input from serialGalileo: ");
					Serial.println(received);
				}
			} 
		}
		
		if (millis() - gdStartTime >= gdTimeout) { 
			Serial.println("data from serialGalileo timeout, setting to default");
			strCData = "";
			aveTemp = 25.0;
			aveRHum = 10.0;
			ok = true;
			return;
			break;
		} 
	}
	
	String strTemp;
	
	// Chop up strCData into individual floats using '\t'
	for (int i = 0; i < 12; i++) {
		// Look for next tab, get part
		int tabindex = strCData.indexOf('\t');
		strTemp += strCData.substring(0, tabindex);
		
		// Trim string
		strCData = strCData.substring(tabindex+1, strCData.length());

		if (strTemp == "-") {
			continue;
		}
		// Choose where to save temp
		switch (i) {
			char chrTemp[6];
			case 0:
				strTemp.toCharArray(chrTemp,6);
				sensor[0].cTemp = atof(chrTemp);
				break;	
			case 1:
				strTemp.toCharArray(chrTemp,6);
				sensor[0].cRh = atof(chrTemp);
				break;	
			case 2:
				strTemp.toCharArray(chrTemp,6);
				sensor[1].cTemp = atof(chrTemp);
				break;	
			case 3:
				strTemp.toCharArray(chrTemp,6);
				sensor[1].cRh = atof(chrTemp);
				break;
			case 4:
				strTemp.toCharArray(chrTemp,6);
				sensor[2].cTemp = atof(chrTemp);
				break;	
			case 5:
				strTemp.toCharArray(chrTemp,6);
				sensor[2].cRh = atof(chrTemp);
				break;	
			case 6:
				strTemp.toCharArray(chrTemp,6);
				sensor[3].cTemp = atof(chrTemp);
				break;	
			case 7:
				strTemp.toCharArray(chrTemp,6);
				sensor[3].cRh = atof(chrTemp);
				break;	
			case 8:
				strTemp.toCharArray(chrTemp,6);
				errorsRead = atoi(chrTemp);
				break;	
			case 9:
				strTemp.toCharArray(chrTemp,6);
				aveTemp = atof(chrTemp);
				break;	
			case 10:
				strTemp.toCharArray(chrTemp,6);
				aveRHum = atof(chrTemp);
				break;
		}
		
		strTemp = "";	// Clear for next iteration
	}
}

int getAnalogValue (int value) { 
// Determine button pressed
	
	// Return an int depending on value read due to different resistances
	if (value > 900) {	// No button pressed
		return 0;
	} else if (value > 430 && value < 470) {	//450
		return 1;
	} else if (value > 335 && value < 375) {	//355
		return 2;
	} else if (value > 198 && value < 238) {	//218
		return 3;
	} else if (value < 20) {	//11
		return 4;
	}

}

int readPushButton () { 
// Returns button pressed; if none pressed, returns 0; w/ debounce
	
	int value = 0;
	int dbPeriod = 100;	// debounce period in ms
	long lastDbTime = 0; 
	int lastValue, justpressed = 0;
	
	while (1) {
		wdt_reset();
		// Get button pressed
		value = getAnalogValue(analogRead(pushPin));
	
		// If different from last value, start debounce timer
		if (value != lastValue) {
			justpressed = 1;	// Button was pressed
			lastDbTime = millis();
		}
		
		// If debounce period has passed and button value is stable
		if (((millis() - lastDbTime) > dbPeriod) && (value == lastValue)) {
			
			// Return value of button just once
			if (justpressed == 1) {
				justpressed = 0;
				return value;
			} else 
				return 0;
		}
		
		// Save read button value for loop
		lastValue = value;
	}
}

void changeSettings () { 
// Change controller settings
// LCD updates only when buttons are pressed due to screen flickering (...which means that time setting screen only updates when buttons are pressed ): )

	int menu = 0, button = 5;	// button is initially 5 so menu switch-case will run once
	
	LCD_settings(menu, 0);
	
	while (1) {
		wdt_reset();
		if (button != 0) {
			
			// Press menu button to exit menu
			if (button == 4) {
				return;
			}
			
			// Left and right screens
			if (button == 1) {
				if (menu == 0) 
					menu = 7;
				else
					menu--;
			} 
			else if (button == 2) {
				if (menu == 7) 
					menu = 0;
				else
					menu++;
			}
		
			if (menu == 5 || menu == 6)
				getTimeDate();
			
			LCD_settings(menu, 0);
		}
		
		// Change screen
		switch (menu) {
			case 0:	// Save to SD Card On/Off
				
				if (button == 3) {
					setting_SD = ~setting_SD;
					LCD_settings(menu, 0);
				}
				break;
				
			case 1:	// Cooling On/Off/Auto
				if (button == 3) {
					if (setting_cooling == 2) 
						setting_cooling = 0;
					else 
						setting_cooling++;
					
					LCD_settings(menu, 0);
				} 
				break;
			
			case 2:	// Heating On/Off/Auto
				if (button == 3) {
					if (setting_heating == 2) 
						setting_heating = 0;
					else 
						setting_heating++;
					
					LCD_settings(menu, 0);
				}
				break;
			
			case 3:	// Intake Fans On/Off/Auto
				if (button == 3) {
					if (setting_intake == 2) 
						setting_intake = 0;
					else 
						setting_intake++;
					
					LCD_settings(menu, 0);
				} 
				break;
			
			case 4:	// Exhaust Fans On/Off/Auto
				if (button == 3) {
					if (setting_exhaust == 2) 
						setting_exhaust = 0;
					else 
						setting_exhaust++;
					
					LCD_settings(menu, 0);
				} 
				break;
			
			case 5:  // Date
			{
				
				if (button == 3) {
				// Start changing date	
					wdt_reset();
					getTimeDate();	//get latest date
					LCD_settings(menu, 0);
					
					// Get current date, convert to ints
					int iniM, iniD, iniY, fnlM, fnlD, fnlY;
					String M_s = "", D_s = "", Y_s = "";
					
					//strDate = YYYY/MM/DD
					Y_s = strDate.substring(0, 4);
					M_s = strDate.substring(5, 7);
					D_s = strDate.substring(8, 10);
					
					iniY = Y_s.toInt();
					iniM = M_s.toInt();
					iniD = D_s.toInt();
					
					// Choose new date
					fnlY = changeTD('Y', menu, 1, iniY, 1900, 2100);
					fnlM = changeTD('M', menu, 2, iniM, 1, 12);
					// Check if current day is valid for month
					if (iniD > daysInMonth(fnlM, fnlY)) 
						iniD = daysInMonth(fnlM, fnlY);
					fnlD = changeTD('D', menu, 3, iniD, 1, daysInMonth(fnlM, fnlY));
					
					// Set new clock
					// Send new date to Galileo
					serialGalileo.println("change date");
					serialGalileo.print(fnlY);
					serialGalileo.print('\t');
					serialGalileo.print(fnlM);
					serialGalileo.print('\t');
					serialGalileo.println(fnlD);

					// Wait for ok from Galileo
					String strReceived = "";
					bool ok = false;
					while (ok == false) {	// wait for msg from galileo
						wdt_reset();
						while (serialGalileo.available() > 0) {	
						wdt_reset();
							char received = serialGalileo.read();
							if (received == '\n') {
								ok = true;
								break;
							}
							else {
								if (isAscii(received)) { 
									strReceived += received;
								} else {
									// Discard unrecognizable string
									Serial.print("Discarded invalid input from serialGalileo: ");
									Serial.println(received);
									strReceived = "";
								}
							}
						}		
					}
					strReceived.trim();	// remove whitespace from both ends
					
					if (strReceived == "set clock ok") {
						Serial.println("set clock ok");
					}
					else {
						Serial.println("error setting clock. pls check");
					}
					
					// Show new date
					getTimeDate();	//get latest date
					LCD_settings(menu, 0);
				}

				break;
			}
			case 6: // Time
			{

				if (button == 3) {
				// Start changing time	
					wdt_reset();
					getTimeDate();	//get latest time
					LCD_settings(menu, 0);
					
					// Get current time, convert to ints
					int inih, inim, inis, fnlh, fnlm, fnls;
					String h_s = "", m_s = "", s_s = "";
					
					//strTime = hh:mm:ss
					h_s = strTime.substring(0, 2);
					m_s = strTime.substring(3, 5);
					s_s = strTime.substring(6, 8);
					
					inih = h_s.toInt();
					inim = m_s.toInt();
					inis = s_s.toInt();
					
					// Choose new time
					fnlh = changeTD('h', menu, 1, inih, 0, 23);
					fnlm = changeTD('m', menu, 2, inim, 0, 59);
					fnls = changeTD('s', menu, 3, inis, 0, 59);
					
					// Set new clock
					// Send new time to Galileo
					serialGalileo.println("change time");
					serialGalileo.print(fnlh);
					serialGalileo.print('\t');
					serialGalileo.print(fnlm);
					serialGalileo.print('\t');
					serialGalileo.println(fnls);
					
					// Wait for ok from Galileo
					String strReceived = "";
					bool ok = false;
					while (ok == false) {	// wait for msg from galileo
						wdt_reset();
						while (serialGalileo.available() > 0) {	
							wdt_reset();
							char received = serialGalileo.read();
							if (received == '\n') {
								ok = true;
								break;
							}
							else{
								if (isAscii(received)) { 
									strReceived += received;
								} else {
									// Discard unrecognizable string
									Serial.print("Discarded invalid input from serialGalileo: ");
									Serial.println(received);
									strReceived = "";
								}
							}
						}		
					}
					strReceived.trim();	// remove whitespace from both ends
					
					if (strReceived == "set clock ok") {
						Serial.println("set clock ok");
					}
					else {
						Serial.println("error setting clock. pls check");
					}
					
					// Show new time
					getTimeDate();	//get latest time
					LCD_settings(menu, 0);
				}
				
				break;
			}
		
			case 7:	// Time interval
			{
				if (button == 3) {
					
					button = 0;
					LCD_settings(menu, 1);
					
					while (button != 3) {
						wdt_reset();
						if (button != 0) {
							
							switch (button) {
								case 1:
									if (interval <= 30000) 
										interval = 30000;
									 else 
										interval -= 30000;
									break;
								
								case 2: 
									if (interval >= 300000)
										interval = 300000;
									else 
										interval += 30000;
									break;
							}
							
							LCD_settings(menu, 1);
						}
						
						button = readPushButton();
						
						if (button == 3) {
							Serial.print("Changed interval to ");
							Serial.println(interval);
						}
					}
					
					LCD_settings(menu, 0);
				}
			}
		}
		
		// Check for new input
		button = readPushButton();
	}
}

int changeTD (char type, int menu, int asteriskPos, int iniValue, int lowerLimit, int upperLimit) { 
// Up/down function in time/date settings	
	
	int tempValue = iniValue, button = 0;
	
	LCD_settings(menu, asteriskPos);
	
	while (1) {
		wdt_reset();
		if (button != 0) {
			switch (button) {
				
				case 1:	// decrease value
				{
					if (tempValue <= lowerLimit) //iikot ang value
						tempValue = upperLimit;
					else
						tempValue--;
					break;
				}	
				case 2: // increase value
				{
					if (tempValue >= upperLimit) //iikot ang value
						tempValue = lowerLimit;
					else 
						tempValue++;
					break;
				}
				case 3: // accept value
				{	
					return tempValue;
					break;
				}
			}
			
			// Save to strings
			switch (type) {
				//strDate = YYYY/MM/DD
				case 'Y':
				{
					strDate = tempValue + strDate.substring(4, 10);
					break;
				}
				case 'M':
				{ 
					// add zero fill
					if (tempValue < 10) 
						strDate = strDate.substring(0, 5) + '0' + tempValue + strDate.substring(7, 10);
					else
						strDate = strDate.substring(0, 5) +  tempValue + strDate.substring(7, 10);
					break;
				}
				case 'D':
				{ 
					if (tempValue < 10) 
						strDate = strDate.substring(0, 8) + '0' + tempValue;
					else
						strDate = strDate.substring(0, 8) + tempValue;
					break;
				}
				//strTime = hh:mm:ss
				case 'h': 
				{
					if (tempValue < 10)  {
						String tempStr = "0";	//can't start String addition w/ a char
						strTime = tempStr + tempValue + strTime.substring(2, 8);
					}	
					else
						strTime = tempValue + strTime.substring(2, 8);
					break;
				}
				case 'm': 
				{
					if (tempValue < 10) 
						strTime = strTime.substring(0, 3) + '0' + tempValue + strTime.substring(5, 8);
					else
						strTime = strTime.substring(0, 3) + tempValue + strTime.substring(5, 8);
					break;
				}
				case 's': 
				{
					if (tempValue < 10) 
						strTime = strTime.substring(0, 6) + '0' + tempValue;
					else
						strTime = strTime.substring(0, 6) + tempValue;
					break;
				}
			}
			
			// Refresh LCD with new value
			LCD_settings(menu, asteriskPos);
		}
		
		button = readPushButton();
	}
}


void LCD_settings (int screenNum, int option) { 
// Show settings menu on LCD
	
	lcd.clear();
	
	switch (screenNum) {
		
		case 0: // Save to SDC: 
			lcd.setCursor(0,0);
			lcd.print("< Save to SDC: >");
			lcd.setCursor(0,1);
			lcd.print("  On  Off       ");
			
			if (setting_SD == true) {
				lcd.setCursor(1,1);
				lcd.print("*");
			} else {
				lcd.setCursor(5,1);
				lcd.print("*");
			}
			break;
		
		case 1: // Cooling:
			lcd.setCursor(0,0);
			lcd.print("< Cooling:     >");
			lcd.setCursor(0,1);
			lcd.print("  On  Off  Auto ");
			
			if (setting_cooling == 0) {
				lcd.setCursor(1,1);
				lcd.print("*");
			} else if (setting_cooling == 1) {
				lcd.setCursor(5,1);
				lcd.print("*");
			} else if (setting_cooling == 2) {
				lcd.setCursor(10,1);
				lcd.print("*");
			}
			break;
		
		case 2: // Heating:
			lcd.setCursor(0,0);
			lcd.print("< Heating:     >");
			lcd.setCursor(0,1);
			lcd.print("  On  Off  Auto ");
			
			if (setting_heating == 0) {
				lcd.setCursor(1,1);
				lcd.print("*");
			} else if (setting_heating == 1) {
				lcd.setCursor(5,1);
				lcd.print("*");
			} else if (setting_heating == 2) {
				lcd.setCursor(10,1);
				lcd.print("*");
			}
			break;
		
		case 3: // Intake Fans:
			lcd.setCursor(0,0);
			lcd.print("< Intake Fan:  >");
			lcd.setCursor(0,1);
			lcd.print("  On  Off  Auto ");
			
			if (setting_intake == 0) {
				lcd.setCursor(1,1);
				lcd.print("*");
			} else if (setting_intake == 1) {
				lcd.setCursor(5,1);
				lcd.print("*");
			} else if (setting_intake == 2) {
				lcd.setCursor(10,1);
				lcd.print("*");
			}
			break;
		
		case 4: // Exhaust Fans:
			lcd.setCursor(0,0);
			lcd.print("< Exhaust Fan: >");
			lcd.setCursor(0,1);
			lcd.print("  On  Off  Auto ");
			
			if (setting_exhaust == 0) {
				lcd.setCursor(1,1);
				lcd.print("*");
			} else if (setting_exhaust == 1) {
				lcd.setCursor(5,1);
				lcd.print("*");
			} else if (setting_exhaust == 2) {
				lcd.setCursor(10,1);
				lcd.print("*");
			}
			break;
		
		case 5: // Date: YYYY / MM / DD
			lcd.setCursor(0,0);
			lcd.print("< Date:        >");
			lcd.setCursor(1,1);
			lcd.print(strDate.substring(0, 4));
			lcd.print(" / ");
			lcd.print(strDate.substring(5, 7));
			lcd.print(" / ");
			lcd.print(strDate.substring(8, 10));
			
			if (option == 1) {
				lcd.setCursor(0,1);
				lcd.print("*");
			} else if (option == 2) {
				lcd.setCursor(7,1);
				lcd.print("*");
			} else if (option == 3) {
				lcd.setCursor(12,1);
				lcd.print("*");
			}
			break;
		
		case 6: // Time: hh : mm : ss
			lcd.setCursor(0,0);
			lcd.print("< Time:        >");
			lcd.setCursor(2,1);
			lcd.print(strTime.substring(0, 2));
			lcd.print(" : ");
			lcd.print(strTime.substring(3, 5));
			lcd.print(" : ");
			lcd.print(strTime.substring(6, 8));
			
			if (option == 1) {
				lcd.setCursor(1,1);
				lcd.print("*");
			} else if (option == 2) {
				lcd.setCursor(6,1);
				lcd.print("*");
			} else if (option == 3) {
				lcd.setCursor(11,1);
				lcd.print("*");
			}
			break;
		
		case 7: // Time interval
		/*	< Interval:    >
			   00 m  30 s
		*/
		
			lcd.setCursor(2,0);
			lcd.print("Interval:");
			lcd.setCursor(3,1);
			
			// Convert interval to mins & secs 
			int tempMin = interval/60000, tempSec = (interval%60000)/1000;
			
			if (tempMin < 10)
				lcd.print("0");
			lcd.print(tempMin);
			lcd.print(" m  ");
			if (tempSec < 10)
				lcd.print("0");
			lcd.print(tempSec);
			lcd.print(" s");
			
			if (option == 0) {
				lcd.setCursor(0,0);
				lcd.print("<");
				lcd.setCursor(15,0);
				lcd.print(">");
			} else if (option == 1) {
				if (interval > 30000) {
					lcd.setCursor(0,1);
					lcd.print("<");
				}
				if (interval < 300000) {
					lcd.setCursor(15,1);
					lcd.print(">");
				}
			}
			
			break;
	}	
}

int daysInMonth (int month, int year) { 
	
	switch (month) {
		case 1: return 31;
		case 2: 
			if (year % 4 == 0)
				return 29;
			else 
				return 28;
		case 3: return 31;
		case 4: return 30;
		case 5: return 31;
		case 6: return 30;
		case 7: return 31;
		case 8: return 31;
		case 9: return 30;
		case 10: return 31;
		case 11: return 30;
		case 12: return 31;
	}
}
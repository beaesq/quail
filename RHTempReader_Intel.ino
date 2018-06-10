/*	Bea Esquivel!
 *	Temperature and Relative Humidity Reader and Controller v1.0 04-18-16
 *	UPLOAD TO GALILEO GEN2 BOARD
 *	
 *	The Intel Galileo board functions as a very expensive real-time clock and 
 *	microSD storage device. The Galileo board also handles the floating point 
 *	calculations of the DHT data corrections because the gizDuino can't.
 *	Serial communication with gizDuino is done using Serial1.
 *	Galileo is programmed to reset during gizDuino's setup().
 */

//	LIBRARIES
#include <SD.h>

typedef struct {
	
	int errorNum;
	double temp, cTemp;
	double rh, cRh;
	
} dhtdata;

String strReceived, strData = "deafult";
String strDate = "", strTime = "";
char chrDate[] = "YYYY/MM/DD", chrTime[] = "HH:mm:ss";	//grr String!
char curDate[] = "abcd/ef/gh";

float aveTemp, aveRHum;
int errorsRead;
dhtdata sensor[4];

char filename[]="data/YYYY/MMDD.txt"; 

unsigned long startTime, forcedResetInterval = 3600000;	// 1hr
unsigned long idleTimeout = 330000, activeTime, loopTime, loopTimeout = 2000;

void setup () {
	// Open serial comm. to USB and Arduino (pins 0 & 1)
	Serial.begin(9600);
	Serial1.begin(4800);
	
	Serial.println("Starting...");
	
	Serial.println("Initializing SD card...");
	if (!SD.begin()) {
		Serial.println("Card failed, or not present");
	}
	else {
		Serial.println("card initialized.");
		getTimeDate();	// get date for filename
		getFilename();	// get file for today
	}

	startTime = millis();
}


void loop () {
	
	loopTime = millis();
	
	// Wait for commands from arduino
	while (Serial1.available() > 0) {		
		// Read buffer
		char chrReceived = Serial1.read();
		
		// If new line is reached (println from arduino)
		if (chrReceived == '\n') { 
			strReceived.trim();	// remove whitespace from both ends
			activeTime = millis();
			
			// Identify command
			if (strReceived == "sending readng") {	
			// Arduino will send measurements, average, actions
				delay(200);	//WHAT DO YOU DO?! missing first byte if removed (missing multiple bytes for 1 & 100 ms)
				strReceived = "";
				getData();
				break;
			}
			
			else if (strReceived == "time please") {	
				getTimeDate();
				strReceived = "";
				break;
			}
			
			else if (strReceived == "save SD") {
				delay(200);
				printToSD();
				strReceived = "";
				break;
			}
			
			else if (strReceived == "change date") {
				delay(200);	//missing bytes at end if not added
				setClock(0);
				strReceived = "";
				break;
			}
			
			else if (strReceived == "change time") {
				delay(200);	//missing bytes at end if not added
				setClock(1);
				strReceived = "";
				break;
			}
			
			else if (strReceived == "reset please") {
				Serial.println("Resetting...");
				system("./opt/cln/galileo/galileo_sketch_reset_script.sh");
				break;
			}
			
			else {	// discard unrecognized string
				strReceived = "";
				break;
			}
		}
		else if (strReceived.length() >= 15) {
			Serial.println("string sent is too long, discarding");
			strReceived = "";
			break;
		}
		else {	
		// If not yet new line, add char from serial buffer to string
			// Check if char is valid
			if (isAscii(chrReceived)) { 
				strReceived += chrReceived;
			} else {	
			// Discard unrecognizable string
				Serial.print("Discarded invalid input from Serial1: ");
				Serial.println(chrReceived);
				strReceived = "";
			}
		}
		
		if (millis() - loopTime >= loopTimeout) {
			Serial.println("loop timeout, resetting");
			system("./opt/cln/galileo/galileo_sketch_reset_script.sh");
			break; // just in case?
		}
	}
	
	// Galileo resets every hour b/c of clock errors; checks if forcedResetInterval has passed and if no actions have been sent by giz for 5 seconds
	if ((millis() - startTime >= forcedResetInterval) && (millis() - activeTime >= 5000)) {
		Serial.println("Forced reset");
		system("./opt/cln/galileo/galileo_sketch_reset_script.sh");
	}

	// Idle reset
	if (millis() - activeTime >= idleTimeout) {
		Serial.println("Idle reset");
		system("./opt/cln/galileo/galileo_sketch_reset_script.sh");
	}
}

void getData () {
// Get data String from Serial1 buffer
	
	String strTemp, strA;
	int i = 0;
	unsigned long gdStartTime, gdTimeout = 2000;
	gdStartTime = millis();
	
	// Get entire data string
	while (Serial1.available() > 0) { 
		char chrA = Serial1.read();
		
		if (chrA == '\n') {
			Serial.println(strA);
			break;	// Exit while loop
		} 
		else if (strA.length() >= 70) {
			Serial.println("data string sent is too long, discarding");
			strA = "too long";
			return;
			break;
		}
		else {
			// Check if char is valid
			if (isAscii(chrA)) {
				strA += chrA;	
			} else {
			// Discard unrecognizable string
				Serial.print("Discarded invalid input from Serial1: ");
				Serial.println(chrA);
				strA = "";
			}
		}
		
		if (millis() - gdStartTime >= gdTimeout) {
			Serial.println("getData timeout, resetting");
			system("./opt/cln/galileo/galileo_sketch_reset_script.sh");
			break; // just in case?
		}
	}

	// Chop up strA into individual floats using '\t'
	for (i = 0; i < 12; i++) {
		// Look for next tab, get part
		int tabindex = strA.indexOf('\t');
		strTemp += strA.substring(0, tabindex);
		
		// Trim string
		strA = strA.substring(tabindex+1, strA.length());

		if (strTemp == "-") {
			continue;
		}
		// Choose where to save temp
		switch (i) {
			char chrTemp[6];
			case 0:
				strTemp.toCharArray(chrTemp,6);
				sensor[0].temp = atof(chrTemp);
				break;	
			case 1:
				strTemp.toCharArray(chrTemp,6);
				sensor[0].rh = atof(chrTemp);
				break;	
			case 2:
				strTemp.toCharArray(chrTemp,6);
				sensor[0].errorNum = atoi(chrTemp);
				break;	
			case 3:
				strTemp.toCharArray(chrTemp,6);
				sensor[1].temp = atof(chrTemp);
				break;
			case 4:
				strTemp.toCharArray(chrTemp,6);
				sensor[1].rh = atof(chrTemp);
				break;	
			case 5:
				strTemp.toCharArray(chrTemp,6);
				sensor[1].errorNum = atoi(chrTemp);
				break;	
			case 6:
				strTemp.toCharArray(chrTemp,6);
				sensor[2].temp = atof(chrTemp);
				break;	
			case 7:
				strTemp.toCharArray(chrTemp,6);
				sensor[2].rh = atof(chrTemp);
				break;	
			case 8:
				strTemp.toCharArray(chrTemp,6);
				sensor[2].errorNum = atoi(chrTemp);
				break;	
			case 9:
				strTemp.toCharArray(chrTemp,6);
				sensor[3].temp = atof(chrTemp);
				break;	
			case 10:
				strTemp.toCharArray(chrTemp,6);
				sensor[3].rh = atof(chrTemp);
				break;
			case 11:
				strTemp.toCharArray(chrTemp,6);
				sensor[3].errorNum = atoi(chrTemp);
				break; 
		}
		
		strTemp = "";	// Clear for next iteration
	}	
	
	// Get corrected data
	for (int i = 0; i < 4; i++) {
		if (sensor[i].errorNum == 0 || sensor[i].errorNum == 1) {
			sensor[i].cTemp = correctedTemp(sensor[i].temp, i);
			sensor[i].cRh = correctedRhum(sensor[i].rh, i);
		}
	}
	
	// Compute average, get number of errors
	computeAvg();
	
	// Send to gizDuino
	for (int i = 0; i < 4; i++) {
		
		if (sensor[i].errorNum == 0 || sensor[i].errorNum == 1) {
			Serial1.print(sensor[i].cTemp);
			Serial1.print("\t");
			Serial1.print(sensor[i].cRh);
			Serial1.print("\t");
		} else {
			Serial1.print("-\t-\t");		
		}
	}
	
	Serial1.print(errorsRead);
	Serial1.print("\t");
	if (errorsRead == 4) {
		Serial1.println("-\t-");	
	}
	else {
		Serial1.print(aveTemp);
		Serial1.print("\t");
		Serial1.println(aveRHum);
	}
}

void computeAvg () { 

	float totalTemp = 0.0, totalRh = 0.0;
	errorsRead = 0;
	
	for (int i = 0; i < 4; i++) { 
		if (sensor[i].errorNum == 0 || sensor[i].errorNum == 1) {
			totalTemp = totalTemp + sensor[i].cTemp;
			totalRh = totalRh + sensor[i].cRh;
		} else {
			errorsRead++;
		}
	}
	
	if (errorsRead == 4) {
	// all sensors returned an error; do not compute avg; do not divide by zero; do not pass go
		return;
	} else {
		aveTemp = totalTemp / (4-errorsRead);
		aveRHum = totalRh / (4-errorsRead);
	}
}

void setClock (int opt) {
// Sets clock; opt: 0-date, 1-time
	
	String strTemp, strA;
	char chrY[5] = "1990", chrM[3] = "01", chrD[3] = "01", chrh[3] = "00", chrm[3] = "00", chrs[3] = "00";
	unsigned long scStartTime, scTimeout = 2000;
	scStartTime = millis();
	
	// Get entire string
	while (Serial1.available() > 0) { 
		char chrA = Serial1.read();
		
		if (chrA == '\n') {
			break;	// Exit while loop
		}
		else if (strA.length() >= 75) {
			Serial.println("clock string sent is too long, discarding");
			strA = "too long";
			break;
		}
		else {
			// Check if char is valid
			if (isAscii(chrA)) {
				strA += chrA;	
			} else {
			// Discard unrecognizable string
				Serial.print("Discarded invalid input from Serial1: ");
				Serial.println(chrA);
				strA = "";
			}
		}
		
		if (millis() - scStartTime >= scTimeout) {
			Serial.println("setClock timeout, resetting");
			system("./opt/cln/galileo/galileo_sketch_reset_script.sh");
			break; // just in case?
		}
	}

	// Chop up time/date string to individual chars
	for (int i = 0; i < 3; i++) {
		// Look for next tab, get part
		int tabindex = strA.indexOf('\t');
		strTemp += strA.substring(0, tabindex);

		// Trim string
		strA = strA.substring(tabindex+1, strA.length());

		// Save strTemp to chars
		if (opt == 0){	// new date
			switch (i) {
				char chrTemp[3];
				int intTemp;
				case 0:	// Year
					strTemp.toCharArray(chrY,5);
					break;	
				case 1:	// Month
					strTemp.toCharArray(chrTemp,3);
					//zero fill
					intTemp = strTemp.toInt();
					if (intTemp < 10) { 
						chrM[0] = '0';
						chrM[1] = chrTemp[0];
						chrM[2] = '\0';
					} else { 
						strTemp.toCharArray(chrM,3);
					}
					break;	
				case 2:	// Day
					strTemp.toCharArray(chrTemp,3);
					//zero fill
					intTemp = strTemp.toInt();
					if (intTemp < 10) { 
						chrD[0] = '0';
						chrD[1] = chrTemp[0];
						chrD[2] = '\0';
					} else { 
						strTemp.toCharArray(chrD,3);
					}
					break;	
			}
		} else if (opt == 1) {	// new time
			switch (i) {
				char chrTemp[3];
				int intTemp;
				case 0:	// hour
					strTemp.toCharArray(chrTemp,3);
					//zero fill
					intTemp = strTemp.toInt();
					if (intTemp < 10) { 
						chrh[0] = '0';
						chrh[1] = chrTemp[0];
						chrh[2] = '\0';
					} else { 
						strTemp.toCharArray(chrh,3);
					}
					break;	
				case 1:	// minute
					strTemp.toCharArray(chrTemp,3);
					//zero fill
					intTemp = strTemp.toInt();
					if (intTemp < 10) { 
						chrm[0] = '0';
						chrm[1] = chrTemp[0];
						chrm[2] = '\0';
					} else { 
						strTemp.toCharArray(chrm,3);
					}
					break;
				case 2:	// second
					strTemp.toCharArray(chrTemp,3);
					//zero fill
					intTemp = strTemp.toInt();
					if (intTemp < 10) { 
						chrs[0] = '0';
						chrs[1] = chrTemp[0];
						chrs[2] = '\0';
					} else { 
						strTemp.toCharArray(chrs,3);
					}
					break;
			}
		}
		
		strTemp = "";	// Clear for next iteration
	}
	
	// Get current time/date
	getTimeDate();
	if (opt == 0) {	// new date, get cur time
		for (int i = 0; i < 2; i++) 
			chrh[i] = chrTime[i];
		for (int i = 0; i < 2; i++)
			chrm[i] = chrTime[i+3];
		for (int i = 0; i < 2; i++)
			chrs[i] = chrTime[i+6];
	} else if (opt == 1) { 	// new time, get cur date
		for (int i = 0; i < 4; i++) 
			chrY[i] = chrDate[i];
		for (int i = 0; i < 2; i++)
			chrM[i] = chrDate[i+5];
		for (int i = 0; i < 2; i++)
			chrD[i] = chrDate[i+8];
	}
	
	// Construct cmd
	char cmd[26] = "/bin/date MMDDhhmmYYYY.ss";	
	cmd[25] = '\0';
	for (int i=0; i<2; i++) { 
		cmd[i+10] = chrM[i];
		cmd[i+12] = chrD[i];
		cmd[i+14] = chrh[i];
		cmd[i+16] = chrm[i];
		cmd[i+23] = chrs[i];
	}
	for (int i=0; i<4; i++) 
		cmd[i+18] = chrY[i];
	
	// Print cmd to serial
	Serial.println("cmd frmt: /bin/date MMDDhhmmYYYY.ss");
	Serial.print("cmd sent: ");
	Serial.println(cmd);
	
	// Send cmd
	char buf[64];
	FILE *ptr;
	if ((ptr = popen(cmd, "r")) != NULL) {
		if (fgets(buf, 64, ptr) != NULL) { 
		}
	}
	(void) pclose(ptr);
	
	Serial1.println("set clock ok");
}

String getClock () {
	
	char *cmd = "/bin/date '+%Y/%m/%d\t%H:%M:%S%n'";
	char buf[64];
	FILE *ptr;

	if ((ptr = popen(cmd, "r")) != NULL) {
		if (fgets(buf, 64, ptr) != NULL) { //if works
			(void) pclose(ptr);	// always remember to close the things you open
			return (String) buf;
		}
		else {
			(void) pclose(ptr);	// or else bad things may happen
			return NULL;
		}
	}
	else {
		(void) pclose(ptr);	// like the expensive board not working or resetting
		return NULL;
	}
}

void getTimeDate () {
	
	// Get time/date from clock
	String strTimeDate = getClock();
	if (strTimeDate != NULL) {
		strTimeDate = strTimeDate.substring(0, strTimeDate.length()-1);
	}
	else {
		// try again
		delay(1000);
		strTimeDate = getClock();
		if (strTimeDate != NULL) 
			strTimeDate = strTimeDate.substring(0, strTimeDate.length()-1);	
		else
			strTimeDate = "1990/01/01	00:00:00";
	}
	
	// Send to arduino
	Serial1.println(strTimeDate);
	Serial.print("\n");
	Serial.println(strTimeDate);
	
	// Chop into date and time
	strDate = strTimeDate.substring(0, 10);
	strTime = strTimeDate.substring(11, 19);
	
	// Convert to char
	strDate.toCharArray(chrDate, strDate.length()+1);
	strTime.toCharArray(chrTime, strTime.length()+1);
	
}

void printToSD () {
	
	if (curDate != chrDate)
		getFilename();
	
	strData = makeDataString();
	
	Serial.print("Opening ");
	Serial.println(filename);
	
	File datafile = SD.open(filename, FILE_WRITE);
	
	// Check if SD is available
	if (datafile) { // if available,
			
		// Add time to data string
		String strA = strDate+"\t"+strTime+"\t"+strData;
		Serial.println(strA);
		// Write data string to SD
		datafile.print(strA);
		datafile.close();
		
		Serial.println("Saved to SD card");
	}
	else { 
		Serial.print("Error opening ");
		Serial.println(filename);
	}
}

String makeDataString () {
// Create String for saving to SD card	
	
	String strA, strTemp;
	char chrTemp[7];
	long mStartTime, mTimeout = 3000;
	mStartTime = millis();
	
	while (Serial1.available() > 0) { 
		char chrA = Serial1.read();
		
		if (chrA == '\n') {;
			break;	// Exit while loop
		} 
		else if (strA.length() >= 6) {
			Serial.println("actions string sent is too long, discarding");
			strA = "";
			break;
		}
		else {
			// Check if char is valid
			if (isAscii(chrA)) {
				strA += chrA;	
			} else {
			// Discard unrecognizable string
				Serial.print("Discarded invalid input from Serial1: ");
				Serial.println(chrA);
				strA = "";
			}
		}
		
		if (millis() - mStartTime >= mTimeout) {
			Serial.println("makeDataString timeout, resetting");
			system("./opt/cln/galileo/galileo_sketch_reset_script.sh");
			break; // just in case?
		}
	}
	
	for (int i = 0; i < 4; i++) {
		if (sensor[i].errorNum == 0 || sensor[i].errorNum == 1) {
		// If no error or checksum error
			sprintf(chrTemp, "%5.2f", sensor[i].temp);
			strTemp += chrTemp;
			strTemp += "\t";
			sprintf(chrTemp, "%5.2f", sensor[i].rh);
			strTemp += chrTemp;
			strTemp += "\t";
			sprintf(chrTemp, "%5.2f", sensor[i].cTemp);
			strTemp += chrTemp;
			strTemp += "\t";
			sprintf(chrTemp, "%5.2f", sensor[i].cRh);
			strTemp += chrTemp;
			strTemp += "\t";
		} else {
		// If error
			strTemp += "-\t-\t-\t-\t";		
		}
		
		strTemp += sensor[i].errorNum;
		strTemp += " ";
		
		switch (sensor[i].errorNum) {
			case 0:
				strTemp += "none";
				break;
			case 1:
				strTemp += "chksm";
				break;
			case 2:
				strTemp += "bus";
				break;
			case 3:
				strTemp += "n.p.";
				break;
			case 4:
				strTemp += "ackTO";
				break;
			case 5:
				strTemp += "syncTO";
				break;
			case 6:
				strTemp += "dataTO";
				break;
			case 7:
				strTemp += "quick";
				break;
		}
		
		strTemp += "\t";	
	}
	
	if (errorsRead == 4) {
		strTemp += "All error'd\t";
	} else {
		sprintf(chrTemp, "%5.2f", aveTemp);
		strTemp += chrTemp;
		strTemp += "\t";
		sprintf(chrTemp, "%5.2f", aveRHum);
		strTemp += chrTemp;
		strTemp += "\t";
	}
	
	strTemp += strA;
	
	return strTemp;
}

void getFilename () { 
// Gets new filename & directory for data	
// New file per week; new directory per year
// data/YYYY/MMDD.txt
	
	char chrDay[] = "DD";
	for (int i = 0; i < 2; i++)
		chrDay[i] = chrDate[i+8];
	int intDay = atoi(chrDay);
	
	char chrMonth[] = "MM";
	for (int i = 0; i < 2; i++) 
		chrMonth[i] = chrDate[i+5];
	int intMonth = atoi(chrMonth);
	
	char chrYear[] = "YYYY";
	for (int i = 0; i < 4; i++)
		chrYear[i] = chrDate[i];
	int intYear = atoi(chrYear);
	
	// Look for existing file to write to
	// Check files in data folder
	if (SD.exists("data/")) {
		
		// Look for same year as today
		char fyear[] = "data/YYYY";
		for (int i = 0; i < 4; i++) 
			fyear[i+5] = chrYear[i];
		
		if (SD.exists(fyear)) {
			// Search for file to use
			char ftemp[] = "data/YYYY/MMDD.txt", chrTDay[] = "DD";
			
			for (int i = 0; i < 4; i++) 
				ftemp[i+5] = chrDate[i];
			for (int i = 0; i < 2; i++)
				ftemp[i+10] = chrDate[i+5];
			for (int i = 0; i < 2; i++)
				ftemp[i+12] = chrDate[i+8];
			
			if (SD.exists(ftemp)) {
				// save to ftemp
				strcpy(filename, ftemp);
			} 
			else {
				// If today is <=06, check in month before
				if (intDay <= 6) {
					
					char chrTMonth[] = "MM", chrTYear[] = "YYYY";
					bool isFileFound = false;

					if (intMonth == 1) {
						// Check in previous year
						sprintf(chrTYear, "%04d", intYear-1);
						sprintf(chrTMonth, "12");
					}
					else {
						strcpy(chrTYear, chrYear);
						sprintf(chrTMonth, "%02d", intMonth-1);
					}
					
					int intTDay;
					
					if (intMonth == 1) 
						intTDay = daysInMonth(12, intYear-1) - (7 - intDay);
					else
						intTDay = daysInMonth(intMonth-1, intYear) - (7 - intDay);
					
					for (int i = 0; i < ((7-intDay)+1); i++) {
						
						sprintf(chrTDay, "%02d", intTDay);
						
						for (int i = 0; i < 4; i++) 
							ftemp[i+5] = chrTYear[i];
						for (int i = 0; i < 2; i++)
							ftemp[i+10] = chrTMonth[i];
						for (int i = 0; i < 2; i++)
							ftemp[i+12] = chrTDay[i];
						
						if (SD.exists(ftemp)) {
							// save to ftemp
							strcpy(filename, ftemp);
							isFileFound = true;
							break;
						} 
					}
					
					if (isFileFound == false) {
						for (int i = 1; i < (intDay + 1); i++) {
							
							sprintf(chrTDay, "%02d", i);
							
							for (int i = 0; i < 4; i++) 
								ftemp[i+5] = chrDate[i];
							for (int i = 0; i < 2; i++)
								ftemp[i+10] = chrDate[i+5];
							for (int i = 0; i < 2; i++)
								ftemp[i+12] = chrTDay[i];
							
							if (SD.exists(ftemp)) {
								// save to ftemp
								strcpy(filename, ftemp);
								break;
							} 
							else {
								if (i == intDay) { 
									makeTxt();
								} 
							}
						}
					}
				} 
				else {
					// Look for today in (today-6, today)
					int intTDay = intDay - 6;
					
					for (int i = 0; i < 7; i++) {
						
						sprintf(chrTDay, "%02d", intTDay+i);
						
						for (int i = 0; i < 4; i++) 
							ftemp[i+5] = chrDate[i];
						for (int i = 0; i < 2; i++)
							ftemp[i+10] = chrDate[i+5];
						for (int i = 0; i < 2; i++)
							ftemp[i+12] = chrTDay[i];
						
						if (SD.exists(ftemp)) {
							// save to ftemp
							strcpy(filename, ftemp);
							break;
						} 
						else {
							if (i == 6) { 
								makeTxt();
							} 
						}
					}
				}
			}
		} else {
			// Make year directory and file
			Serial.println("Year directory does not exist");
			
			char dir[] = "data/YYYY";
			for (int i = 0; i < 4; i++) 
				dir[i+5] = chrDate[i];
			Serial.print("making ");
			Serial.println(dir);
			SD.mkdir(dir);

			if(!SD.exists(dir))
				Serial.println("error creating directory");
			else
				Serial.println("directory making ok");

			makeTxt();
		}
		
	} else {
		Serial.println("data/ does not exist");
		
		char dir[] = "data/YYYY";
		for (int i = 0; i < 4; i++) 
			dir[i+5] = chrDate[i];
		Serial.print("making ");
		Serial.println(dir);
		SD.mkdir(dir);
		
		if(!SD.exists(dir))
			Serial.println("error creating directory");
		else
			Serial.println("directory making ok");
		
		makeTxt();
	}
	
	// Save current date for checking if new date
	strcpy(curDate, chrDate);
}

void makeTxt () { 
// Make directory & file
	for (int i = 0; i < 4; i++) 
		filename[i+5] = chrDate[i];
	for (int i = 0; i < 2; i++)
		filename[i+10] = chrDate[i+5];
	for (int i = 0; i < 2; i++)
		filename[i+12] = chrDate[i+8];
	
	Serial.print("file does not exist, making ");
	Serial.println(filename);
	
	char line[100]; 
	strcpy(line, "touch /media/mmcblk0p1/");
	strcat(line, filename);
	Serial.println(line);
	system(line);
	
	File dataFile = SD.open(filename, FILE_WRITE);
	if (dataFile) {
		Serial.println("file making ok");
		String header = "Date\t\tTime\t\tTemp1\tRH1\tcTemp1\tcRH1\tError1\tTemp2\tRH2\tcTemp2\tcRH2\tError2\tTemp3\tRH3\tcTemp3\tcRH3\tError3\tTemp4\tRH4\tcTemp4\tcRH4\tError4\tAvgTemp\tAvgRH\tActions";
		dataFile.println(header);
		dataFile.close();
	}
	
	if(!SD.exists(filename))
		Serial.println("error creating file");
	else
		Serial.println("file making ok");
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

double correctedTemp (float reading, int sensorNum) { 
// Correct sensor readings accrdg to calibration equation

	double b0, b1, b2, b3, b4, b5, b6, b7;
	double min, max;
	
	switch (sensorNum) {
		case 0:
			min = 24.2;
			max = 36.5;
			b0 = -266789.87799126200000000000;
			b1 = 63876.76494692680000000000;
			b2 = -6528.89224922879000000000;
			b3 = 369.30978015306800000000;
			b4 = -12.48595982489830000000;
			b5 = 0.25231517171896100000;
			b6 = -0.00282192223356212000;
			b7 = 0.00001347532574011270;
			break;
		case 1:
			min = 25.2;
			max = 36.2;
			b0 = -922856.73718936000000000000;
			b1 = 213786.50540053600000000000;
			b2 = -21166.04746520490000000000;
			b3 = 1160.96950730007000000000;
			b4 = -38.10172150456780000000;
			b5 = 0.74819041262439200000;
			b6 = -0.00813958759775028000;
			b7 = 0.00003784560697893970;
			break;
		case 2: 
			min = 24.2;
			max = 36.9;
			b0 = -122226.02461843800000000000;
			b1 = 29296.57033261620000000000;
			b2 = -2994.23811535416000000000;
			b3 = 169.18196064466100000000;
			b4 = -5.70766476688856000000;
			b5 = 0.11498172306935900000;
			b6 = -0.00128077498497030000;
			b7 = 0.00000608583113612746;
			break;
		case 3:
			min = 24.8;
			max = 36.1;
			b0 = -1066114.71364709000000000000;
			b1 = 250438.10744777000000000000;
			b2 = -25140.92412028560000000000;
			b3 = 1398.16349176661000000000;
			b4 = -46.52182935890240000000;
			b5 = 0.92615550576168700000;
			b6 = -0.01021465671598420000;
			b7 = 0.00004814842538214370;
			break;
		default:
			return 0;
	} 
	
	if ((reading <= max) && (reading >= min)) {
		return 	(double) b0 + 
			b1 * reading +
			b2 * pow(reading, 2.0) +
			b3 * pow(reading, 3.0) +
			b4 * pow(reading, 4.0) +
			b5 * pow(reading, 5.0) +
			b6 * pow(reading, 6.0) +
			b7 * pow(reading, 7.0); 
	}
	else 
		return reading;
	
}

double correctedRhum (float reading, int sensorNum) { 
// Correct sensor readings accrdg to calibration equation

	double b0, b1, b2, b3, b4, b5, b6, b7;
	double min, max;
	
	switch (sensorNum) {
		case 0:
			min = 20.0;
			max = 100.0;
			b0 = -173.31013096437500000000;
			b1 = 46.67440796942110000000;
			b2 = -3.42882250259796000000;
			b3 = 0.12081914337704100000;
			b4 = -0.00229658017409309000;
			b5 = 0.00002428623953871330;
			b6 = -0.00000013481997100871;
			b7 = 0.00000000030680851375;
			break;
		case 1:
			min = 38.5;
			max = 100.0;
			b0 = -13818.40131393700000000000;
			b1 = 1592.72759068561000000000;
			b2 = -77.07532949695550000000;
			b3 = 2.03495530794369000000;
			b4 = -0.03164230084700380000;
			b5 = 0.00028985547581795800;
			b6 = -0.00000144920976137866;
			b7 = 0.00000000305313737477;
			break;
		case 2: 
			min = 37.0;
			max = 100.0;
			b0 = -4210.42902933730000000000;
			b1 = 493.97311114734800000000;
			b2 = -24.18691725397520000000;
			b3 = 0.64634950964085100000;
			b4 = -0.01016242072403780000;
			b5 = 0.00009401406825431260;
			b6 = -0.00000047391850061827;
			b7 = 0.00000000100435394562;
			break;
		case 3:
			min = 39.3;
			max = 100.0;
			b0 = -12389.66068949710000000000;
			b1 = 1368.16962528169000000000;
			b2 = -63.31810846984900000000;
			b3 = 1.59723338203821000000;
			b4 = -0.02372067903650250000;
			b5 = 0.00020759838070777600;
			b6 = -0.00000099248961801810;
			b7 = 0.00000000200183051666;
			break;
		default:
			return 0;
	}
	
	if ((reading <= max) && (reading >= min)) {
		return 	(double) b0 + 
			b1 * reading +
			b2 * pow(reading, 2.0) +
			b3 * pow(reading, 3.0) +
			b4 * pow(reading, 4.0) +
			b5 * pow(reading, 5.0) +
			b6 * pow(reading, 6.0) +
			b7 * pow(reading, 7.0); 
	}
	else 
		return reading;
}
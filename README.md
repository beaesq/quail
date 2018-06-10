# quail

Uses cooling & heating devices to adjust temperature, intake & exhaust fans to reduce humidity. 

Requires 2 Arduino-compatible boards. Boards are serially connected.
	- First board (used gizDuino+ 644) is main board used to control cooling, heating, & fans via relays, read temperature & relative humidity from 4 DHT22 sensors, manage push button inputs to control the display & change settings, and manage the LED indicators for the relays and the LCD.
	- Second board (used Intel Galileo Gen 2) provides the RTC, computes the corrected temp & humidity accdg to sensor calibrations, and saves readings to an SD card.

An LCD and four push buttons are used to display the curent date & time, average temp. & r. humidity, and the temp. & r. humidity for each sensor. The cooling & heating devices and intake & exhaust fans can be powered on or off individually from the display. The time and date and SD card saving settings can also be changed.

**Notes:**\
	- dht22 library not included\
	- RHTempReader_Intel uses double precision floats, so calculations may be inaccurate for some boards.

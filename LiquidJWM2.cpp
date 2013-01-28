#include "LiquidJWM2.h"

/*
  LiquidJWM2 i2c OLED driver for MCP23008
  from a libary hacked by Sam C. Lin / http://www.lincomatic.com
  BASED ON  
   LiquidTWI by Matt Falcon (FalconFour) / http://falconfour.com
   modified by Stephanie Maks / http://planetstephanie.net
   logic gleaned from Adafruit RGB LCD Shield library
  AND
	MCP23007 Library by Adafruit Industries/Limor Fried

  Compatible with MCP23008 I/O Expander and NewHaven OLED display (NHD-0420DZW-AY5-ND)
*/

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <Wire.h>
#if defined(ARDUINO) && (ARDUINO >= 100) //scl
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

//NOTE: even though it is not used, you MUST ground R/W or connect it to GPIO pin 0  
// R/W pin = 0
// RS pin = 1
// Enable pin = 2
// Data pin 4 = 3
// Data pin 5 = 4
// Data pin 6 = 5
// Data pin 7 = 6

/*
These constants reflect which output on the MCP23008 maps to the corresponding pin on the OLED
*/
#define RW_PIN 		0
#define RS_PIN 		1
#define ENABLE_PIN 	2
#define D4_PIN		3
#define D5_PIN		4
#define D6_PIN		5
#define D7_PIN		6    
boolean readBusyState = false;
    
static inline void wiresend(uint8_t x) {
#if ARDUINO >= 100
  Wire.write((uint8_t)x);
#else
  Wire.send(x);
#endif
}

static inline uint8_t wirerecv(void) {
#if ARDUINO >= 100
  return Wire.read();
#else
  return Wire.receive();
#endif
}

// Note that resetting the Arduino doesn't reset the LCD, so we
// can't assume that its in that state when a sketch starts (and the
// LiquidJWM2 constructor is called). This is why we save the init commands
// for when the sketch calls begin(), except configuring the expander, which
// is required by any setup.

LiquidJWM2::LiquidJWM2(uint8_t i2cAddr) {
  //  if (i2cAddr > 7) i2cAddr = 7;
  _i2cAddr = i2cAddr; // transfer this function call's number into our internal class state
  Serial.println("Inside LiquidJWM2 constructor");
  }

void LiquidJWM2::begin(uint8_t cols, uint8_t lines, uint8_t dotsize) {
  // SEE PAGE 21 OF NEWHAVEN DATASHEET FOR INITIALIZATION SPECIFICATION!
  // Give the OLED time to power up before sending it commands 
	delay(50);

	Wire.begin();
	Serial.println("Called Wire.begin()");

    // first thing we do is get the GPIO expander's head working straight, with a boatload of junk data.
    Wire.beginTransmission(MCP23008_ADDRESS | _i2cAddr);
    wiresend(MCP23008_IODIR);
    wiresend(0xFF);
    wiresend(0x00);
    wiresend(0x00);
    wiresend(0x00);
    wiresend(0x00);
    wiresend(0x00);
    wiresend(0x00);
    wiresend(0x00);
    wiresend(0x00);
    wiresend(0x00);	
    Wire.endTransmission();
	Serial.println("Done writing to GPIO expander");
	
    // now we set the GPIO expander's I/O direction to output for all "pins"
    Wire.beginTransmission(MCP23008_ADDRESS | _i2cAddr);
    wiresend(MCP23008_IODIR);
    wiresend(0x00); // all output: 00000000 for pins 1...8
    Wire.endTransmission();
	Serial.println("End GPIO setup transmission");

	//pullUp(RW_PIN, LOW);
	
  if (lines > 1) {
    _displayfunction |= LCD_2LINE;
  }
  _numlines = lines;
  _currline = 0;
  _numcols = cols;

  // for some 1 line displays you can select a 10 pixel high font
  if ((dotsize != 0) && (lines == 1)) {
    _displayfunction |= LCD_5x10DOTS;
  } else {
	_displayfunction |= LCD_5x8DOTS;
  }

  //put the LCD into 4 bit mode
  // start with a non-standard command to make it realize we're speaking 4-bit here
  // per LCD datasheet, first command is a single 4-bit burst, 0011.
  //-----
  //  we cannot assume that the LCD panel is powered at the same time as
  //  the arduino, so we have to perform a software reset as per page 45
  //  of the HD44780 datasheet - (kch)
  //-----

  // bit pattern for the burstBits function is
    //
    //  7   6   5   4   3   2   1   0
    // LT  D7  D6  D5  D4  EN  RS  n/c
    //-----
	
	/*
		write4bits(0x03); // Missing step from doc. Thanks to Elco Jacobs
		delayMicroseconds(5000);
		write4bits(0x02);
		delayMicroseconds(5000);
		write4bits(0x02);
		delayMicroseconds(5000);
		write4bits(0x08);
   
		delayMicroseconds(5000);
		
		burstBits8(B10011100);

  */
	Serial.println("Start writing to LCD");

    burstBits8(B00011100); // send D4 & D5 high with enable
	delayMicroseconds(50);	
	Serial.println("send D4 D5 high with enable");
	burstBits8(B00011000); // send  D4 & D5 high with !enable
	delayMicroseconds(5000);
	Serial.println("send D4 D5 high with !enable");
		
	burstBits8(B00010100); // send D5 high with enable
	delayMicroseconds(50);	
	burstBits8(B00010000); // send D5 high with !enable
	delayMicroseconds(5000);
	
	burstBits8(B00010100); // send D5 high with enable
	delayMicroseconds(50);	
	burstBits8(B00010000); // send D5 high with !enable
	delayMicroseconds(5000);
		
	burstBits8(B01000100); // send D7 high with enable
	delayMicroseconds(50);	
	burstBits8(B01000000); // send D7 high with !enable
	delayMicroseconds(5000);	
		
	Serial.println("finish warm-up");
	
	delay(5); // this shouldn't be necessary, but sometimes 16MHz is stupid-fast.
	
	command(LCD_FUNCTIONSET | _displayfunction);  
	delayMicroseconds(5000);
	
	command(0x08);	// Turn Off
	delayMicroseconds(5000);
	Serial.println("Sent turn off command");
	
	command(0x01);	// Clear Display
	delayMicroseconds(5000);
	Serial.println("Sent clear display command");
	
	command(0x06);	// Set Entry Mode
	delayMicroseconds(5000);
	Serial.println("Sent entry mode command");
	
	command(0x02);	// Home Cursor
	delayMicroseconds(5000);
	Serial.println("Sent home cursor command");
	
	command(0x0C);	// Turn On - enable cursor & blink
	delayMicroseconds(5000);
	Serial.println("Sent enable cursor and blink command");

}
/********** high level commands, for the user! */
void LiquidJWM2::clear()
{
  command(LCD_CLEARDISPLAY);  // clear display, set cursor position to zero
  delayMicroseconds(2000);  // this command takes a long time!
}

void LiquidJWM2::home()
{
  command(LCD_RETURNHOME);  // set cursor position to zero
  delayMicroseconds(2000);  // this command takes a long time!
}

void LiquidJWM2::setCursor(uint8_t col, uint8_t row)
{
  int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
   if ( row >= _numlines )
  {
    row = 0; //write to first line if out off bounds
  } 
  if (col >= _numcols) {
	col = 0;
  }
  command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

// Turn the display on/off (quickly)
void LiquidJWM2::noDisplay() {
  _displaycontrol &= ~LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidJWM2::display() {
  _displaycontrol |= LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turns the underline cursor on/off
void LiquidJWM2::noCursor() {
  _displaycontrol &= ~LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidJWM2::cursor() {
  _displaycontrol |= LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turn on and off the blinking cursor
void LiquidJWM2::noBlink() {
  _displaycontrol &= ~LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidJWM2::blink() {
  _displaycontrol |= LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// These commands scroll the display without changing the RAM
void LiquidJWM2::scrollDisplayLeft(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void LiquidJWM2::scrollDisplayRight(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void LiquidJWM2::leftToRight(void) {
  _displaymode |= LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This is for text that flows Right to Left
void LiquidJWM2::rightToLeft(void) {
  _displaymode &= ~LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'right justify' text from the cursor
void LiquidJWM2::autoscroll(void) {
  _displaymode |= LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'left justify' text from the cursor
void LiquidJWM2::noAutoscroll(void) {
  _displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// Allows us to fill the first 8 CGRAM locations
// with custom characters
void LiquidJWM2::createChar(uint8_t location, uint8_t charmap[]) {
  readBusyState = true;
  location &= 0x7; // we only have 8 locations 0-7
  command(LCD_SETCGRAMADDR | (location << 3));
 for (int i=0; i<8; i++) {
	readBusyState = true;
		write(charmap[i]);
	}
}

/*********** mid level commands, for sending data/cmds */
inline void LiquidJWM2::command(uint8_t value) {
  send(value, LOW);
  if (readBusyState) readBusy();
  readBusyState = false;
}

#if defined(ARDUINO) && (ARDUINO >= 100) //scl
inline size_t LiquidJWM2::write(uint8_t value) {
  if (value < 8 && value >= 0) readBusyState = true;
  send(value, HIGH);
  if (readBusyState) readBusy();
  readBusyState = false;
  return 1;
}
#else
inline void LiquidJWM2::write(uint8_t value) {
  if (value < 8 && value >= 0) readBusyState = true;
  send(value, HIGH);
  if (readBusyState) readBusy();
  readBusyState = false;
}
#endif

/************ low level data pushing commands **********/
// Allows to set the backlight, if the LCD backpack is used
void LiquidJWM2::setBacklight(uint8_t status) {
    bitWrite(_displaycontrol,3,status); // flag that the backlight is enabled, for burst commands
    burstBits8((_displaycontrol & LCD_BACKLIGHT)?0x80:0x00);
}

/*
This function will write either a command or data and "burst" it to the expander over I2C.
The OLED controller operates on 8-bit values, but we are in 4-bit mode, so we need the pass the value to the OLED in two pieces (i.e., "crunch" two different sets of four bits) 
*/
void LiquidJWM2::send(uint8_t value, uint8_t mode) {
	crunchBits(value, mode, 1);
	crunchBits(value, mode, 0);
}

void LiquidJWM2::crunchBits(uint8_t value, uint8_t mode, uint8_t high) {
	byte buf = 0;
	
	if (high) buf = (value & B11110000) >> 1; // isolate high 4 bits, shift over to data pins (bits 6-3: x1111xxx)
	else buf = (value & B1111) << 3; // isolate low 4 bits, shift over to data pins (bits 6-3: x1111xxx)	

    if (mode) buf |= 3 << 1; // here we can just enable enable, since the value is immediately written to the pins
    else buf |= 2 << 1; // if RS (mode), turn RS and enable on. otherwise, just enable. (bits 2-1: xxxxx11x)
    
	burstBits8(buf); // bits are now present at LCD with enable active in the same write
	// no need to delay since these things take WAY, WAY longer than the time required for enable to settle (1us in LCD implementation?)
	buf &= ~(1<<2); // toggle enable low
    burstBits8(buf); // send out the same bits but with enable low now; LCD crunches these 4 bits.
}

// This function "bursts" 8 bits to the GPIO chip whenever we need to, avoiding repetitive code.
void LiquidJWM2::burstBits8(uint8_t value) {
  Wire.beginTransmission(MCP23008_ADDRESS | _i2cAddr);
  wiresend(MCP23008_GPIO);
  wiresend(value); // last bits are crunched, we're done.
  while(Wire.endTransmission());
  }

/* The NewHaven OLED uses the D7 pin both as an input and an output.  According to the datasheet, you should always wait for the busy to go LOW before you issue a command (otherwise you risk writing data to the OLED while it is in the middle of something, meaning that you'll also certainly end up with a garbled display that will require a power cycle to start working again.
NOTE:  Checking the busy pin slows things down a bit (but not horribly).  YOU ONLY NEED TO USE IT IF YOU ARE DEFINING/WRITING CUSTOM CHARACTERS.  If you are using the built-in font tables, you don't need to call this function at all. 
*/
void LiquidJWM2::readBusy() {
	int busy = 1;
	//First, configure the MCP23008 to let us read from the busy pin
    Wire.beginTransmission(MCP23008_ADDRESS | _i2cAddr);
    wiresend(MCP23008_IODIR);
    wiresend(B01000000); //
    Wire.endTransmission();

	do {
		burstBits8(B00000001); //send ENABLE low
		burstBits8(B00000101); //toggle ENABLE high
		Wire.beginTransmission(MCP23008_ADDRESS | _i2cAddr);
		//This next block of code reads the "BUSY" pin 
		Wire.write(MCP23008_GPIO);	
		Wire.endTransmission();
		Wire.requestFrom(MCP23008_ADDRESS | _i2cAddr, 1);
		busy = (Wire.read() >> D7_PIN) & 0x1;
		//Now, toggle the 
		burstBits8(B00000001);
		burstBits8(B00000101);
		} while (busy);  //keep looping until the busy pin tells us that the OLED is done
	
	//Finally, switch the MCP23008 back to output mode for the busy pin
    Wire.beginTransmission(MCP23008_ADDRESS | _i2cAddr);
    wiresend(MCP23008_IODIR);
    wiresend(0x00); // all output: 00000000 for pins 1...8
    Wire.endTransmission();
	//And switch RW out of read mode
	burstBits8(B00000000);
}
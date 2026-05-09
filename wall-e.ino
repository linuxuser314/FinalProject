
/*
BoeBot Final Project
Created by Creed Truman

College of the Albemarle,
Department of Engineering and Mathematics,
Sudeepa Pathak,
EGR-150-AF01

To be presented 5/8/2026.

Made with help from Google Gemini
*/


//This is necessary for IO port definitions, interrupt handling, and uint8_t
#include <avr/io.h>
#include <stdint.h>
#include <avr/interrupt.h>

//Used for SPI IO
#include <SPI.h>

//Defines pinstructs and pin information

//Necessary for setting up pins
struct PWMStruct{
  uint8_t modeBit;
  volatile uint8_t* timer;
  volatile uint8_t* PWMPtr;
};
struct PinStruct{
  volatile uint8_t* port;
  volatile uint8_t* ddr;
  volatile uint8_t* pin;
  uint8_t bit;
  PWMStruct PWMData;
};


//Placeholder
constexpr PWMStruct NO_PWM = {0, nullptr, nullptr};

//This is where I will define the pin constants. I am using the PinStruct to store all necessary information about each pin in one place, which should make it easier to write generic functions for pin manipulation and PWM control.
//I do not have PWM set up for all pins just yet, so I am using the NO_PWM for those pins (3, 9, 10, 11).
constexpr PinStruct PIN_0 = {&PORTD, &DDRD, &PIND, 0, NO_PWM};
constexpr PinStruct PIN_1 = {&PORTD, &DDRD, &PIND, 1, NO_PWM};
constexpr PinStruct PIN_2 = {&PORTD, &DDRD, &PIND, 2, NO_PWM};
constexpr PinStruct PIN_3 = {&PORTD, &DDRD, &PIND, 3, NO_PWM};
constexpr PinStruct PIN_4 = {&PORTD, &DDRD, &PIND, 4, NO_PWM};
constexpr PinStruct PIN_5 = {&PORTD, &DDRD, &PIND, 5, {COM0B1, &TCCR0A, &OCR0B}};
constexpr PinStruct PIN_6 = {&PORTD, &DDRD, &PIND, 6, {COM0A1, &TCCR0A, &OCR0A}};
constexpr PinStruct PIN_7 = {&PORTD, &DDRD, &PIND, 7, NO_PWM};
constexpr PinStruct PIN_8 = {&PORTB, &DDRB, &PINB, 0, NO_PWM};
constexpr PinStruct PIN_9 = {&PORTB, &DDRB, &PINB, 1, NO_PWM};
constexpr PinStruct PIN_10 = {&PORTB, &DDRB, &PINB, 2, NO_PWM};
constexpr PinStruct PIN_11 = {&PORTB, &DDRB, &PINB, 3, NO_PWM};
constexpr PinStruct PIN_12 = {&PORTB, &DDRB, &PINB, 4, NO_PWM}; 
constexpr PinStruct PIN_13 = {&PORTB, &DDRB, &PINB, 5, NO_PWM};

//These are just constants for readability and convenience.
constexpr uint8_t ON = 1;
constexpr uint8_t OFF = 0;
constexpr uint8_t IN = 0;
constexpr uint8_t OUT = 1;

//Audio start positions and durations. Basically these are just telling the bot where to find specific sound effects in the audio file.
constexpr uint32_t BOOT_START = 0;
constexpr uint32_t BOOT_DURATION = 3000;
constexpr uint32_t SCREAM_STARTS[] =    {33885, 34777, 36879, 38873, 49575};
constexpr uint32_t SCREAM_DURATIONS[] = {  892, 2160 , 1958 , 2360 , 1037 };
constexpr uint32_t TADA_START = 157708;
constexpr uint32_t TADA_DURATION = 1957;
constexpr uint32_t WALLE_START = 20412;
constexpr uint32_t WALLE_DURATION = 2418;
constexpr uint32_t AHA_START = 45170;
constexpr uint32_t AHA_DURATION = 822;
constexpr uint32_t KISS_START = 50640;
constexpr uint32_t KISS_DURATION = 2264;
constexpr uint32_t WARBLE_START = 74910;
constexpr uint32_t WARBLE_DURATION = 2310;
constexpr uint32_t EFFECT_START[] = {TADA_START, WALLE_START, AHA_START, KISS_START, WARBLE_START};
constexpr uint32_t EFFECT_DURATION[] = {TADA_DURATION, WALLE_DURATION, AHA_DURATION, KISS_DURATION, WARBLE_DURATION};
volatile uint32_t systemMillis = 0;

//These are variables that store the audio state
volatile bool isPlaying = false;
volatile uint32_t totalSamples = 0;
volatile uint32_t currentSample = 0;

//This is some background data for the message packet system.
struct msgStruct{
	char id;
	uint8_t val;
};
volatile msgStruct lastPacket = {'\0', 0};

//Custom versions of digitalRead, pinMode, and digitalWrite (mine are about 50x faster)
static bool myDigitalRead(const PinStruct target){
  return *target.pin & (1 << target.bit);
}
static void myPinMode(const PinStruct target, bool mode){
  if(mode) *target.ddr |= (1 << target.bit);
 else *target.ddr &= ~(1 << target.bit);
}
static void myDigitalWrite(const PinStruct target, bool level){
 if(level) *target.port |= (1 << target.bit);
 else *target.port &= ~(1 << target.bit);
}

//This sets up stuff to work so I can run the servos and audio simultaneously
static void initTimers012ForServoAudioTimekeeping(){
 
 cli();

 //Servo code
 myPinMode(PIN_9, OUT);
 myPinMode(PIN_10, OUT);

 //Setting the Waveform Generation Module to Mode 14: Fast PWM with ICR1 as TOP.
 //This allows us to set a custom TOP value for a specific PWM frequency, which is necessary for accurate servo control.
 TCCR1A = (1 << WGM11) | (1 << COM1A1) | (1 << COM1B1);
 TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);

 //Sets the TOP value for 50Hz
 ICR1 = 39999;

 //Comments need to be reorganized to reflect code restructuring on the TCRR modifications. 
 //Sets the prescaler to 8 so that it only increments the timer every 8 clock cycles
 //This, combined with the TOP value, gives us a PWM frequency of 50Hz, which is standard for servo control.

 //We need to set the Compare Output Mode (COM) bits to enable the multiplexer to select PWM instead of Digital.
 //This is equivalent to enablePWM

 //Set the motors to be stopped initially (1.5ms pulse)
 OCR1A = 3000;
 OCR1B = 3000; 

 //Pin 3 PWM Setup
 
 myPinMode(PIN_3, OUT);
 //Sets Pin 3 Waveform Generation Mode to Mode 3: Fast PWM
 //This means that Timer0 starts at 0, counts up to 255, and then resets to 0.
 TCCR2A = (1 << WGM20) | (1 << WGM21) | (1 << COM2B1);
 //Sets the clock select bits to use a prescaler of 0, which gives us a PWM frequency of about 62.5kHz.
 TCCR2B = (1 << CS20);

 //Timer0 setup for 8kHz interrupt

 //Puts timer 0 in CTC mode, which causes it to "spring back" after it hits a top value.
 TCCR0A = (1 << WGM01);

 //Enables a prescaler of 8
 TCCR0B = (1 << CS01);

 //This brings it together. 16MHz with a precaler of 8 is 2MHz. Dividng that by 250 yields 8kHz, which is our desired update frequency.
 OCR0A = 249;

 //Enables the interrupts
 TIMSK0 = (1 << OCIE0A);
 sei();
}

//Equivalent to millis() but collaborates with the audio setup
static uint32_t myMillis(){
 cli();
 uint32_t tempTime = systemMillis;
 sei();
 return tempTime;
}

//Equivalent to delay() but using myMillis()
inline void myDelay(unsigned long ms){
 unsigned long startTime = myMillis();
 while(myMillis() - startTime < ms);
}


//Accepts parameters between -100 and 100 to drive the robot
static void drive(int8_t left, int8_t right){
 //Drives the robot. It starts by taking 3000 (1.5ms). Then it takes the left and right variables (which range from -100 to 100) and multiplies them by 4.This gives us a range from -400 to 400. Adding them to the 3000 gives us a range from 2600 to 3400, or 1.3ms to 1.7ms to control the servos. The right motor is reversed because of the way it is oriented on the robot.
 OCR1A = 3000 + (uint16_t)left * 4;
 OCR1B = 3000 - (uint16_t)right * 4;
}
static void driveL(int8_t left){
 //Drives the robot. It starts by taking 3000 (1.5ms). Then it takes the left and right variables (which range from -100 to 100) and multiplies them by 4.This gives us a range from -400 to 400. Adding them to the 3000 gives us a range from 2600 to 3400, or 1.3ms to 1.7ms to control the servos. The right motor is reversed because of the way it is oriented on the robot.
 OCR1A = 3000 + (uint16_t)left * 4;
}
static void driveR(int8_t right){
 //Drives the robot. It starts by taking 3000 (1.5ms). Then it takes the left and right variables (which range from -100 to 100) and multiplies them by 4.This gives us a range from -400 to 400. Adding them to the 3000 gives us a range from 2600 to 3400, or 1.3ms to 1.7ms to control the servos. The right motor is reversed because of the way it is oriented on the robot.
 OCR1B = 3000 - (uint16_t)right * 4;
}

//This function runs every 125 microseconds (8khz) to update myMillis, load the next byte of audio from the flash chip, and send it to the speaker
static void on8kHzPulse(){
 static uint8_t withinMillisCounter = 0;
 withinMillisCounter ++;
 if(withinMillisCounter >= 8){
  systemMillis ++;
  withinMillisCounter = 0;
 }
 //Serial pulling and audio output writing here!
 if(isPlaying){
  //This line is doing three things. It is writing zeros (0x00 in hex) to the SPI bus. This forces this flash chip to send the next data byte out.
  //Then we assign it to OCR2B, which is the register that holds the switching value for Pin 3 PWM. We are setting the pin value to the volume level of that byte.
  OCR2B = SPI.transfer(0x00);
  currentSample ++;
  if(currentSample >= totalSamples){
    isPlaying = false;
    OCR2B = 0;
    myDigitalWrite(PIN_2, ON);
  }
 }
 
}

//This runs on8kHzPulse every 125 microseconds
ISR(TIMER0_COMPA_vect){
 on8kHzPulse();
}

//This is called to initiate audio playblack.
static void playAudio(uint32_t start, uint32_t duration){
 //Audio playing initiation goes here!
 isPlaying = false;
 myDigitalWrite(PIN_2, ON);
 start *= 8;
 currentSample = 0;
 totalSamples = duration * 8;
 myDigitalWrite(PIN_2, OFF);
 SPI.transfer(0x03);//Standard memory read start

 //Send the starting memory address
 SPI.transfer((start >> 16) & 0xFF);
 SPI.transfer((start >> 8) & 0xFF);
 SPI.transfer(start & 0xFF);
 isPlaying = true;

}

//Detects when a button is pressed
static bool checkForButtonPress() {

  //Returns 1 when it detects the button change from on to off
  //The static keyword makes this variable state persistent across all function calls.
  static bool buttonIsPressed = false;

  bool buttonVal = !myDigitalRead(PIN_8);

  //This checks if the button is pressed and it was not previously pressed, update buttonIsPressed and returns 1.
  if (buttonVal && !buttonIsPressed) {
    buttonIsPressed = true;
    return 1;
  }
  //If the button is not pressed but it was previously being pressed, reset the buttonIsPressed for the next press.
  else if (!buttonVal && buttonIsPressed == true) {
    buttonIsPressed = false;
  }

  //End the function and return 0 if the button was not being pressed.
  return 0;

}

//Sends a special serial packet
static void serialTransmitMsg(char id, uint8_t val){
	//This is a simple function to transmit a message with an ID and a value. The ID is a single character that identifies the type of message, and the value is an 8-bit unsigned integer that contains the data.
	uint8_t msg[5] = {(uint8_t)'<', (uint8_t)id, val, (uint8_t)(val + (uint8_t)id), (uint8_t)'>'}; //The message is formatted as <ID, value, checksum>, where the checksum is simply the sum of the ID and value. This is a very basic form of error checking to ensure that the message is received correctly. The start and end characters (< and >) are used to indicate the beginning and end of the message, which can be useful for parsing the message on the receiving end. The null 0 at the end is to make Serial.print happy
	Serial.write(msg, 5);
}

//Checks for a new serial message
static void checkSerial(){
  while(Serial.available()){
    //This It is called whenever a character is received on the serial port. This is where you would put code to handle incoming serial data, such as parsing messages or storing data in a buffer.
    static uint8_t rxIndex = 0;
    static uint8_t rxBuffer[5];

    rxBuffer[rxIndex] = Serial.read(); //Read the received character from the UDR0 register and store it in the rxBuffer. This is necessary to clear the receive buffer and allow the next character to be received.
    rxIndex++;
    
    if(rxIndex == 5){
      if(rxBuffer[0] == '<' && rxBuffer[4] == '>' && rxBuffer[3] == (uint8_t)(rxBuffer[1] + rxBuffer[2])){
        //This checks if the received message is valid by checking the start and end characters and the checksum. If the message is valid, you would put code here to handle the message based on the ID and value. For example, if the ID is 'A', you might set a variable to the value, or if the ID is 'S', you might call a function with the value as an argument.
        lastPacket.id = rxBuffer[1];
        lastPacket.val = rxBuffer[2];
        rxIndex = 0;
        break;
      }
      else{
        rxBuffer[0] = rxBuffer[1];
        rxBuffer[1] = rxBuffer[2];
        rxBuffer[2] = rxBuffer[3];
        rxBuffer[3] = rxBuffer[4];
        rxIndex = 4;
      }
    }
  }


}

//Reads the latest serial message
static msgStruct readSerialMsg(){
	cli();
	char tmpid = lastPacket.id;
	uint8_t tmpval = lastPacket.val;
	lastPacket.id = '\0';
	lastPacket.val = 0;
	sei();
	return (msgStruct){tmpid, tmpval};
}

//equivalent to setup() but with less overhead
static void mySetup(){
 initTimers012ForServoAudioTimekeeping();
 Serial.begin(9600);

 //Initiate SPI
  myPinMode(PIN_2, OUT);
  myDigitalWrite(PIN_2, ON);
  SPI.begin();
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  


 playAudio(BOOT_START, BOOT_START + BOOT_DURATION);
 myDelay(BOOT_DURATION + 1000);

  //Sets PIN_8 to INPUT_PULLUP for whisker detection
 myPinMode(PIN_8, IN);
 myDigitalWrite(PIN_8, ON);

 myPinMode(PIN_7, OUT);
}

//equivalent to loop() but with less overhead
static void myLoop(){

//For precision button control
 static uint8_t precisionFactor = 0;

 //Whisker detection logic goes here. If it hits, make a random scream and jump back in fright!
 if(checkForButtonPress()){
  uint8_t track = random(sizeof(SCREAM_STARTS) / sizeof(SCREAM_STARTS[0]));
  playAudio(SCREAM_STARTS[track], SCREAM_DURATIONS[track]);
  drive(-100, -100);
  myDelay(400);
 }

 //Read the serial packets!
  checkSerial();
  msgStruct currentMsg = readSerialMsg();

  //If there is a message, run a switch statement for the message id.
  if(currentMsg.id){
    switch(currentMsg.id){
        //For L and R joystick positions, update that wheel. If the range is close to 0, take it down to 0 to prevent the servos from wiggling while they're off. Cut the range from -100, 100 to -20, 20 if precisionFactor is true.
      case('L'):
          if(currentMsg.val >= 137 || currentMsg.val <= 117) driveL(map(currentMsg.val, 0, 255, 100 - precisionFactor * 80, -100 + precisionFactor * 80));
          else driveL(0);
        break;
      case('R'):
        if(currentMsg.val >= 137 || currentMsg.val <= 117) driveR( map(currentMsg.val, 0, 255, 100 - precisionFactor * 80 , -100 + precisionFactor * 80));
        else driveR(0);
        break;

        //Enables/disables laser
      case('Z'):
        myDigitalWrite(PIN_7, currentMsg.val);
        break;
        //Plays an audio track when the controller button is pressed
      case('A'):
        playAudio(EFFECT_START[currentMsg.val], EFFECT_DURATION[currentMsg.val]);
        break;

        //enables/disables the precision factor
      case('P'):
        precisionFactor = currentMsg.val;
        break;;
      default:
      break;
    }
  }
 myDelay(10);
}

//Runs mySetup and myLoop. Critically, it does not call the standard Arduino init() function because that would break my timers.
int main(){
 mySetup();
 while(true){
  myLoop();
 }
}
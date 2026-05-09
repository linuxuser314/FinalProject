//This code is adapted from the example from PS2X_lib by Bill Porter. It simply recieves the data from the joystick and streams it over Serial using my custom transmitMsg code.

//Imports the PS2 controller library.
#include <PS2X_lib.h>

//Defines the pins being used for the controller
#define PS2_DAT        8  //14    
#define PS2_CMD        11  //15
#define PS2_SEL        10  //16
#define PS2_CLK        12  //17

#define pressures   false
#define rumble      false

PS2X ps2x; // create PS2 Controller Class

//Extra variables I need
int error = 0;
byte type = 0;


//This code is all for the checksummed transmission and such.
//It is slightly complex, but basically is is just sending a data packet.
inline void serialTransmitMsg(char id, uint8_t val){
	//This is a simple function to transmit a message with an ID and a value. The ID is a single character that identifies the type of message, and the value is an 8-bit unsigned integer that contains the data.
	uint8_t msg[5] = {(uint8_t)'<', (uint8_t)id, val, (uint8_t)(val + (uint8_t)id), (uint8_t)'>'}; //The message is formatted as <ID, value, checksum>, where the checksum is simply the sum of the ID and value. This is a very basic form of error checking to ensure that the message is received correctly. The start and end characters (< and >) are used to indicate the beginning and end of the message, which can be useful for parsing the message on the receiving end.
	Serial.write(msg, 5);
}	

void setup(){
 
  Serial.begin(9600);
  
  delay(1000);  //Added delay to allow the PS2 controller to initialize.
   
  //Starts the PS2 controller
  error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);
  
  //If there's an error, STOP and go into an infinite blinking cycle.
  if(error){
    Serial.println("ERROR CONNECTING TO GAME CONTROLLER");
    pinMode(13, OUTPUT);
    while(true){
      digitalWrite(13, HIGH);
      delay(300);
      digitalWrite(13, LOW);
      delay(300);
    }
  }
  
}

void loop() {
    ps2x.read_gamepad(false, 0); //read controller

    //Send left and right joystick positions over serial packets
    serialTransmitMsg('L', ps2x.Analog(PSS_LY));
    serialTransmitMsg('R', ps2x.Analog(PSS_RY));

    //If a button state has changed, check each individual button for presses or releases to transmit the relevant data packets.
    if(ps2x.NewButtonState()){

        //If the left button is pressed/released, send the packet to enable/disable the laser beam.
      if(ps2x.ButtonPressed(PSB_L2)){
        serialTransmitMsg('Z', 1);
      }
      else if(ps2x.ButtonReleased(PSB_L2)){
        serialTransmitMsg('Z', 0);
      }

      //Various buttons trigger various audio (A) affects. This just sends the message for them.
      if(ps2x.ButtonPressed(PSB_START))serialTransmitMsg('A',0);
      if(ps2x.ButtonPressed(PSB_CROSS)) serialTransmitMsg('A', 1);
      if(ps2x.ButtonPressed(PSB_SQUARE)) serialTransmitMsg('A', 2);
      if(ps2x.ButtonPressed(PSB_CIRCLE)) serialTransmitMsg('A', 3);
      if(ps2x.ButtonPressed(PSB_TRIANGLE)) serialTransmitMsg('A', 4);

      //Enables precision mode
      if(ps2x.ButtonPressed (PSB_R2)) serialTransmitMsg('P',1);
      if(ps2x.ButtonReleased(PSB_R2)) serialTransmitMsg('P',0);

    }
    
  //Extra delay so it doesn't choke on the serial
  delay(50);  
}

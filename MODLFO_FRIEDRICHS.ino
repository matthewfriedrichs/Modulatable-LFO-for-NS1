//Modulatable LFO: NS1 firmware modified by Matthew Friedrichs
//
//To use the LFO connect C1 to A1 and C2 to A2.
//C1 will control the rate of the LFO.
//C2 will control the LFO shape.
//Everything else is optional.
//DAC1 is the output for the LFO.
//A3 is the modulation input for the rate.
//A4 is the modulation input for the shape.
//Please do not plug anything into A5. Weird stuff will happen.
//
//There is an extra MIDI gate out on pin 6.
//This is right below pin 5 which is the defualt gate pin.
//Pin 7 is the LFO reset pin.
//Why not try connecting it to pin 6?
//Pin 8 is the one-shot pin.
//Connect a constant high voltage to turn on one-shot mode.
//Use one of the static voltages by C2.
//Or try a button to periodically pause the LFO at the end of cycle.
//
//Pin 9 is the MIDI square wave output.
//If no MIDI is recieved, the defualt note will be a C.
//Use this to tune your oscillators by ear.
//A0 and M<- are outputs for the LOFI digital noise.
//Pin 13 shows the rate of the LFO.
//It doubles as an extra square wave LFO.
//DAC0 is still MIDI V/Oct.
//
//Below is the documentation for the standard Firmware.

//NS1nanosynth firmware NS1NANOSYNTH_CC_NO_MOZZI_01
//
//This is the basic software with CC# mapped to the digipot.
//In this version MOZZI is not installed, there are problems with I2C compatibility.
//To supply anyway a simple second oscillator with MIDI we use the tone output on the D9 pin.
//A solution could be either to use twi_non_block functions OR implement a software I2C (more CPU-time consuming)
//MIDI is OK on channel 1, pitch bend is managed (+/- 2 semitones) ,modwheel is on DAC1.
//CC numbers are from 30 to 33 mapped to digipot A to D. Mapping is a simple multiplication by two.
//remember that DIGIPOTS are 'floating' and they should be connected as the user wants!
//to setup the main timebase that checks USB midi events, we use the Timer1 functions.
//See Fritzing sketches for reference

//modification also includes: 
//                          CODE CLEANING
//
//
//insert instruction on arcore and lins installation here:
//........................................................

#include "Wire.h"       //i2c lib to drive the quad digipot chip
#include <TimerOne.h>   //Timer lib to generate interrupt callback (used for MIDI USB checking)

#include <SPI.h>         // Remember this line!
#include <DAC_MCP49xx.h> // DAC libs (remember we use an external ref voltage of 2.5V)

DAC_MCP49xx dac(DAC_MCP49xx::MCP4922, 4, -1);   //NS1nanosynth has DAC SS on pin D4

#define MIN_NOTE 36
#define MAX_NOTE MIN_NOTE+61
#define TRIGGER_PIN 5 // default GATE pin on NS1nanosynth.
#define TRIGGER_PIN2 6
#define NOTES_BUFFER 127

const byte NOTEON = 0x09;
const byte NOTEOFF = 0x08;
const byte CC = 0x0B;
const byte PB = 0x0E;

//////////////////////////////////////////////////////////////
// start of variables that you are likely to want to change
//////////////////////////////////////////////////////////////
byte MIDI_CHANNEL = 0; // Initial MIDI channel (0=1, 1=2, etc...), can be adjusted with notes 12-23
//////////////////////////////////////////////////////////////
// end of variables that you are likely to want to change
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
// start of variables for LFO
//////////////////////////////////////////////////////////////
//LFO wave vars
byte lfoSqr[2] = {1, 0};
byte lfoTri[3] = {0, 1, 0};
float lfo[4];
volatile float acc = 0;
//Input Pins
#define LFO_RATE A1
#define LFO_SHAPE A2
#define MOD_RATE A3
#define MOD_SHAPE A4
#define CLEAR A5
#define RESET_PIN 7
#define ONE_SHOT 8
#define LFO_LED 13
//Input Vars
short rate = 0;
short shape = 0;
short modRate = 0;
short modShape = 0;
float shapeSmall = 0;
float modShapeSmall = 0;
short lfoGate = 0;
short lfoGateOld = 0;
//Tone Pin
//This will add the Arduino Tone genrator to Pin 9 as described in the above comments.
//This oscillator will not track MIDI notes below C3
#define TONE_PIN 9
//Add a digital noise source
#define NOISE_PIN A0
//////////////////////////////////////////////////////////////
// end of variables for LFO
//////////////////////////////////////////////////////////////

// Starting here are things that probably shouldn't be adjusted unless you're prepared to fix/enhance the code.
unsigned short notePointer = 0;
int notes[NOTES_BUFFER];
int noteNeeded=0;
float currentNote=0;
byte analogVal = 0;
float glide=0;
int mod=0;
float currentMod=0;
int bend=0;

int DacVal[] = {0, 68, 137, 205, 273, 341, 410, 478, 546, 614, 683, 751, 819, 887, 956, 1024, 1092, 1160, 1229, 1297, 1365, 1433, 1502, 1570, 1638, 1706, 1775, 1843, 1911, 1979, 2048, 2116, 2184, 2252, 2321, 2389, 2457, 2525, 2594, 2662, 2730, 2798, 2867, 2935, 3003, 3071, 3140, 3208, 3276, 3344, 3413, 3481, 3549, 3617, 3686, 3754, 3822, 3890, 3959, 4027, 4095};

byte addresses[4] = { 0x00, 0x10, 0x60, 0x70 }; //digipot address
byte digipot_addr= 0x2C;  //i2c bus digipot IC addr
byte valorepot=0; //only for debug routine... delete.
byte ccpot0_ready=0;
byte ccpot1_ready=0;
byte ccpot2_ready=0;
byte ccpot3_ready=0;
byte pot0=0;
byte pot1=0;
byte pot2=0;
byte pot3=0;


unsigned short DacOutA=0;
unsigned short DacOutB=0;
 
void setup(){
  pinMode( TRIGGER_PIN, OUTPUT ); // set GATE pin to output mode
  analogWrite( TRIGGER_PIN, 0);  //GATE down
  pinMode( TRIGGER_PIN2, OUTPUT ); // set GATE2 pin to output mode
  analogWrite( TRIGGER_PIN2, 0);  //GATE down
  pinMode( TONE_PIN, OUTPUT ); // set TONE pin to output mode
  analogWrite( TONE_PIN, 0);  //Default Value
  tone(TONE_PIN, 261.625565); //run C if no MIDI
  pinMode( NOISE_PIN, OUTPUT );
  analogWrite( NOISE_PIN, 0);
  pinMode( LFO_LED, OUTPUT );
  analogWrite( LFO_LED, 0);
  pinMode( LFO_RATE, INPUT );
  pinMode( LFO_SHAPE, INPUT );
  pinMode( MOD_RATE, INPUT );
  pinMode( MOD_SHAPE, INPUT );
  pinMode( CLEAR, OUTPUT );
  pinMode( RESET_PIN, INPUT );
  pinMode( ONE_SHOT, INPUT );
  dac.setGain(2);
  Wire.begin();
  Timer1.initialize(8000);          //check MIDI packets each XXX ms
  Timer1.attachInterrupt(updateNS1); // blinkLED to run every 0.15 seconds
  
}

void i2c_send(byte addr, byte a, byte b)      //wrapper for I2C routines
{
    Wire.beginTransmission(addr);
    Wire.write(a);
    Wire.write(b);
    Wire.endTransmission();
//    Wire.send(address);             // send register address
//    Wire.send(val);                 // send value to write
//    Wire.endTransmission();         // end transmission
}

void DigipotWrite(byte pot,byte val)        //write a value on one of the four digipots in the IC
{
          i2c_send( digipot_addr, 0x40, 0xff );
          i2c_send( digipot_addr, 0xA0, 0xff );
          i2c_send( digipot_addr, addresses[pot], val);  
}

void updateNS1(){
  while(MIDIUSB.available() > 0) { 
      // Repeat while notes are available to read.
      MIDIEvent e;
      e = MIDIUSB.read();
      if(e.type == NOTEON) {
        if(e.m1 == (0x90 + MIDI_CHANNEL)){
          if(e.m2 >= MIN_NOTE && e.m2 <= MAX_NOTE){
            if(e.m3==0)         //Note in the right range, if velocity=0, remove note
              removeNote(e.m2);
            else                //Note in right range and velocity>0, add note
              addNote(e.m2);              
          } else if (e.m2 < MIN_NOTE) {
            //out of lower range hook      
          } else if (e.m2 > MAX_NOTE) {
            //out of upper range hook
          }
        }
      }
      
      if(e.type == NOTEOFF) {
        if(e.m1 == 0x80 + MIDI_CHANNEL){
          removeNote(e.m2);
        }
      }
      
      // set modulation wheel
      if (e.type == CC && e.m2 == 1)
      {
        if (e.m3 <= 3)
        {
          // set mod to zero
         mod=0;
         dac.outputB(0);
        } 
        else 
        {
          mod=e.m3;
          DacOutB=mod*32;
          dac.outputB(DacOutB);
        }
      }

      //set digipots A to D with CC from 30 to 33
      if (e.type == CC && e.m2 == 30){
        ccpot0_ready=1;
        pot0=e.m3<<1;
      }
      if (e.type == CC && e.m2 == 31){
        ccpot1_ready=1;
        pot1=e.m3<<1;
      }
      if (e.type == CC && e.m2 == 32){
        ccpot2_ready=1;
        pot2=e.m3<<1;
      }
      if (e.type == CC && e.m2 == 33){
        ccpot3_ready=1;
        pot3=e.m3<<1;
      }

      
      // set pitch bend
      if (e.type == PB){
       if(e.m1 == (0xE0 + MIDI_CHANNEL)){
          // map bend somewhere between -127 and 127, depending on pitch wheel
          // allow for a slight amount of slack in the middle (63-65)
          // with the following mapping pitch bend is +/- two semitones
          if (e.m3 > 65){
            bend=map(e.m3, 64, 127, 0, 136);
          } else if (e.m3 < 63){
            bend=map(e.m3, 0, 64, -136, 0);
          } else {
            bend=0;
          }
          
          if (currentNote>0){
            playNote (currentNote, 0);
          }
        }
      }
      
      MIDIUSB.flush();
   }
   
  if (noteNeeded>0){
    // on our way to another note
    if (currentNote==0){
      // play the note, no glide needed
      playNote (noteNeeded, 0);
      
      // set last note and current note, clear out noteNeeded becase we are there
      currentNote=noteNeeded;
      noteNeeded=0;
    } else {
      if (glide>0){
        // glide is needed on our way to the note
        if (noteNeeded>int(currentNote)) {
          currentNote=currentNote+glide;
          if (int(currentNote)>noteNeeded) currentNote=noteNeeded;     
        } else if (noteNeeded<int(currentNote)) {
          currentNote=currentNote-glide;
          if (int(currentNote)<noteNeeded) currentNote=noteNeeded;
        } else {
          currentNote=noteNeeded;
        }
      } else {
        currentNote=noteNeeded;
      }
      playNote (int(currentNote), 0);
      if (int(currentNote)==noteNeeded){
        noteNeeded=0;
      }
    }
  } else {
    if (currentNote>0){
    }
  }
}

//audioMath
static float wrap(float num)
 {
  return(num - ((int)num));
 }

 static float wrapx(float num, float val)
 {
  num /= val;
  return((num - ((int)num)) * val);
 }

 static float crossfade(float num1, float num2, float percent)
 {
  return(num1 + ((-num1 + num2) * percent));
 }

 static float clip(float num, float val)
 {
  if (num > val)
  {
    num = val;
  }
  return num;
 }

 void loop(){
  //it is necessary to move the i2c routines out of the callback. probably due to some interrupt handling!
 if(ccpot0_ready){
    DigipotWrite(0,pot0);
    ccpot0_ready=0;
    }
 if(ccpot1_ready){
    DigipotWrite(1,pot1);
    ccpot1_ready=0;
    }
 if(ccpot2_ready){
    DigipotWrite(2,pot2);
    ccpot2_ready=0;
    }
 if(ccpot3_ready){
    DigipotWrite(3,pot3);
    ccpot3_ready=0;
    }
    //Run the LFO
    //Read the CLEAR output pin between each read as a buffer
    rate = analogRead(LFO_RATE);
    analogRead(CLEAR);
    modRate = analogRead(MOD_RATE);
    analogRead(CLEAR);
    shape = analogRead(LFO_SHAPE);
    analogRead(CLEAR);
    modShape = analogRead(MOD_SHAPE);
    shapeSmall = shape / 341.00;
    modShapeSmall = modShape / 341.00;
    shapeSmall+=modShapeSmall;
    clip(shapeSmall, 3);
    acc += (.000001 + ((pow((float)(rate/1023.00), 4) + pow((float)(modRate/1023.00), 4)) * 5000)/44100);
    if (analogRead(ONE_SHOT) > 200)
    {
      acc = clip(acc, 1.00);
    }
    else
    {
      acc = wrap(acc);
    }
    analogRead(CLEAR);
    //reset the LFO
    lfoGate = digitalRead(RESET_PIN);
    analogRead(CLEAR);
    if (lfoGate > lfoGateOld)
    {
      acc = 0;
    }
    lfoGateOld = lfoGate;
    //LFO calc
    lfo[0] = 1 - acc;
    lfo[1] = crossfade(lfoTri[(int)(acc * 2)], lfoTri[(int)wrapx(1 + (acc * 2), 2.00)], wrap(acc * 2));
    lfo[2] = lfoSqr[(int)(acc * 2)];
    lfo[3] = acc;
    dac.outputB(crossfade(lfo[(int)(shapeSmall)], lfo[1 + (int)(shapeSmall)], wrap((float)shapeSmall)) * 4095);
    //Write square wave state to pin 13 LED
    //This also gives an extra sqare wave LFO
    analogWrite(LFO_LED, lfo[2] * 255);
    //Just for fun: a digital noise source
    analogWrite(NOISE_PIN, random(256));
    
    
 }



void playNote(byte noteVal, float myMod)
  {

    analogVal = map(noteVal, MIN_NOTE, MAX_NOTE, 0, 2550)/10;  //  analogVal = map(noteVal, MIN_NOTE, MAX_NOTE, 0, 2550+oscAdjust)/10;
  if (analogVal > 255)
  {
  analogVal=255;
  }
  if (myMod != 0)
  {
    //analogVal=myMod+int(1.0*analogVal+(1.0*myMod*(mod/127)/40));
  }
//  DacOutB=DacOutB+myMod;  //attenzione!! non volendo suono MOLTO PARTRICOLARE su dacB !!!!!
  // see if this note needs pitch bend
    if (bend != 0)
    {
    analogVal=analogVal+bend;
    }
//  analogWrite(NOTE_PIN1, analogVal);
      int DacOutA=DacVal[noteVal-MIN_NOTE];
      if (bend != 0)
      {
       DacOutA=DacOutA+bend;
      }
      dac.outputA(DacOutA);
      analogWrite(TRIGGER_PIN, 255); //GATE ON
      analogWrite(TRIGGER_PIN2, 255); //GATE ON
      //Tone reference oscillator
      //This ocillator will not track MIDI notes below C3
      float noteDiv = (noteVal / 12.00);
      tone(TONE_PIN, (int) (pow(2, (noteDiv - 4)) * 261.625565));
      
      
      
}

void addNote(byte note){
  boolean found=false;
  // a note was just played
  
  // see if it was already being played
  if (notePointer>0){
    for (int i=notePointer; i>0; i--){
      if (notes[i]==note){
        // this note is already being played
        found=true;
        
        // step forward through the remaining notes, shifting each backward one
        for (int j=i; j<notePointer; j++){
          notes[j]=notes[j+1];
        }
        
        // set the last note in the buffer to this note
        notes[notePointer]=note;
        
        // done adding note
        break;
      }
    }
  }
  
  if (found==false){
    // the note wasn't already being played, add it to the buffer
    notePointer=(notePointer+1) % NOTES_BUFFER;
    notes[notePointer]=note; 
  }
  
  noteNeeded=note;
}

void removeNote(byte note){
  boolean complete=false;
  
  // handle most likely scenario
  if (notePointer==1 && notes[1]==note){
    // only one note played, and it was this note
    //analogWrite(NOTE_PIN1, 0);
    notePointer=0;
    currentNote=0;
    
    // turn light off
    analogWrite(TRIGGER_PIN, 0);
    analogWrite(TRIGGER_PIN2, 0);
 
  } else {
    // a note was just released, but it was one of many
    for (int i=notePointer; i>0; i--){
      if (notes[i]==note){
        // this is the note that was being played, was it the last note?
        if (i==notePointer){
          // this was the last note that was being played, remove it from the buffer
          notes[i]=0;
          notePointer=notePointer-1;
          
          // see if there is another note still being held
          if (i>1){
            // there are other that are still being held, sound the most recently played one
            addNote(notes[i-1]);
            complete=true;
          }
        } 
        else{
          // this was not the last note that was being played, just remove it from the buffer and shift all other notes
          for (int j=i; j<notePointer; j++){
            notes[j]=notes[j+1];
          }
          
          // set the last note in the buffer to this note
          notes[notePointer]=note;
          notePointer=notePointer-1;
          complete=true;
        }
        
        if (complete==false){
          // we need to stop all sound
          //analogWrite(NOTE_PIN1, 0);
          
          // make sure notePointer is cleared back to zero
          notePointer=0;
          currentNote=0;
          noteNeeded=0;
          break;
        }
        
        // finished processing the release of the note that was released, just quit
        break;
        //F<3
      }
    }
  }
}


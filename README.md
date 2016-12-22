# Modulatable-LFO-for-NS1
This is a modification to the stock NS1 firmware that adds a fully featured LFO and some other goodies.
Check out the audio demo.
https://soundcloud.com/user-787346291/ns1-modulatable-lfo-demo

Connect C1 to A1 and C2 to A2. C1 controls LFO rate. C2 controls LFO shape.
A3 and A4 are the modulation input. A3 is the LFO rate modulation. A4 is the shape modulation.
Please do not connect anything to A5. Weird stuff will happen.

There is an extra MIDI gate output on pin 6 right below the standard gate out of pin 5.
The LFO rest pin is pin 7. Try using the extra MIDI gate pin as a reset trigger.
LFO one shot is on pin 8. Send a static high voltage to turn on one shot mode.
You can use the static voltages next to C2 or even a button.

Pin 9 has an extra audio rate square wave oscillator controlled by MIDI.
Pins A0 and M<- are LOFI noise outputs.
Pin 13 is an extra square wave LFO that tracks the rate of the oscillator.

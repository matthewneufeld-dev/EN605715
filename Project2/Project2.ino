/*
  EN.605.715.81.SP26 Software Development for Real-Time Embedded Systems
  Han Nguyen, Matthew Neufeld, Xinyu Xiao | February 7th, 2026
  Project 2 - Serial Transmission of Temperature
  Project Requirement: Calibrate a temperature sensor connected to an Arduino.

  Using a Round Robbin with Interrupts design, get the Arduino to capture the
  temperature and convert to Fahrenheit

  After the temperature has stabilized, start recording the Arduino
  temperature at a periodic rate of around 10s at room temperature, continue
  to record periodic temperatures after placing the Arduino in the refrigerator
  for 5 minutes, and continue to record periodic temperatures after removing
  from refrigerator and staying in the room for 5 minutes.

  You should have the ISR on a timer, such that it runs about every 10 seconds
  Transmit the time and temp across a Serial bus like USB to your Host,
  others could be SPI or I2C if your Host had one of those busses.
  Export as a comma separated value file, read into a spreadsheet program
  and plot the temperature vs time since we began the program/timer
*/

//Parameters
#define ANALOG_PIN A0          //Set the ADC pin used
#define ANALOG_REF 5.0         //Analog Reference is +5V for Arduino UNO R3
#define calibrationValue 0.57  //Temperature measurement calibration value
#define circularBufferSize 5   //Size of the circular buffer to determine that measurement is stable
#define readingStableDelta 2   //Maximum delta to initially determine that temperature has stabilized

//Measurement Variables
volatile float rawAnalogInput;        //Analog input read by ISR
float localRaw = 0.0;                 //Analog input copied in main loop
float voltageInput;                   //Calculated voltage input based on analog reference
float temperatureInputC;              //Calculated input temperature in C
float temperatureInputF;              //Calculated input temperature in F
volatile bool newMeasurement = false; //ISR flag to main loop if a new measurement is available
volatile byte ISRCounter = 0;         //ISR counter how many interrupts occured

//Circular Buffer Variables
float rawAnalogInputBuffer[circularBufferSize] = {0.0, 0.0, 0.0, 0.0, 0.0};
byte rawAnalogInputIndex = 0;
int readingCount = 0;
volatile bool stabilizedReading = false;
float maxReadingDelta = 0;
volatile unsigned long secondsSinceStart = 0;  //Seconds since recording started incremented by ISR
unsigned long localSecondsSinceStart = 0;      //Local seconds since start copied in main loop

//Arduino UNO R3 Specifications:
//Default: 10-bit ADC
//Map input voltages: (0, +5VDC) -> (0, 1023)
//Resolution: 5 V / 1023 units or 0.0049 volts (4.9 mV) per unit.
float analogToVoltageMultiplier = ANALOG_REF / 1023.0;

void setup() {
  //Arduino Uno R3 Timer #1 Setup
  //System Clock - 16 MHz
  //Selecting prescaler to 256: Timer 1 Frequency = 16MHz/256 = 62.5 KHz    
  //Pulse Time = 1/62.5 KHz =  16us
  //For a 1s timer: 1s/16us = 62500. This is the value the OCR register should have
  //We cannot do 10 seconds because Timer1 is only 16-bit, so max value is 65,535
  //Reference: https://electronoobs.com/eng_arduino_tut140.php
  cli();                    // Stop interrupts until settings are done
  TCCR1A = 0;               // Reset entire TCCR1A to 0 
  TCCR1B = 0;               // Reset entire TCCR1B to 0
  TCCR1B |= B00000100;      // Set CS12 | CS11 | CS10 to 100 so we get prescalar 256  
  TIMSK1 |= B00000010;      // Set OCIE1A to 1 so we enable compare match A
  OCR1A = 62500 - 1;        // Set OCR register for compare (Counts are 0..OCR1A inclusive)
  Serial.begin(9600);
  analogReference(DEFAULT); //Set analog read resolution
  Serial.println("Initial temperature readings started!");
  Serial.println("Data format: [Time],[Temperature in F]");
  sei();                    //Enable back the interrupts
}
void loop() {
  // Measurement
  if (newMeasurement) {
    noInterrupts();            //Begin critical section
    localRaw = rawAnalogInput; //Copy analog input read by ISR
    localSecondsSinceStart = secondsSinceStart;
    newMeasurement = false;    //Flag for new measurement
    interrupts();              //End critical section
    voltageInput = localRaw * analogToVoltageMultiplier;
    temperatureInputC = (voltageInput - 1.25) / 0.005;
    temperatureInputF = temperatureInputC*1.8 + 32;
    
    //Put value in buffer
    rawAnalogInputBuffer[rawAnalogInputIndex] = temperatureInputF;
    if (rawAnalogInputIndex < (circularBufferSize - 1) ) {
      rawAnalogInputIndex++;
      readingCount++;
    } else {
      rawAnalogInputIndex = 0;
      readingCount++;
    }
    Serial.print(localSecondsSinceStart);
    Serial.print(",");
    Serial.println(temperatureInputF + calibrationValue);
  }

  // Stabilization Calculation
  if (!stabilizedReading) {
    maxReadingDelta = 0;
    for (byte i = 0; i <= circularBufferSize - 2; i++) {
      if ( (abs(rawAnalogInputBuffer[i+1] - rawAnalogInputBuffer[i])) > maxReadingDelta ) {
        maxReadingDelta = abs(rawAnalogInputBuffer[i+1] - rawAnalogInputBuffer[i]);
      }
    }
    if (maxReadingDelta <= readingStableDelta && readingCount > circularBufferSize - 1) {
      stabilizedReading = true;
      Serial.println("Initial temperature readings have stabilized.");
    }
  }
  //Error checking - should only have values between 0 and 1023
  if (localRaw < 0.0) {
    Serial.println("Analog Read Low Error! Value less than zero.");
  }
  if (localRaw > 1023.0) {
    Serial.println("Analog Read High Error! Value more than 1023.");
  }
}

//Interrupt Service Routine:
//With the settings above, this IRS will trigger each 1000ms.
//We will only read the raw analog input here to keep the interrupt short.
ISR(TIMER1_COMPA_vect){
  //Set the timer back to 0 to reset for the next interrupt
  TCNT1  = 0;
  secondsSinceStart++; //Increment seconds since start
  if (!stabilizedReading) {
    //If readings are not stabilized, read every second
    rawAnalogInput = analogRead(ANALOG_PIN); //Okay to do analogRead at 1 Hz
    newMeasurement = true;
  } else {
    //If readings are stabilized, read every 10 seconds
    ISRCounter++; //Increase the counter every second
    if (ISRCounter >= 10) {
      rawAnalogInput = analogRead(ANALOG_PIN); //Okay to do analogRead at 0.1 Hz
      newMeasurement = true;
      ISRCounter = 0;
    }
  }
}
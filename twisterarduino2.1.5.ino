

// Version 2.1.5

//  2.1.3 - added zero crossing delay
//  2.1.4 - added emergency stop button - currently commented out
//  2.1.5 - fixed three-digit decimal feature, added summary of configuration
//          before starting.


#include "TimerOne.h"

char getchr()
  {
    char ch;

    while(Serial.available() <= 0); //stay here til a character is available

    ch = Serial.read();
    //Serial.print(ch);
    return ch;
  }


int intscan1000(long *val)
  {
   /* This function tries to input a decimal number and convert it to
      an int, multiplied by 1000 to preserve the decimal value, up to 
      three digits. More than three decimal digits are thrown away (truncated).
      If a problem is encountered (non-digit input), the function returns
      a -1, otherwise 0
    */
   char chr;
   int denom;
   
   *val = 0;
   denom = 100;

   chr = getchr();
   
   while(chr != '\r')
     {
      //Serial.println("loop");
      if(isDigit(chr))
        {
         *val = *val*10 + chr - '0';
         //Serial.print("Here 1 ");
         //Serial.println(*val);
        }
      else if(chr == '.')
        {
         *val = *val*1000;   //****
         //Serial.print("Here 2 ");
         //Serial.println(*val);
         chr = getchr();
         while(chr != '\r')
           {
            //Serial.println("Here 3");
            //Serial.flush();
            if(isDigit(chr))
              {
               *val = *val + (chr - '0') * denom;
               //Serial.print("Here 4 ");
               //Serial.println(*val);
               //Serial.flush();
               chr = getchr();
               denom = denom/10;
              }
            else
              {
               //Serial.println("Error");
               *val = 0;
               return -1;
              }
           }
         return 0;
        }
      else
        {
         //Serial.println("Error\n");
         *val = 0;
         return -2;
        }
      chr = getchr();
     }

   *val = *val*1000;
   //Serial.println();
   //Serial.flush();
   return 0;
   
  } //END intscan1000

  int print1000(long val)
     {
      // This routine prints the argument which is a long that repesents a value that has
      // three decimal digits, like those created by intscan1000

      long whole, frac;

      whole = val / 1000;
      frac = val - whole * 1000;
      Serial.print(whole);
      Serial.print('.');
      if(frac < 10) Serial.print("00");
      else if (frac < 100) Serial.print("0");
      Serial.println(frac); 
      return 0;
     }

   // Following are parameters of the twister hardware, represented as long ints
   long int stepsPerRev_i,       // Set by dip switches on the motor controller
         // pulley ratios - numerator and demoninator are stored separately
         //                 to save the accuracy of the fraction for ints
            pulleyRatioNum_i,   // pulley ratio numerator
            pulleyRatioDenom_i, // pulley ratio denominator
            stepsPerDegree_i,   // calculated steps per degree
            usecsPerStep_i,     // microseconds per step

            totalStepsCW_i,     // calculated number of CW steps
            totalStepsCCW_i,    // calculated number of CCW steps
            totalSpan_i,        // total number of steps in span of movement
            totalSteps_i;       // total number of pulses generated        

   // The following represent values received from the PC
   long int degreesPerSec_i,    // desired platform speed, degrees/sec
            degreesPeruSec_i,   // desired platform speed, degrees/usec
            degreesCW_i,        // desired movement CW, in degrees
            degreesCCW_i,       // desired movement CCW, degrees
            secsPerRun_i;       // experiment run length, seconds

   long int direction_i;
   
int directionPin = 4; // direction pin to the motor controller
int pulsePin = 3;     // pulse pin to the motor controller
int MMMotorPin = 5;   // MotorOn pin to MM 
int MMdirPin = 6;     // pin that specifies direction to the MM
int emergency = 8;    // emergency stop pin (normally closed, when pressed, switch opens

uint8_t Active;       // 0 - no experiment is going, 1 - experiment in progress 

volatile uint8_t motorOn;  // 0 = Motor off, 1 = Motor on
volatile uint8_t dir;      // current direction, 0 = CW, 1 = CCW
volatile uint8_t panic;    // emergency stop indicator
volatile long numSteps;    // Keeps track of the number of steps taken so far
volatile long stepPos;     // Current position in number of steps (+ = CW, - = CCW)
long maxDelayTimeDir;      // delay between direction changes in secs.
long maxDelayTimeZero;     // delay when crossing zero, in secs.
long maxDelayDir;          // number of steps to delay when changing direction
long stepsPerSec;          // number of steps per second (calculated)
long maxDelayZero;         // number of steps to delay when crossing zero
long maxDelay;             // how many steps to delay
long delayCount;           // Count of number of steps we have delayed so far

void turnMotorOn()
  {
   motorOn = 1;
   digitalWrite(MMMotorPin, 1);
  } // END turnMotorOn

void turnMotorOff()
  {
   motorOn = 0;
   digitalWrite(MMMotorPin, 0);
  } // END turnMotorOff


void setup()
  {

   // Set up connections used between the arduino and motor controller`
   pinMode(directionPin, OUTPUT);
   pinMode(pulsePin, OUTPUT);
   pinMode(MMdirPin, OUTPUT);
   pinMode(MMMotorPin, OUTPUT);
   pinMode(LED_BUILTIN, OUTPUT);
   pinMode(emergency, INPUT_PULLUP);
   

   // Initial values for hardware constants - these shouldn't change unless the
   //   hardware is changed 

   stepsPerRev_i =       51200; // set via dip switches on motor controller
   // These next two values set the motor-to-platform ratio. They are stored as two
   // values (num/denom) so integer math can be used for subsequent calculations 
   pulleyRatioNum_i =    36;
   pulleyRatioDenom_i =  7;

   stepsPerDegree_i = stepsPerRev_i * pulleyRatioNum_i / 
                      (360 * pulleyRatioDenom_i);
   

   Serial.begin(9600);
   //Serial.flush();
   Serial.println();
   Serial.print(F("stepsPerDegree "));
   Serial.println(stepsPerDegree_i);
   
   Active = 0;  // no experiment in progress initially
   turnMotorOff();
   numSteps = 0; // no pulses to motor initially
   panic = 0;   // emergency stop not pushed
   
   Timer1.initialize(1000); //initializes Timer 1 - time will change in loop() 
   Timer1.attachInterrupt(pulseint); 

   //Serial.flush();
   Serial.println();
   Serial.println("Hello from arduino!"); // Announce arduino is ready!
  } // END setup

char incoming = 0;

//void serialEvent()
//   {
//    incoming = Serial.read();
//    if( incoming = 's' || incoming == 'S')
//       {
//        noInterrupts();
//        Active = 0;
//        turnMotorOff();
//        panic = 1;
//       }
//   }

//Interrupt handling function
void pulseint() 
  {
    // If the emergency button is pressed shut everything down 
   //if(digitalRead(emergency) == 1)
   //  {
      //noInterrupts();
      //Active = 0; EMERGENCY BUTTON IS OFF. Uncomment these lines to turn it on.
      //turnMotorOff(); EMERGENCY BUTTON IS OFF. Uncomment these lines to turn it on.
      //panic = 1; EMERGENCY BUTTON IS OFF. Uncomment these lines to turn it on.
      //Serial.println("PANIC!");
   //  } 

    if(Serial.available() > 0)
      {
       incoming = Serial.read();
       if(incoming == 's' || incoming == 'S')
          {
           noInterrupts();
           Active = 0;
           turnMotorOff();
           panic = 1;
           //Serial.println("Panic Stop!!");
          }
      }
      
   if (Active)
     {
      // Count this clock step. If we have reach the total number of steps for
      // the experiment, keep moving to center then shut everything down

      numSteps++;
      if(numSteps >= totalSteps_i) 
        {
         // We have reached the time specified for the experiment, but we'll
         // keep moving back to center (ie, when stepPos == 0).

         if (stepPos == 0) // we've reached center, shut everything down.
           {
            turnMotorOff();
            Active = 0;
           } 
        }

      // If we are moving, then keep moving

      if (motorOn)
        {
         // This is the actual pulse to the motor

         digitalWrite(pulsePin, 1);
         digitalWrite(pulsePin, 0);
  
         // Check if we need to change direction

         if(dir == 0) stepPos++; else stepPos--;
         if(stepPos >= totalStepsCW_i)
           {
            dir = 1;                       //change from CW to CCW
            digitalWrite(directionPin, 1);
            digitalWrite(MMdirPin, 1);
            if(maxDelayDir > 0)
              {
               turnMotorOff();
               maxDelay = maxDelayDir;
               delayCount = 0;  
              }
           }
         else if(stepPos <= -totalStepsCCW_i)
           {
            dir = 0;                      //change from CCW to CW
            digitalWrite(directionPin, 0);
            digitalWrite(MMdirPin, 0);
            if(maxDelayDir > 0)
              {
               turnMotorOff();
               maxDelay = maxDelayDir;
               delayCount = 0;
              }
           }
         if (stepPos == 0)
            if(maxDelayZero > 0)
              {
                turnMotorOff();
                maxDelay = maxDelayZero;
                delayCount = 0; 
              }
        }
      else
        { // if motor is off, check to see if we need to delay before turning
          // motor back on
          if (maxDelay > 0)
            { 
             delayCount++; 
             if (delayCount > maxDelay) // Done delaying - turn motor back on
               {
                delayCount = 0;
                turnMotorOn();
                // motorOn = 1;
                //digitalWrite(pulsePin, 1);
                //digitalWrite(pulsePin, 0);
               }
            } // END if maxdelay
        } // END if motorOn

     } // END if Active

  } // END pulseint 


void loop()
  {
    int result;
    char ch;
    uint8_t olddir;
        
   // Get all parameters from user for this experiment

TRY: Serial.print(F("Enter platform speed (degrees per second) "));
   result = intscan1000(&degreesPerSec_i);
   print1000(degreesPerSec_i);
   //Serial.println(result);
   Serial.println(degreesPerSec_i);
   
   if(degreesPerSec_i > 5000 || degreesPerSec_i < 0)
     {
      Serial.println(F("Number must be (0 < 5). Please try again)"));
      goto TRY;
     }
   //Serial.println(result);
   //Serial.println(degreesPerSec_i,2);
   //Serial.println();

   Serial.print(F("Enter degrees in CW direction "));
   result = intscan1000(&degreesCW_i);
   print1000(degreesCW_i);

   Serial.print(F("Enter degrees in CCW direction "));
   result = intscan1000(&degreesCCW_i);
   print1000(degreesCCW_i);
    
   Serial.print(F("Enter initial direction (0=CW, 1=CCW) "));
   intscan1000(&direction_i);
   dir = direction_i;
   olddir = dir;
   print1000(direction_i);
   
   // Caclulate total pulses in each direction, and total span of movement
   totalStepsCW_i  = stepsPerDegree_i * degreesCW_i / 1000;
   totalStepsCCW_i = stepsPerDegree_i * degreesCCW_i / 1000;
   totalSpan_i     = totalStepsCW_i + totalStepsCCW_i;
   Serial.print(F("totalStepsCW_i "));
   Serial.println(totalStepsCW_i);
   Serial.print(F("totalStepsCCW_i "));
   Serial.println(totalStepsCCW_i);
   
   Serial.print(F("Enter time to delay between direction changes (secs) "));
   intscan1000(&maxDelayTimeDir);
   print1000(maxDelayTimeDir);
   
   Serial.print(F("maxDelayTimeDir "));
   Serial.println(maxDelayTimeDir);
   
   stepsPerSec = stepsPerDegree_i * degreesPerSec_i /1000;
   Serial.print(F("stepsPerSec "));
   Serial.println(stepsPerSec);
   
   maxDelayDir = stepsPerSec * maxDelayTimeDir/1000; 
   
   Serial.print(F("maxDelayDir "));
   Serial.println(maxDelayDir);
     
   Serial.print(F("Enter time to delay around Zero Crossing (secs) "));
   intscan1000(&maxDelayTimeZero);
   maxDelayZero = stepsPerSec * maxDelayTimeZero/1000;
   
   print1000(maxDelayTimeZero);
   Serial.print(F("maxDelayTimeZero "));
   Serial.println(maxDelayTimeZero);
    
   Serial.print(F("Enter total time for experiment "));
   intscan1000(&secsPerRun_i);
   print1000(secsPerRun_i);
   
   // Determine speed and total number of steps required
   usecsPerStep_i = (long)1000000 / (stepsPerDegree_i * degreesPerSec_i / 1000);
   Serial.print(F("usecsPerStep "));
   Serial.println(usecsPerStep_i);
   totalSteps_i = stepsPerSec * secsPerRun_i/1000;
   Serial.print(F("totalSteps_i "));
   Serial.println(totalSteps_i);
   
   // print out everything

   //Serial.println();
   //Serial.print(F("Steps per revolution "));
   //Serial.println(stepsPerRev_i);

   //Serial.print(F("Pulley ratio numerator "));
   //Serial.println(pulleyRatioNum_i);

   //Serial.print(F("Pulley ratio denominator "));
   //Serial.println(pulleyRatioDenom_i);

   //Serial.print(F("Steps per degree "));
   //Serial.println(stepsPerDegree_i); 

   Serial.println();
   Serial.println(F("Experiment parameters:"));
   
   Serial.print(F("Degrees per Second "));
   print1000(degreesPerSec_i);

   //Serial.print(F("Total steps CW "));
   //Serial.println(totalStepsCW_i);
   Serial.print(F("Degrees CW "));
   print1000(degreesCW_i);
    
   //Serial.print(F("Total steps CCW "));
   //Serial.println(totalStepsCCW_i); 
   Serial.print(F("Degrees CCW "));
   print1000(degreesCCW_i);
    
   Serial.print(F("Initial direction: "));
   if(dir == 0) Serial.println(F("CW"));
   else         Serial.println(F("CCW"));

   //Serial.print(F("Total end to end steps "));
   //Serial.println(totalSpan_i);
   
   //Serial.print(F("Microseconds per step "));
   //Serial.println(usecsPerStep_i);

   //Serial.print(F("Total number of steps "));
   //Serial.println(totalSteps_i);

   Serial.print(F("Delay between direction changes "));
   //Serial.println(maxDelayTimeDir/1000);
   print1000(maxDelayTimeDir);
    
   Serial.print(F("Delay at zero crossing "));
   //Serial.println(maxDelayTimeZero/1000);
   print1000(maxDelayTimeZero);
    
   Serial.print(F("Total time for experiment "));
   //Serial.println(secsPerRun_i/1000);
   print1000(secsPerRun_i);
    
   Serial.println();
   Serial.println(F("Hit ENTER to start experiment ('R' to re-enter, 'S' to stop)"));
   ch = getchr();

   if(ch == 'R' | ch == 'r')
      {
        ch = getchr();
        goto TRY;
      }
   
   Serial.println(F("Experiment started"));
   
   Timer1.initialize(usecsPerStep_i); //initializes the Timer 1
   Timer1.attachInterrupt(pulseint); 
   numSteps = 0;
   stepPos = 0;
   Active = 1;
   turnMotorOn();
   //olddir = 10; 
   
   digitalWrite(LED_BUILTIN, 1);
   while(Active)
     {
      if(dir != olddir)  //direction change detected
        {
         if(dir == 0) Serial.println("CW");
         else         Serial.println("CCW");
         Serial.flush();
         //olddir = dir;
        }
      olddir = dir;
//      if(stepPos == 0) //
//        {
//         Serial.println("Zero");
//         Serial.flush();
//         while(stepPos == 0); //prevents duplicate messages
//        }
     } // END while Active

   if( panic == 1)
     {
      Serial.println("PANIC!");
      Serial.flush();
      //noInterrupts();
     }
   digitalWrite(LED_BUILTIN, 0);
   Serial.println(F("Done!!"));
   turnMotorOff();
   //motorOn = 0; 
   //digitalWrite(MMMotorPin, 0);
   
   Serial.println(stepPos);
   Serial.println("Hit any key to rerun");
   ch = getchr();
   Serial.println(); Serial.println();
   Serial.flush();
   
  } //END loop

volatile long pos_comp[nr_of_coordinated_motors];
volatile long next_pos_comp[nr_of_coordinated_motors];
long last_target[nr_of_coordinated_motors];
const unsigned long default_4361_start_config = 0
| _BV(0) //x_target requires start
| _BV(4)  //use shaddow motion profiles
| _BV(5) //external start is an start
;

void prepareTMC4361() {
  pinModeFast(START_SIGNAL_PIN,INPUT);
  digitalWriteFast(START_SIGNAL_PIN,LOW);

  //initialize CS pin
  digitalWriteFast(CS_4361_1_PIN,LOW);
  pinModeFast(CS_4361_1_PIN,OUTPUT);
  digitalWriteFast(CS_4361_2_PIN,LOW);
  pinModeFast(CS_4361_2_PIN,OUTPUT);
  digitalWriteFast(CS_4361_3_PIN,LOW);
  pinModeFast(CS_4361_3_PIN,OUTPUT);

  //will be released after setup is complete   
  for (char i=0; i<nr_of_coordinated_motors; i++) {
    pinModeFast(motors[i].target_reached_interrupt_pin,INPUT);
    digitalWriteFast(motors[i].target_reached_interrupt_pin,LOW);
    writeRegister(i, TMC4361_START_CONFIG_REGISTER, 0
      | _BV(5) //external start is an start (to ensure start is an input)
    );   
  }

  digitalWriteFast(START_SIGNAL_PIN,HIGH);
  pinModeFast(START_SIGNAL_PIN,OUTPUT);
}

void initialzeTMC4361() {
//  //reset the quirrel
//  resetTMC4361(true,false);

  digitalWriteFast(START_SIGNAL_PIN,HIGH);

  //initialize CS pin
  digitalWriteFast(CS_4361_1_PIN,LOW);
  digitalWriteFast(CS_4361_2_PIN,LOW);
  digitalWriteFast(CS_4361_3_PIN,LOW);

  //will be released after setup is complete   
  for (char i=0; i<nr_of_coordinated_motors; i++) {
    digitalWriteFast(motors[i].target_reached_interrupt_pin,LOW);
    writeRegister(i, TMC4361_START_CONFIG_REGISTER, 0
      | _BV(5) //external start is an start (to ensure start is an input)
    );
  }
//  //enable the reset again
//  delay(1);
//  resetTMC4361(false,true);
//  delay(10);

  //preconfigure the TMC4361
  for (char i=0; i<nr_of_coordinated_motors;i++) {
    long g_conf = 0 | _BV(5);
    if (differential_encoder_motors & _BV(i)) {
      g_conf |= _BV(12) | 0x2000000ul; //we don't use direct values and we forward the clock & diff encoder
    }
    writeRegister(i, TMC4361_GENERAL_CONFIG_REGISTER, g_conf); //we don't use direct values
    writeRegister(i, TMC4361_RAMP_MODE_REGISTER,_BV(2) | 1); // trapezoidal positioning
    writeRegister(i, TMC4361_SH_RAMP_MODE_REGISTER,_BV(2) |1); // trapezoidal positioning
//    writeRegister(i, TMC4361_RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps)
//    writeRegister(i, TMC4361_SH_RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps)
    writeRegister(i,TMC4361_CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
    writeRegister(i,TMC4361_START_DELAY_REGISTER, 512); //NEEDED so THAT THE SQUIRREL CAN RECOMPUTE EVERYTHING!
    //TODO shouldn't we add target_reached - just for good measure??
    setStepsPerRevolutionTMC4361(i,motors[i].steps_per_revolution);
    writeRegister(i, TMC4361_START_CONFIG_REGISTER, default_4361_start_config);   
    unsigned long filter = (0x20000ul /*2<<16*/) | (0x400000ul /*4<<20*/); //filter start
    filter |= (2<<8) | (0x4000ul /*4<<20*/); //filter ref
//    Serial.println(filter);
    writeRegister(i,TMC4361_INPUT_FILTER_REGISTER,filter);

    last_target[i]=0;
  }
}

const char setStepsPerRevolutionTMC4361(unsigned char motor_nr, unsigned int steps) {
#ifdef DEBUG_MOTOR_CONTFIG
  Serial.print(F("Settings steps per rev for #"));
  Serial.print(motor_nr);
  Serial.print(F(" to "));
  Serial.println(steps);
#endif
  //configure the motor type
  unsigned long motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps<<4;
  writeRegister(motor_nr,TMC4361_STEP_CONF_REGISTER,motorconfig);
  motors[motor_nr].steps_per_revolution = steps;
  return 0;
}

const char homeMotorTMC4361(unsigned char motor_nr, unsigned long timeout, 
  double homing_fast_speed, double homing_low_speed, long homing_retraction,
  double homming_accel, boolean right_homing_point) 
{
  unsigned char homing_state = 0;
  unsigned long ref_conf;
  unsigned long status;
  long time_start;

  writeRegister(motor_nr, TMC4361_START_CONFIG_REGISTER, 0
    | _BV(10)//immediate start        
    | _BV(5) //external start is an start (to ensure start is an input)
    //since we just start 
  );   

  //comfigire homing movement config - acceleration
  writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER, 0);
  writeRegister(motor_nr, TMC4361_X_ACTUAL_REGISTER, 0); 
  writeRegister(motor_nr, TMC4361_A_MAX_REGISTER,FIXED_22_2_MAKE(homming_accel)); //set maximum acceleration
  writeRegister(motor_nr, TMC4361_D_MAX_REGISTER,FIXED_22_2_MAKE(homming_accel)); //set maximum deceleration
  ref_conf = readRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER);

  time_start = millis();
  while (homing_state != 255) {
    if (((millis() - time_start)&0xFF) == 0) {
      messenger.sendCmd(kWait,homing_state);
    }
    if ((millis() - time_start) > timeout) {
      messenger.sendCmd(kError,-1);
      homing_state = 255;
    }
    switch (homing_state) {
      case 0:  // start movement towards the endstop
        readRegister(motor_nr, TMC4361_EVENTS_REGISTER);
        if (!right_homing_point) {
          // homing to the left
          writeRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER, 0x00000901);
          writeRegister(motor_nr, TMC4361_V_MAX_REGISTER, FIXED_23_8_MAKE(-homing_fast_speed));
        } else {
          // homing to the right
          writeRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER, 0x00002102);
          writeRegister(motor_nr, TMC4361_V_MAX_REGISTER, FIXED_23_8_MAKE(homing_fast_speed));
        }
        writeRegister(motor_nr, TMC4361_RAMP_MODE_REGISTER, 1);
        homing_state = 1;
        messenger.sendCmd(kWait,homing_state);
        break;

      case 1:  // wait until endstop is reached, start retraction
        status = readRegister(motor_nr, TMC4361_STATUS_REGISTER);
        if (status & ((1<<8)|(1<<7))) {
          readRegister(motor_nr, TMC4361_EVENTS_REGISTER);
          writeRegister(motor_nr, TMC4361_V_MAX_REGISTER, 0);
          writeRegister(motor_nr, TMC4361_X_ACTUAL_REGISTER, 0); 
          writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER, 0); 
          writeRegister(motor_nr, TMC4361_RAMP_MODE_REGISTER, 5);
          writeRegister(motor_nr, TMC4361_V_MAX_REGISTER, FIXED_23_8_MAKE(homing_fast_speed));
          if (!right_homing_point) {
            // retract to the right
            writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER, homing_retraction);
          } else {
            // retract to the left
            writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER, -homing_retraction);
          }
          homing_state = 2;
          messenger.sendCmd(kWait,homing_state);
        }
        break;
      
      case 2:  // wait until retracted, abort with error -2 if endstop stays active, start slow movement towards endstop
        status = readRegister(motor_nr, TMC4361_STATUS_REGISTER);
        if (status & (1<<0)) {
          if (!(status & ((1<<8)|(1<<7)))) {
            readRegister(motor_nr, TMC4361_EVENTS_REGISTER);
            if (!right_homing_point) {
              // homing to the left
              writeRegister(motor_nr, TMC4361_V_MAX_REGISTER, FIXED_23_8_MAKE(-homing_low_speed));
            } else {
              // homing to the right
              writeRegister(motor_nr, TMC4361_V_MAX_REGISTER, FIXED_23_8_MAKE(homing_low_speed));
            }
            writeRegister(motor_nr, TMC4361_RAMP_MODE_REGISTER, 1);
            homing_state = 3;
            messenger.sendCmd(kWait,homing_state);
          } else {
            homing_state = 255;
            messenger.sendCmd(kError,-2);
          }
        }
        break;
      
      case 3:  // wait until endstop is reached, this is the home position
        status = readRegister(motor_nr, TMC4361_STATUS_REGISTER);
        if (status & ((1<<8)|(1<<7))) {
          readRegister(motor_nr, TMC4361_EVENTS_REGISTER);
          writeRegister(motor_nr, TMC4361_V_MAX_REGISTER, 0);
          writeRegister(motor_nr, TMC4361_X_ACTUAL_REGISTER, 0); 
          writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER, 0); 
          homing_state = 255;
          messenger.sendCmd(kWait,homing_state);
        }
        break;
      
      default: // end homing
        homing_state = 255;
    }
  }

  //reset everything
  writeRegister(motor_nr, TMC4361_V_MAX_REGISTER, 0);
  writeRegister(motor_nr, TMC4361_RAMP_MODE_REGISTER, 5);
  writeRegister(motor_nr, TMC4361_A_MAX_REGISTER,0); 
  writeRegister(motor_nr, TMC4361_D_MAX_REGISTER,0); 
  writeRegister(motor_nr, TMC4361_START_CONFIG_REGISTER, default_4361_start_config);   
  writeRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER, ref_conf);

  return 0;
}

inline long getMotorPositionTMC4361(unsigned char motor_nr) {
  return readRegister(motor_nr, TMC4361_X_TARGET_REGISTER);
  //TODO do we have to take into account that the motor may never have reached the x_target??
  //vactual!=0 -> x_target, x_pos sonst or similar
}

void moveMotorTMC4361(unsigned char motor_nr, long target_pos, double vMax, double aMax, long jerk, boolean isWaypoint) {
  long aim_target;
  //calculate the value for x_target so taht we go over pos_comp
  long last_pos = last_target[motor_nr]; //this was our last position
  next_direction[motor_nr]=(target_pos)>last_pos? 1:-1;  //and for failsafe movement we need to write down the direction
  if (isWaypoint) {
    aim_target = target_pos+(target_pos-last_pos); // 2*(target_pos-last_pos)+last_pos;
  } 
  else {
    aim_target=target_pos;
  }

#ifdef DEBUG_MOTION  
  Serial.print(F("Preparing motor "));
  Serial.print(motor_nr,DEC);
  if (isWaypoint) {
    Serial.print(" via");
  } 
  else {
    Serial.print(" to");
  }
  Serial.print(F(": pos="));
  Serial.print(target_pos);
  Serial.print(F(" ["));
  Serial.print(last_target[motor_nr]);
  Serial.print(F(" -> "));
  Serial.print(target_pos);
  Serial.print(F(" -> "));
  Serial.print(aim_target);
  Serial.print(F("], vMax="));
  Serial.print(vMax);
  Serial.print(F(", aMax="));
  Serial.print(aMax);
  Serial.print(F(": jerk="));
  Serial.print(jerk);
  Serial.println();
#endif    
#ifdef DEBUG_MOTION_SHORT
  Serial.print('M');
  Serial.print(motor_nr,DEC);
  if (isWaypoint) {
    Serial.print(F(" v "));
    Serial.print(target_pos);
  }  
  Serial.print(F(" t "));
  Serial.print(aim_target);
  Serial.print(F(" @ "));
  Serial.println(vMax);
#endif

  long fixed_a_max = FIXED_22_2_MAKE(aMax);

  writeRegister(motor_nr, TMC4361_SH_V_MAX_REGISTER,FIXED_23_8_MAKE(vMax)); //set the velocity 
  writeRegister(motor_nr, TMC4361_SH_A_MAX_REGISTER,fixed_a_max); //set maximum acceleration
  writeRegister(motor_nr, TMC4361_SH_D_MAX_REGISTER,fixed_a_max); //set maximum deceleration
  writeRegister(motor_nr, TMC4361_SH_BOW_1_REGISTER,jerk);
  writeRegister(motor_nr, TMC4361_SH_BOW_2_REGISTER,jerk);
  writeRegister(motor_nr, TMC4361_SH_BOW_3_REGISTER,jerk);
  writeRegister(motor_nr, TMC4361_SH_BOW_4_REGISTER,jerk);
  //pos comp is not shaddowed
  next_pos_comp[motor_nr] = target_pos;
  writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER,aim_target);

  last_target[motor_nr]=target_pos;
  //Might be usefull, but takes a lot of space
#ifdef DEBUG_MOTION_REGISTERS
  Serial.println('S');
  Serial.println(motor_nr,DEC);
  Serial.println((long)readRegister(motor_nr,TMC4361_X_TARGET_REGISTER));
  Serial.println(readRegister(motor_nr,TMC4361_SH_V_MAX_REGISTER));
  Serial.println(readRegister(motor_nr,TMC4361_SH_A_MAX_REGISTER));
  Serial.println(readRegister(motor_nr,TMC4361_SH_D_MAX_REGISTER));
  Serial.println(readRegister(motor_nr,TMC4361_SH_BOW_1_REGISTER));
  Serial.println(readRegister(motor_nr,TMC4361_SH_BOW_2_REGISTER));
  Serial.println(readRegister(motor_nr,TMC4361_SH_BOW_3_REGISTER));
  Serial.println(readRegister(motor_nr,TMC4361_SH_BOW_4_REGISTER));
  Serial.println();
#endif
}

inline void signal_start() {
#ifdef RX_TX_BLINKY
  RXLED1;
  TXLED1;
#endif
  //prepare the pos compr registers
  for (char i=0; i< nr_of_coordinated_motors; i++) {
    //clear the event register
    readRegister(i, TMC4361_EVENTS_REGISTER);
    pos_comp[i] = next_pos_comp[i];
    //write the new pos_comp
    writeRegister(i, TMC4361_POSITION_COMPARE_REGISTER,next_pos_comp[i]);
    //and mark it written 
    next_pos_comp[i] = 0;
    direction[i] = next_direction[i];
  }

    readRegister(2,TMC4361_START_CONFIG_REGISTER);

  //start the motors, please
  digitalWriteFast(START_SIGNAL_PIN,LOW);
  delayMicroseconds(3); //the call by itself may have been enough
  digitalWriteFast(START_SIGNAL_PIN,HIGH);

    readRegister(2,TMC4361_X_ACTUAL_REGISTER);
    readRegister(2,TMC4361_X_TARGET_REGISTER);
    readRegister(2,TMC4361_POSITION_COMPARE_REGISTER);
    readRegister(2,TMC4361_V_MAX_REGISTER);
    readRegister(2,TMC4361_A_MAX_REGISTER);
    readRegister(2,TMC4361_D_MAX_REGISTER);

#ifdef DEBUG_MOTION_REGISTERS
  //and deliver some additional logging
  for (char i=0; i< nr_of_coordinated_motors; i++) {
    Serial.println('R');
    Serial.println(i,DEC);
    Serial.println((long)readRegister(i,TMC4361_X_ACTUAL_REGISTER));
    Serial.println((long)readRegister(i,TMC4361_X_TARGET_REGISTER));
    Serial.println((long)readRegister(i,TMC4361_POSITION_COMPARE_REGISTER));
    Serial.println(readRegister(i,TMC4361_V_MAX_REGISTER));
    Serial.println(readRegister(i,TMC4361_A_MAX_REGISTER));
    Serial.println(readRegister(i,TMC4361_D_MAX_REGISTER));
    Serial.println(readRegister(i,TMC4361_BOW_1_REGISTER));
    Serial.println(readRegister(i,TMC4361_BOW_2_REGISTER));
    Serial.println(readRegister(i,TMC4361_BOW_3_REGISTER));
    Serial.println(readRegister(i,TMC4361_BOW_4_REGISTER));
    Serial.println();
  }
#endif
#ifdef DEBUG_MOTION_TRACE
  Serial.println(F("Sent start signal"));
#endif
#ifdef DEBUG_MOTION_START
  Serial.println('S');
#endif
#ifdef DEBUG_MOTION_REGISTERS
  Serial.println();
#endif
#ifdef RX_TX_BLINKY
  RXLED0;
  TXLED0;
#endif
}

//this method is needed to manually verify that the motors are to running beyond their targe
void checkTMC4361Motion() {
  if (target_motor_status & (_BV(nr_of_coordinated_motors)-1)) {
    //now check for every motor if iis alreaday ove the target..
    for (unsigned char i=0; i< nr_of_coordinated_motors; i++) {
      //and deliver some additional logging
      if (target_motor_status & _BV(i) & ~motor_status) {
#ifdef RX_TX_BLINKY
        RXLED1;
#endif
        unsigned long motor_pos = readRegister(i, TMC4361_X_ACTUAL_REGISTER);
        if ((direction[i]==1 && motor_pos>=pos_comp[i])
          || (direction[i]==-1 && motor_pos<=pos_comp[i])) {
#ifdef DEBUG_MOTION_TRACE_SHORT
          Serial.print('*');
#endif
          motor_target_reached(i);
        }
#ifdef RX_TX_BLINKY
        RXLED0;
#endif
      }
    }
  }
}  

void setMotorPositionTMC4361(unsigned char motor_nr, long position) {
#ifdef DEBUG_MOTION_SHORT
  Serial.print('M');
  Serial.print(motor_nr);
  Serial.println(F(":=0"));
#endif
  unsigned long oldStartRegister = readRegister(motor_nr, TMC4361_START_CONFIG_REGISTER);
  //we want an immediate start
  writeRegister(motor_nr, TMC4361_START_CONFIG_REGISTER, _BV(5));   // keep START as an input

  //we write x_actual, x_target and pos_comp to the same value to be safe 
  writeRegister(motor_nr, TMC4361_V_MAX_REGISTER,0);
  writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER,position);
  writeRegister(motor_nr, TMC4361_X_ACTUAL_REGISTER,position);
  writeRegister(motor_nr, TMC4361_POSITION_COMPARE_REGISTER,position);
  last_target[motor_nr]=position;

  //back to normal
  writeRegister(motor_nr, TMC4361_START_CONFIG_REGISTER, oldStartRegister);  
}

const char configureEndstopTMC4361(unsigned char motor_nr, boolean left, boolean active_high) {
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Enstop config before "));
  Serial.println( readRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER),HEX);
#endif
  unsigned long endstop_config = getClearedEndStopConfigTMC4361(motor_nr, left);
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Cleared enstop config before "));
  Serial.println(endstop_config, HEX);
#endif
  if (left) {
    if (active_high) {
#ifdef DEBUG_ENDSTOPS
      Serial.print(F("TMC4361 motor "));
      Serial.print(motor_nr);
      Serial.println(F(" - configuring left end stop as active high"));
#endif
      endstop_config |= 0 
        | _BV(0) //STOP_LEFT enable
        | _BV(2) //positive Stop Left stops motor
          | _BV(11) //X_LATCH if stopl becomes active ..
            ;
    } 
    else {
#ifdef DEBUG_ENDSTOPS
      Serial.print(F("TMC4361 motor "));
      Serial.print(motor_nr);
      Serial.println(F(" - configuring left end stop as active low"));
#endif
      endstop_config |= 0 
        | _BV(0) //STOP_LEFT enable
        | _BV(10) //X_LATCH if stopl becomes inactive ..
          ;
    }
  } 
  else {
    if (active_high) {
#ifdef DEBUG_ENDSTOPS
      Serial.print(F("TMC4361 motor "));
      Serial.print(motor_nr);
      Serial.println(F(" - cConfiguring right end stop as active high"));
#endif
      endstop_config |= 0 
        | _BV(1) //STOP_RIGHT enable
        | _BV(3) //positive Stop right stops motor
          | _BV(13) //X_LATCH if storl becomes active ..
            ;
    } 
    else {
#ifdef DEBUG_ENDSTOPS
      Serial.print(F("TMC4361 motor "));
      Serial.print(motor_nr);
      Serial.println(F(" - configuring right end stop as active low"));
#endif
      endstop_config |= 0 
        | _BV(1) //STOP_LEFT enable
        | _BV(12) //X_LATCH if stopr becomes inactive ..
          ;
    }
  }
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("New endstop config "));
  Serial.println(endstop_config, HEX);
#endif
  writeRegister(motor_nr,TMC4361_REFERENCE_CONFIG_REGISTER, endstop_config);
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Written enstop config "));
  Serial.println( readRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER),HEX);
#endif
  return 0;
}

const char configureVirtualEndstopTMC4361(unsigned char motor_nr, boolean left, long positions) {
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Enstop config before "));
  Serial.println( readRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER),HEX);
#endif
  unsigned long endstop_config = getClearedEndStopConfigTMC4361(motor_nr, left);
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Cleared enstop config before "));
  Serial.println(endstop_config);
#endif
  unsigned long position_register;
  if (left) {
#ifdef DEBUG_ENDSTOPS
    Serial.print(F("TMC4361 motor "));
    Serial.print(motor_nr);
    Serial.print(F(" - configuring left virtual endstop at "));
    Serial.println(positions);
#endif
    endstop_config |= _BV(6);
    position_register = TMC4361_VIRTUAL_STOP_LEFT_REGISTER;
    //we doe not latch since we know where they are??
  } 
  else {
#ifdef DEBUG_ENDSTOPS
    Serial.print(F("TMC4361 motor "));
    Serial.print(motor_nr);
    Serial.print(F(" - configuring right virtual endstop at"));
    Serial.println(positions);
#endif
    endstop_config |= _BV(7);
#ifdef DEBUG_ENDSTOPS_DETAIL
    Serial.print(F("Cleared enstop config before "));
    Serial.println(endstop_config);
#endif

    position_register = TMC4361_VIRTUAL_STOP_RIGHT_REGISTER;
    //we doe not latch since we know where they are??
  }
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("New enstop config "));
  Serial.println(endstop_config);
#endif
  writeRegister(motor_nr,TMC4361_REFERENCE_CONFIG_REGISTER, endstop_config);
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Written enstop config "));
  Serial.println( readRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER),HEX);
#endif
  writeRegister(motor_nr,position_register, positions);
  return NULL; //TODO this has to be implemented ...
}

inline unsigned long getClearedEndStopConfigTMC4361(unsigned char motor_nr, boolean left) {
  unsigned long endstop_config = readRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER);
  //clear everything
  unsigned long clearing_pattern; // - a trick to ensure the use of all 32 bits
  if (left) {
    clearing_pattern = TMC4361_LEFT_ENDSTOP_REGISTER_PATTERN;
  } 
  else {
    clearing_pattern = TMC4361_RIGHT_ENDSTOP_REGISTER_PATTERN;
  }
  clearing_pattern = ~clearing_pattern;
  endstop_config &= clearing_pattern;
  return endstop_config;
}  

void configureEncoderTMC4361(unsigned char motor_nr, unsigned int steps_per_revolution, unsigned int microsteps, unsigned int encoder_increments_per_revolution, boolean encoder_inverted, boolean encoder_differential) {
  //configure the motor type
  unsigned long motorconfig;
  switch (microsteps) {
    case 1:
      motorconfig = 8;
      break;
    case 2:
      motorconfig = 7;
      break;
    case 4:
      motorconfig = 6;
      break;
    case 8:
      motorconfig = 5;
      break;
    case 16:
      motorconfig = 4;
      break;
    case 32:
      motorconfig = 3;
      break;
    case 64:
      motorconfig = 2;
      break;
    case 128:
      motorconfig = 1;
      break;
    default:
      motorconfig = 0;
      break;
  }
  motorconfig |= (steps_per_revolution&0xFFF)<<4;
  writeRegister(motor_nr,TMC4361_STEP_CONF_REGISTER,motorconfig);
  //in the background the tmc4361 can recalulate everything ...
  motors[motor_nr].steps_per_revolution = steps_per_revolution;
  motors[motor_nr].microsteps = microsteps;
  writeRegister(motor_nr, TMC4361_ENCODER_INPUT_RESOLUTION_REGISTER, encoder_increments_per_revolution); // write encoder resolution into register
  //update the encode differentiality in general config
  unsigned long general_config = readRegister(motor_nr, TMC4361_GENERAL_CONFIG_REGISTER);
  //delete the dfferential encoder setting
  general_config &= ~0x1c00ul;
  //TODO inthery we can set different encoder types here - but we only know the abn types
  if (encoder_differential) {
    differential_encoder_motors |= _BV(motor_nr);
  } 
  else {
    general_config |= (1ul << 12ul);
    differential_encoder_motors &= ~_BV(motor_nr);
  }
  writeRegister(motor_nr, TMC4361_GENERAL_CONFIG_REGISTER, general_config);
  //set the encoder inversion in the encoder input config
  if (steps_per_revolution==0 || microsteps==0 || encoder_increments_per_revolution==0) {
    writeRegister(motor_nr, TMC4361_ENCODER_INPUT_CONFIG_REGISTER, 0); //no encoder
  } 
  else {
    unsigned long encoder_input_config = 0;
    //TODO this disables the closed loop effectively - should be caled befor enabling CL - which makes sense anyway
    if (encoder_inverted) {
      encoder_input_config |= (1ul << 29ul);
    }
    writeRegister(motor_nr, TMC4361_ENCODER_INPUT_CONFIG_REGISTER, encoder_input_config);
  }
}

inline void resetTMC4361(boolean shutdown, boolean bringup) {
  if (shutdown) {
    // Use HWBE pin to reset motion controller TMC4361
    PORTE &= ~(_BV(2));          //to check (reset for motion controller)
  }
  if (shutdown && bringup) {
    delay(1);
  }
  if (bringup) {
    PORTE |= _BV(2);
  }
}









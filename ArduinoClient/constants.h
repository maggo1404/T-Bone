//hard coded value
#define MAX_MOTOR_CURRENT 1500

//register
#define TMC4361_GENERAL_CONFIG_REGISTER 0x0
#define TMC4361_REFERENCE_CONFIG_REGISTER 0x01
#define TMC4361_START_CONFIG_REGISTER 0x2
#define TMC4361_INPUT_FILTER_REGISTER 0x3
#define TMC4361_SPIOUT_CONF_REGISTER 0x04
#define TMC4361_STEP_CONF_REGISTER 0x0A
#define TMC4361_EVENT_CLEAR_CONF_REGISTER 0x0c
#define TMC4361_INTERRUPT_CONFIG_REGISTER 0x0d
#define TMC4361_EVENTS_REGISTER 0x0e
#define TMC4361_STATUS_REGISTER 0x0f
#define TMC4361_START_OUT_ADD_REGISTER 0x11
#define TMC4361_GEAR_RATIO_REGISTER 0x12
#define TMC4361_START_DELAY_REGISTER 0x13
#define TMC4361_RAMP_MODE_REGISTER 0x20
#define TMC4361_X_ACTUAL_REGISTER 0x21 
#define TMC4361_V_ACTUAL_REGISTER 0x22
#define TMC4361_V_MAX_REGISTER 0x24
#define TMC4361_V_START_REGISTER 0x25
#define TMC4361_V_STOP_REGISTER 0x26
#define TMC4361_A_MAX_REGISTER 0x28
#define TMC4361_D_MAX_REGISTER 0x29
#define TMC4361_BOW_1_REGISTER 0x2d
#define TMC4361_BOW_2_REGISTER 0x2e
#define TMC4361_BOW_3_REGISTER 0x2f
#define TMC4361_BOW_4_REGISTER 0x30
#define TMC4361_CLK_FREQ_REGISTER 0x31
#define TMC4361_POS_COMP_REGISTER 0x32
#define TMC4361_VIRTUAL_STOP_LEFT_REGISTER 0x33
#define TMC4361_VIRTUAL_STOP_RIGHT_REGISTER 0x34
#define TMC4361_X_LATCH_REGISTER 0x36
#define TMC4361_X_TARGET_REGISTER 0x37
#define TMC4361_X_TARGET_PIPE_0_REGSISTER 0x38
#define TMC4361_SH_RAMP_MODE_REGISTER 0x40
#define TMC4361_SH_V_MAX_REGISTER 0x41
#define TMC4361_SH_V_START_REGISTER 0x42
#define TMC4361_SH_V_STOP_REGISTER 0x43
#define TMC4361_SH_VBREAK_REGISTER 0x44
#define TMC4361_SH_A_MAX_REGISTER 0x45
#define TMC4361_SH_D_MAX_REGISTER 0x46
#define TMC4361_SH_BOW_1_REGISTER 0x49
#define TMC4361_SH_BOW_2_REGISTER 0x4a
#define TMC4361_SH_BOW_3_REGISTER 0x4b
#define TMC4361_SH_BOW_4_REGISTER 0x4c
#define TMC4361_COVER_LOW_REGISTER 0x6c
#define TMC4361_COVER_HIGH_REGISTER 0x6d
#define TMC4361_START_OUT_ADD_REGISTER 0x11
//some nice calculation s
//simple FP math see https://ucexperiment.wordpress.com/2012/10/28/fixed-point-math-on-the-arduino-platform/
#define FIXED_23_8_MAKE(a)     (int32_t)((a*(1ul << 8ul)))
#define FIXED_22_2_MAKE(a)     (int32_t)((a*(1ul << 2ul)))

//how to mask REFERENCE_CONFIG_REGISTER if you want to configure just one end 
#define LEFT_ENDSTOP_REGISTER_PATTERN (_BV(0) | _BV(2) | _BV(6) | _BV(10) | _BV(11) | _BV(14))
#define RIGHT_ENDSTOP_REGISTER_PATTERN (_BV(1) | _BV(3) | _BV(7) | _BV(12) | _BV(13) | _BV(15))
#define X_TARGET_IN_DIRECTION(m,t) ((inversed_motors | _BV(m))? -t:t)

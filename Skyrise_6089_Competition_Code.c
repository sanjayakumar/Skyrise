
#pragma config(UART_Usage, UART1, uartVEXLCD, baudRate19200, IOPins, None, None)
#pragma config(UART_Usage, UART2, uartNotUsed, baudRate4800, IOPins, None, None)
#pragma config(I2C_Usage, I2C1, i2cSensors)
#pragma config(Sensor, dgtl7,  fourBarEncoder, sensorQuadEncoder)
#pragma config(Sensor, dgtl9,  leftEncoder,    sensorQuadEncoder)
#pragma config(Sensor, dgtl11, rightEncoder,   sensorQuadEncoder)
#pragma config(Sensor, I2C_1,  ,               sensorQuadEncoderOnI2CPort,    , AutoAssign)
#pragma config(Sensor, I2C_2,  ,               sensorQuadEncoderOnI2CPort,    , AutoAssign)
#pragma config(Motor,  port2,           leftSlideMotor, tmotorVex393_MC29, openLoop)
#pragma config(Motor,  port3,           backRight,     tmotorVex393_MC29, openLoop, reversed, driveRight, encoderPort, I2C_1)
#pragma config(Motor,  port4,           frontLeft,     tmotorVex393_MC29, openLoop, driveLeft)
#pragma config(Motor,  port6,           frontRight,    tmotorVex393_MC29, openLoop, reversed, driveRight)
#pragma config(Motor,  port7,           backLeft,      tmotorVex393_MC29, openLoop, driveLeft, encoderPort, I2C_2)
#pragma config(Motor,  port8,           rightSlideMotor, tmotorVex393_MC29, openLoop, reversed)
#pragma config(Motor,  port9,           fourBarMotor,  tmotorVex393_MC29, openLoop, reversed)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//

#pragma platform(VEX)

//Competition Control and Duration Settings
#pragma competitionControl(Competition)
#pragma autonomousDuration(20)
#pragma userControlDuration(120)

#include "Vex_Competition_Includes.c"   //Main competition background code...do not modify!

// Include the lcd button get utility function
#include "getlcdbuttons.c"

// DEBUG defines. Enable ONLY for testing
//#define DEBUG_PID
//#define DEBUG_IME



// Parameter DEFINEs
#define SLIDE_MAX_HEIGHT		  		1130
#define FOUR_BAR_MAX_HEIGHT 			700
#define LEFT_SLIDE_SENSOR_INDEX		leftEncoder
#define RIGHT_SLIDE_SENSOR_INDEX	rightEncoder
#define SLIDE_SENSOR_SCALE    		1
#define FOUR_BAR_SENSOR_SCALE 		-1
#define LEFT_SLIDE_MOTOR_INDEX		leftSlideMotor
#define RIGHT_SLIDE_MOTOR_INDEX		rightSlideMotor
#define FOUR_BAR_SENSOR_INDEX 		fourBarEncoder
#define FOUR_BAR_MOTOR_INDEX  		fourBarMotor
#define SLIDE_MOTOR_SCALE     		1
#define FOUR_BAR_MOTOR_SCALE 			-1
#define SLIDE_MOTOR_DRIVE_MAX			127.0
#define SLIDE_MOTOR_DRIVE_MIN			(-75.0)
#define FOUR_BAR_MOTOR_DRIVE_MAX	127.0
#define FOUR_BAR_MOTOR_DRIVE_MIN 	(-80.0)
#define MAX_SLIDE_MOTOR_POWER_DELTA			30
#define MAX_FOUR_BAR_MOTOR_POWER_DELTA	30
// Absolute value of joystick below this number is considered neutral
#define JOYSTICK_MAX_NEUTRAL 			15
#define SLIDE_INTEGRAL_LIMIT  		800
#define FOUR_BAR_INTEGRAL_LIMIT 	0
#define NUM_PID_CONTROLS 					3
#define LEFT_LIFT_PID_INDEX 			0
#define RIGHT_LIFT_PID_INDEX 			1
#define FOUR_BAR_PID_INDEX 				2

// This is the preset height for picking up the skyrise section at the highest point (to deliver the first one)
#define HEIGHT_FOR_FIRST_SKYRISE_SECTION 229

#define USER_CONTROL_LOOP_TIME	50 // Milliseconds
#define PID_LOOP_TIME						25

// Structure to store PID parameters -- note we have 3; one for each slide and one for the Arm
typedef struct {
	float pidRequestedValue;

	float Kp;
	float Ki;
	float Kd;

	int pid_sensor_index;
	float pid_sensor_scale;
	float pid_sensor_previous_value;
	float previous_speed;

	int pid_motor_index;
	float pid_motor_scale;

	float previous_motor_power;
	float previous_error;
	float errorIntegral;

	float max_motor_power;
	float min_motor_power;
	float pid_integral_limit;

	int max_height;
	int min_height;

	bool pid_active;

	float max_motor_power_delta;

} pidTaskParameters;

pidTaskParameters pid[ NUM_PID_CONTROLS ];

// These could be constants but leaving
// as variables allows them to be modified in the debugger "live"
float  slide_Kp = 2.2;
float  slide_Ki = 0.0;
float  slide_Kd = 1;

float  fourBar_Kp = 1.5;
float  fourBar_Ki = 0.00;
float  fourBar_Kd = 1;

#ifdef DEBUG_PID
short start_debug_stream = false;
int debug_delay_counter = 0;
#endif // DEBUG PID

#ifdef DEBUG_IME
float max_mismatch = 0.0;
#endif

/*-----------------------------------------------------------------------------*/
/*                                                                             */
/*  pid control task                                                           */
/*                                                                             */
/*-----------------------------------------------------------------------------*/


task PidController()
{
	float  pidSensorCurrentValue;

	float  pidError;
	float  pidDerivative;
	float  pidDrive;


	int i, j;

	for (i = 0; i < NUM_PID_CONTROLS; i++) {
		pid[i].previous_error  = 0;
		pid[i].errorIntegral   = 0;
		pid[i].previous_motor_power = 0;
		pid[i].pid_sensor_previous_value = 0;
		pid[i].previous_speed = 0;
		pid[i].pid_sensor_previous_value = 0;
		pid[i].pid_active = true;
	}

	j = 0;

	while( true ) {

		for (i = 0; i < NUM_PID_CONTROLS; i++) {

			// If the current pid is not active, skip the loop
			if (!pid[i].pid_active)
				continue;

			// Read the sensor value and scale
			pidSensorCurrentValue = SensorValue[ pid[i].pid_sensor_index] * pid[i].pid_sensor_scale;

			// calculate error
			pidError = pid[i].pidRequestedValue - pidSensorCurrentValue;

			// integral - if Ki is not 0
			if( pid[i].Ki != 0 )
			{
				pid[i].errorIntegral =  pid[i].errorIntegral + pidError;
				// If we are inside controlable window then integrate the error
				if( abs(pid[i].errorIntegral) > pid[i].pid_integral_limit )
					pid[i].errorIntegral =  sgn(pid[i].errorIntegral)*pid[i].pid_integral_limit;
				//else
				//	pid[i].errorIntegral = 0;
			}
			else
				pid[i].errorIntegral = 0;

			// calculate the derivative
			pidDerivative = pidError - pid[i].previous_error;
			pid[i].previous_error  = pidError;

			// calculate drive
			pidDrive = (pid[i].Kp * pidError) + (pid[i].Ki * pid[i].errorIntegral) + (pid[i].Kd * pidDerivative);

			// limit PWM to user supplied range (usually -127 to +127)
			if( pidDrive >  pid[i].max_motor_power )
				pidDrive = pid[i].max_motor_power;
			if( pidDrive < pid[i].min_motor_power )
				pidDrive = pid[i].min_motor_power;


			// Limit Rate of change per iteration of 40 Hz loop
			if (abs(pidDrive - pid[i].previous_motor_power) > pid[i].max_motor_power_delta) {
				if (pidDrive > pid[i].previous_motor_power)
					pidDrive = pid[i].previous_motor_power + pid[i].max_motor_power_delta;
				else
					pidDrive = pid[i].previous_motor_power - pid[i].max_motor_power_delta;
			}
			pid[i].previous_motor_power = pidDrive;

			// send to motor
			motor[ pid[i].pid_motor_index ] = pidDrive * pid[i].pid_motor_scale;

			// DEBUG START
			// calculate (smoothed) speed
			// speed = (pidSensorCurrentValue - pid[i].pid_sensor_previous_value)/loop_time;
			// speed = (speed + pid[i].previous_speed)/2.0;
			// pid[i].previous_speed = speed;
			// pid[i].pid_sensor_previous_value = pidSensorCurrentValue;
			// if (i == 0 /*&& abs(speed) > .1 && (pidSensorCurrentValue > 1050 || pidSensorCurrentValue < 10)*/)
			// writeDebugStreamLine("Speed: %f time: %f sensor %d", pid[0].previous_speed*1000.0/6.0, time1[T1], pidSensorCurrentValue);

#ifdef DEBUG_PID
			pid[i].pid_sensor_previous_value = pidSensorCurrentValue;
			if (i==0 && start_debug_stream) {
				float sensor_mismatch = pid[0].pid_sensor_previous_value - pid[1].pid_sensor_previous_value;
				if (abs(sensor_mismatch) > abs(max_mismatch))
					max_mismatch = sensor_mismatch;
				// writeDebugStreamLine("pidDrive: %f pidError: %f max_mismatch: %f PID: %f %f %f", pidDrive, pidError, max_mismatch, pid[i].Kp * pidError, pid[i].Ki * pid[i].errorIntegral, pid[i].Kd * pidDerivative);
				writeDebugStreamLine("pidDrive: %f pidError: %f max_mismatch: %f sensor_mismatch %f ", pidDrive, pidError, max_mismatch, sensor_mismatch);
			}
#endif // DEBUG_PID

		}

		// Run at 40Hz
		wait1Msec( PID_LOOP_TIME );
	}
}

///////////
// NOTE: the motor power in init_lift() and zero_reset_slide() and zero_reset_arm() MUST be the same.
///////////

task zero_reset_slide()
{
	// PIDs for both slides must already be inactive for both slides

	// Turn Motors Off
	motor[pid[LEFT_LIFT_PID_INDEX].pid_motor_index] = 0;
	motor[pid[RIGHT_LIFT_PID_INDEX].pid_motor_index] = 0;

	// Wait 3 Seconds
	wait1Msec(3000);

	// Start to move down the slide at LOW power
	motor[pid[LEFT_LIFT_PID_INDEX].pid_motor_index] = -60 * pid[LEFT_LIFT_PID_INDEX].pid_motor_scale;
	motor[pid[RIGHT_LIFT_PID_INDEX].pid_motor_index] = -60 * pid[RIGHT_LIFT_PID_INDEX].pid_motor_scale;

	// Wait while buttons are pressed
	while ((vexRT[Btn8DXmtr2] == 1))
	{
		wait1Msec(50);
	}

	// Turn Motors Off
	motor[pid[LEFT_LIFT_PID_INDEX].pid_motor_index] = 0;
	motor[pid[RIGHT_LIFT_PID_INDEX].pid_motor_index] = 0;

	wait1Msec(200); // Should be the SAME as init_lift()

	// Set zero positions for each pid that is not active
	SensorValue[pid[LEFT_LIFT_PID_INDEX].pid_sensor_index] = 0;
	SensorValue[pid[RIGHT_LIFT_PID_INDEX].pid_sensor_index] = 0;

	// Set pids back to active
	pid[LEFT_LIFT_PID_INDEX].pid_active = true;
	pid[RIGHT_LIFT_PID_INDEX].pid_active = true;
}

///////////
// NOTE: the motor power in init_lift() and zero_reset_slide() and zero_reset_arm() MUST be the same.
///////////

task zero_reset_arm()
{
	// PIDs for the arm must already be inactive
	// Turn Motor Off
	motor[pid[FOUR_BAR_PID_INDEX].pid_motor_index] = 0;

	// Wait 3 Seconds
	wait1Msec(3000);

	// Start to move down the arm at LOW power
	motor[pid[FOUR_BAR_PID_INDEX].pid_motor_index] = -25 * pid[FOUR_BAR_PID_INDEX].pid_motor_scale;

	// Wait while buttons are pressed
	while ((vexRT[Btn7DXmtr2] == 1))
	{
		wait1Msec(50);
	}

	// Turn Motors Off
	motor[pid[FOUR_BAR_PID_INDEX].pid_motor_index] = 0;

	wait1Msec(200); // Should be the SAME as init_lift()

	// Set zero positions for pid
	SensorValue[pid[FOUR_BAR_PID_INDEX].pid_sensor_index] = 0;

	// Set pid back to active
	pid[FOUR_BAR_PID_INDEX].pid_active = true;
}

///////////
// NOTE: the motor power in init_lift() and zero_reset_slide() and zero_reset_arm() MUST be the same.
///////////
void init_lift(){

	int i;

	// Move arm down
	motor[pid[LEFT_LIFT_PID_INDEX].pid_motor_index] = -64*pid[LEFT_LIFT_PID_INDEX].pid_motor_scale;
	motor[pid[RIGHT_LIFT_PID_INDEX].pid_motor_index] = -64*pid[RIGHT_LIFT_PID_INDEX].pid_motor_scale;
	wait1Msec(250);
	motor[pid[FOUR_BAR_PID_INDEX].pid_motor_index] = -25*pid[FOUR_BAR_PID_INDEX].pid_motor_scale;

	wait1Msec(200);
	motor[pid[LEFT_LIFT_PID_INDEX].pid_motor_index] = 0;
	motor[pid[RIGHT_LIFT_PID_INDEX].pid_motor_index] = 0;
	motor[pid[FOUR_BAR_PID_INDEX].pid_motor_index] = 0;


	// Wait after lift moves down to give it time to settle down
	wait1Msec(200);

	for(i = 0; i < NUM_PID_CONTROLS; i++) {
		pid[i].pidRequestedValue = 0;
		SensorValue[pid[i].pid_sensor_index] = 0;
	}
}



// ***                               *** //
// *** Start of Autonomous Functions *** //
// ***                               *** //

#define AUTO_MAX_DELTA 10
#define AUTO_LOOP_TIME 15

int power_ramp(int current, int target) {
	if (target > current) {
		current += AUTO_MAX_DELTA;
		if (current > target)
			return target;
		else
			return current;
		} else {
		current -= AUTO_MAX_DELTA;
		if (current < target)
			return target;
		else
			return current;
	}
}



long ime1, ime2;
long initial_ime1, initial_ime2;

#ifdef DEBUG_IME

#define IME_HISTORY_LENGTH 10
int ime1_index = 0;
int ime2_index = 0;
int ime1_history[IME_HISTORY_LENGTH];
int ime2_history[IME_HISTORY_LENGTH];

#define MAX_DELTA 200

void check_ime() {
	int i;

	ime1_history[ime1_index++] = ime1;
	ime2_history[ime2_index++] = ime2;

	ime1_index = (ime1_index != IME_HISTORY_LENGTH) ? ime1_index : 0;
	ime2_index = (ime2_index != IME_HISTORY_LENGTH) ? ime2_index : 0;

	if (abs(ime1-ime1_history[ime1_index]) < MAX_DELTA) {
		return;
	}

	if (abs(ime2-ime2_history[ime2_index]) < MAX_DELTA) {
		return;
	}

	writeDebugStreamLine("ime1_index: %d ime2_index %d", ime1_index, ime2_index);
	for (i=0; i<IME_HISTORY_LENGTH; i++) {
		writeDebugStreamLine("%d", ime1_history[i]);
	}

	writeDebugStreamLine(" ");
	for (i=0; i<IME_HISTORY_LENGTH; i++) {
		writeDebugStreamLine("%d", ime2_history[i]);
	}

}
#endif DEBUG_IME


void move(char dir, int dist, int power)
{
	int X1, Y1, X2;


	// Initial value of motor power

	X1 = 0;
	Y1 = 0;
	X2 = 0;

	// Get IME values before the start of the movement
	ime1 = initial_ime1 = nMotorEncoder(backRight);
	ime2 = initial_ime2 = nMotorEncoder(backLeft);


#ifdef DEBUG_IME
	check_ime();
#endif // DEBUG_IME

	// Set target values of IME's
float temp;
	switch(dir){
	case 'f':
		Y1 = power;
		break;
	case 'b':
		Y1 = -power;
		break;
	case 'r':
		X1 = power;
		break;
	case 'l':
		X1 = -power;
		break;
	case 'c':
		X2 = power;
		break;
	case 'a':
		X2 = -power;
		break;
	case 'z': // diagonal back left
		X1 = -power;
		temp = -power*100/127;
		Y1 = temp;
		writeDebugStreamLine("X1: %d", X1);
		writeDebugStreamLine("X1: %d", Y1);
		break;
	case 'q': // diagonal back right
		X1 = power;
		temp = -power*100/127;
		Y1 = temp;
	}

#define FR 0
#define BR 1
#define FL 2
#define BL 3
	int target_power[4];
	int current_power[4];
	int motor_index[4];
	bool target_power_reached[4];
	int i;


	target_power[FR] = Y1 - X2 - X1;
	target_power[BR] = Y1 - X2 + X1;
	target_power[FL] = Y1 + X2 + X1;
	target_power[BL] = Y1 + X2 - X1;


	motor_index[FR] = frontRight;
	motor_index[BR] = backRight;
	motor_index[FL] = frontLeft;
	motor_index[BL] = backLeft;

	for(i=0; i < 4; i++) {
		current_power[i] = 0;
		target_power_reached[i] = false;
	}

	while(!target_power_reached[FR] ||
		!target_power_reached[BR] ||
	!target_power_reached[FL] ||
	!target_power_reached[BL] ) {
		for (i=0; i < 4; i++) {
			current_power[i] = power_ramp(current_power[i], target_power[i]);
			motor[motor_index[i]] = current_power[i];
			if (abs(current_power[i]) >= abs(target_power[i])) {
				target_power_reached[i] = true;
			}
		}

		ime1 = nMotorEncoder(backRight);
		ime2 = nMotorEncoder(backLeft);
#ifdef DEBUG_IME
		check_ime();
#endif // DEBUG_IME

		if ((abs(ime1 - initial_ime1) + abs(ime2 - initial_ime2))/2 > abs(dist))
		{
			//	writeDebugStreamLine("ime1Delta: %f ime2Delta: %f", abs(ime1 - initial_ime1), abs(ime2 - initial_ime2));
			break;
		}


		wait1Msec(AUTO_LOOP_TIME);
	}

	// Wait until target values are reached

	do {
		ime1 = nMotorEncoder(backRight);
		ime2 = nMotorEncoder(backLeft);
		// writeDebugStreamLine("ime1Delta: %f ime2Delta: %f", abs(ime1 - initial_ime1), abs(ime2 - initial_ime2));
#ifdef DEBUG_IME
		check_ime();
#endif // DEBUG_IME
	} while ( (abs(ime1 - initial_ime1) + abs(ime2 - initial_ime2))/2 < abs(dist));

	// Stop motors
	for(i=0; i < 4; i++) {
		motor[motor_index[i]] = 0;
	}
}

void turn(char dir, int angle, int power) {
	move(dir, angle, power);
}


// Note this function can return before slide reaches position (it is asynchronous)
void move_slide_to_position(int position) {
	pid[LEFT_LIFT_PID_INDEX].pidRequestedValue = position;
	pid[RIGHT_LIFT_PID_INDEX].pidRequestedValue = position;
}

// Note this function can return before slide reaches position (it is asynchronous)
void move_arm_to_position(int position) {
	pid[FOUR_BAR_PID_INDEX].pidRequestedValue = position;
}


void do_autonomous_red_skyrise() {

	// Getting the Skyrise section
	int wait_time_between_steps = 10;

  // Step 1: Move slide up
	move_slide_to_position(228);

	// Step 2: Move Back
	move('b', 230, 60);
	wait1Msec(wait_time_between_steps);

	// Step 3: Move Left to face Skyrise section autoloader
	move('l', 710, 100);
	wait1Msec(50);

	// Step 4: Move forward to grip Skyrise section
	move('f', 260, 55);
	wait1Msec(wait_time_between_steps);

	// Step 5: Raise the Arm
	move_slide_to_position(550);
	wait1Msec(900); // This wait is longer because the slide functions are asynchronous
	                // In other words -- they don't complete the action before returning

	// Step 6a: Move Back
	move('b', 140, 127);
	wait1Msec(wait_time_between_steps);

	// Step 6b: Partially Lower the Slide
	move_slide_to_position(75); // notice no waiting

	// Step 7: Move Right
	move('r', 850, 127);
	wait1Msec(wait_time_between_steps);

	// Step 8: Move Back
	move('b', 1300, 127);
	wait1Msec(wait_time_between_steps);


	// Step 9a: Turn towards Skyrise deliver base
	turn('a', 820, 127);
	wait1Msec(wait_time_between_steps);

	// Step 9b: Need to lower arm to move back cube out of the way!
	move_slide_to_position(250);


	// Step 10: Move forward to be on top of base
	move('f', 278, 127);
	wait1Msec(wait_time_between_steps);

	// Step 11: Lower arm to deliver Skyrise section
	move_slide_to_position(0);
	wait1Msec(500); // Notice: hard-coded wait time of 1/2 second to let robot complete planting the Skyrise section

	// Step 12: Backup
	move('b', 290, 127);
	wait1Msec(wait_time_between_steps);

	// Step 13: Turn back to original angle
	turn('c', 820, 127);
	wait1Msec(wait_time_between_steps);

	//Step 14a: Raise Cube to prepare for delivery
	move_slide_to_position(385);

	// Step 14b: Move forward
	move('f', 1100, 127);
	wait1Msec(wait_time_between_steps);

	// Step 15: Turn clockwise so back towards Skyrise section
	turn('c', 830, 127);
	wait1Msec(wait_time_between_steps);

	// Step 16: Move backwards so cube over skyrise
	move('b',245, 127);
	wait1Msec(wait_time_between_steps);

	// Step 17: Lower Cube on to skyrise
	move_slide_to_position(0);
	wait1Msec(500); // Again a hardcoded wait of 1/2 second

	// Step 18: Move forward -- hopefully Cube drops on Skyrise and you're home free!
	move('f', 300, 127);

}

void do_autonomous_blue_NO_skyrise() {
	move_slide_to_position(750);
	 move('b', 150, 127);
	 move('r', 1200, 127);
	 move_arm_to_position(695);
	 move('r', 1550, 127);
	 wait1Msec(300);
	 move('f', 385, 127);
	 wait1Msec(500);
	 move_slide_to_position(350);
	 wait1Msec(1000);
	 move('b', 850, 127);
	 move_slide_to_position(0);
	 move_arm_to_position(0);
}

void do_autonomous_red_NO_skyrise() {
	 move_slide_to_position(750);
	 move('b', 150, 127);
	 move('l', 1200, 127);
	 move_arm_to_position(695);
	 move('l', 1600, 127);
	 wait1Msec(300);
	 move('f', 180, 127);
	 wait1Msec(500);
	 move_slide_to_position(350);
	 wait1Msec(2000);
	 move('b', 400, 127);
	 move('b', 850, 127);
	 move_slide_to_position(0);
	 move_arm_to_position(0);
}
///////////////////////////////
//
// BLUE SKYRISE AUTONOMOUS
//
///////////////////////////////

void do_autonomous_blue_skyrise() {

	// Getting the Skyrise section
	int wait_time_between_steps = 10;

  // Step 1: Move slide up
	move_slide_to_position(228);

	// Step 2: Move Back
	move('b', 190, 60);
	wait1Msec(wait_time_between_steps);


	// Step 3: Move Right to face Skyrise section autoloader
	move('r', 710,127);
	wait1Msec(50);



	// Step 4: Move forward to grip Skyrise section
	move('f', 260, 100);
	wait1Msec(200);

	// Step 5: Raise the Arm
	move_slide_to_position(600);
	wait1Msec(900); // This wait is longer because the slide functions are asynchronous
	                // In other words -- they don't complete the action before returning

	// Step 6a: Move Back
	move('b', 140, 127);
	wait1Msec(wait_time_between_steps);


	// Step 6b: Partially Lower the Slide
	move_slide_to_position(75); // notice no waiting

	// Step 7: Move Left
	move('l', 850, 127);
	wait1Msec(wait_time_between_steps);

	// Step 8: Move Back
	move('b', 1400, 127);
	wait1Msec(wait_time_between_steps);


	// Step 9a: Turn towards Skyrise deliver base
	turn('c', 815, 127);
	wait1Msec(wait_time_between_steps);


	// Step 9b: Need to lower arm to move back cube out of the way!
	move_slide_to_position(250);


	// Step 10: Move forward to be on top of base
	move('f', 240, 127);
	wait1Msec(wait_time_between_steps);


	// Step 11: Lower arm to deliver Skyrise section
	move_slide_to_position(0);
	wait1Msec(500); // Notice: hard-coded wait time of 1/2 second to let robot complete planting the Skyrise section

	// Step 12: Backup
	move('b', 300, 127);
	wait1Msec(wait_time_between_steps);

	// Step 13: Turn back to original angle
	turn('a', 820, 127);
	wait1Msec(wait_time_between_steps);

	//Step 14a: Raise Cube to prepare for delivery
	move_slide_to_position(385);

	// Step 14b: Move forward
	move('f', 1125, 127);
	wait1Msec(wait_time_between_steps);


	// Step 15: Turn counter-clockwise so back towards Skyrise section
	turn('a', 820, 127);
	wait1Msec(wait_time_between_steps);

	// Step 16: Move backwards so cube over skyrise
	move('b',165, 127);
	wait1Msec(wait_time_between_steps);

	// Step 17: Lower Cube on to skyrise
	move_slide_to_position(0);
	wait1Msec(500); // Again a hardcoded wait of 1/2 second

	// Step 18: Move forward -- hopefully Cube drops on Skyrise and you're home free!
	move('f', 300, 127);
}


void pid_init() {
	// Initialize PID parameters for all 3 PID tasks (left lift, right lift, four-bar lift)
	pid[LEFT_LIFT_PID_INDEX].Kp = slide_Kp;
	pid[LEFT_LIFT_PID_INDEX].Ki = slide_Ki;
	pid[LEFT_LIFT_PID_INDEX].Kd = slide_Kd;

	pid[LEFT_LIFT_PID_INDEX].pid_sensor_index = LEFT_SLIDE_SENSOR_INDEX;
	pid[LEFT_LIFT_PID_INDEX].pid_sensor_scale = SLIDE_SENSOR_SCALE;

	pid[LEFT_LIFT_PID_INDEX].pid_motor_index = LEFT_SLIDE_MOTOR_INDEX;
	pid[LEFT_LIFT_PID_INDEX].pid_motor_scale = SLIDE_MOTOR_SCALE;

	pid[LEFT_LIFT_PID_INDEX].max_motor_power = SLIDE_MOTOR_DRIVE_MAX;
	pid[LEFT_LIFT_PID_INDEX].min_motor_power = SLIDE_MOTOR_DRIVE_MIN;
	pid[LEFT_LIFT_PID_INDEX].max_motor_power_delta = MAX_SLIDE_MOTOR_POWER_DELTA;

	pid[LEFT_LIFT_PID_INDEX].pid_integral_limit = SLIDE_INTEGRAL_LIMIT;

	pid[LEFT_LIFT_PID_INDEX].max_motor_power_delta = MAX_SLIDE_MOTOR_POWER_DELTA;
	pid[LEFT_LIFT_PID_INDEX].max_height = SLIDE_MAX_HEIGHT;
	pid[LEFT_LIFT_PID_INDEX].min_height = 0;


	pid[RIGHT_LIFT_PID_INDEX].Kp = slide_Kp;
	pid[RIGHT_LIFT_PID_INDEX].Ki = slide_Ki;
	pid[RIGHT_LIFT_PID_INDEX].Kd = slide_Kd;

	pid[RIGHT_LIFT_PID_INDEX].pid_sensor_index = RIGHT_SLIDE_SENSOR_INDEX;
	pid[RIGHT_LIFT_PID_INDEX].pid_sensor_scale = SLIDE_SENSOR_SCALE;

	pid[RIGHT_LIFT_PID_INDEX].pid_motor_index = RIGHT_SLIDE_MOTOR_INDEX;
	pid[RIGHT_LIFT_PID_INDEX].pid_motor_scale = SLIDE_MOTOR_SCALE;

	pid[RIGHT_LIFT_PID_INDEX].max_motor_power = SLIDE_MOTOR_DRIVE_MAX;
	pid[RIGHT_LIFT_PID_INDEX].min_motor_power = SLIDE_MOTOR_DRIVE_MIN;
	pid[RIGHT_LIFT_PID_INDEX].max_motor_power_delta = MAX_SLIDE_MOTOR_POWER_DELTA;

	pid[RIGHT_LIFT_PID_INDEX].pid_integral_limit = SLIDE_INTEGRAL_LIMIT;

	pid[RIGHT_LIFT_PID_INDEX].max_motor_power_delta = MAX_SLIDE_MOTOR_POWER_DELTA;
	pid[RIGHT_LIFT_PID_INDEX].max_height = SLIDE_MAX_HEIGHT;
	pid[RIGHT_LIFT_PID_INDEX].min_height = 35;

	pid[FOUR_BAR_PID_INDEX].Kp = fourBar_Kp;
	pid[FOUR_BAR_PID_INDEX].Ki = fourBar_Ki;
	pid[FOUR_BAR_PID_INDEX].Kd = fourBar_Kd;

	pid[FOUR_BAR_PID_INDEX].pid_sensor_index = FOUR_BAR_SENSOR_INDEX;
	pid[FOUR_BAR_PID_INDEX].pid_sensor_scale = FOUR_BAR_SENSOR_SCALE;

	pid[FOUR_BAR_PID_INDEX].pid_motor_index = FOUR_BAR_MOTOR_INDEX;
	pid[FOUR_BAR_PID_INDEX].pid_motor_scale = FOUR_BAR_MOTOR_SCALE;

	pid[FOUR_BAR_PID_INDEX].max_motor_power = FOUR_BAR_MOTOR_DRIVE_MAX;
	pid[FOUR_BAR_PID_INDEX].min_motor_power = FOUR_BAR_MOTOR_DRIVE_MIN;
	pid[FOUR_BAR_PID_INDEX].max_motor_power_delta = MAX_FOUR_BAR_MOTOR_POWER_DELTA;

	pid[FOUR_BAR_PID_INDEX].pid_integral_limit = FOUR_BAR_INTEGRAL_LIMIT;

	pid[FOUR_BAR_PID_INDEX].max_motor_power_delta = MAX_FOUR_BAR_MOTOR_POWER_DELTA;

	pid[FOUR_BAR_PID_INDEX].max_height = FOUR_BAR_MAX_HEIGHT;
	pid[FOUR_BAR_PID_INDEX].min_height = 0;
}

////////////////////////////////////////////////////////////////////////////////
////
//// LCD Button Selection Code from http://www.vexforum.com/showthread.php?t=77853
//// courtesy user jpearman
////
////////////////////////////////////////////////////////////////////////////////

// global hold the auton selection
static int MyAutonomous = 0;

/*-----------------------------------------------------------------------------*/
/*  Display autonomous selection                                               */
/*-----------------------------------------------------------------------------*/

// max number of auton choices
#define MAX_CHOICE  4

void
LcdAutonomousSet( int value, bool select = false )
{
    // Cleat the lcd
    clearLCDLine(0);
    clearLCDLine(1);

    // Display the selection arrows
    displayLCDString(1,  0, l_arr_str);
    displayLCDString(1, 13, r_arr_str);

    // Save autonomous mode for later if selected
    if(select)
        MyAutonomous = value;

    // If this choice is selected then display ACTIVE
    if( MyAutonomous == value )
        displayLCDString(1, 5, "ACTIVE");
    else
        displayLCDString(1, 5, "select");




    // Show the autonomous names
    switch(value) {
        case    0:
            displayLCDString(0, 0, "RED Skyrise");
            break;
        case    1:
            displayLCDString(0, 0, "RED NO");
            break;
        case    2:
            displayLCDString(0, 0, "BLUE Skyrise");
            break;
        case    3:
            displayLCDString(0, 0, "BLUE NO");
            break;
        default:
            displayLCDString(0, 0, "Unknown");
            break;
        }
}

/*-----------------------------------------------------------------------------*/
/*  Rotate through a number of choices and use center button to select         */
/*-----------------------------------------------------------------------------*/

void
LcdAutonomousSelection()
{
    TControllerButtons  button;
    int  choice = 0;

    // Turn on backlight
    bLCDBacklight = true;

    // diaplay default choice
    LcdAutonomousSet(0);

    while( bIfiRobotDisabled )
        {
        // this function blocks until button is pressed
        button = getLcdButtons();

        // Display and select the autonomous routine
        if( ( button == kButtonLeft ) || ( button == kButtonRight ) ) {
            // previous choice
            if( button == kButtonLeft )
                if( --choice < 0 ) choice = MAX_CHOICE;
            // next choice
            if( button == kButtonRight )
                if( ++choice > MAX_CHOICE ) choice = 0;
            LcdAutonomousSet(choice);
            }

        // Select this choice
        if( button == kButtonCenter )
            LcdAutonomousSet(choice, true );

        // Don't hog the cpu !
        wait1Msec(10);
        }
}


/////////////////////////////////////////////////////////////////////////////////////////
//
//                          Pre-Autonomous Functions
//
// You may want to perform some actions before the competition starts. Do them in the
// following function.
//
/////////////////////////////////////////////////////////////////////////////////////////

void pre_auton()
{
  // Set bStopTasksBetweenModes to false if you want to keep user created tasks running between
  // Autonomous and Tele-Op modes. You will need to manage all user created tasks if set to false.
  bStopTasksBetweenModes = true;

	// All activities that occur before the competition starts
	// Example: clearing encoders, setting servo positions, ...

 	// Initialize PID parameters
	pid_init();

	// Initialize Lift
	init_lift();

	// start the PID task
	startTask( PidController );

	// Ask for user input on LCD as to which Autonomous to run
	LcdAutonomousSelection();
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//                                 Autonomous Task
//
// This task is used to control your robot during the autonomous phase of a VEX Competition.
// You must modify the code to add your own robot specific commands here.
//
/////////////////////////////////////////////////////////////////////////////////////////
#define RED_SKYRISE 	0
#define RED_NO 				1
#define BLUE_SKYRISE 	2
#define BLUE_NO 			3


task autonomous()
{
	init_lift();
	// Reset Drive IMEs
	resetMotorEncoder(backLeft);
	resetMotorEncoder(backRight);

	switch( MyAutonomous ) {
        case    0:
            do_autonomous_red_skyrise();
            break;
        case    1:
            do_autonomous_red_NO_skyrise();
            break;
    		case    2:
            do_autonomous_blue_skyrise();
            break;
        case    3:
            do_autonomous_blue_NO_skyrise();
            break;
        default:
            break;
   }
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//                                 User Control Task
//
// This task is used to control your robot during the user control phase of a VEX Competition.
// You must modify the code to add your own robot specific commands here.
//
/////////////////////////////////////////////////////////////////////////////////////////

task usercontrol()
{


	// NOTE: The following three initialization routines are run again because we have elected to
	// set bStopTasksBetweenModes = true in pre_auton

	// Initialize PID parameters
	pid_init();

	// Initialize Lift
	// Moved to start of actual autonomous init_lift();

	// start the PID task
	startTask( PidController );


	// use joystick to modify the requested position
	while( true )
	{
		float pidRequestedValue;


		// maximum change for pidRequestedValue will be 127/4*20, around 640 counts per second
		// free spinning motor is 100rmp so 1.67 rotations per second
		// 1.67 * 360 counts is 600

		// Button 8D RESETs both slides
		if (vexRT[Btn8DXmtr2] == 1)
		{
			if (pid[LEFT_LIFT_PID_INDEX].pid_active) {
				pid[LEFT_LIFT_PID_INDEX].pid_active = false;
				pid[RIGHT_LIFT_PID_INDEX].pid_active = false;

				// Doing the next two lines in the zero_reset_slide() task led to a race condition where
				// on recovery the slide would slide back up. This fixes it.
				pid[LEFT_LIFT_PID_INDEX].pidRequestedValue = 0;
				pid[RIGHT_LIFT_PID_INDEX].pidRequestedValue = 0;

				startTask(zero_reset_slide);
			}
			pidRequestedValue = 0; // Dummy statement to resolve if-then-else ambiguity! Do not remove


			} else if (vexRT(Btn6UXmtr2) == 1){
			  // Preset Value -- for now it is just for FIRST Skyrise pickup
			  pidRequestedValue = HEIGHT_FOR_FIRST_SKYRISE_SECTION;
			  pidRequestedValue = HEIGHT_FOR_FIRST_SKYRISE_SECTION;
      } else {
			// joystick control of slide arms

			if (abs(vexRT[Ch2Xmtr2]) > JOYSTICK_MAX_NEUTRAL) {
				// Calculate desired speed
				pidRequestedValue = pid[LEFT_LIFT_PID_INDEX].pidRequestedValue;
				pidRequestedValue += vexRT[Ch2Xmtr2]/5.0;

				if (pidRequestedValue  >= pid[LEFT_LIFT_PID_INDEX].max_height) {
					pidRequestedValue = pid[LEFT_LIFT_PID_INDEX].max_height;
				}

				if (pidRequestedValue <= pid[LEFT_LIFT_PID_INDEX].min_height) {
					pidRequestedValue = pid[LEFT_LIFT_PID_INDEX].min_height;
				}

#ifdef DEBUG_PID
				start_debug_stream = true;
				debug_delay_counter = 0;

				} else {
					if (++debug_delay_counter > 20)
						start_debug_stream = false;

				//					if (pid[0].previous_speed > 0.01)


				//					DebugStreamLine("speed: %f", pid[0].previous_speed*1000.0/60.0);
#endif DEBUG_PID
			}

			pid[LEFT_LIFT_PID_INDEX].pidRequestedValue = pidRequestedValue;
			pid[RIGHT_LIFT_PID_INDEX].pidRequestedValue = pidRequestedValue;
		}

    // Button 7D RESETs ARM (Four Bar)
		if (vexRT[Btn7DXmtr2] == 1)
		{
			int dummy;

			if (pid[FOUR_BAR_PID_INDEX].pid_active) {
				pid[FOUR_BAR_PID_INDEX].pid_active = false;

				// Doing the next line in the zero_reset_arm() task led to a race condition where
				// on recovery the arm would swing back up. This fixes it.
				pid[FOUR_BAR_PID_INDEX].pidRequestedValue = 0;

				startTask(zero_reset_arm);
			}
			dummy = 0; // Dummy statement to resolve if-then-else ambiguity! Do not remove
			} else {

			// control of four-bar-linkage arm
			if (abs(vexRT[Ch3Xmtr2]) > JOYSTICK_MAX_NEUTRAL) {
				float fourBarRequestedValue = pid[FOUR_BAR_PID_INDEX].pidRequestedValue;
				fourBarRequestedValue = fourBarRequestedValue + (vexRT[ Ch3Xmtr2 ]/5.0);
				if (fourBarRequestedValue > FOUR_BAR_MAX_HEIGHT) {
					fourBarRequestedValue = FOUR_BAR_MAX_HEIGHT;
				}
				if (fourBarRequestedValue < 0) {
					fourBarRequestedValue = 0;
				}
				pid[FOUR_BAR_PID_INDEX].pidRequestedValue = fourBarRequestedValue;
			}
		}

		// Preset Height

		//Create "deadzone" variables. Adjust threshold value to increase/decrease deadzone
		int X2 = vexRT[Ch4], Y1 = vexRT[Ch2], X1 = vexRT[Ch1];

		// NOTE: Change to make speed = joystick_offset^2/128

		//Create "deadzone" for Y1/Ch2
		if(abs(Y1) > JOYSTICK_MAX_NEUTRAL)
			Y1 = sgn(Y1)*Y1*Y1/128;
		else
			Y1 = 0;
		//Create "deadzone" for X1/Ch1
		if(abs(X1) > JOYSTICK_MAX_NEUTRAL)
			X1 = sgn(X1)*X1*X1/128;
		else
			X1 = 0;
		//Create "deadzone" for X2/Ch4
		if(abs(X2) > JOYSTICK_MAX_NEUTRAL)
			X2 = sgn(X2)*X2*X2/128;
		else
			X2 = 0;

		// On Aria's request DISABLING diagonal move
		if (abs(Y1) > abs(X1)) {
			X1 = 0;
	  } else {
	    Y1 = 0;
	  }

		//Remote Control Commands
		motor[frontRight] = Y1 - X2 - X1;
		motor[backRight] =  Y1 - X2 + X1;
		motor[frontLeft] = Y1 + X2 + X1;
		motor[backLeft] =  Y1 + X2 - X1;


		wait1Msec(USER_CONTROL_LOOP_TIME);
	}
}

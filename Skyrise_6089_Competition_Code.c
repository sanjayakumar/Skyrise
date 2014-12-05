
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
#define SLIDE_MAX_HEIGHT		  		2320
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
#define SLIDE_MOTOR_MAX			127.0
#define SLIDE_MOTOR_MIN			(-80.0)
#define FOUR_BAR_MOTOR_MAX	127.0
#define FOUR_BAR_MONOR_MIN 	(-80.0)
#define MAX_SLIDE_MOTOR_POWER_DELTA			30
#define MAX_FOUR_BAR_MOTOR_POWER_DELTA	30
// Absolute value of joystick below this number is considered neutral
#define JOYSTICK_MAX_NEUTRAL 			15
#define SLIDE_INTEGRAL_LIMIT  		800
#define FOUR_BAR_INTEGRAL_LIMIT 	0
#define NUM_PID_CONTROLS 					4
#define LEFT_LIFT_PID_INDEX 			0
#define RIGHT_LIFT_PID_INDEX 			1
#define FOUR_BAR_PID_INDEX 				2

#define SLOW_MODE_FACTOR					0.35

// Parameters for Drive PID
#define DRIVE_PID_INDEX			3

#define DRIVE_MOTOR_SCALE		1

#define DRIVE_MOTOR_MAX						127
#define DRIVE_MOTOR_MIN						-127
#define DRIVE_INTEGRAL_LIMIT			8000
#define DRIVE_SENSOR_SCALE		1
#define DRIVE_MOTOR_POWER_DELTA		127

#define DRIVE_SENSOR_THRESHOLD 		20 // How close to target we need to be to consider that we're there

// This is the preset height for picking up the skyrise section at the highest point (to deliver the first one)
#define SKYRISE_MIDDLE_INTAKE_HEIGHT 430
#define SKYRISE_LOW_INTAKE_HEIGHT    278

#define USER_CONTROL_LOOP_TIME	50 // Milliseconds
#define PID_LOOP_TIME						25

#define MAX_COUNT 20
#define MIN_CHANGE_PER_LOOP 3

#define FL	0
#define BL	1
#define FR	2
#define BR	3


int motor_index[4];
int motor_direction[4];


// Structure to store PID parameters -- note we have 3; one for each slide and one for the Arm
typedef struct {
	volatile float pidRequestedValue;

	float Kp;
	float Ki;
	float Kd;

	float K_value_scale; // Used ONLY by Drive PID

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
float  slide_Kp = 1.8;
float  slide_Ki = 0.01;
float  slide_Kd = 0;

float  fourBar_Kp = 1.2;
float  fourBar_Ki = 0.00;
float  fourBar_Kd = 0.5;

float drive_Kp = 0.9;
float drive_Ki = 0;
float drive_Kd = 1.5;

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

#ifdef DEBUG_PID
	int max_mismatch = 0;
#endif /* DEBUG_PID */


	int i, j;

	for (i = 0; i < NUM_PID_CONTROLS; i++) {
		pid[i].previous_error  = 0;
		pid[i].errorIntegral   = 0;
		pid[i].previous_motor_power = 0;
		pid[i].pid_sensor_previous_value = 0;
		pid[i].previous_speed = 0;
		pid[i].pid_sensor_previous_value = 0;
		pid[i].K_value_scale = 1;
		pid[i].pid_active = true;
	}

	j = 0;

	//int pid_loop_count = 0;

	while( true ) {

		//writeDebugStreamLine("PID Loop count: %d", pid_loop_count++);



		for (i = 0; i < NUM_PID_CONTROLS; i++) {

			// If the current pid is not active, skip the loop
			if (!pid[i].pid_active)
				continue;

			// Read the sensor value and scale
			if (i != DRIVE_PID_INDEX) {
				pidSensorCurrentValue = SensorValue[ pid[i].pid_sensor_index] * pid[i].pid_sensor_scale;
			} else { // For Drive PID Index
				pidSensorCurrentValue = (nMotorEncoder[backLeft] * motor_direction[BL] + nMotorEncoder[backRight]*motor_direction[BR])/2.0;
			}

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
			pidDerivative =  pidError - pid[i].previous_error;
			pid[i].previous_error  = pidError;

			// calculate drive
			pidDrive = pid[i].K_value_scale * (pid[i].Kp * pidError) + (pid[i].Ki * pid[i].errorIntegral) + (pid[i].Kd * pidDerivative);

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

			// send to motor (SORRY -- MAJOR KLUDGE FOLLOWS FOR DRIVE MOTORS!!!
			if (i != DRIVE_PID_INDEX) {
				motor[ pid[i].pid_motor_index ] = pidDrive * pid[i].pid_motor_scale;
			} else {

			  //writeDebugStreamLine("pidDrive: %f pidError: %f  PID: %f %f %f", pidDrive, pidError,
			  //pid[i].K_value_scale*pid[i].Kp * pidError, pid[i].K_value_scale*pid[i].Ki * pid[i].errorIntegral, pid[i].K_value_scale*pid[i].Kd * pidDerivative);
				motor[motor_index[FL]] = motor_direction[FL]*pidDrive;
				motor[motor_index[BL]] = motor_direction[BL]*pidDrive;

				motor[motor_index[FR]] = motor_direction[FR]*pidDrive;
				motor[motor_index[BR]] = motor_direction[BR]*pidDrive;
			}


#ifdef DEBUG_PID
			pid[i].pid_sensor_previous_value = pidSensorCurrentValue;
			if (i == 0 && abs(vexRT[Ch2Xmtr2]) > JOYSTICK_MAX_NEUTRAL ) {
				float sensor_mismatch = pid[0].pid_sensor_previous_value - pid[1].pid_sensor_previous_value;
				if (abs(sensor_mismatch) > abs(max_mismatch))
					max_mismatch = sensor_mismatch;
				// writeDebugStreamLine("pidDrive: %f pidError: %f  PID: %f %f %f", pidDrive, pidError, pid[i].Kp * pidError, pid[i].Ki * pid[i].errorIntegral, pid[i].Kd * pidDerivative);
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

	for (i=0; i<IME_HISTORY_LENGTH; i++) {
		writeDebugStreamLine("%d", ime1_history[i]);
	}

	writeDebugStreamLine(" ");
	for (i=0; i<IME_HISTORY_LENGTH; i++) {
		writeDebugStreamLine("%d", ime2_history[i]);
	}

}
#endif DEBUG_IME

void wait_for_move_done(int target_dist) {
	// Wait until target values are reached

	float ime1, ime2, dist1, dist2;

	float old_dist1, old_dist2;
	int count;


  count = 0;
  old_dist1 = old_dist2 = 5000; // Some "large value
	do {
		ime1 = nMotorEncoder(backLeft);
		ime2 = nMotorEncoder(backRight);
#ifdef DEBUG_IME
		check_ime();
#endif // DEBUG_IME
		wait1Msec(PID_LOOP_TIME);
		dist1 = abs(ime1);
		dist2 = abs(ime2);

		if (abs(dist1 - old_dist1) < MIN_CHANGE_PER_LOOP && abs(dist2 - old_dist2) < MIN_CHANGE_PER_LOOP)
			count++;
		else
			count = 0;
	  if (count == MAX_COUNT)
	  	return;

		old_dist1 = dist1;
		old_dist2 = dist2;

	} while (abs((dist1 + dist2)/2 - abs(target_dist)) > DRIVE_SENSOR_THRESHOLD);
}

void start_move(char dir, int dist, int power)
{

switch(dir){
	case 'f':
		motor_direction[FR] = 1;
		motor_direction[BR] = 1;
		motor_direction[FL] = 1;
		motor_direction[BL] = 1;
		pid[DRIVE_PID_INDEX].K_value_scale = 1.0;

		break;
	case 'b':
		motor_direction[FR] = -1;
		motor_direction[BR] = -1;
		motor_direction[FL] = -1;
		motor_direction[BL] = -1;
		pid[DRIVE_PID_INDEX].K_value_scale = 1.0;

		break;
	case 'r':
		motor_direction[FR] = -1;
		motor_direction[BR] = 1;
		motor_direction[FL] = 1;
		motor_direction[BL] = -1;
		pid[DRIVE_PID_INDEX].K_value_scale = 3;

		break;
	case 'l':
		motor_direction[FR] = 1;
		motor_direction[BR] = -1;
		motor_direction[FL] = -1;
		motor_direction[BL] = 1;
		pid[DRIVE_PID_INDEX].K_value_scale = 3;

		break;
	case 'c':
		motor_direction[FR] = -1;
		motor_direction[BR] = -1;
		motor_direction[FL] = 1;
		motor_direction[BL] = 1;
		pid[DRIVE_PID_INDEX].K_value_scale = 1.5;

		break;
	case 'a':
		motor_direction[FR] = 1;
		motor_direction[BR] = 1;
		motor_direction[FL] = -1;
		motor_direction[BL] = -1;
		pid[DRIVE_PID_INDEX].K_value_scale = 1.5;

		break;
	}

	pid[DRIVE_PID_INDEX].max_motor_power = power;

	pid[DRIVE_PID_INDEX].min_motor_power = -power;

	pid[DRIVE_PID_INDEX].pidRequestedValue = dist;

	resetMotorEncoder(backLeft);
	resetMotorEncoder(backRight);




}

void move(char dir, int dist, int power){
	start_move(dir, dist, power);
	wait_for_move_done(dist);
}

void turn(char dir, int angle, int power) {
	move(dir, angle, power);
	wait_for_move_done(angle);
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

#define SLIDE_TARGET_THRESHOLD 20


void wait_for_arm_done() {
	float pidSensorCurrentValue;
	float pidError, old_pidError;
	int count;

	count = 0;
	old_pidError = 5000; // Some "large" value

	do {
		// Read the sensor value and scale
		pidSensorCurrentValue = SensorValue[ pid[2].pid_sensor_index] * pid[2].pid_sensor_scale;

		// calculate error
		pidError = pid[2].pidRequestedValue - pidSensorCurrentValue;

		if (abs(old_pidError - pidError) < MIN_CHANGE_PER_LOOP)
			count++;
		else
			count = 0;
	  if (count == MAX_COUNT)
	  	return;

	  old_pidError = pidError;

		wait1Msec(PID_LOOP_TIME);
	} while (abs (pidError) > SLIDE_TARGET_THRESHOLD);
}

void wait_for_slide_done() {

	float pidSensorCurrentValue;
	float pidError, old_pidError;
	int count;

	count = 0;
	old_pidError = 5000; // Some "large" value


	do {
		// Read the sensor value and scale
		pidSensorCurrentValue = SensorValue[ pid[LEFT_LIFT_PID_INDEX].pid_sensor_index] * pid[LEFT_LIFT_PID_INDEX].pid_sensor_scale;


		pidError = pid[LEFT_LIFT_PID_INDEX].pidRequestedValue - pidSensorCurrentValue;

		pidSensorCurrentValue = SensorValue[ pid[RIGHT_LIFT_PID_INDEX].pid_sensor_index] * pid[RIGHT_LIFT_PID_INDEX].pid_sensor_scale;

		pidError += pid[RIGHT_LIFT_PID_INDEX].pidRequestedValue - pidSensorCurrentValue;

		pidError = pidError/2.0;

		if (abs(old_pidError - pidError) < MIN_CHANGE_PER_LOOP)
			count++;
		else
			count = 0;
	  if (count == MAX_COUNT)
	  	return;

		old_pidError  = pidError;

		wait1Msec(PID_LOOP_TIME);
	} while (abs (pidError) > SLIDE_TARGET_THRESHOLD);
}

void do_autonomous_red_skyrise() {

	// Getting the Skyrise section

	//Step 1: Move back one tile
	start_move('b', 250, 127);

	//Step 2: Raise slide to align with skyrise
	move_slide_to_position(600);
	wait_for_move_done(250);
	move('l', 60, 127);
	wait_for_slide_done();

	move_slide_to_position(435);
	wait_for_slide_done();

	//Step 3: Move forward to get skyrise
	move('f', 280, 80);
	wait1Msec(250);


	//Step 4: Raise slide to take out skyrise
	move_slide_to_position(950);
	wait_for_slide_done();

	move('b', 25, 127);
	move('l', 45, 127);

	start_move('b', 925, 120);
	move_slide_to_position(450);
	wait_for_move_done(925);

	wait_for_slide_done();

	move('b', 148, 115);

	move_slide_to_position(0);
	wait_for_slide_done();

	move('b', 200, 55);

	move_slide_to_position(400);
	wait_for_slide_done();
	move_slide_to_position(1250);

		move('f', 440, 127);
	move('r', 175, 127);
	turn('c', 1120, 127);
	move('b', 270, 127);

	move_slide_to_position(0);
	wait_for_slide_done();
	move('f', 235, 127);


	/*move('f', 1050, 127);

	move('l', 805, 127);

	move('b', 115, 127);

	move_slide_to_position(0);
	wait_for_slide_done();

	move('f', 165, 127);*/

}


#ifdef TWO_SKYRISE
void do_autonomous_red_two_skyrise() {

	// Getting the Skyrise section

	// Step 1: Move slide up
	move_slide_to_position(450);
	wait_for_slide_done();

	// Step 2: Move Forward to Grab Skyrise
	start_move('f', 100, 127);
	wait_for_move_done(100);

	// Step 3: Raise slide up
	move_slide_to_position(950);
	wait_for_slide_done();

	// Step 4: Move back to take out skyrise
	start_move('b', 815, 127);
	wait_for_move_done(815);

	// Step 5: Lower arm to deliver skyrise
	move_slide_to_position(0);
	wait_for_slide_done(); // This wait is longer because the slide functions are asynchronous
	// In other words -- they don't complete the action before returning

	// Step 6a: Move Back after delivery
	start_move('b', 140, 127);
	wait_for_move_done(140);

	// Step 6b: Raise arm over skyrise
	move_slide_to_position(525);
	wait_for_slide_done();

	// Step 7a: Move forward towards auto-loader
	start_move('f', 500, 127);
	wait_for_move_done(500);

	// Step 7b: Lower arm more
	move_slide_to_position(425);
	wait_for_slide_done();

	//Step 7c: Move forward more
	start_move('f', 600, 127);
	wait_for_move_done(600);

	// Step 8: Raise slide
	move_slide_to_position(1200);
	wait_for_slide_done();

	// Step 9a: Move back to take out second skyrise
	start_move('b', 790, 127);
	wait_for_move_done(790);

	// Step 9b: Lower arm to place second skyrise
	move_slide_to_position(600);
	wait_for_slide_done();


	// Step 10: Move back to deliver second skyrise
	start_move('b', 140, 127);
	wait_for_move_done(140);

}

#endif // TWO_SKYRISE

void do_autonomous_blue_cube_only() {
	move('l', 230, 127);
	move_slide_to_position(1660);
	move('b', 1650, 127);
	wait_for_slide_done();
	move_arm_to_position(410);
	wait_for_arm_done();
	turn('c', 1405, 100);
	move_slide_to_position(700);
	wait_for_slide_done();
	move('b', 400, 127);

}

void do_autonomous_red_cube_only() {
	move('r', 280, 127);
	move_slide_to_position(1710);
	move('b', 1650, 127);
	wait_for_slide_done();

	move_arm_to_position(410);
	wait_for_arm_done();
	turn('a', 1405, 100);
	move_slide_to_position(700);
	wait_for_slide_done();
	move('b', 400, 127);

}
///////////////////////////////
//
// BLUE SKYRISE AUTONOMOUS
//
///////////////////////////////



void do_autonomous_blue_skyrise() {

	// Getting the Skyrise section

	//Step 1: Move back one tile
	start_move('b', 250, 127);


	//Step 2: Raise slide to align with skyrise
	move_slide_to_position(600);
	wait_for_move_done(250);

	move('r', 70, 127);
	wait_for_slide_done();

	move_slide_to_position(435);
	wait_for_slide_done();

	//Step 3: Move forward to get skyrise
	move('f', 280, 80);
	wait1Msec(250);


	//Step 4: Raise slide to take out skyrise
	move_slide_to_position(950);
	wait_for_slide_done();
	move('b', 25, 127);
  move('r', 95, 127);
	start_move('b', 950, 120);

	move_slide_to_position(450);
	wait_for_move_done(950);

	wait_for_slide_done();

	move('b', 148, 115);

	move_slide_to_position(0);
	wait_for_slide_done();


	move('b', 200, 55);

	move_slide_to_position(400);
	wait_for_slide_done();
	move_slide_to_position(1250);

	move('f', 440, 127);
	move('l', 175, 127);
	turn('a', 1120, 127);
	move('b', 270, 127);

	move_slide_to_position(0);
	wait_for_slide_done();
	move('f', 235, 127);


/*
	move('f', 1150, 127);

	move('r', 805, 127);

	move('b', 115, 127);

	move_slide_to_position(0);
	wait_for_slide_done();

	move('f', 165, 127); */

}

void do_nothing() {
	// Like it says!!
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

	pid[LEFT_LIFT_PID_INDEX].max_motor_power = SLIDE_MOTOR_MAX;
	pid[LEFT_LIFT_PID_INDEX].min_motor_power = SLIDE_MOTOR_MIN;
	pid[LEFT_LIFT_PID_INDEX].max_motor_power_delta = MAX_SLIDE_MOTOR_POWER_DELTA;

	pid[LEFT_LIFT_PID_INDEX].pid_integral_limit = SLIDE_INTEGRAL_LIMIT;

	pid[LEFT_LIFT_PID_INDEX].max_height = SLIDE_MAX_HEIGHT;
	pid[LEFT_LIFT_PID_INDEX].min_height = 0;


	pid[RIGHT_LIFT_PID_INDEX].Kp = slide_Kp;
	pid[RIGHT_LIFT_PID_INDEX].Ki = slide_Ki;
	pid[RIGHT_LIFT_PID_INDEX].Kd = slide_Kd;

	pid[RIGHT_LIFT_PID_INDEX].pid_sensor_index = RIGHT_SLIDE_SENSOR_INDEX;
	pid[RIGHT_LIFT_PID_INDEX].pid_sensor_scale = SLIDE_SENSOR_SCALE;

	pid[RIGHT_LIFT_PID_INDEX].pid_motor_index = RIGHT_SLIDE_MOTOR_INDEX;
	pid[RIGHT_LIFT_PID_INDEX].pid_motor_scale = SLIDE_MOTOR_SCALE;

	pid[RIGHT_LIFT_PID_INDEX].max_motor_power = SLIDE_MOTOR_MAX;
	pid[RIGHT_LIFT_PID_INDEX].min_motor_power = SLIDE_MOTOR_MIN;
	pid[RIGHT_LIFT_PID_INDEX].max_motor_power_delta = MAX_SLIDE_MOTOR_POWER_DELTA;

	pid[RIGHT_LIFT_PID_INDEX].pid_integral_limit = SLIDE_INTEGRAL_LIMIT;

	pid[RIGHT_LIFT_PID_INDEX].max_height = SLIDE_MAX_HEIGHT;
	pid[RIGHT_LIFT_PID_INDEX].min_height = 35;

	pid[FOUR_BAR_PID_INDEX].Kp = fourBar_Kp;
	pid[FOUR_BAR_PID_INDEX].Ki = fourBar_Ki;
	pid[FOUR_BAR_PID_INDEX].Kd = fourBar_Kd;

	pid[FOUR_BAR_PID_INDEX].pid_sensor_index = FOUR_BAR_SENSOR_INDEX;
	pid[FOUR_BAR_PID_INDEX].pid_sensor_scale = FOUR_BAR_SENSOR_SCALE;

	pid[FOUR_BAR_PID_INDEX].pid_motor_index = FOUR_BAR_MOTOR_INDEX;
	pid[FOUR_BAR_PID_INDEX].pid_motor_scale = FOUR_BAR_MOTOR_SCALE;

	pid[FOUR_BAR_PID_INDEX].max_motor_power = FOUR_BAR_MOTOR_MAX;
	pid[FOUR_BAR_PID_INDEX].min_motor_power = FOUR_BAR_MONOR_MIN;
	pid[FOUR_BAR_PID_INDEX].max_motor_power_delta = MAX_FOUR_BAR_MOTOR_POWER_DELTA;

	pid[FOUR_BAR_PID_INDEX].pid_integral_limit = FOUR_BAR_INTEGRAL_LIMIT;

	pid[FOUR_BAR_PID_INDEX].max_height = FOUR_BAR_MAX_HEIGHT;
	pid[FOUR_BAR_PID_INDEX].min_height = 0;



	pid[DRIVE_PID_INDEX].Kp = drive_Kp;
	pid[DRIVE_PID_INDEX].Ki = drive_Ki;
	pid[DRIVE_PID_INDEX].Kd = drive_Kd;

	pid[DRIVE_PID_INDEX].pid_sensor_scale = DRIVE_SENSOR_SCALE;

	pid[DRIVE_PID_INDEX].max_motor_power = DRIVE_MOTOR_MAX;
	pid[DRIVE_PID_INDEX].min_motor_power = DRIVE_MOTOR_MIN;
	pid[DRIVE_PID_INDEX].max_motor_power_delta = DRIVE_MOTOR_POWER_DELTA;

	pid[DRIVE_PID_INDEX].pid_integral_limit = DRIVE_INTEGRAL_LIMIT;


	// Initialize Sensors
	resetMotorEncoder(backLeft);
	resetMotorEncoder(backRight);

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
#define MAX_CHOICE  5

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
		displayLCDString(0, 0, "RED CUBE Only");
		break;
	case    2:
		displayLCDString(0, 0, "BLUE Skyrise");
		break;
	case    3:
		displayLCDString(0, 0, "BLUE CUBE Only");
		break;
	case		4:
		displayLCDString(0, 0, "DO NOTHING");
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

	// display default choice
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

	// All activities that occur before the competition
	// Example: clearing encoders, setting servo positions, ...

	// Init Motor index array
	motor_index[FR] = frontRight;
	motor_index[BR] = backRight;
	motor_index[FL] = frontLeft;
	motor_index[BL] = backLeft;

	// Initialize PID parameters
	pid_init();

	// Initialize Lift
	init_lift();

	// start the PID task
	startTask( PidController, 6 );

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
#define RED_SKYRISE 	 0
#define RED_CUBE_ONLY	 1
#define BLUE_SKYRISE 	 2
#define BLUE_CUBE_ONLY 3


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
		do_autonomous_red_cube_only();
		break;
	case    2:
		do_autonomous_blue_skyrise();
		break;
	case    3:
		do_autonomous_blue_cube_only();
		break;
	case		4:
		do_nothing();
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
	startTask( PidController, 6 );
	wait1Msec(100);

	// For User Control we don't use PID drive functions
	pid[DRIVE_PID_INDEX].pid_active = false;



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
			pidRequestedValue = SKYRISE_MIDDLE_INTAKE_HEIGHT;
			} else if (vexRT[Btn6DXmtr2]) {
			pidRequestedValue = SKYRISE_LOW_INTAKE_HEIGHT;
			} else if (vexRT[Btn5DXmtr2]) {
			pidRequestedValue = 140; // Slide Height to pick up second cube
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
			} else {
					pidRequestedValue = (SensorValue[RIGHT_SLIDE_SENSOR_INDEX]+SensorValue[LEFT_SLIDE_SENSOR_INDEX])/2; // Stay where you are currently
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

		// Aria's request #2: Slow mode when 5U pressed
		if (vexRT[Btn5U]) {
			float temp;
			temp = X1 * SLOW_MODE_FACTOR;
			X1 = temp;
			temp = X2 * SLOW_MODE_FACTOR;
			X2 = temp;
			temp = Y1 * SLOW_MODE_FACTOR;
			Y1 = temp;

		}

		//Remote Control Commands
		motor[frontRight] = Y1 - X2 - X1;
		motor[backRight] =  Y1 - X2 + X1;
		motor[frontLeft] = Y1 + X2 + X1;
		motor[backLeft] =  Y1 + X2 - X1;


		wait1Msec(USER_CONTROL_LOOP_TIME);
	}
}

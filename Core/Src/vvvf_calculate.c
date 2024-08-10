#include "vvvf_calculate.h"
#include "chm_she_calculate.h"
#include "stm32f4xx_hal.h"

#include <math.h>
#include "arm_math.h"

#include <stdlib.h>

float32_t Triangle_Wave(float32_t Angle);
float32_t Square_Wave(float32_t Angle, float32_t Period);
float32_t Center60_Modulation(float32_t sine_wave_angle, uint32_t N);
float32_t calculate_variable_pwm_frequency(float32_t pwm_frequency_start, float32_t output_frequency_start, float32_t pwm_frequency_end, float32_t output_frequency_end);

enum pwm_type {

	ASYNC_SPWM,
	SYNC_SPWM,
	SHIFTED_SYNC_SPWM,

	SQUAREWAVE_SYNC,
	WIDE_3P,
	ONE_P,

	CHM,
	WIDE_CHM,
    WIDE_CHM_STEPPED,
	SHE,

};

enum motor_mode {

	accelerating,
	braking
};

#define calculation_sample_rate 100000
#define pwm_max_output_value 1800

float32_t target_frequency;

float32_t output_frequency;
float32_t pwm_frequency;
uint8_t current_pwm_mode;
uint8_t current_motor_mode = accelerating;

float32_t v_hz_ratio;
float32_t modulation_index;

float32_t time_step;
float32_t carrier_wave_angle;
float32_t sine_wave_angle;

float32_t current_calculation;

float32_t Triangle_Wave(float32_t Angle)
{
	float32_t four_d_period = 90;
	float32_t two_d_period = 180;
	uint32_t Wave_Num = Angle / 360;
	Angle -= Wave_Num * 360;
	if (Angle <= two_d_period)
	{
		return Angle <= four_d_period ? Angle / four_d_period : 2 - Angle / four_d_period;
	}
	else
	{
		Angle -= two_d_period;
		return -(Angle <= four_d_period ? Angle / four_d_period : 2 - Angle / four_d_period);
	}
}

float32_t Square_Wave(float32_t Angle, float32_t Period)
{
	float32_t Square = 0;
	float32_t two_d_period = (Period * 0.5);
	uint32_t Wave_Num = 0;
	Wave_Num = Angle / Period;
	Angle -= Wave_Num * Period;
	Square = Angle <= two_d_period ? 1 : -1;
	return Square;
}

float32_t Center60_Modulation(float32_t sine_wave_angle, uint32_t N)
{
	float32_t T_Main_Wave = Square_Wave(sine_wave_angle + 60, 120);
	float32_t T_Division_Positive_Wave = Triangle_Wave(3 * N * sine_wave_angle);
	if (T_Main_Wave >= 0)
	{
		return T_Division_Positive_Wave >= 0 ? T_Division_Positive_Wave : -T_Division_Positive_Wave;
	}
	else
	{
		return T_Division_Positive_Wave <= 0 ? T_Division_Positive_Wave : -T_Division_Positive_Wave;
	}
}

float32_t calculate_variable_pwm_frequency(float32_t pwm_frequency_start, float32_t output_frequency_start, float32_t pwm_frequency_end, float32_t output_frequency_end){

	return ((output_frequency - output_frequency_start) * (pwm_frequency_end - pwm_frequency_start) / (output_frequency_end - output_frequency_start)) + pwm_frequency_start;
}


void calculate_angle (void)
{
	current_calculation += 1;
	current_calculation = current_calculation >= calculation_sample_rate ? current_calculation - calculation_sample_rate : current_calculation;

	time_step += ((360 * 1) / calculation_sample_rate);

	sine_wave_angle += ((360 * output_frequency) / calculation_sample_rate);
	carrier_wave_angle += ((360 * pwm_frequency) / calculation_sample_rate);

	sine_wave_angle = sine_wave_angle >= 360 ? sine_wave_angle - 360 : sine_wave_angle;
	carrier_wave_angle = carrier_wave_angle >= 360 ? carrier_wave_angle - 360 : carrier_wave_angle;

}

/*

void calculate_2_level (void)
{
#define __PI_3 1.0471975511965977461542144610931676280657f //pi divided by 3
#define __PI_23 2.0943951023931954923084289221863352561314f //2pi divided by 3
#define __PI_43 4.1887902047863909846168578443726705122628f //4pi divided by 3
#define __PI_2 1.5707963267948966192313216916397514420985f //pi divided by 2
#define __2PI 6.2831853071795864769252867665590057683943f //2pi
#define __PI_180 0.017453292519943f //pi divided by 180. Used for converting degrees into radians

#define __4_PI 1.27323954474
#define __PI_4 0.785398163397

	int array_l;

	float32_t sine; //variable to store the calculated sine value
	float32_t cosine; //variable to store the calculated cosine value
	arm_sin_cos_f32(sine_wave_angle, &sine, &cosine); //calculate sine and cosine from the provided angle using a lookup table and interpolation
	float32_t u_phase = sine; //u phase has no phase shift so use the calculated sine value
	float32_t v_phase = (-0.866025403784438646763723 * cosine - 0.5 * sine); //calculate the v phase with -120 degrees phase shift using the calculated sine and cosine values
	float32_t w_phase = (0.866025403784438646763723 * cosine - 0.5 * sine); //calculate the w phase with +120 degrees phase shift using the calculated sine and cosine values

	float32_t carrier = 0; //variable to store the current value of the carrier wave

	uint32_t pwm_u = 0; //variables to store the calculated output state of the pwm for each phase
	uint32_t pwm_v = 0;
	uint32_t pwm_w = 0;

	switch (current_pwm_mode){

	case ASYNC_SPWM:

		carrier = -Triangle_Wave(carrier_wave_angle);

		pwm_u = modulation_index * u_phase >= carrier ? 1 : 0;
		pwm_v = modulation_index * v_phase >= carrier ? 1 : 0;
		pwm_w = modulation_index * w_phase >= carrier ? 1 : 0;

		break;

	case SYNC_SPWM:

		carrier = -Triangle_Wave(pwm_frequency * sine_wave_angle);

		pwm_u = modulation_index * u_phase >= carrier ? 1 : 0;
		pwm_v = modulation_index * v_phase >= carrier ? 1 : 0;
		pwm_w = modulation_index * w_phase >= carrier ? 1 : 0;

		break;

	case SHIFTED_SYNC_SPWM:

		carrier = Triangle_Wave(pwm_frequency * sine_wave_angle);

		pwm_u = modulation_index * u_phase >= carrier ? 1 : 0;
		pwm_v = modulation_index * v_phase >= carrier ? 1 : 0;
		pwm_w = modulation_index * w_phase >= carrier ? 1 : 0;

		break;

	case SQUAREWAVE_SYNC:

		carrier = Center60_Modulation(sine_wave_angle, (pwm_frequency-1)*0.5);

		pwm_u = modulation_index * u_phase >= carrier ? 1 : 0;
		pwm_v = modulation_index * v_phase >= carrier ? 1 : 0;
		pwm_w = modulation_index * w_phase >= carrier ? 1 : 0;

		break;

	case WIDE_3P:

		pwm_u = get_P_with_switchingangle(
			_WideAlpha[(int)(500 * modulation_index) + 1] * __PI_180,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			'B', sine_wave_angle * __PI_180);

		pwm_v = get_P_with_switchingangle(
			_WideAlpha[(int)(500 * modulation_index) + 1] * __PI_180,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			'B', (sine_wave_angle *__PI_180) + __PI_43);

		pwm_w = get_P_with_switchingangle(
			_WideAlpha[(int)(500 * modulation_index) + 1] * __PI_180,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			'B', (sine_wave_angle *__PI_180) + __PI_23);


	break;

	case ONE_P:

		pwm_u = Square_Wave(sine_wave_angle, 360);
		pwm_v = Square_Wave(sine_wave_angle + 240, 360);
		pwm_w = Square_Wave(sine_wave_angle + 120, 360);

		break;

	case CHM:

		switch ((int)pwm_frequency){
			case 15:

				array_l = (int)(1000 * modulation_index) + 1;
				pwm_u = get_P_with_switchingangle(
					_7Alpha[array_l][0] * __PI_180,
					_7Alpha[array_l][1] * __PI_180,
					_7Alpha[array_l][2] * __PI_180,
					_7Alpha[array_l][3] * __PI_180,
					_7Alpha[array_l][4] * __PI_180,
					_7Alpha[array_l][5] * __PI_180,
					_7Alpha[array_l][6] * __PI_180,
					_7Alpha_Polary[array_l], sine_wave_angle * __PI_180);

				pwm_v = get_P_with_switchingangle(
					_7Alpha[array_l][0] * __PI_180,
					_7Alpha[array_l][1] * __PI_180,
					_7Alpha[array_l][2] * __PI_180,
					_7Alpha[array_l][3] * __PI_180,
					_7Alpha[array_l][4] * __PI_180,
					_7Alpha[array_l][5] * __PI_180,
					_7Alpha[array_l][6] * __PI_180,
					_7Alpha_Polary[array_l], (sine_wave_angle *__PI_180) + __PI_43);

				pwm_w = get_P_with_switchingangle(
					_7Alpha[array_l][0] * __PI_180,
					_7Alpha[array_l][1] * __PI_180,
					_7Alpha[array_l][2] * __PI_180,
					_7Alpha[array_l][3] * __PI_180,
					_7Alpha[array_l][4] * __PI_180,
					_7Alpha[array_l][5] * __PI_180,
					_7Alpha[array_l][6] * __PI_180,
					_7Alpha_Polary[array_l], (sine_wave_angle *__PI_180) + __PI_23);

				break;

			case 13:

				array_l = (int)(1000 * modulation_index) + 1;
				pwm_u = get_P_with_switchingangle(
					_6Alpha[array_l][0] * __PI_180,
					_6Alpha[array_l][1] * __PI_180,
					_6Alpha[array_l][2] * __PI_180,
					_6Alpha[array_l][3] * __PI_180,
					_6Alpha[array_l][4] * __PI_180,
					_6Alpha[array_l][5] * __PI_180,
					__PI_2,
					_6Alpha_Polary[array_l], sine_wave_angle * __PI_180);

				pwm_v = get_P_with_switchingangle(
					_6Alpha[array_l][0] * __PI_180,
					_6Alpha[array_l][1] * __PI_180,
					_6Alpha[array_l][2] * __PI_180,
					_6Alpha[array_l][3] * __PI_180,
					_6Alpha[array_l][4] * __PI_180,
					_6Alpha[array_l][5] * __PI_180,
					__PI_2,
					_6Alpha_Polary[array_l], (sine_wave_angle *__PI_180) + __PI_43);

				pwm_w = get_P_with_switchingangle(
					_6Alpha[array_l][0] * __PI_180,
					_6Alpha[array_l][1] * __PI_180,
					_6Alpha[array_l][2] * __PI_180,
					_6Alpha[array_l][3] * __PI_180,
					_6Alpha[array_l][4] * __PI_180,
					_6Alpha[array_l][5] * __PI_180,
					__PI_2,
					_6Alpha_Polary[array_l], (sine_wave_angle *__PI_180) + __PI_23);

				break;

			case 11:

				array_l = (int)(1000 * modulation_index) + 1;
				pwm_u = get_P_with_switchingangle(
					_5Alpha[array_l][0] * __PI_180,
					_5Alpha[array_l][1] * __PI_180,
					_5Alpha[array_l][2] * __PI_180,
					_5Alpha[array_l][3] * __PI_180,
					_5Alpha[array_l][4] * __PI_180,
					__PI_2,
					__PI_2,
					_5Alpha_Polary[array_l], sine_wave_angle * __PI_180);

				pwm_v = get_P_with_switchingangle(
					_5Alpha[array_l][0] * __PI_180,
					_5Alpha[array_l][1] * __PI_180,
					_5Alpha[array_l][2] * __PI_180,
					_5Alpha[array_l][3] * __PI_180,
					_5Alpha[array_l][4] * __PI_180,
					__PI_2,
					__PI_2,
					_5Alpha_Polary[array_l], (sine_wave_angle *__PI_180) + __PI_43);

				pwm_w = get_P_with_switchingangle(
					_5Alpha[array_l][0] * __PI_180,
					_5Alpha[array_l][1] * __PI_180,
					_5Alpha[array_l][2] * __PI_180,
					_5Alpha[array_l][3] * __PI_180,
					_5Alpha[array_l][4] * __PI_180,
					__PI_2,
					__PI_2,
					_5Alpha_Polary[array_l], (sine_wave_angle *__PI_180) + __PI_23);

				break;

			case 9:

				array_l = (int)(1000 * modulation_index) + 1;
				pwm_u = get_P_with_switchingangle(
					_4Alpha[array_l][0] * __PI_180,
					_4Alpha[array_l][1] * __PI_180,
					_4Alpha[array_l][2] * __PI_180,
					_4Alpha[array_l][3] * __PI_180,
					__PI_2,
					__PI_2,
					__PI_2,
					_4Alpha_Polary[array_l], sine_wave_angle *__PI_180);

				pwm_v = get_P_with_switchingangle(
					_4Alpha[array_l][0] * __PI_180,
					_4Alpha[array_l][1] * __PI_180,
					_4Alpha[array_l][2] * __PI_180,
					_4Alpha[array_l][3] * __PI_180,
					__PI_2,
					__PI_2,
					__PI_2,
					_4Alpha_Polary[array_l], (sine_wave_angle *__PI_180) + __PI_43);

				pwm_w = get_P_with_switchingangle(
					_4Alpha[array_l][0] * __PI_180,
					_4Alpha[array_l][1] * __PI_180,
					_4Alpha[array_l][2] * __PI_180,
					_4Alpha[array_l][3] * __PI_180,
					__PI_2,
					__PI_2,
					__PI_2,
					_4Alpha_Polary[array_l], (sine_wave_angle *__PI_180) + __PI_23);

				break;

			case 7:

				array_l = (int)(1000 * modulation_index) + 1;
				pwm_u = get_P_with_switchingangle(
					_3Alpha[array_l][0] * __PI_180,
					_3Alpha[array_l][1] * __PI_180,
					_3Alpha[array_l][2] * __PI_180,
					__PI_2,
					__PI_2,
					__PI_2,
					__PI_2,
					_3Alpha_Polary[array_l], sine_wave_angle * __PI_180);

				pwm_v = get_P_with_switchingangle(
					_3Alpha[array_l][0] * __PI_180,
					_3Alpha[array_l][1] * __PI_180,
					_3Alpha[array_l][2] * __PI_180,
					__PI_2,
					__PI_2,
					__PI_2,
					__PI_2,
					_3Alpha_Polary[array_l], (sine_wave_angle *__PI_180) + __PI_43);

				pwm_w = get_P_with_switchingangle(
					_3Alpha[array_l][0] * __PI_180,
					_3Alpha[array_l][1] * __PI_180,
					_3Alpha[array_l][2] * __PI_180,
					__PI_2,
					__PI_2,
					__PI_2,
					__PI_2,
					_3Alpha_Polary[array_l], (sine_wave_angle *__PI_180) + __PI_23);

				break;

			case 5:

				array_l = (int)(1000 * modulation_index) + 1;
				pwm_u = get_P_with_switchingangle(
					_2Alpha[array_l][0] * __PI_180,
					_2Alpha[array_l][1] * __PI_180,
					__PI_2,
					__PI_2,
					__PI_2,
					__PI_2,
					__PI_2,
					_2Alpha_Polary[array_l], sine_wave_angle * __PI_180);

				pwm_v = get_P_with_switchingangle(
					_2Alpha[array_l][0] * __PI_180,
					_2Alpha[array_l][1] * __PI_180,
					__PI_2,
					__PI_2,
					__PI_2,
					__PI_2,
					__PI_2,
					_2Alpha_Polary[array_l], (sine_wave_angle *__PI_180) + __PI_43);

				pwm_w = get_P_with_switchingangle(
					_2Alpha[array_l][0] * __PI_180,
					_2Alpha[array_l][1] * __PI_180,
					__PI_2,
					__PI_2,
					__PI_2,
					__PI_2,
					__PI_2,
					_2Alpha_Polary[array_l], (sine_wave_angle *__PI_180) + __PI_23);

				break;
		}
		break;

	case WIDE_CHM:

	    switch((int)pwm_frequency){
		case 15:

		array_l = (int)(1000 * modulation_index) - 999;
		pwm_u =  get_P_with_switchingangle(
			_7WideAlpha[array_l][0] * __PI_180,
			_7WideAlpha[array_l][1] * __PI_180,
			_7WideAlpha[array_l][2] * __PI_180,
			_7WideAlpha[array_l][3] * __PI_180,
			_7WideAlpha[array_l][4] * __PI_180,
			_7WideAlpha[array_l][5] * __PI_180,
			_7WideAlpha[array_l][6] * __PI_180,
			'B', sine_wave_angle * __PI_180);

		pwm_v =  get_P_with_switchingangle(
			_7WideAlpha[array_l][0] * __PI_180,
			_7WideAlpha[array_l][1] * __PI_180,
			_7WideAlpha[array_l][2] * __PI_180,
			_7WideAlpha[array_l][3] * __PI_180,
			_7WideAlpha[array_l][4] * __PI_180,
			_7WideAlpha[array_l][5] * __PI_180,
			_7WideAlpha[array_l][6] * __PI_180,
			'B', (sine_wave_angle *__PI_180) + __PI_43);

		pwm_w =  get_P_with_switchingangle(
			_7WideAlpha[array_l][0] * __PI_180,
			_7WideAlpha[array_l][1] * __PI_180,
			_7WideAlpha[array_l][2] * __PI_180,
			_7WideAlpha[array_l][3] * __PI_180,
			_7WideAlpha[array_l][4] * __PI_180,
			_7WideAlpha[array_l][5] * __PI_180,
			_7WideAlpha[array_l][6] * __PI_180,
			'B', (sine_wave_angle *__PI_180) + __PI_23);

		break;

	case 13:

		array_l = (int)(1000 * modulation_index) - 999;
		pwm_u =  get_P_with_switchingangle(
			_6WideAlpha[array_l][0] * __PI_180,
			_6WideAlpha[array_l][1] * __PI_180,
			_6WideAlpha[array_l][2] * __PI_180,
			_6WideAlpha[array_l][3] * __PI_180,
			_6WideAlpha[array_l][4] * __PI_180,
			_6WideAlpha[array_l][5] * __PI_180,
			__PI_2,
			'A', sine_wave_angle * __PI_180);

		pwm_v =  get_P_with_switchingangle(
			_6WideAlpha[array_l][0] * __PI_180,
			_6WideAlpha[array_l][1] * __PI_180,
			_6WideAlpha[array_l][2] * __PI_180,
			_6WideAlpha[array_l][3] * __PI_180,
			_6WideAlpha[array_l][4] * __PI_180,
			_6WideAlpha[array_l][5] * __PI_180,
			__PI_2,
			'A', (sine_wave_angle *__PI_180) + __PI_43);

		pwm_w =  get_P_with_switchingangle(
			_6WideAlpha[array_l][0] * __PI_180,
			_6WideAlpha[array_l][1] * __PI_180,
			_6WideAlpha[array_l][2] * __PI_180,
			_6WideAlpha[array_l][3] * __PI_180,
			_6WideAlpha[array_l][4] * __PI_180,
			_6WideAlpha[array_l][5] * __PI_180,
			__PI_2,
			'A', (sine_wave_angle *__PI_180) + __PI_23);

		break;

	case 11:

		array_l = (int)(1000 * modulation_index) - 999;
		pwm_u =  get_P_with_switchingangle(
			_5WideAlpha[array_l][0] * __PI_180,
			_5WideAlpha[array_l][1] * __PI_180,
			_5WideAlpha[array_l][2] * __PI_180,
			_5WideAlpha[array_l][3] * __PI_180,
			_5WideAlpha[array_l][4] * __PI_180,
			__PI_2,
			__PI_2,
			'B', sine_wave_angle * __PI_180);

		pwm_v =  get_P_with_switchingangle(
			_5WideAlpha[array_l][0] * __PI_180,
			_5WideAlpha[array_l][1] * __PI_180,
			_5WideAlpha[array_l][2] * __PI_180,
			_5WideAlpha[array_l][3] * __PI_180,
			_5WideAlpha[array_l][4] * __PI_180,
			__PI_2,
			__PI_2,
			'B', (sine_wave_angle *__PI_180) + __PI_43);

		pwm_w =  get_P_with_switchingangle(
			_5WideAlpha[array_l][0] * __PI_180,
			_5WideAlpha[array_l][1] * __PI_180,
			_5WideAlpha[array_l][2] * __PI_180,
			_5WideAlpha[array_l][3] * __PI_180,
			_5WideAlpha[array_l][4] * __PI_180,
			__PI_2,
			__PI_2,
			'B', (sine_wave_angle *__PI_180) + __PI_23);

		break;

	case 9:

		array_l = (int)(1000 * modulation_index) - 799;
		pwm_u =  get_P_with_switchingangle(
			_4WideAlpha[array_l][0] * __PI_180,
			_4WideAlpha[array_l][1] * __PI_180,
			_4WideAlpha[array_l][2] * __PI_180,
			_4WideAlpha[array_l][3] * __PI_180,
			__PI_2,
			__PI_2,
			__PI_2,
			'B', sine_wave_angle * __PI_180);

		pwm_v =  get_P_with_switchingangle(
				_4WideAlpha[array_l][0] * __PI_180,
				_4WideAlpha[array_l][1] * __PI_180,
				_4WideAlpha[array_l][2] * __PI_180,
				_4WideAlpha[array_l][3] * __PI_180,
				__PI_2,
				__PI_2,
				__PI_2,
			'A', (sine_wave_angle *__PI_180) + __PI_43);

		pwm_w =  get_P_with_switchingangle(
				_4WideAlpha[array_l][0] * __PI_180,
				_4WideAlpha[array_l][1] * __PI_180,
				_4WideAlpha[array_l][2] * __PI_180,
				_4WideAlpha[array_l][3] * __PI_180,
				__PI_2,
				__PI_2,
				__PI_2,
			'A', (sine_wave_angle *__PI_180) + __PI_23);

		break;

	case 7:

		array_l = (int)(1000 * modulation_index) - 799;
		pwm_u =  get_P_with_switchingangle(
			_3WideAlpha[array_l][0] * __PI_180,
			_3WideAlpha[array_l][1] * __PI_180,
			_3WideAlpha[array_l][2] * __PI_180,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			'B', sine_wave_angle * __PI_180);

		pwm_v =  get_P_with_switchingangle(
				_3WideAlpha[array_l][0] * __PI_180,
				_3WideAlpha[array_l][1] * __PI_180,
				_3WideAlpha[array_l][2] * __PI_180,
				__PI_2,
				__PI_2,
				__PI_2,
				__PI_2,
			'B', (sine_wave_angle *__PI_180) + __PI_43);

		pwm_w =  get_P_with_switchingangle(
				_3WideAlpha[array_l][0] * __PI_180,
				_3WideAlpha[array_l][1] * __PI_180,
				_3WideAlpha[array_l][2] * __PI_180,
				__PI_2,
				__PI_2,
				__PI_2,
				__PI_2,
			'B', (sine_wave_angle *__PI_180) + __PI_23);

		break;

	case 5:

		array_l = (int)(1000 * modulation_index) - 799;
		pwm_u =  get_P_with_switchingangle(
			_2WideAlpha[array_l][0] * __PI_180,
			_2WideAlpha[array_l][1] * __PI_180,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			'A', sine_wave_angle * __PI_180);

		pwm_v =  get_P_with_switchingangle(
			_2WideAlpha[array_l][0] * __PI_180,
			_2WideAlpha[array_l][1] * __PI_180,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			'A', (sine_wave_angle *__PI_180) + __PI_43);

		pwm_w =  get_P_with_switchingangle(
			_2WideAlpha[array_l][0] * __PI_180,
			_2WideAlpha[array_l][1] * __PI_180,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			__PI_2,
			'A', (sine_wave_angle *__PI_180) + __PI_23);

			break;
	}

		break;

    case WIDE_CHM_STEPPED:

        switch((int)pwm_frequency)

        {
            case 5:
                array_l = (int)(1000 * modulation_index) - 799;
                pwm_u =  get_P_with_switchingangle(
                	_Stepped_2WideAlpha[array_l][0] * __PI_180,
                    _Stepped_2WideAlpha[array_l][1] * __PI_180,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    'A', sine_wave_angle * __PI_180);

                pwm_v =  get_P_with_switchingangle(
                    _Stepped_2WideAlpha[array_l][0] * __PI_180,
                    _Stepped_2WideAlpha[array_l][1] * __PI_180,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    'A', (sine_wave_angle *__PI_180) + __PI_43);

                pwm_w =  get_P_with_switchingangle(
                    _Stepped_2WideAlpha[array_l][0] * __PI_180,
                    _Stepped_2WideAlpha[array_l][1] * __PI_180,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    'A', (sine_wave_angle *__PI_180) + __PI_23);

                break;

            case 3:
                array_l = (int)(500 * modulation_index) + 1;

                pwm_u =  get_P_with_switchingangle(
                    _Stepped_WideAlpha[array_l] * __PI_180,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    'B', sine_wave_angle * __PI_180);

                pwm_v =  get_P_with_switchingangle(
                    _Stepped_WideAlpha[array_l] * __PI_180,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    'B', (sine_wave_angle *__PI_180) + __PI_43);

                pwm_w =  get_P_with_switchingangle(
                    _Stepped_WideAlpha[array_l] * __PI_180,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    __PI_2,
                    'B', (sine_wave_angle *__PI_180) + __PI_23);

                break;

        }

        break;

	case SHE:

		switch((int)pwm_frequency)
		{
		case 3:
			pwm_u = get_P_with_switchingangle(
				_1Alpha_SHE[(int)(1000 * modulation_index) + 1] * __PI_180,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				'B', sine_wave_angle * __PI_180);
			pwm_v = get_P_with_switchingangle(
				_1Alpha_SHE[(int)(1000 * modulation_index) + 1] * __PI_180,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				'B', sine_wave_angle *__PI_180 + __PI_43);
			pwm_w = get_P_with_switchingangle(
				_1Alpha_SHE[(int)(1000 * modulation_index) + 1] * __PI_180,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				'B', sine_wave_angle *__PI_180 + __PI_23);
			break;

		case 5:
			array_l = (int)(1000 * modulation_index) + 1;
			pwm_u = get_P_with_switchingangle(
				_2Alpha_SHE[array_l][0] * __PI_180,
				_2Alpha_SHE[array_l][1] * __PI_180,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				'A', sine_wave_angle * __PI_180);
			pwm_v = get_P_with_switchingangle(
				_2Alpha_SHE[array_l][0] * __PI_180,
				_2Alpha_SHE[array_l][1] * __PI_180,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				'A', sine_wave_angle *__PI_180 + __PI_43);
			pwm_w = get_P_with_switchingangle(
				_2Alpha_SHE[array_l][0] * __PI_180,
				_2Alpha_SHE[array_l][1] * __PI_180,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				'A', sine_wave_angle *__PI_180 + __PI_23);
			break;

		case 7:
			array_l = (int)(1000 * modulation_index) + 1;
			pwm_u = get_P_with_switchingangle(
				_3Alpha_SHE[array_l][0] * __PI_180,
				_3Alpha_SHE[array_l][1] * __PI_180,
				_3Alpha_SHE[array_l][2] * __PI_180,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				'B', sine_wave_angle * __PI_180);
			pwm_v = get_P_with_switchingangle(
				_3Alpha_SHE[array_l][0] * __PI_180,
				_3Alpha_SHE[array_l][1] * __PI_180,
				_3Alpha_SHE[array_l][2] * __PI_180,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				'B', sine_wave_angle *__PI_180 + __PI_43);
			pwm_w = get_P_with_switchingangle(
				_3Alpha_SHE[array_l][0] * __PI_180,
				_3Alpha_SHE[array_l][1] * __PI_180,
				_3Alpha_SHE[array_l][2] * __PI_180,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				M_PI_2,
				'B', sine_wave_angle *__PI_180 + __PI_23);
			break;

		case 11:
			array_l = (int)(1000 * modulation_index) + 1;
			pwm_u = get_P_with_switchingangle(
				_5Alpha_SHE[array_l][0] * __PI_180,
				_5Alpha_SHE[array_l][1] * __PI_180,
				_5Alpha_SHE[array_l][2] * __PI_180,
				_5Alpha_SHE[array_l][3] * __PI_180,
				_5Alpha_SHE[array_l][4] * __PI_180,
				M_PI_2,
				M_PI_2,
				'A', sine_wave_angle * __PI_180);
			pwm_v = get_P_with_switchingangle(
				_5Alpha_SHE[array_l][0] * __PI_180,
				_5Alpha_SHE[array_l][1] * __PI_180,
				_5Alpha_SHE[array_l][2] * __PI_180,
				_5Alpha_SHE[array_l][3] * __PI_180,
				_5Alpha_SHE[array_l][4] * __PI_180,
				M_PI_2,
				M_PI_2,
				'A', sine_wave_angle *__PI_180 + __PI_43);
			pwm_w = get_P_with_switchingangle(
				_5Alpha_SHE[array_l][0] * __PI_180,
				_5Alpha_SHE[array_l][1] * __PI_180,
				_5Alpha_SHE[array_l][2] * __PI_180,
				_5Alpha_SHE[array_l][3] * __PI_180,
				_5Alpha_SHE[array_l][4] * __PI_180,
				M_PI_2,
				M_PI_2,
				'A', sine_wave_angle *__PI_180 + __PI_23);
			break;
			}

		break;

	default:

		break;

	}

	TIM1->CCR1 = pwm_u * pwm_max_output_value;
	TIM8->CCR1 = pwm_u * pwm_max_output_value;

	TIM1->CCR2 = pwm_v * pwm_max_output_value;
	TIM8->CCR2 = pwm_v * pwm_max_output_value;

	TIM1->CCR3 = pwm_w * pwm_max_output_value;
	TIM8->CCR3 = pwm_w * pwm_max_output_value;

}

*/

void calculate_3_level (void)
{
	float32_t sine; //variable to store the calculated sine value
	float32_t cosine; //variable to store the calculated cosine value
	arm_sin_cos_f32(sine_wave_angle, &sine, &cosine); //calculate sine and cosine from the provided angle using a lookup table and interpolation
	float32_t u_phase = sine; //u phase has no phase shift so use the calculated sine value
	float32_t v_phase = (-0.866025403784438646763723 * cosine - 0.5 * sine); //calculate the v phase with -120 degrees phase shift using the calculated sine and cosine values
	float32_t w_phase = (0.866025403784438646763723 * cosine - 0.5 * sine); //calculate the w phase with +120 degrees phase shift using the calculated sine and cosine values

	float32_t carrier = 0; //variable to store the current value of the carrier wave

	uint32_t pwm_u_h = 0; //variables to store the calculated output state of the pwm for each phase
	uint32_t pwm_u_l = 0;
	uint32_t pwm_v_h = 0;
	uint32_t pwm_v_l = 0;
	uint32_t pwm_w_h = 0;
	uint32_t pwm_w_l = 0;

	switch (current_pwm_mode){

	case ASYNC_SPWM:

		carrier = -0.5 * Triangle_Wave(carrier_wave_angle);

		pwm_u_h = modulation_index * u_phase >= (carrier + 0.5) ? 1 : 0;
		pwm_u_l = modulation_index * u_phase >= (carrier - 0.5) ? 1 : 0;

		pwm_v_h = modulation_index * v_phase >= (carrier + 0.5) ? 1 : 0;
		pwm_v_l = modulation_index * v_phase >= (carrier - 0.5) ? 1 : 0;

		pwm_w_h = modulation_index * w_phase >= (carrier + 0.5) ? 1 : 0;
		pwm_w_l = modulation_index * w_phase >= (carrier - 0.5) ? 1 : 0;

		break;

	case SYNC_SPWM:

		carrier = -0.5 * Triangle_Wave(pwm_frequency * sine_wave_angle);

		pwm_u_h = modulation_index * u_phase >= (carrier + 0.5) ? 1 : 0;
		pwm_u_l = modulation_index * u_phase >= (carrier - 0.5) ? 1 : 0;

		pwm_v_h = modulation_index * v_phase >= (carrier + 0.5) ? 1 : 0;
		pwm_v_l = modulation_index * v_phase >= (carrier - 0.5) ? 1 : 0;

		pwm_w_h = modulation_index * w_phase >= (carrier + 0.5) ? 1 : 0;
		pwm_w_l = modulation_index * w_phase >= (carrier - 0.5) ? 1 : 0;

		break;

	case SHIFTED_SYNC_SPWM:

		carrier = 0.5 * Triangle_Wave(pwm_frequency * sine_wave_angle);

		pwm_u_h = modulation_index * u_phase >= (carrier + 0.5) ? 1 : 0;
		pwm_u_l = modulation_index * u_phase >= (carrier - 0.5) ? 1 : 0;

		pwm_v_h = modulation_index * v_phase >= (carrier + 0.5) ? 1 : 0;
		pwm_v_l = modulation_index * v_phase >= (carrier - 0.5) ? 1 : 0;

		pwm_w_h = modulation_index * w_phase >= (carrier + 0.5) ? 1 : 0;
		pwm_w_l = modulation_index * w_phase >= (carrier - 0.5) ? 1 : 0;

		break;

	}

	TIM1->CCR1 = pwm_u_h * pwm_max_output_value;
	TIM8->CCR1 = pwm_u_l * pwm_max_output_value;

	TIM1->CCR2 = pwm_v_h * pwm_max_output_value;
	TIM8->CCR2 = pwm_v_l * pwm_max_output_value;

	TIM1->CCR3 = pwm_w_h * pwm_max_output_value;
	TIM8->CCR3 = pwm_w_l * pwm_max_output_value;

}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{

	if(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
	{
		current_motor_mode = accelerating;
		target_frequency += 0.00004;
		if(target_frequency >= 90) target_frequency = 90;
	}
	else
	{
		current_motor_mode = braking;
		target_frequency -= 0.00004;
		if(target_frequency <= 1) target_frequency = 1;
	}

	/*
	target_frequency += (0.00004 * inc);
	if(target_frequency >= 75){
		inc = -1;
		//current_motor_mode = braking;
*/


	sound_data();
	calculate_angle();

}

void sound_data (void) //207 ptr
{


	if(output_frequency >= 2)
	{
		output_frequency = target_frequency;
	}
	else
	{
		output_frequency = 2;
	}

	v_hz_ratio = 0.02273214285714285714285714285714;

	if (output_frequency >= 40)
	{
		current_pwm_mode = SYNC_SPWM;
		pwm_frequency = 3;
		v_hz_ratio = 0.01785714285714285714285714285714;
	}
	else if (output_frequency >= 28)
	{
		current_pwm_mode = SYNC_SPWM;
		pwm_frequency = 9;
	}
	else if (output_frequency >= 15)
	{
		current_pwm_mode = SYNC_SPWM;
		pwm_frequency = 21;
	}
	else if (output_frequency >= 8)
	{
		current_pwm_mode = SYNC_SPWM;
		pwm_frequency = 33;
	}
	else
	{
		current_pwm_mode = SYNC_SPWM;
		pwm_frequency = 57;
	}

	modulation_index = v_hz_ratio * output_frequency;

}

/*

void sound_data (void) //nagoya 2000 gto
{

	output_frequency = target_frequency;

	v_hz_ratio = 0.0404203030075;

	if (output_frequency >= 31.5)
	{
		current_pwm_mode = SIX_STEP;
	}
	else if (output_frequency >= 30.7125)
	{
		current_pwm_mode = WIDE_3P;
	}
	else if (output_frequency >= 28.35)
	{
		current_pwm_mode = QUASI_SIX_STEP;
		pwm_frequency = 3;
		v_hz_ratio = 0.031746031746;
	}
	else if (output_frequency >= 22.05)
	{
		current_pwm_mode = QUASI_SIX_STEP;
		pwm_frequency = 5;
		v_hz_ratio = 0.031746031746;
	}
	else if (output_frequency >= 18.9)
	{
		current_pwm_mode = SYNC_SPWM;
		pwm_frequency = 9;
	}
	else if (output_frequency >= 16.667)
	{
		current_pwm_mode = SYNC_SPWM;
		pwm_frequency = 15;
	}
	else
	{
		current_pwm_mode = ASYNC_SPWM;
		pwm_frequency = 250;
	}

	modulation_index = v_hz_ratio * output_frequency;

}

void sound_data (void) //alstom
{

	v_hz_ratio = 0.018;

	if (output_frequency >= 70)
	{
		current_pwm_mode = SIX_STEP;
	}
	else if (output_frequency >= 64)
	{
		current_pwm_mode = SHE;
		pwm_frequency = 3;
	}
	else if (output_frequency >= 57)
	{
		current_pwm_mode = SHE;
		pwm_frequency = 5;
	}
	else if (output_frequency >= 52)
	{
		current_pwm_mode = SHE;
		pwm_frequency = 7;
	}
	else if (output_frequency >= 35.7)
	{
		current_pwm_mode = SHE;
		pwm_frequency = 11;
	}
	else if (output_frequency >= 27.5)
	{
		current_pwm_mode = SYNC_SPWM;
		pwm_frequency = 15;
	}
	else if (output_frequency >= 11.3)
	{
		current_pwm_mode = SYNC_SPWM;
		pwm_frequency = 21;
	}
	else
	{
		current_pwm_mode = ASYNC_SPWM;
		pwm_frequency = 200;
	}

	modulation_index = v_hz_ratio * output_frequency;

}

void sound_data (void) E231 Hitachi
{

	v_hz_ratio = 0.02121666666666666666666666666667;

	if (output_frequency >= 58)
	{
		current_pwm_mode = SIX_STEP;
	}
	else if (output_frequency >= 50)
	{
		current_pwm_mode = WIDE_3P;
	}
	else if (output_frequency >= 40)
	{
		current_pwm_mode = ASYNC_SPWM;
		pwm_frequency = calculate_variable_pwm_frequency(700,40,1800,50);
	}
	else if (output_frequency >= 20)
	{
		current_pwm_mode = ASYNC_SPWM;
		pwm_frequency = calculate_variable_pwm_frequency(1050,20,700,40);
	}
	else
	{
		current_pwm_mode = ASYNC_SPWM;
		pwm_frequency = 1050;
	}

	modulation_index = v_hz_ratio * output_frequency;

}


void sound_data(void) //siemens
{
	output_frequency = target_frequency;

	int a = 2, b = 3;
	float k[2][3] = {
		{0.0193294460641, 0.0222656250000, 0},
		{0.014763975813, 0.018464, 0.013504901961},
	};
	float B[2][3] = {
		{0.10000000000, -0.07467187500, 0},
		{0.10000000000, -0.095166, 0.100000000000},
	};

	float amplitude;

	if (current_motor_mode == accelerating)
	{
		if (80 <= output_frequency)
		{
			current_pwm_mode = SIX_STEP;
		}
		else if (59 <= output_frequency)
		{
			a = 1;
			b = 2;
			current_pwm_mode = WIDE_3P;
		}
		else if (57 <= output_frequency)
		{
			a = 1;
			b = 2;
			current_pwm_mode = WIDE_CHM;
			pwm_frequency = 5;
		}
		else if (53.5 <= output_frequency)
		{
			a = 1;
			b = 2;
			current_pwm_mode = WIDE_CHM;
			pwm_frequency = 7;
		}
		else if (43.5 <= output_frequency)
		{
			a = 1;
			b = 1;
			current_pwm_mode = CHM;
			pwm_frequency = 7;
		}
		else if (36.7 <= output_frequency)
		{
			a = 1;
			b = 1;
			current_pwm_mode = CHM;
			pwm_frequency = 9;
		}
		else if (30 <= output_frequency)
		{
			a = 1;
			b = 1;
			current_pwm_mode = CHM;
			pwm_frequency = 11;
		}
		else if (27 <= output_frequency)
		{
			a = 1;
			b = 1;
			current_pwm_mode = CHM;
			pwm_frequency = 13;
		}
		else if (24 <= output_frequency)
		{
			a = 1;
			b = 1;
			current_pwm_mode = CHM;
			pwm_frequency = 15;
		}
		else
		{
			a = 1;
			b = 1;
			pwm_frequency = 400;

			current_pwm_mode = ASYNC_SPWM;
			if (5.6 <= output_frequency)
				pwm_frequency = 400;
			else if (5 <= output_frequency)
				pwm_frequency = 350;
			else if (4.3 <= output_frequency)
				pwm_frequency = 311;
			else if (3.4 <= output_frequency)
				pwm_frequency = 294;
			else if (2.7 <= output_frequency)
				pwm_frequency = 262;
			else if (2.0 <= output_frequency)
				pwm_frequency = 233;
			else if (1.5 <= output_frequency)
				pwm_frequency = 223;
			else if (0.5 <= output_frequency)
				pwm_frequency = 196;
			else
				pwm_frequency = 175;

		}
	}
	else if (current_motor_mode == braking)
	{
		if (79.5 <= output_frequency)
		{
			current_pwm_mode = SIX_STEP;
		}
		else if (70.7 <= output_frequency)
		{
			a = 2;
			b = 2;
			current_pwm_mode = WIDE_3P;
		}
		else if (63.35 <= output_frequency)
		{
			a = 2;
			b = 2;
			current_pwm_mode = WIDE_CHM;
			pwm_frequency = 5;
		}
		else if (56.84 <= output_frequency)
		{
			a = 2;
			b = 2;
			current_pwm_mode = WIDE_CHM;
			pwm_frequency = 7;
		}
		else if (53.5 <= output_frequency)
		{
			a = 2;
			b = 1;
			current_pwm_mode = CHM;
			pwm_frequency = 7;
		}
		else if (41 <= output_frequency)
		{
			a = 2;
			b = 3;
			current_pwm_mode = CHM;
			pwm_frequency = 7;
		}
		else if (34.5 <= output_frequency)
		{
			a = 2;
			b = 3;
			current_pwm_mode = CHM;
			pwm_frequency = 9;
		}
		else if (28.9 <= output_frequency)
		{
			a = 2;
			b = 3;
			current_pwm_mode = CHM;
			pwm_frequency = 11;
		}
		else if (24.9 <= output_frequency)
		{
			a = 2;
			b = 3;
			current_pwm_mode = CHM;
			pwm_frequency = 13;
		}
		else if (22.4 <= output_frequency)
		{
			a = 2;
			b = 3;
			current_pwm_mode = CHM;
			pwm_frequency = 15;
		}
		else
		{
			a = 2;
			b = 3;
			pwm_frequency = 400;
			current_pwm_mode = ASYNC_SPWM;
		}
	}

	amplitude =  ((k[a - 1][b - 1] * output_frequency) + B[a - 1][b - 1]);
	amplitude = amplitude >= 1.25 ? 1.25 : amplitude;

	if (output_frequency == 0)
		amplitude = 0;

	modulation_index = amplitude;
}

*/


#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ssd1306.h"
#include "fonts.h"
#include "eeprom.h" //eeprom emulation library from https://github.com/microtechnics-main/stm32-eeprom-emulation

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart2;

float adc;

int btn1_flag = 0; // left btn
int btn2_flag = 0; // right btn

int btn2_last_state = 0;
int btn2_dur = 0; // btn2 press duration
int btn2_sp_flag; // short press detected
int btn2_lp_flag; // long press detected
int btn2_rise_flag = 0;
int inv_state = 0;


uint32_t adc_zero_val, adc_high_val; // sets during the calibration

// calibration table in range 0-3.3 V / 0-100000 Pa (formatted to float)
float x_cal[70] =
    {
        0.000000, 0.165000, 0.247500, 0.330000, 0.490586, 0.641178, 0.756048, 0.771899, 0.795728, 0.815578, 0.835450, 0.863279, 0.903043, 0.958701, 0.982530, 1.002423, 1.042187, 1.074016, 1.141672, 1.249179, 1.348749, 1.619609, 1.962233, 2.261134, 2.408596, 2.579950, 2.651670, 2.691519, 2.727390, 2.759261, 2.791132, 2.823003, 2.894659, 2.926530, 2.938445, 2.950381, 2.970295, 2.990166, 3.006059, 3.017974, 3.037782, 3.061632, 3.069483, 3.081419, 3.085355, 3.085270, 3.097206, 3.109099, 3.112993, 3.128907, 3.128865, 3.132759, 3.140610, 3.148482, 3.152376, 3.164163, 3.175887, 3.195398, 3.210995, 3.222676, 3.234399, 3.242039, 3.253805, 3.257677, 3.269232, 3.277019, 3.288722, 3.292573, 3.296318, 3.300000
    };

float y_cal[70] =
    {
        0.100000, 0.400000, 0.600000, 1.000000, 2.256220, 5.074364, 9.726596, 10.599673, 11.696064, 12.747622, 13.725196, 15.146896, 17.346120, 21.125771, 23.310944, 24.794148, 28.394086, 31.339410, 38.183390, 50.738388, 64.980503, 125.203718, 276.562082, 520.512820, 709.695243, 1029.341553, 1209.207553, 1318.801300, 1420.692265, 1530.251617, 1648.259843, 1775.368495, 2163.365979, 2330.198008, 2447.746681, 2540.037141, 2668.881849, 2873.549766, 3055.978062, 3210.139281, 3585.000031, 3908.054683, 4257.715098, 4418.249066, 4583.615229, 4812.916657, 4994.384086, 5310.747198, 5645.646505, 5931.235174, 6077.783671, 6461.052815, 7039.134391, 7575.916435, 8053.658844, 9102.635529, 10671.887038, 14139.413380, 17838.714608, 21430.767123, 25125.330477, 30926.318017, 35383.614060, 38076.786554, 49219.220391, 55622.542530, 66012.312876, 71908.974682, 83260.972240, 100000.000000
    };

float linterp(float x, float x1, float y1, float x2, float y2) // linear interpolation, where x - input value, x1 y1 - the beginning, x2 y2 - end
{
    float y = y1 + (y2 - y1) * ((x - x1) / (x2 - x1));
    return y;
}

float convert_p(float voltage, float adc_low, float adc_high)
{
    if (voltage < (adc_low - 50) || voltage > (adc_high + 100))
        return -1; // check if values in range

    float sc_voltage = (voltage - adc_low) * (3.3 / (adc_high - adc_low)); // scaling if the calibration data does not match the table
    if (voltage > adc_high)
        sc_voltage = 3.3; // because the high bound is floating, its better to overwrite too large values, as they are not useful

    float p = 0;
    for (int i = 0; i < 70; i++)
    {
        if (x_cal[i] > sc_voltage)
        {
            p = linterp(sc_voltage, x_cal[i - 1], y_cal[i - 1], x_cal[i], y_cal[i]);
            break;
        }
        if (x_cal[i] == sc_voltage)
        {
            p = y_cal[i];
            break;
        }
    }

    return p;
}

void show_pressure(float p, int units, int gt, int mode) // shows pressure in selected units, gets in torr; mode=0 - show pressure, mode=1 - show setpoint
{
    char disp_bufer[50] = {0};
    char disp_bufer1[20] = {0};
    char units_msg[5];
    float m = 0; // mantiss ?
    int exp = 0; // exponent ?

    float disp_gt[5] = {1.0, 0.67, 1.65, 1.31, 1.12}; // N2 H2 Ar Ne He
    char gas_type[5][3] = {"N2", "H2", "Ar", "Ne", "He"};

    if (p <= 0 && mode == 0) // check if input is correct
    {
        SSD1306_Fill(SSD1306_COLOR_BLACK);
        SSD1306_GotoXY(20, 0);
        sprintf(disp_bufer, "RECAL IS");
        SSD1306_Puts(disp_bufer, &Dot18, SSD1306_COLOR_WHITE);
        SSD1306_GotoXY(30, 16);
        sprintf(disp_bufer, "NEEDED");
        SSD1306_Puts(disp_bufer, &Dot18, SSD1306_COLOR_WHITE);
        SSD1306_UpdateScreen();
        HAL_Delay(300);
        SSD1306_InvertDisplay(1);
        HAL_Delay(300);
        SSD1306_InvertDisplay(0);
        return;
    }

    if(p<=0 && mode == 1)
    {
        sprintf(disp_bufer, "ERROR");
        SSD1306_GotoXY(40, 9);
        SSD1306_Puts(disp_bufer, &Dot18, SSD1306_COLOR_WHITE);
        return;
    }

    p = p * disp_gt[gt]; // true only for low pressure; for high pressure another calibration table is needed (i dont have one)
    switch (units)
    {
    case 0:
        strcpy(units_msg, "torr");
        break;
    case 1:
        p = (p*1.333);
         strcpy(units_msg, "mbar");
        break;
    default:
        break;
    }
    if (p >= 1.0)
    {

        if (p < 9.99)
        {
            memset(disp_bufer, 0, 50);
            sprintf(disp_bufer, "%1.2f", p);
        }

        else if (p < 100)
        {
            memset(disp_bufer, 0, 50);
            sprintf(disp_bufer, "%2.1f", p);
        }

        else
        {
            memset(disp_bufer, 0, 50);
            sprintf(disp_bufer, "%d", (int)p);
        }
        if (mode == 0)
        {
            SSD1306_Fill(SSD1306_COLOR_BLACK);
            SSD1306_GotoXY(0, 6);
            SSD1306_Puts(disp_bufer, &Dot36, SSD1306_COLOR_WHITE);
            SSD1306_GotoXY(0, -1);
            SSD1306_Puts(units_msg, &Dot14, SSD1306_COLOR_WHITE);

            SSD1306_GotoXY(114, -1);
            SSD1306_Puts(gas_type[gt], &Dot14, SSD1306_COLOR_WHITE);
        }
        if (mode == 1)
        {
            if (p<100 || p>1000) SSD1306_GotoXY(43, 9);
            if(p>100 && p<1000) SSD1306_GotoXY(47, 9);
            SSD1306_Puts(disp_bufer, &Dot18, SSD1306_COLOR_WHITE);
        }
    }

    else
    {
        memset(disp_bufer, 0, 50);
        m = p;
        exp = 0;
        while (m < 1)
        {
            m = m * 10.0;
            exp++;
        }
        if (mode == 0)
        {
            sprintf(disp_bufer, "%1.2f", m);
            sprintf(disp_bufer1, "^-%d", exp);
            SSD1306_Fill(SSD1306_COLOR_BLACK);
            SSD1306_GotoXY(0, 6);
            SSD1306_Puts(disp_bufer, &Dot36, SSD1306_COLOR_WHITE);
            SSD1306_GotoXY(45, -1);
            SSD1306_Puts(disp_bufer1, &Dot14, SSD1306_COLOR_WHITE);
            SSD1306_GotoXY(0, -1);
            SSD1306_Puts(units_msg, &Dot14, SSD1306_COLOR_WHITE);
            SSD1306_GotoXY(114, -1);
            SSD1306_Puts(gas_type[gt], &Dot14, SSD1306_COLOR_WHITE);
        }
        if (mode == 1)
        {
            sprintf(disp_bufer, "%1.2f^-%d", m, exp);
            SSD1306_GotoXY(30, 9);
            SSD1306_Puts(disp_bufer, &Dot18, SSD1306_COLOR_WHITE);
        }
    }
}

void set_out_on() { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET); } // remember that the output is inverse
void set_out_off() { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET); }

void my_main()
{
    float voltage = 0;
    char disp_bufer[50] = {0};
    char uart_bufer[100] = {0};
    EEPROM_Init();
    EEPROM_Read(PARAM_1, &adc_zero_val);
    EEPROM_Read(PARAM_2, &adc_high_val);
    if (adc_zero_val==0) adc_zero_val = 1300;
    if (adc_high_val==0) adc_high_val = 2800;
    // some sort of initialization
    set_out_off();

    HAL_TIM_Base_Start_IT(&htim2);
    HAL_ADCEx_Calibration_Start(&hadc1);
    HAL_ADC_Start_IT(&hadc1); // yes it should be here


 	sprintf(uart_bufer, "Micro pirani gauge rev 2.1\r\nCalibration data: ZERO=%d mV, ATM=%d mV\r\n", adc_zero_val, adc_high_val);
 	HAL_UART_Transmit(&huart2, uart_bufer, 80, 20);

    SSD1306_Init();
    SSD1306_Fill(SSD1306_COLOR_WHITE);
    SSD1306_UpdateScreen();
    HAL_Delay(500);
    SSD1306_Clear();
    SSD1306_GotoXY(5, 0);
    SSD1306_Puts("MPI rev 2.1", &Dot18, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(23, 16);
    SSD1306_Puts("IRFC R&D", &Dot18, SSD1306_COLOR_WHITE);
    SSD1306_UpdateScreen();
    HAL_Delay(1500);
    SSD1306_Clear();

    int state = 0;    // for state machine
    int units = 0;    // 0 = torr, 1 = mbar
    int gas_type = 0; // 0 = nitrogren, 1 - hydrogen, 2 = argon, 3 = neon, 4 = helium
    int prev_state = 0;
    int st1 = 0; // flag for one-time actions

    float sp_val = 0.001; // setpoint value
    int sp_dir = 0;       // setpoint > or < than value   0 - less, 1 - more
    int sp_state = 0;     // setpoint state   0 - n/a, 1 - on, 2 - off(inverted)
    int sp_pos = 0;
    float i = 0.1; // speed of increasing value (when button is pressed for too long)

    while (1)
    {

    	HAL_ADC_Start_IT(&hadc1);

        voltage = (3.3 / (float)4096) * (float)(adc + 1); // converts ADC reading to (float)voltage

        float pressure = convert_p(voltage, (adc_zero_val / 1000.0), (adc_high_val / 1000.0)) / 133.3; // converts pressure from voltage to torrs; checks if voltage in range;
                                                                                                       // because calibration table was originally given in pascals, divide by 133.3
        memset(uart_bufer, 0, 100);
        if(pressure>0)
        {
        	if(units==0) sprintf(uart_bufer, "Pressure=%f torr, Voltage=%d mV, Setpoint=%.4f torr\r\n", pressure, (int)(voltage*1000), sp_val);
        	if(units==1) sprintf(uart_bufer, "Pressure=%f mbar, Voltage=%d mV, Setpoint=%.4f mbar\r\n", pressure, (int)(voltage*1000), sp_val);
        }
        else sprintf(uart_bufer, "ERROR: Recalibration is needed\r\n");
     	HAL_UART_Transmit(&huart2, uart_bufer, 80, 80);

     	if (prev_state != state)
            SSD1306_Clear();

        if (sp_dir == 0) // a some of logic for setpoint
        {
            if (pressure < sp_val)
            {
                if (sp_state == 0)
                    set_out_off();
                if (sp_state == 1)
                    set_out_on();
            }
            else
            {
                if (sp_state == 0)
                    set_out_off();
                if (sp_state == 1)
                    set_out_off();
            }
        }

        if (sp_dir == 1)
        {
            if (pressure > sp_val)
            {
                if (sp_state == 0)
                    set_out_off();
                if (sp_state == 1)
                    set_out_on();
            }
            else
            {
                if (sp_state == 0)
                    set_out_off();
                if (sp_state == 1)
                    set_out_off();
            }
        }

        prev_state = state;
        if (state == 0) // standart mode - pressure read
        {

            show_pressure(pressure, units, gas_type, 0); // shows pressure on display
            SSD1306_UpdateScreen();

            if (btn1_flag == 1) // if left btn is pressed - changes the units
            {

                SSD1306_Fill(SSD1306_COLOR_BLACK);

                units++;
                if (units > 1)
                    units = 0;
                btn1_flag = 0;
            }

            if (btn2_sp_flag == 1) // if long_press flag equals 0 - changes the gas type
            {
                gas_type++;
                if (gas_type > 4)
                    gas_type = 0;

                btn2_sp_flag = 0;
            }

            if (btn2_lp_flag == 1) // if long_press flag equals 1 - changes the mode
            {
                state++;
                btn2_lp_flag = 0;
            }
        }

        if (state == 1) // setpoint mode - sets the output on, if P in range
        {
            if (btn1_flag == 1)
            {
                sp_pos++;
                btn1_flag = 0;
            }

            if (sp_pos == 0)
            {
                SSD1306_Fill(SSD1306_COLOR_BLACK);
                SSD1306_GotoXY(17, 10);
                SSD1306_Puts("Setpoint", &Dot18, SSD1306_COLOR_WHITE);
                SSD1306_UpdateScreen();

                if (btn2_sp_flag == 1)
                {
                    btn2_sp_flag = 0;
                    state++;
                }
            }

            if (sp_pos == 1)
            {
                SSD1306_Fill(SSD1306_COLOR_BLACK);
                SSD1306_GotoXY(3, 1);
                if (sp_dir == 0)
                    SSD1306_Puts("<", &Dot36, SSD1306_COLOR_WHITE);
                if (sp_dir == 1)
                    SSD1306_Puts(">", &Dot36, SSD1306_COLOR_WHITE);
                if (sp_dir > 1)
                    sp_dir = 0;

                if (btn2_sp_flag == 1)
                {
                    sp_dir++;
                    btn2_sp_flag = 0;
                }

                show_pressure(sp_val, units, gas_type, 1);
                SSD1306_DrawRectangle(0, 8, 20, 18, SSD1306_COLOR_WHITE);

            }

            if (sp_pos == 2)
            {
                SSD1306_Fill(SSD1306_COLOR_BLACK);
                SSD1306_GotoXY(3, 1);
                if (sp_dir == 0)
                    SSD1306_Puts("<", &Dot36, SSD1306_COLOR_WHITE);
                if (sp_dir == 1)
                    SSD1306_Puts(">", &Dot36, SSD1306_COLOR_WHITE);
                show_pressure(sp_val, units, gas_type, 1);
                SSD1306_DrawRectangle(20, 8, 95, 18, SSD1306_COLOR_WHITE);
                if (btn2_sp_flag == 1)
                {

                    if (sp_val < 1)
                        sp_val += 0.005;
                    if (sp_val < 10 && sp_val > 1)
                        sp_val += 0.5;
                    if (sp_val < 100 && sp_val > 10)
                        sp_val += 1;
                    if (sp_val > 100)
                        sp_val += 10;

                    if(units==0)
                	{
                    	if (sp_val > 750) sp_val = 0.001;
                	}
                    if(units==1)
                    {
                    	if (sp_val > 1000) sp_val = 0.001;
                    }

                    btn2_sp_flag = 0;
                }

                if (btn2_lp_flag == 1)
                {
                    if (sp_val < 0.01) i += 0.005;
                    if (sp_val < 0.1)  i += 0.05;
                    if (sp_val < 10 && sp_val > 0.1) i += 1;
                    if (sp_val > 10) i += 10;
                    if (sp_val > 100) i += 50;
                    sp_val += i;
                    i += 0.001;
                    if (i > 10) i = 10;

                    if(units==0)
                	{
                    	if (sp_val > 750) sp_val = 0.001;
                	}
                    if(units==1)
                    {
                    	if (sp_val > 1000) sp_val = 0.001;
                    }

                }
                if (btn2_lp_flag == 0) i = 0;
            }
            if (sp_pos == 3)
            {
                SSD1306_Fill(SSD1306_COLOR_BLACK);
                if (sp_state == 0)
                {
                    SSD1306_GotoXY(33, 4);
                    SSD1306_Puts("OFF", &Dot36, SSD1306_COLOR_WHITE);
                }
                if (sp_state == 1)
                {
                    SSD1306_GotoXY(41, 4);
                    SSD1306_Puts("ON", &Dot36, SSD1306_COLOR_WHITE);
                }

                if (btn2_sp_flag == 1)
                {
                    btn2_sp_flag = 0;
                    sp_state++;
                }

                if (sp_state > 1) sp_state = 0;
            }
            if (sp_pos > 3) sp_pos = 0;

            SSD1306_UpdateScreen();
        }

        if (state == 2) // calibration mode
        {

            if (st1 == 0)
            {
                SSD1306_Fill(SSD1306_COLOR_BLACK);

                SSD1306_GotoXY(17, 10);
                SSD1306_Puts("Zero set", &Dot18, SSD1306_COLOR_WHITE);
                SSD1306_UpdateScreen();

                if (btn1_flag == 1)
                {
                    st1 = 1;
                    btn1_flag = 0;
                }

                if (btn2_sp_flag == 1)
                {
                    btn2_sp_flag = 0;
                    state++;
                    st1 = 0;
                }
            }

            if (st1 == 1)
            {
            	SSD1306_Fill(SSD1306_COLOR_BLACK);
                SSD1306_GotoXY(0, 0);
                SSD1306_Puts("Hold the right", &Dot14, SSD1306_COLOR_WHITE);
                SSD1306_GotoXY(0, 10);
                SSD1306_Puts("button", &Dot14, SSD1306_COLOR_WHITE);
                SSD1306_UpdateScreen();
                if (btn1_flag == 1)
                {
                    btn1_flag = 0;
                    st1=0;
                }
                if (btn2_lp_flag == 1)  st1 = 2;
            }

            if (st1 == 2)
            {
            	SSD1306_Fill(SSD1306_COLOR_BLACK);
                SSD1306_GotoXY(0, 0);
                SSD1306_Puts("If P < 10^-4 torr", &Dot14, SSD1306_COLOR_WHITE);
                SSD1306_GotoXY(0, 10);
                SSD1306_Puts("press left button", &Dot14, SSD1306_COLOR_WHITE);
                SSD1306_GotoXY(0, 20);
                SSD1306_Puts("to confirm", &Dot14, SSD1306_COLOR_WHITE);
                SSD1306_UpdateScreen();

                if (btn1_flag == 1)
                {
                    btn1_flag = 0;
                    adc_zero_val = (int)((3300 / (float)4096) * (float)(adc + 1));
                    EEPROM_Write(PARAM_1, adc_zero_val); // 1.3
                    st1++;
                    SSD1306_Fill(SSD1306_COLOR_BLACK);
                    SSD1306_GotoXY(0, 0);
                    SSD1306_Puts("Saved", &Dot18, SSD1306_COLOR_WHITE);
                    SSD1306_GotoXY(0, 15);
                    sprintf(disp_bufer, "%d mV", adc_zero_val);
                    SSD1306_Puts(disp_bufer, &Dot18, SSD1306_COLOR_WHITE);
                    SSD1306_UpdateScreen();
                    HAL_Delay(2000);
                }
            }

            if (st1 == 3)
            {
                SSD1306_Fill(SSD1306_COLOR_BLACK);
                SSD1306_GotoXY(0, 0);
                SSD1306_Puts("If P > 750 torr", &Dot14, SSD1306_COLOR_WHITE);
                SSD1306_GotoXY(0, 10);
                SSD1306_Puts("press left button", &Dot14, SSD1306_COLOR_WHITE);
                SSD1306_GotoXY(0, 20);
                SSD1306_Puts("to confirm", &Dot14, SSD1306_COLOR_WHITE);
                SSD1306_UpdateScreen();

                if (btn1_flag == 1)
                {
                    st1 = 0;
                    btn1_flag = 0;
                    adc_high_val = (int)((3300 / (float)4096) * (float)(adc + 1));
                    EEPROM_Write(PARAM_2, adc_high_val); // 2.8
                    SSD1306_Fill(SSD1306_COLOR_BLACK);
                    SSD1306_GotoXY(0, 0);
                    SSD1306_Puts("Saved", &Dot18, SSD1306_COLOR_WHITE);
                    SSD1306_GotoXY(0, 15);
                    sprintf(disp_bufer, "%d mV", adc_high_val);
                    SSD1306_Puts(disp_bufer, &Dot18, SSD1306_COLOR_WHITE);
                    SSD1306_UpdateScreen();
                    HAL_Delay(2000);
                }
            }
        }

        if (state > 2)
        {
            st1 = 0;
            state = 0;
        }
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) // ADC complete interrupt
{
    if (hadc->Instance == ADC1) // check if the interrupt comes from ACD1
    {
        adc = HAL_ADC_GetValue(&hadc1); // read from ADC
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) // GPIO interrupt
{

    if (GPIO_Pin == GPIO_PIN_12) // left button
    {
        HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
        btn1_flag = 1;
        HAL_TIM_Base_Start_IT(&htim1);
    }

    if (GPIO_Pin == GPIO_PIN_13) // right button
    {
        HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
        btn2_flag = 1;
        HAL_TIM_Base_Start_IT(&htim1);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) // timer interrupts
{
    if (htim->Instance == TIM1)
    {
        HAL_TIM_Base_Stop_IT(&htim1);
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_12);
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_13);
        NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
        HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    }

    if (htim->Instance == TIM2)
    {

        int lp_thresh = 500; // duration for long press detection

        int btn2_cur_state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13); // read current state for right button

        if (btn2_cur_state == 0 && (btn2_rise_flag == 1)) // if rise last time, but now zero -- short press detection
        {
            btn2_sp_flag = 1;
            btn2_rise_flag = 0;
        }

        if (btn2_last_state == 1 && btn2_cur_state == 1) btn2_dur++; // if btn still pressed, increase duration
        else
        {
            btn2_dur = 0;
            btn2_lp_flag = 0;
        }
        if (btn2_dur >= 10000) btn2_dur = 0; // duration counter overflow check

        if (btn2_dur >= lp_thresh) // if duration > threshold - detected long press
        {
            btn2_rise_flag = 0;
            btn2_lp_flag = 1;
            btn2_dur = 0;
        }

        else
        {
            if (btn2_cur_state == 1 && (btn2_last_state == 0)) btn2_rise_flag = 1; // if duration < threshold and we detect rise - flag=1 for short press detection
        }

        btn2_last_state = btn2_cur_state; // save last state
    }
}

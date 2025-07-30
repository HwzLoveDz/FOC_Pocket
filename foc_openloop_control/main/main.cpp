/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_simplefoc.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#if CONFIG_SOC_MCPWM_SUPPORTED
#define USING_MCPWM
#endif

#define ADC_UNIT ADC_UNIT_1
#define ADC_ATTEN ADC_ATTEN_DB_11
#define ADC_SAMPLE_NUM 32   // 每次采样32次取平均
#define DEFAULT_VREF 0 // mV,只有ESP32可调,其它芯片从E-fuse获取

#define ADC_CHANNEL_NUM 3
#define ADC_CHANNEL_0 ADC1_CHANNEL_3 // GPIO4
#define ADC_CHANNEL_1 ADC1_CHANNEL_4 // GPIO32
#define ADC_CHANNEL_2 ADC1_CHANNEL_5 // GPIO33

#define CURRENT_SENSE_OFFSET_A -5.0f  // A相补偿，单位uA
#define CURRENT_SENSE_OFFSET_B -3.0f  // B相补偿，单位uA
#define CURRENT_SENSE_OFFSET_C -5.0f  // C相补偿，单位uA
#define CURRENT_SENSE_RATIO_A 11600.0 // A相电流检测比例
#define CURRENT_SENSE_RATIO_B 11500.0 // B相电流检测比例
#define CURRENT_SENSE_RATIO_C 11000.0 // C相电流检测比例
#define RREF 2495.0                   // 4.99k/2欧
#define VREF 3.3                      // 参考电压，单位V
// #define VREF_ZERO           1.625 // 静止时实际测得的VSOUT电压（单位V）
static esp_adc_cal_characteristics_t adc_chars;
float vref_zero[ADC_CHANNEL_NUM] = {0}; // 三相静止时VSOUT校准值（单位V）

#define ILOAD_AVG_WINDOW 4 // 电流均值滤波窗口大小
float iload_buffer[ADC_CHANNEL_NUM][ILOAD_AVG_WINDOW] = {{0}};
int iload_index[ADC_CHANNEL_NUM] = {0};
int iload_count[ADC_CHANNEL_NUM] = {0};

void hall_phase_current_adc_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL_0, ADC_ATTEN);
    adc1_config_channel_atten(ADC_CHANNEL_1, ADC_ATTEN);
    adc1_config_channel_atten(ADC_CHANNEL_2, ADC_ATTEN);
    esp_adc_cal_characterize(ADC_UNIT, ADC_ATTEN, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);
}

BLDCMotor motor = BLDCMotor(14 / 2, 6.5 / 2, 330, (1.2333e-3) / 2);
BLDCDriver6PWM driver = BLDCDriver6PWM(11, 14, 12, 15, 13, 16, 17, 0); /*!< Create BLDCDriver6PWM object with pin numbers */
MA600A ma600a = MA600A(SPI2_HOST, GPIO_NUM_21, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9);

extern "C" void app_main(void)
{
    hall_phase_current_adc_init();

    // 启动时自动校准三相VREF_ZERO
    adc1_channel_t channels[ADC_CHANNEL_NUM] = {ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2};
    for (int ch = 0; ch < ADC_CHANNEL_NUM; ++ch)
    {
        int sum = 0;
        for (int i = 0; i < 100; ++i)
        {
            sum += adc1_get_raw(channels[ch]);
            vTaskDelay(1);
        }
        int raw = sum / 100;
        uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, &adc_chars); // 单位mV
        vref_zero[ch] = voltage / 1000.0f;                              // 单位V
    }

    SimpleFOCDebug::enable(); /*!< Enable Debug */
    Serial.begin(115200);

    ma600a.init();  /*!< Initialize MA600A sensor */
    motor.linkSensor(&ma600a);  /*!< Link sensor to motor */
    driver.voltage_power_supply = 8.4; /*!< Set power supply voltage */
    driver.voltage_limit = 2.0;        /*!< Set voltage limit to the motor */
    driver.init();
    motor.linkDriver(&driver);

    motor.controller = MotionControlType::velocity_openloop; /*!< Set openloop control mode */
    // motor.foc_modulation = FOCModulationType::SpaceVectorPWM;   /*!< Set svpwm mode */
    // motor.controller = MotionControlType::angle;                /*!< Set position control mode */

    /*!< Set velocity pid */
    // motor.PID_velocity.P = 0.9f;
    // motor.PID_velocity.I = 2.2f;
    // motor.voltage_limit = 2.0;
    // motor.voltage_sensor_align = 2;

    /*!< Set angle pid */
    // motor.LPF_velocity.Tf = 0.05;
    // motor.P_angle.P = 15.5;
    // motor.P_angle.D = 0.05;
    // motor.velocity_limit = 200;

    // Serial.begin(115200);
    // motor.useMonitoring(Serial);
    motor.init();                                               /*!< Initialize motor */
    // motor.initFOC();                                            /*!< Align sensor and start fog */

    TickType_t last_adc_tick = xTaskGetTickCount();

    while (1)
    {
        // motor.loopFOC();
        motor.move(1.2f);

        // 每50ms检测一次三相电流
        if (xTaskGetTickCount() - last_adc_tick >= pdMS_TO_TICKS(50))
        {
            float iload_avg[ADC_CHANNEL_NUM] = {0};
            for (int ch = 0; ch < ADC_CHANNEL_NUM; ++ch)
            {
                int raw = 0, sum = 0;
                for (int i = 0; i < ADC_SAMPLE_NUM; ++i)
                {
                    int val = adc1_get_raw(channels[ch]);
                    sum += val;
                }
                raw = sum / ADC_SAMPLE_NUM;
                uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, &adc_chars); // 单位mV
                float vsout = voltage / 1000.0;                                 // 转为V
                float sense_ratio = (ch == 0) ? CURRENT_SENSE_RATIO_A : (ch == 1) ? CURRENT_SENSE_RATIO_B
                                                                                  : CURRENT_SENSE_RATIO_C;
                float offset_uA = (ch == 0) ? CURRENT_SENSE_OFFSET_A : (ch == 1) ? CURRENT_SENSE_OFFSET_B
                                                                                 : CURRENT_SENSE_OFFSET_C;
                                                                                 
                // ILOAD = (VSOUT - VREF) * CURRENT_SENSE_RATIO / RREF
                float iload = (vsout - vref_zero[ch]) * sense_ratio / RREF; // 单位A
                float iload_mA = iload * 1000.0f + offset_uA / 1000.0f;     // 补偿后单位mA

                // 均值滤波
                iload_buffer[ch][iload_index[ch]] = iload_mA;
                iload_index[ch] = (iload_index[ch] + 1) % ILOAD_AVG_WINDOW;
                if (iload_count[ch] < ILOAD_AVG_WINDOW)
                    iload_count[ch]++;
                float iload_sum = 0;
                for (int i = 0; i < iload_count[ch]; ++i)
                {
                    iload_sum += iload_buffer[ch][i];
                }
                iload_avg[ch] = iload_sum / iload_count[ch];
            }
            // 计算母线电流 Ibus = (|IA| + |IB| + |IC|) / 2
            float ibus = (fabsf(iload_avg[0]) + fabsf(iload_avg[1]) + fabsf(iload_avg[2])) / 2.0f;
            // FireWater格式输出，输出三相均值电流,母线电流,MA600A传感器角度
            printf("data:%.2f,%.2f,%.2f,%.2f,%.2f\n", iload_avg[0], iload_avg[1], iload_avg[2], ibus, ma600a.getSensorAngle());
            last_adc_tick = xTaskGetTickCount();
        }

        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    ma600a.deinit();
    driver.deinit();
}

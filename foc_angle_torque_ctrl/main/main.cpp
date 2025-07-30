#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_simplefoc.h"

#if CONFIG_SOC_MCPWM_SUPPORTED
#define USING_MCPWM
#endif
/*
// 定义GPIO引脚
#define LED_GPIO GPIO_NUM_0

// 硬件对象定义
BLDCMotor motor = BLDCMotor(14 / 2, 6.5 / 2, 330, (1.2333e-3) / 2);
BLDCDriver6PWM driver = BLDCDriver6PWM(11, 14, 12, 15, 13, 16, 17, 0);
MA600A ma600a = MA600A(SPI2_HOST, GPIO_NUM_21, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9);

// 电机控制任务
void motor_control_task(void *pvParameters) {

    while (1) {
        // 执行FOC计算
        motor.loopFOC();

        // 应用力矩
        motor.move(torque);
        
        // get the current angle from the sensor
        // float rad_angle = ma600a.getSensorAngle();
        // current_angle = (rad_angle / (2 * PI)) * 360
        // printf("current angle: %.2f\n", rad_angle * 180 / PI);

        // 适当延迟以保持控制频率
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

extern "C" void app_main(void) {

    // 1. GPIO配置
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),  // 选择GPIO0
        .mode = GPIO_MODE_OUTPUT,            // 输出模式
        .pull_up_en = GPIO_PULLUP_DISABLE,   // 禁用上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // 禁用下拉
        .intr_type = GPIO_INTR_DISABLE       // 禁用中断
    };
    gpio_config(&io_conf);

    // 2. 点亮LED（设置低电平，因为开发板LED通常是低电平驱动）
    gpio_set_level(LED_GPIO, 1);

    printf("LED on GPIO0 is now ON\n");

    // initialise magnetic sensor hardware
    ma600a.init();
    // link the motor to the sensor
    motor.linkSensor(&ma600a);

    // driver config
    // power supply voltage [V]
    driver.voltage_power_supply = 5.0;
    // driver.voltage_limit = 7.4;
    // pwm frequency [Hz]
    // 20kHz is the default value
    driver.pwm_frequency = 50000;
    driver.init();
    // link the motor to the driver
    motor.linkDriver(&driver);

    // aligning voltage 
    motor.voltage_sensor_align = 2;
    // choose FOC modulation (optional)
    motor.foc_modulation = FOCModulationType::SpaceVectorPWM;

    // set motion control loop to be used
    motor.controller = MotionControlType::torque;

    // 配置PID参数（力反馈关键参数）
    // velocity PI controller parameters
    motor.PID_velocity.P = 0.03f;
    motor.PID_velocity.I = 1;
    // motor.PID_velocity.D = 0.0001f;
    // maximal voltage to be set to the motor
    motor.voltage_limit = 2.0;

    // velocity low pass filtering time constant
    // the lower the less filtered
    motor.LPF_velocity.Tf = 0.01f;

    // angle P controller
    // motor.P_angle.P = 20;

    // initialize motor
    motor.init();
    // align sensor and start FOC
    motor.initFOC();
    
    // 创建任务
    xTaskCreatePinnedToCore(
        motor_control_task,    // 任务函数
        "motor_control",       // 任务名称
        4096,                 // 堆栈大小
        NULL,                 // 参数
        configMAX_PRIORITIES - 1, // 高优先级
        NULL,                 // 任务句柄
        0                     // 核心0
    );

    printf("Motor control system started\n");
}
*/

#define DETENT_STEPS 16

// 硬件对象定义
BLDCMotor motor = BLDCMotor(14 / 2, 6.5 / 2, 330, (1.2333e-3) / 2);
BLDCDriver6PWM driver = BLDCDriver6PWM(11, 14, 12, 15, 13, 16, 17, 0);
MA600A ma600a = MA600A(SPI2_HOST, GPIO_NUM_21, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9);


float Kp = 4.5;
float Kd = 0.1;
float targetAngle = 0.0;

// 将角度归一到[-PI, PI]
float wrapAngle(float angle) {
  while (angle > M_PI)  angle -= 2*M_PI;
  while (angle <= -M_PI) angle += 2*M_PI;
  return angle;
}

// 更新棘轮目标角度
void updateTargetAngle() {
    float currentAngle = motor.shaftAngle();
    float detentAngle = 2.0 * M_PI / DETENT_STEPS;
    float halfDetent = detentAngle * 0.5;
    float diff = wrapAngle(currentAngle - targetAngle);
    if (diff > halfDetent) targetAngle += detentAngle;
    else if (diff < -halfDetent) targetAngle -= detentAngle;
    targetAngle = wrapAngle(targetAngle);
}

extern "C" void app_main() {
    // FreeRTOS tick 1000Hz（菜单配置）
    // 驱动器配置
    driver.voltage_power_supply = 8.4;
    driver.voltage_limit = 3;
    driver.init();
    motor.linkDriver(&driver);

    // 角度传感器初始化
    ma600a.init();
    motor.linkSensor(&ma600a);

    // choose FOC modulation (optional)
    motor.foc_modulation = FOCModulationType::SpaceVectorPWM;

    // set motion control loop to be used
    motor.controller = MotionControlType::torque;

    // 配置PID参数（力反馈关键参数）
    // velocity PI controller parameters
    motor.PID_velocity.P = 0.03f;
    motor.PID_velocity.I = 1;
    // motor.PID_velocity.D = 0.0001f;
    // maximal voltage to be set to the motor
    motor.voltage_limit = 3;

    // velocity low pass filtering time constant
    // the lower the less filtered
    motor.LPF_velocity.Tf = 0.01f;

    // 电机初始化与标定
    motor.init();
    motor.initFOC();

    while (true) {
        motor.loopFOC();            // 执行FOC控制:contentReference[oaicite:7]{index=7}
        updateTargetAngle();       // 更新目标角

        float currentAngle = motor.shaftAngle();
        float error = wrapAngle(targetAngle - currentAngle);
        float torque = Kp * error - Kd * motor.shaftVelocity();
        motor.move(torque);        // 输出力矩:contentReference[oaicite:8]{index=8}

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

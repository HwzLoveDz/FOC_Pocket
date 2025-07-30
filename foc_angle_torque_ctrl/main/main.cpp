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

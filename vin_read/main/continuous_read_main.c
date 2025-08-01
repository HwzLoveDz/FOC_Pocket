
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define ADC_CHANNEL      ADC1_CHANNEL_2    // GPIO3
#define ADC_UNIT         ADC_UNIT_1
#define DEFAULT_VREF     0             // mV, 可根据实际校准值调整
#define DIVIDER_RATIO    ((100.0 + 20.0) / 20.0) // 6.0

void app_main(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_12); // 0~3.3V

    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_characterize(ADC_UNIT, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);

    while (1) {
        int raw = adc1_get_raw(ADC_CHANNEL);
        uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, &adc_chars); // 单位mV
        float vbus = voltage * DIVIDER_RATIO / 1000.0; // 单位V

        printf("ADC Raw: %d, Voltage: %ldmV, VBUS: %.2fV\n", raw, voltage, vbus);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

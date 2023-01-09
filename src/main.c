/*
 * This code demonstrates how to use the I2C with DS3231RTC module
 * connected to the NodeMCU-32s.
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Zoltan Uglar
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "i2c_ds3231.h"
#include "rom/uart.h"

void serial_input_task(void *pvParameters)
{
    char base_text[] = "********************************************************************************************************\n"
                       "Inputs:\n"
                       "DT - Show current date and time\n"
                       "ST - Show temperature\n"
                       "To set the new date and time enter:\n"
                       "\"sec(0-59),min(0-59),hour(0-23),dow(1-Sun),date(1-31),month(1-12),year(00-99)\" No spaces. No leading 0.\n"
                       "********************************************************************************************************\n";

    printf("%s", base_text);

    uint8_t buf_len = 21;
    char buf[buf_len];
    uint8_t idx = 0;
    uint8_t my_char;
    STATUS s;
    char *date_time = NULL;

    // Clear whole buffer
    memset(buf, 0, buf_len);
    while (1)
    {
        // Read cahracters from serial
        s = uart_rx_one_char(&my_char);

        if (s == OK)
        {
            // check new line character
            if (my_char == 10)
            {
                if (buf[0] == 'D' && buf[1] == 'T')
                {
                    date_time = NULL;
                    ESP_ERROR_CHECK(ds3231_get_date_time(&date_time, DATE_AND_TIME_24));
                    ESP_LOGI(MAIN_TAG, "Current date and time: %s", date_time);
                    free(date_time);
                    memset(buf, 0, buf_len);
                    idx = 0;
                }
                else if (buf[0] == 'S' && buf[1] == 'T')
                {
                    float temp = 0.0;
                    ESP_ERROR_CHECK(ds3231_get_temperature(&temp));
                    ESP_LOGI(MAIN_TAG, "Current temperature: %f degrees Celsius", temp);
                    memset(buf, 0, buf_len);
                    idx = 0;
                }
                else if (buf[0] == 'O' && buf[1] == 'K')
                {
                    if(osf_bit_value)
                    {
                        status_reg_value = status_reg_value & ~(1 << 7);
                        ESP_ERROR_CHECK(ds3231_write_data(DS3231_STATUS_REGISTER_ADDRESS, 1, &status_reg_value, 1));
                        status_reg_value = 0;
                        ESP_LOGW(MAIN_TAG, "Date and time have been confirmed!");
                    }
                    else
                    {
                        ESP_LOGI(MAIN_TAG, "Status Register: 0x%02X, OSF bit: %d", status_reg_value, osf_bit_value);
                    }
                    memset(buf, 0, buf_len);
                    idx = 0;
                }
                else
                {
                    ESP_ERROR_CHECK(ds3231_set_date_time(buf));
                    date_time = NULL;
                    ESP_ERROR_CHECK(ds3231_get_date_time(&date_time, DATE_AND_TIME_24));
                    ESP_LOGI(MAIN_TAG, "New date and time: %s", date_time);
                    free(date_time);
                    memset(buf, 0, buf_len);
                    idx = 0;
                }
            }
            else
            {
                if (idx < buf_len - 1)
                {
                    buf[idx] = (char)my_char;
                    idx++;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main()
{
    TaskHandle_t serialInputTaskHandle = NULL;
    // Configure the I2C environment and install driver.
    ESP_LOGI(MAIN_TAG, "Configure the I2C environment and install driver");
    ESP_ERROR_CHECK(i2c_ds3231_init());

    // Create Serial Input Task
    xTaskCreate(serial_input_task, "Serial Input Task", 3072, NULL, 1, &serialInputTaskHandle);

    osf_bit_value = 0;
    status_reg_value = 0;
    ESP_ERROR_CHECK(ds3231_power_lost(&osf_bit_value, &status_reg_value));

    if (osf_bit_value)
    {
        ESP_LOGW(MAIN_TAG, "Oscillator either is stopped or was stopped for some period. ");
        ESP_LOGW(MAIN_TAG, "Status Register: 0x%02X, OSF bit: %d", status_reg_value, osf_bit_value);
        char *date_time = NULL;
        ESP_ERROR_CHECK(ds3231_get_date_time(&date_time, DATE_AND_TIME_24));
        ESP_LOGW(MAIN_TAG, "Current date and time: %s", date_time);
        free(date_time);
        ESP_LOGW(MAIN_TAG, "If the time is correct please enter OK otherwise please enter the new time.");
    }

    // Delete "setup and loop" task
    vTaskDelete(NULL);

    // loop
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
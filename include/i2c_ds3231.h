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

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include "driver/i2c.h"
#include "sdkconfig.h"

#include "string.h"
#include <stdio.h>
#include "time.h"

#define I2C_MODE I2C_MODE_MASTER      // I2C mode is master
#define I2C_MASTER_SCL_IO GPIO_NUM_22 // gpio number for I2C master clock
#define I2C_MASTER_SDA_IO GPIO_NUM_21 // gpio number for I2C master data
#define I2C_MASTER_PORT I2C_NUM_0     // I2C port number for master dev
#define I2C_MASTER_FREQ_HZ 400000     // I2C master clock frequency (400KHz)
#define I2C_MASTER_TX_BUF_DISABLE 0   // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE 0   // I2C master doesn't need buffer

#define DS3231_ADDRESS 0x68                 // DS3231RTC address
#define DS3231_TIME_ADDRESS 0x00         // Address of Seconds Register of DS3231
#define DS3231_STATUS_REGISTER_ADDRESS 0x0F // Address of Status Register of DS3231
#define DS3231_ADDRESS_TEMPERATURE 0x11     // Address of Temperature Register of DS3231

#define DS3231_12HOUR_FLAG 0x40
#define DS3231_12HOUR_MASK 0x1F
#define DS3231_PM_FLAG 0x20
#define DS3231_MONTH_MASK 0x1F

#define I2CDEV_TIMEOUT 1000

typedef enum
{
  DATE_AND_TIME_24,
  DATE_AND_TIME_AM_PM,
  ONLY_DATE,
  ONLY_TIME_24,
  ONLY_TIME_AM_PM,
  UNIX_TIMESTAMPS
} date_time_format;

static const char MAIN_TAG[] = "main";
static const char DS3231_TAG[] = "ds3231";

// Device mutex
SemaphoreHandle_t mutex;


// OSF bit global value
uint8_t osf_bit_value;
// Control/Status register global value
uint8_t status_reg_value;

/**
 * @brief Configure the I2C environment and install driver.
 *
 * @param void
 * @return
 * - ESP_OK Success.
 * - ESP_ERR_INVALID_ARG Parameter error .
 * - ESP_FAIL Driver installation error.
 */
esp_err_t i2c_ds3231_init(void);

/**
 * @brief Read data from registers.
 *
 * @param address Address of start of reading data.
 * @param address_size Size of addresses for reading.
 * @param [out] rx_buffer Buffer to store values of addresses.
 * @param rx_buffer_size Size of buffer.
 * @return
 * - ESP_OK Success.
 * - ESP_ERR_INVALID_ARG Parameter error.
 * - ESP_FAIL Sending command error, slave hasn't ACK the transfer.
 * - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
 * - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
 */
esp_err_t ds3231_read_data(const uint8_t address, const size_t address_size, uint8_t *rx_buffer, size_t rx_buffer_size);

/**
 * @brief Write data to registers.
 *
 * @param address Address of start of reading data.
 * @param address_size Size of addresses for reading.
 * @param tx_buffer Buffer of values to write into register.
 * @param tx_buffer_size Size of buffer.
 * @return
 * - ESP_OK Success.
 * - ESP_ERR_INVALID_ARG Parameter error.
 * - ESP_FAIL Sending command error, slave hasn't ACK the transfer.
 * - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
 * - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
 */
esp_err_t ds3231_write_data(const uint8_t address, const size_t address_size, uint8_t *tx_buffer, size_t tx_buffer_size);

/**
 * @brief Check OSF (Oscillator Stop Flag) of status register. Check DS3231 documentation for details.
 *
 * @param [out] status OSF bit
 * @param [out] status_register value of Control/Status register.
 * @return
 * - ESP_OK Success.
 * - ESP_ERR_INVALID_ARG Parameter error.
 * - ESP_FAIL Sending command error, slave hasn't ACK the transfer.
 * - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
 * - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
 */
esp_err_t ds3231_power_lost(uint8_t *status, uint8_t *status_register);

/**
 * @brief Convert a binary coded decimal value to decimal. RTC stores time/date values as BCD.
 *
 * @param value BCD value.
 * @return Decimal value.
 */
uint8_t bcd2dec(uint8_t value);

/**
 * @brief Convert a decimal value to BCD format for the RTC registers.
 *
 * @param value Decimal value.
 * @return BCD value.
 */
uint8_t dec2bcd(uint8_t value);

/**
 * @brief Store date and time into string. The function does not execute the freeing memory.
 *
 * @param [out] str_buffer The pointer to the destination array where the resulting C string is copied.
 * @param dt_format The date and time format for printing.
 * @return
 * - ESP_OK Success.
 * - ESP_ERR_NO_MEM: If memory allocation is failed.
 * - ESP_ERR_INVALID_ARG Parameter error.
 * - ESP_FAIL Sending command error, slave hasn't ACK the transfer.
 * - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
 * - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
 */
esp_err_t ds3231_get_date_time(char **str_buffer, date_time_format dt_format);

/**
 * @brief Set new date and time into DS3231.
 *
 * @param str_buffer The pointer of new date and time string.
 * @return
 * - ESP_OK Success.
 * - ESP_ERR_INVALID_ARG Parameter error.
 * - ESP_FAIL Sending command error, slave hasn't ACK the transfer.
 * - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
 * - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
 */
esp_err_t ds3231_set_date_time(char *date_time_str);

/**
 * @brief Get temperature
 *
 * @param [out] temp Temperature, degrees Celsius
 * @return
 * - ESP_OK Success.
 */
esp_err_t ds3231_get_temperature(float *temp);
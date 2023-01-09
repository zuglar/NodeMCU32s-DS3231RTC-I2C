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

esp_err_t i2c_ds3231_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ};

    mutex = xSemaphoreCreateMutex();
    if (!mutex)
    {
        ESP_LOGE(DS3231_TAG, "Could not create device mutex");
        return ESP_FAIL;
    }

    esp_err_t result = i2c_param_config(I2C_MASTER_PORT, &conf);
    if (result != ESP_OK)
    {
        return result;
    }
    return i2c_driver_install(I2C_MASTER_PORT, I2C_MODE_MASTER, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

esp_err_t ds3231_read_data(const uint8_t address, const size_t address_size, uint8_t *rx_buffer, size_t rx_buffer_size)
{
    esp_err_t result = ESP_OK;

    if (mutex != NULL)
    {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            // i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            // i2c_master_start(cmd);
            // i2c_master_write_byte(cmd, (DS3231_ADDRESS << 1), true);
            // i2c_master_write(cmd, &address, address_size, true);
            // i2c_master_start(cmd);
            // i2c_master_write_byte(cmd, (DS3231_ADDRESS << 1) | I2C_MASTER_READ, true);
            // i2c_master_read(cmd, rx_buffer, rx_buffer_size, I2C_MASTER_LAST_NACK);
            // i2c_master_stop(cmd);

            // esp_err_t result = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, pdMS_TO_TICKS(1000));

            // i2c_cmd_link_delete(cmd);

            // Or we can use
            // esp_err_t i2c_master_write_read_device(i2c_port_t i2c_num, uint8_t device_address, const uint8_t *write_buffer,
            //                              size_t write_size, uint8_t *read_buffer, size_t read_size, TickType_t ticks_to_wait)
            // Perform a write followed by a read to a device on the I2C bus. A repeated start signal is used between the write
            // and read, thus, the bus is not released until the two transactions are finished. This function is a wrapper
            // to i2c_master_start(), i2c_master_write(), i2c_master_read(), etc… It shall only be called in I2C master mode.
            //
            result = i2c_master_write_read_device(I2C_MASTER_PORT, DS3231_ADDRESS, &address, address_size,
                                                  rx_buffer, rx_buffer_size, pdMS_TO_TICKS(I2CDEV_TIMEOUT));
        }

        xSemaphoreGive(mutex);
    }

    return result;
}

esp_err_t ds3231_write_data(const uint8_t address, const size_t address_size, uint8_t *tx_buffer, size_t tx_buffer_size)
{
    esp_err_t result = ESP_OK;

    if (mutex != NULL)
    {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, DS3231_ADDRESS << 1, true);
            i2c_master_write(cmd, &address, address_size, true);

            i2c_master_write(cmd, tx_buffer, tx_buffer_size, true);
            i2c_master_stop(cmd);
            result = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, pdMS_TO_TICKS(I2CDEV_TIMEOUT));
            if (result != ESP_OK)
                ESP_LOGE(DS3231_TAG, "Could not write to device [0x%02x at %d]: %d (%s)", address, I2C_MASTER_PORT, result, esp_err_to_name(result));

            i2c_cmd_link_delete(cmd);
        }

        xSemaphoreGive(mutex);
    }

    return result;
}

esp_err_t ds3231_power_lost(uint8_t *status, uint8_t *status_register)
{
    esp_err_t result = ds3231_read_data(DS3231_STATUS_REGISTER_ADDRESS, 1, status_register, 1);
    if (result == ESP_OK)
    {
        *status = *status_register >> 7;
        // *status = buffer & 0x80;
    }

    return result;
}

uint8_t bcd2dec(uint8_t value)
{
    return value - 6 * (value >> 4);
}

uint8_t dec2bcd(uint8_t value)
{
    return value + 6 * (value / 10);
}

esp_err_t ds3231_get_date_time(char **str_buffer, date_time_format dt_format)
{
    if (dt_format != DATE_AND_TIME_24 && dt_format != DATE_AND_TIME_AM_PM && dt_format != ONLY_DATE &&
        dt_format != ONLY_TIME_24 && dt_format != ONLY_TIME_AM_PM && dt_format != UNIX_TIMESTAMPS)
        return ESP_ERR_INVALID_ARG;

    uint8_t buffer_size = 30;
    // Allocate memory for *str_buffer
    *str_buffer = (char *)malloc(buffer_size * sizeof(char));

    if (str_buffer == NULL)
        return ESP_ERR_NO_MEM;

    // Allocate memory for rx_result
    uint8_t *rx_result = (uint8_t *)malloc(7 * sizeof(uint8_t));

    if (rx_result == NULL)
        return ESP_ERR_NO_MEM;

    esp_err_t result = ESP_OK;

    result = ds3231_read_data(DS3231_TIME_ADDRESS, 1, rx_result, 7);

    if (result != ESP_OK)
        return result;

    // Allocate memory for time
    struct tm *time = malloc(1 * sizeof(struct tm));

    if (time == NULL)
        return ESP_ERR_NO_MEM;

    time->tm_sec = bcd2dec(rx_result[0]);
    time->tm_min = bcd2dec(rx_result[1]);

    if (rx_result[2] & DS3231_12HOUR_FLAG)
    {
        /* 12H */
        time->tm_hour = bcd2dec(rx_result[2] & DS3231_12HOUR_MASK) - 1;
        /* AM/PM? */
        if (rx_result[2] & DS3231_PM_FLAG)
            time->tm_hour += 12;
    }
    else
    {
        time->tm_hour = bcd2dec(rx_result[2]); /* 24H */
    }
    time->tm_wday = bcd2dec(rx_result[3]) - 1;
    time->tm_mday = bcd2dec(rx_result[4]);
    time->tm_mon = bcd2dec(rx_result[5] & DS3231_MONTH_MASK) - 1;
    time->tm_year = bcd2dec(rx_result[6]) + 100;
    time->tm_isdst = 0;

    size_t res = 0;

    if (dt_format == DATE_AND_TIME_24)
        res = strftime(*str_buffer, buffer_size, "%Y-%m-%d %H:%M:%S", time);
    else if (dt_format == DATE_AND_TIME_AM_PM)
        res = strftime(*str_buffer, buffer_size, "%Y-%m-%d %I:%M:%S %p", time);
    else if (dt_format == ONLY_DATE)
        res = strftime(*str_buffer, buffer_size, "%Y-%m-%d", time);
    else if (dt_format == ONLY_TIME_24)
        res = strftime(*str_buffer, buffer_size, "%H:%M:%S", time);
    else if (dt_format == ONLY_TIME_AM_PM)
        res = strftime(*str_buffer, buffer_size, "%I:%M:%S %p", time);
    else if (dt_format == UNIX_TIMESTAMPS)
    {
        time_t t_of_day = mktime(time);
        int i = sprintf(*str_buffer, "%ld", (long)t_of_day);
        if (i < 0)
            result = ESP_ERR_INVALID_ARG;
    }

    if (res == 0 && dt_format != UNIX_TIMESTAMPS)
    {
        result = ESP_ERR_INVALID_ARG;
    }

    // Freeing allocated memory
    if (rx_result != NULL)
        free(rx_result);

    // Freeing allocated memory
    if (time != NULL)
        free(time);

    return result;
}

esp_err_t ds3231_set_date_time(char *date_time_str)
{
    // esp_err_t result = ESP_OK;

    uint8_t str_len = strlen(date_time_str);
    char delim[] = ",";
    uint8_t num_of_delimiters = 0;
    uint8_t position = 0;

    int sec = 0;
    int min = 0;
    int hour = 0;
    int dow = 0;
    int day = 0;
    int month = 0;
    int year = 0;

    // check number of delimiters in the string
    for (uint8_t i = 0; i < str_len; i++)
    {
        if (date_time_str[i] == ',')
            num_of_delimiters++;
    }

    if (num_of_delimiters != 6)
        return ESP_ERR_INVALID_ARG;

    // split string by delimiter and convert the result into integer
    char *ptr = strtok(date_time_str, delim);
    sec = atoi(ptr);

    while (ptr != NULL)
    {
        ptr = strtok(NULL, delim);

        if (position == 0)
            min = atoi(ptr);
        else if (position == 1)
            hour = atoi(ptr);
        else if (position == 2)
            dow = atoi(ptr);
        else if (position == 3)
            day = atoi(ptr);
        else if (position == 4)
            month = atoi(ptr);
        else if (position == 5)
            year = atoi(ptr);

        position++;
    }

    // check input values
    if (sec < 0 || sec > 59 || min < 0 || min > 59 || hour < 0 || hour > 23 || dow < 1 || dow > 7 || day < 1 || day > 31 || month < 1 || month > 12 || year < 0 || year > 99)
        return ESP_ERR_INVALID_ARG;

    if ((year % 4) == 0 && month == 2 && (day < 1 || day > 29))
        return ESP_ERR_INVALID_ARG;

    if ((year % 4) != 0 && month == 2 && (day < 1 || day > 28))
        return ESP_ERR_INVALID_ARG;

    if (day == 31 && month != 1 && month != 3 && month != 5 && month != 7 && month != 8 && month != 10 && month != 12)
        return ESP_ERR_INVALID_ARG;

    uint8_t data[7];
    data[0] = dec2bcd(sec);
    data[1] = dec2bcd(min);
    data[2] = dec2bcd(hour);
    data[3] = dec2bcd(dow);
    data[4] = dec2bcd(day);
    data[5] = dec2bcd(month);
    data[6] = dec2bcd(year);

    return ds3231_write_data(DS3231_TIME_ADDRESS, 1, data, 7);
}

esp_err_t ds3231_get_temperature(float *temp)
{
    uint8_t data[2];
    esp_err_t result = ds3231_read_data(DS3231_ADDRESS_TEMPERATURE, 1, data, 2);
    if (result == ESP_OK)
    {
        *temp = ((int16_t)(int8_t)data[0] << 2 | data[1] >> 6) * 0.25;
    }

    return result;
}

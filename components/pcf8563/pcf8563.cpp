#include "pcf8563.h"

namespace {
constexpr int kI2cTimeoutMs = 20;
constexpr uint8_t kControlStatus1Reg = 0x00;
constexpr uint8_t kClkoutControlReg = 0x0D;
constexpr uint8_t kTimeReg = 0x02;
constexpr uint8_t kClkoutDisabled = 0x00;

uint8_t bcd_to_dec(uint8_t value)
{
    return static_cast<uint8_t>(((value >> 4) * 10U) + (value & 0x0FU));
}

uint8_t dec_to_bcd(uint8_t value)
{
    return static_cast<uint8_t>(((value / 10U) << 4) | (value % 10U));
}

bool write_bytes(i2c_master_dev_handle_t i2c_dev, const uint8_t* data, size_t len)
{
    return i2c_master_transmit(i2c_dev, data, len, kI2cTimeoutMs) == ESP_OK;
}

bool read_reg(i2c_master_dev_handle_t i2c_dev, uint8_t reg, uint8_t* data, size_t len)
{
    return i2c_master_transmit_receive(i2c_dev, &reg, sizeof(reg), data, len, kI2cTimeoutMs) == ESP_OK;
}

} // namespace

bool pcf8563_probe(i2c_master_dev_handle_t i2c_dev)
{
    uint8_t dummy = 0;
    return read_reg(i2c_dev, kControlStatus1Reg, &dummy, 1);
}

bool pcf8563_disable_clkout(i2c_master_dev_handle_t i2c_dev)
{
    const uint8_t payload[] = {
        kClkoutControlReg,
        kClkoutDisabled,
    };
    return write_bytes(i2c_dev, payload, sizeof(payload));
}

bool pcf8563_get_time(i2c_master_dev_handle_t i2c_dev, std::tm& timeinfo)
{
    uint8_t raw[7] = {};
    if(!read_reg(i2c_dev, kTimeReg, raw, sizeof(raw)))
    {
        return false;
    }

    timeinfo = {};
    timeinfo.tm_sec = bcd_to_dec(raw[0] & 0x7FU);
    timeinfo.tm_min = bcd_to_dec(raw[1] & 0x7FU);
    timeinfo.tm_hour = bcd_to_dec(raw[2] & 0x3FU);
    timeinfo.tm_mday = bcd_to_dec(raw[3] & 0x3FU);
    timeinfo.tm_wday = bcd_to_dec(raw[4] & 0x07U);
    timeinfo.tm_mon = static_cast<int>(bcd_to_dec(raw[5] & 0x1FU)) - 1;

    const int year = bcd_to_dec(raw[6]);
    timeinfo.tm_year = ((raw[5] & 0x80U) != 0U) ? year : (year + 100);
    timeinfo.tm_isdst = -1;

    return true;
}

bool pcf8563_set_time(i2c_master_dev_handle_t i2c_dev, const std::tm& timeinfo)
{
    if(timeinfo.tm_sec < 0 || timeinfo.tm_sec > 59 ||
       timeinfo.tm_min < 0 || timeinfo.tm_min > 59 ||
       timeinfo.tm_hour < 0 || timeinfo.tm_hour > 23 ||
       timeinfo.tm_mday < 1 || timeinfo.tm_mday > 31 ||
       timeinfo.tm_mon < 0 || timeinfo.tm_mon > 11 ||
       timeinfo.tm_wday < 0 || timeinfo.tm_wday > 6)
    {
        return false;
    }

    const int full_year = timeinfo.tm_year + 1900;
    if(full_year < 1900 || full_year > 2099)
    {
        return false;
    }

    const uint8_t century_bit = full_year < 2000 ? 0x80U : 0x00U;
    const uint8_t payload[] = {
        kTimeReg,
        dec_to_bcd(static_cast<uint8_t>(timeinfo.tm_sec)),
        dec_to_bcd(static_cast<uint8_t>(timeinfo.tm_min)),
        dec_to_bcd(static_cast<uint8_t>(timeinfo.tm_hour)),
        dec_to_bcd(static_cast<uint8_t>(timeinfo.tm_mday)),
        dec_to_bcd(static_cast<uint8_t>(timeinfo.tm_wday)),
        static_cast<uint8_t>(dec_to_bcd(static_cast<uint8_t>(timeinfo.tm_mon + 1)) | century_bit),
        dec_to_bcd(static_cast<uint8_t>(full_year % 100)),
    };

    return write_bytes(i2c_dev, payload, sizeof(payload));
}

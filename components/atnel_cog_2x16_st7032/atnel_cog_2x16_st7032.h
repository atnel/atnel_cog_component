#pragma once

// ==========================================================================
// ATNEL COG 2x16 - ST7032i I2C Character LCD component for ESPHome
// Based on MK_LCD ver 3.0 by Miroslaw Kardas (ATNEL)
// Ported to ESPHome C++ component
// ==========================================================================

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/display/display.h"

#include <vector>
#include <string>

namespace esphome {
namespace atnel_cog_2x16_st7032 {

// ST7032 I2C control bytes
static const uint8_t ST7032_CTRL_BYTE      = 0x00;  // followed by command bytes
static const uint8_t ST7032_CTRL_BYTE_CO   = 0x80;  // followed by 1 command byte
static const uint8_t ST7032_CTRL_BYTE_RS   = 0x40;  // followed by DDRAM data byte(s)

// ST7032 commands
static const uint8_t ST7032_CLEAR_DISPLAY  = 0x01;
static const uint8_t ST7032_RETURN_HOME    = 0x02;

static const uint8_t ST7032_FUNCTION_SET   = 0x20;
static const uint8_t ST7032_FUNC_DL        = 0x10;  // 8-bit interface
static const uint8_t ST7032_FUNC_N         = 0x08;  // 2-line display
static const uint8_t ST7032_FUNC_DH        = 0x04;  // double height font
static const uint8_t ST7032_FUNC_IS        = 0x01;  // instruction table select

static const uint8_t ST7032_OSC_FREQ       = 0x10;
static const uint8_t ST7032_OSC_BS         = 0x08;  // 1/4 bias
static const uint8_t ST7032_OSC_F2         = 0x04;

static const uint8_t ST7032_POWER_ICON     = 0x50;
static const uint8_t ST7032_POWER_Ion      = 0x08;  // ICON display on
static const uint8_t ST7032_POWER_Bon      = 0x04;  // booster on

static const uint8_t ST7032_FOLLOWER       = 0x60;
static const uint8_t ST7032_FOLLOWER_Fon   = 0x08;
static const uint8_t ST7032_FOLLOWER_Rab2  = 0x04;

static const uint8_t ST7032_CONTRAST_SET   = 0x70;

static const uint8_t ST7032_DISPLAY_ON_OFF = 0x08;
static const uint8_t ST7032_DISPLAY_D      = 0x04;  // display on
static const uint8_t ST7032_DISPLAY_C      = 0x02;  // cursor on
static const uint8_t ST7032_DISPLAY_B      = 0x01;  // blink on

static const uint8_t ST7032_SET_DDRAM      = 0x80;
static const uint8_t ST7032_SET_CGRAM      = 0x40;

static const uint8_t ST7032_ENTRY_MODE     = 0x04;
static const uint8_t ST7032_ENTRY_INC      = 0x02;  // increment cursor

// DDRAM line addresses
static const uint8_t ST7032_LINE1_ADDR     = 0x00;
static const uint8_t ST7032_LINE2_ADDR     = 0x40;

// Timing
static const uint32_t ST7032_WRITE_DELAY_US     = 30;
static const uint32_t ST7032_HOME_CLEAR_DELAY_US = 1200;

// Contrast limits
static const uint8_t ST7032_CONTRAST_MIN = 0;
static const uint8_t ST7032_CONTRAST_MAX = 63;


struct UserCharacter {
    uint8_t position;
    uint8_t data[8];
};


class AtnelCogDisplay : public display::Display, public i2c::I2CDevice {
 public:
    void setup() override;
    void update() override;
    void dump_config() override;
    float get_setup_priority() const override { return setup_priority::PROCESSOR; }

    // get_display_type not overridden - uses default from Display base class

    // Required pure virtual implementations from display::Display
    void draw_pixel_at(int x, int y, Color color) override {
        // Character LCD has no pixel addressing - intentionally empty
    }
    display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }
    int get_height_internal() override { return this->rows_; }
    int get_width_internal() override { return this->columns_; }

    void set_columns(uint8_t columns) { this->columns_ = columns; }
    void set_rows(uint8_t rows) { this->rows_ = rows; }
    void set_contrast(uint8_t contrast) { this->contrast_ = contrast; }
    void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
    void set_writer(std::function<void(AtnelCogDisplay &)> &&writer) { this->writer_ = std::move(writer); }

    void set_user_character(uint8_t position, const std::vector<uint8_t> &data);

    // Display API - text operations
    void print(const char *str);
    void print(uint8_t column, uint8_t row, const char *str);
    void printf(const char *format, ...) __attribute__((format(printf, 2, 3)));
    void printf(uint8_t column, uint8_t row, const char *format, ...) __attribute__((format(printf, 4, 5)));
    void strftime(const char *format, ESPTime time);
    void strftime(uint8_t column, uint8_t row, const char *format, ESPTime time);

    // Display control
    void clear();
    void clear_line(uint8_t line);
    void set_cursor(uint8_t column, uint8_t row);
    void cursor_on();
    void cursor_off();
    void blink_on();
    void blink_off();
    void backlight();
    void no_backlight();
    void set_contrast_value(uint8_t value);

 protected:
    void write_command_(uint8_t cmd);
    void write_data_(uint8_t data);
    void write_char_(char c);
    void load_user_characters_();
    void init_display_();

    uint8_t columns_{16};
    uint8_t rows_{2};
    uint8_t contrast_{32};
    uint8_t display_control_{0};
    bool backlight_state_{true};
    GPIOPin *reset_pin_{nullptr};

    std::vector<UserCharacter> user_characters_;
    std::function<void(AtnelCogDisplay &)> writer_;

    // internal cursor tracking
    int cursor_col_{0};
    int cursor_row_{0};
};

}  // namespace atnel_cog_2x16
}  // namespace esphome

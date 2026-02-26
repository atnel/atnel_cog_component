// ==========================================================================
// ATNEL COG 2x16 - ST7032i I2C Character LCD component for ESPHome
// Based on MK_LCD ver 3.0 by Miroslaw Kardas (ATNEL)
// Ported to ESPHome C++ component
// ==========================================================================

#include "atnel_cog_2x16_st7032.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace atnel_cog_2x16_st7032 {

static const char *const TAG = "atnel_cog_2x16_st7032";

// -----------------------------------------------------------------------
// Low-level I2C write: command
// -----------------------------------------------------------------------
void AtnelCogDisplay::write_command_(uint8_t cmd) {
    uint8_t buf[2];
    buf[0] = ST7032_CTRL_BYTE;   // Co=0, RS=0 -> command
    buf[1] = cmd;
    this->write(buf, 2);
    delayMicroseconds(ST7032_WRITE_DELAY_US);
}

// -----------------------------------------------------------------------
// Low-level I2C write: data (DDRAM / CGRAM)
// -----------------------------------------------------------------------
void AtnelCogDisplay::write_data_(uint8_t data) {
    uint8_t buf[2];
    buf[0] = ST7032_CTRL_BYTE_RS;  // Co=0, RS=1 -> data
    buf[1] = data;
    this->write(buf, 2);
    delayMicroseconds(ST7032_WRITE_DELAY_US);
}

// -----------------------------------------------------------------------
// Write single character with CGRAM remapping (0x80-0x87 -> 0x00-0x07)
// This is the MK_LCD trick to avoid null in C strings
// -----------------------------------------------------------------------
void AtnelCogDisplay::write_char_(char c) {
    uint8_t ch = static_cast<uint8_t>(c);
    // Remap 0x80-0x87 to 0x00-0x07 for user-defined characters
    if( ch >= 0x80 && ch <= 0x87 ) {
        ch = ch & 0x07;
    }
    this->write_data_(ch);
    this->cursor_col_++;
}

// -----------------------------------------------------------------------
// ST7032 initialization sequence (from MK_LCD blueprint)
// -----------------------------------------------------------------------
void AtnelCogDisplay::init_display_() {
    // Hardware reset via RST pin (if configured)
    if( this->reset_pin_ != nullptr ) {
        this->reset_pin_->setup();
        this->reset_pin_->digital_write(false);  // RST LOW
        delay(10);
        this->reset_pin_->digital_write(true);   // RST HIGH - release
    }

    // Wait for power stabilization (after hardware reset)
    delay(40);

    // Function set: 8-bit, 2-line, instruction table 1 (IS=1)
    this->write_command_(ST7032_FUNCTION_SET
        | ST7032_FUNC_DL
        | ST7032_FUNC_N
        | ST7032_FUNC_IS);

    // Internal oscillator frequency: 1/4 bias, F2 adjustment
    this->write_command_(ST7032_OSC_FREQ
        | ST7032_OSC_BS
        | ST7032_OSC_F2);

    // Power/ICON/Contrast: ICON display on (no booster yet - contrast sets it)
    this->write_command_(ST7032_POWER_ICON
        | ST7032_POWER_Ion);

    // Set contrast (this also enables booster via POWER_ICON_Bon)
    this->set_contrast_value(this->contrast_);

    // Follower control: follower on, Rab2
    this->write_command_(ST7032_FOLLOWER
        | ST7032_FOLLOWER_Fon
        | ST7032_FOLLOWER_Rab2);

    // CRITICAL: wait for follower circuit and booster to stabilize
    delay(200);

    // Display ON, cursor OFF, blink OFF
    this->display_control_ = ST7032_DISPLAY_D;
    this->write_command_(ST7032_DISPLAY_ON_OFF | this->display_control_);

    delayMicroseconds(300);

    // Clear display
    this->write_command_(ST7032_CLEAR_DISPLAY);
    delayMicroseconds(ST7032_HOME_CLEAR_DELAY_US);
}

// -----------------------------------------------------------------------
// Load user-defined characters into CGRAM
// -----------------------------------------------------------------------
void AtnelCogDisplay::load_user_characters_() {
    // Switch to normal instruction table (IS=0) for CGRAM access
    this->write_command_(ST7032_FUNCTION_SET
        | ST7032_FUNC_DL
        | ST7032_FUNC_N);

    for( const auto &uc : this->user_characters_ ) {
        // Set CGRAM address: position * 8
        this->write_command_(ST7032_SET_CGRAM | (uc.position << 3));
        for( uint8_t i = 0; i < 8; i++ ) {
            this->write_data_(uc.data[i]);
        }
    }

    // Switch back to instruction table 1 (IS=1)
    this->write_command_(ST7032_FUNCTION_SET
        | ST7032_FUNC_DL
        | ST7032_FUNC_N
        | ST7032_FUNC_IS);

    // Return cursor to DDRAM
    this->write_command_(ST7032_SET_DDRAM | ST7032_LINE1_ADDR);
}

// -----------------------------------------------------------------------
// ESPHome component: setup
// -----------------------------------------------------------------------
void AtnelCogDisplay::setup() {
    ESP_LOGCONFIG(TAG, "Setting up ATNEL COG ST7032 display...");
    this->init_display_();

    if( !this->user_characters_.empty() ) {
        this->load_user_characters_();
    }

    ESP_LOGCONFIG(TAG, "ATNEL COG ST7032 display initialized successfully");
}

// -----------------------------------------------------------------------
// ESPHome component: update (called every update_interval)
// -----------------------------------------------------------------------
void AtnelCogDisplay::update() {
    // Clear display before re-rendering
    this->write_command_(ST7032_CLEAR_DISPLAY);
    delayMicroseconds(ST7032_HOME_CLEAR_DELAY_US);
    this->cursor_col_ = 0;
    this->cursor_row_ = 0;

    if( this->writer_ ) {
        this->writer_(*this);
    }
}

// -----------------------------------------------------------------------
// ESPHome component: dump_config
// -----------------------------------------------------------------------
void AtnelCogDisplay::dump_config() {
    ESP_LOGCONFIG(TAG, "ATNEL COG ST7032 I2C LCD:");
    ESP_LOGCONFIG(TAG, "  Dimensions: %dx%d", this->columns_, this->rows_);
    ESP_LOGCONFIG(TAG, "  Contrast: %d", this->contrast_);
    ESP_LOGCONFIG(TAG, "  User characters: %d", this->user_characters_.size());
    LOG_I2C_DEVICE(this);
    LOG_UPDATE_INTERVAL(this);
}

// -----------------------------------------------------------------------
// Set user character (called from Python codegen)
// -----------------------------------------------------------------------
void AtnelCogDisplay::set_user_character(uint8_t position, const std::vector<uint8_t> &data) {
    UserCharacter uc;
    uc.position = position;
    for( uint8_t i = 0; i < 8 && i < data.size(); i++ ) {
        uc.data[i] = data[i];
    }
    this->user_characters_.push_back(uc);
}

// -----------------------------------------------------------------------
// Public API: print
// -----------------------------------------------------------------------
void AtnelCogDisplay::print(const char *str) {
    while( *str ) {
        this->write_char_(*str++);
    }
}

void AtnelCogDisplay::print(uint8_t column, uint8_t row, const char *str) {
    this->set_cursor(column, row);
    this->print(str);
}

// -----------------------------------------------------------------------
// Public API: printf
// -----------------------------------------------------------------------
void AtnelCogDisplay::printf(const char *format, ...) {
    char buf[64];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    this->print(buf);
}

void AtnelCogDisplay::printf(uint8_t column, uint8_t row, const char *format, ...) {
    this->set_cursor(column, row);
    char buf[64];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    this->print(buf);
}

// -----------------------------------------------------------------------
// Public API: strftime
// -----------------------------------------------------------------------
void AtnelCogDisplay::strftime(const char *format, ESPTime time) {
    char buf[64];
    size_t ret = time.strftime(buf, sizeof(buf), format);
    if( ret > 0 ) {
        this->print(buf);
    }
}

void AtnelCogDisplay::strftime(uint8_t column, uint8_t row, const char *format, ESPTime time) {
    this->set_cursor(column, row);
    this->strftime(format, time);
}

// -----------------------------------------------------------------------
// Public API: cursor / display control
// -----------------------------------------------------------------------
void AtnelCogDisplay::set_cursor(uint8_t column, uint8_t row) {
    if( column >= this->columns_ || row >= this->rows_ ) {
        return;
    }
    this->cursor_col_ = column;
    this->cursor_row_ = row;

    uint8_t addr = (row == 0) ? ST7032_LINE1_ADDR : ST7032_LINE2_ADDR;
    this->write_command_(ST7032_SET_DDRAM | (addr + column));
}

void AtnelCogDisplay::clear() {
    this->write_command_(ST7032_CLEAR_DISPLAY);
    delayMicroseconds(ST7032_HOME_CLEAR_DELAY_US);
    this->cursor_col_ = 0;
    this->cursor_row_ = 0;
}

void AtnelCogDisplay::clear_line(uint8_t line) {
    if( line >= this->rows_ ) {
        return;
    }
    this->set_cursor(0, line);
    for( uint8_t i = 0; i < this->columns_; i++ ) {
        this->write_data_(' ');
    }
    this->set_cursor(0, line);
}

void AtnelCogDisplay::cursor_on() {
    this->display_control_ |= ST7032_DISPLAY_C;
    this->write_command_(ST7032_DISPLAY_ON_OFF | this->display_control_);
}

void AtnelCogDisplay::cursor_off() {
    this->display_control_ &= ~ST7032_DISPLAY_C;
    this->write_command_(ST7032_DISPLAY_ON_OFF | this->display_control_);
}

void AtnelCogDisplay::blink_on() {
    this->display_control_ |= ST7032_DISPLAY_B;
    this->write_command_(ST7032_DISPLAY_ON_OFF | this->display_control_);
}

void AtnelCogDisplay::blink_off() {
    this->display_control_ &= ~ST7032_DISPLAY_B;
    this->write_command_(ST7032_DISPLAY_ON_OFF | this->display_control_);
}

void AtnelCogDisplay::backlight() {
    this->backlight_state_ = true;
    // ST7032 COG: backlight is controlled via external GPIO pin, not I2C
    // This flag can be read by lambda or used with output component
    ESP_LOGD(TAG, "Backlight ON (external GPIO control required)");
}

void AtnelCogDisplay::no_backlight() {
    this->backlight_state_ = false;
    ESP_LOGD(TAG, "Backlight OFF (external GPIO control required)");
}

void AtnelCogDisplay::set_contrast_value(uint8_t value) {
    // Wrap-around behavior (MK_LCD style) - useful for encoder-based adjustment
    if( value > ST7032_CONTRAST_MAX ) {
        value = ST7032_CONTRAST_MIN;
    }
    this->contrast_ = value;
    // Low 4 bits of contrast
    this->write_command_(ST7032_CONTRAST_SET | (value & 0x0F));
    // High 2 bits of contrast + booster ON
    this->write_command_((value >> 4) | ST7032_POWER_ICON | ST7032_POWER_Bon);
}

}  // namespace atnel_cog_2x16
}  // namespace esphome

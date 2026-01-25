#include "Amoled_DisplayPanel.h"
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include <Wire.h>
#include <esp_adc_cal.h>
#include <esp_log.h>

static void waitMs(uint32_t durationMs) {
    uint32_t start = millis();
    while ((int32_t)(millis() - start) < (int32_t)durationMs) {
        yield();
    }
}

static bool g_touchWireStarted = false;

static void pinInputIfValid(int8_t pin) {
    if (pin >= 0) {
        pinMode(pin, INPUT);
    }
}

static void pinOutputLowIfValid(int8_t pin) {
    if (pin >= 0) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
}

static void touchResetIfAvailable(const AmoledHwConfig &hwConfig) {
    if (hwConfig.tp_rst < 0) {
        return;
    }
    pinMode(hwConfig.tp_rst, OUTPUT);
    digitalWrite(hwConfig.tp_rst, LOW);
    waitMs(10);
    digitalWrite(hwConfig.tp_rst, HIGH);
    waitMs(10);
}

static void touchPrepareIntPin(const AmoledHwConfig &hwConfig) {
    if (hwConfig.tp_int < 0) {
        return;
    }
    if (hwConfig.tp_rst < 0) {
        pinMode(hwConfig.tp_int, INPUT_PULLUP);
    } else {
        pinMode(hwConfig.tp_int, INPUT);
    }
}

Amoled_DisplayPanel::Amoled_DisplayPanel(AmoledHwConfig hw_config)
    : hwConfig(hw_config), displayBus(nullptr), display(nullptr), _touchDrv(nullptr), _wakeupMethod(WAKEUP_FROM_NONE),
      _sleepTimeUs(0), currentBrightness(0) {
    _rotation = 0;
}

Amoled_DisplayPanel::~Amoled_DisplayPanel() {
    uninstallSD();

    if (_touchDrv) {
        delete _touchDrv;
        _touchDrv = nullptr;
    }
    if (display) {
        display->setBrightness(0);
        digitalWrite(hwConfig.lcd_en, LOW);
        delete display;
        display = nullptr;
    }
    if (displayBus) {
        delete displayBus;
        displayBus = nullptr;
    }
}

bool Amoled_DisplayPanel::begin(Amoled_Display_Panel_Color_Order order) {
    bool display_ok = true;
    colorOrder = order;

    if (!initTouch()) {
        // Touch is optional for rendering; keep the display usable if touch fails.
        ESP_LOGW("Amoled_DisplayPanel", "Touch init failed; continuing without touch");
    }
    display_ok &= initDisplay(order);

    return display_ok;
}

bool Amoled_DisplayPanel::installSD() {
    if (hwConfig.sd_cs < 0 || hwConfig.sd_sclk < 0 || hwConfig.sd_mosi < 0 || hwConfig.sd_miso < 0) {
        return false;
    }
    pinMode(hwConfig.sd_cs, OUTPUT);
    digitalWrite(hwConfig.sd_cs, HIGH);

    SD_MMC.setPins(hwConfig.sd_sclk, hwConfig.sd_mosi, hwConfig.sd_miso);

    return SD_MMC.begin("/sdcard", true, false);
}

void Amoled_DisplayPanel::uninstallSD() {
    if (hwConfig.sd_cs < 0) {
        return;
    }
    SD_MMC.end();
    digitalWrite(hwConfig.sd_cs, LOW);
    pinMode(hwConfig.sd_cs, INPUT);
}

void Amoled_DisplayPanel::setBrightness(uint8_t level) {
    uint16_t brightness = level * 16;

    brightness = brightness > 255 ? 255 : brightness;
    brightness = brightness < 0 ? 0 : brightness;

    if (brightness > this->currentBrightness) {
        for (int i = this->currentBrightness; i <= brightness; i++) {
            display->setBrightness(i);
            waitMs(1);
        }
    } else {
        for (int i = this->currentBrightness; i >= brightness; i--) {
            display->setBrightness(i);
            waitMs(1);
        }
    }
    this->currentBrightness = brightness;
}

uint8_t Amoled_DisplayPanel::getBrightness() { return (this->currentBrightness + 1) / 16; }

Amoled_Display_Panel_Type Amoled_DisplayPanel::getModel() { return panelType; }

const char *Amoled_DisplayPanel::getTouchModelName() {
    return _touchDrv ? _touchDrv->getModelName() : "unknown";
}

void Amoled_DisplayPanel::enableTouchWakeup() { _wakeupMethod = WAKEUP_FROM_TOUCH; }

void Amoled_DisplayPanel::enableButtonWakeup() { _wakeupMethod = WAKEUP_FROM_BUTTON; }

void Amoled_DisplayPanel::enableTimerWakeup(uint64_t time_in_us) {
    _wakeupMethod = WAKEUP_FROM_TIMER;
    _sleepTimeUs = time_in_us;
}

void Amoled_DisplayPanel::sleep() {
    if (WAKEUP_FROM_NONE == _wakeupMethod) {
        return;
    }

    sleepBrightnessLevel = getBrightness();
    setBrightness(0);
    if (display) {
        display->displayOff();
    }
    pinOutputLowIfValid(hwConfig.lcd_en);
    uninstallSD();

    if (WAKEUP_FROM_TOUCH != _wakeupMethod) {
        if (_touchDrv) {
            pinMode(hwConfig.tp_int, OUTPUT);
            digitalWrite(hwConfig.tp_int, LOW); // Before touch to set sleep, it is necessary to set INT to LOW
            if (hwConfig.tp_rst >= 0) {
                _touchDrv->sleep();
                delete _touchDrv;
                _touchDrv = nullptr;
                touchType = TOUCH_UNKNOWN;
            }
        }
    }

    switch (_wakeupMethod) {
    case WAKEUP_FROM_TOUCH: {
        int16_t x_array[1];
        int16_t y_array[1];
        uint8_t get_point = 1;
        pinMode(hwConfig.tp_int, INPUT);

        // Wait for the finger to be lifted from the screen
        while (!digitalRead(hwConfig.tp_int)) {
            waitMs(100);
            // Clear touch buffer
            getPoint(x_array, y_array, get_point);
        }

        waitMs(2000); // Wait for the interrupt level to stabilize
        esp_sleep_enable_ext1_wakeup(_BV(hwConfig.tp_int), ESP_EXT1_WAKEUP_ANY_LOW);
    } break;
    case WAKEUP_FROM_BUTTON:
        esp_sleep_enable_ext1_wakeup(_BV(0), ESP_EXT1_WAKEUP_ANY_LOW);
        break;
    case WAKEUP_FROM_TIMER:
        esp_sleep_enable_timer_wakeup(_sleepTimeUs);
        break;
    default:
        // Default GPIO0 Wakeup
        esp_sleep_enable_ext1_wakeup(_BV(0), ESP_EXT1_WAKEUP_ANY_LOW);
        break;
    }

    pinInputIfValid(hwConfig.lcd_cs);
    pinInputIfValid(hwConfig.lcd_sclk);
    pinInputIfValid(hwConfig.lcd_sdio0);
    pinInputIfValid(hwConfig.lcd_sdio1);
    pinInputIfValid(hwConfig.lcd_sdio2);
    pinInputIfValid(hwConfig.lcd_sdio3);
    pinInputIfValid(hwConfig.lcd_rst);
    pinInputIfValid(hwConfig.sd_cs);
    pinInputIfValid(hwConfig.sd_sclk);
    pinInputIfValid(hwConfig.sd_mosi);
    pinInputIfValid(hwConfig.sd_miso);

    Wire.end();

    pinInputIfValid(hwConfig.i2c_scl);
    pinInputIfValid(hwConfig.i2c_sda);

    Serial.end();

    esp_deep_sleep_start();
}

bool Amoled_DisplayPanel::wakeup() {
    if (hwConfig.lcd_en >= 0) {
        pinMode(hwConfig.lcd_en, OUTPUT);
        digitalWrite(hwConfig.lcd_en, HIGH);
    }

    // Re-init touch on wake (tp_rst may not be wired).
    if (_touchDrv) {
        delete _touchDrv;
        _touchDrv = nullptr;
    }
    if (!initTouch()) {
        ESP_LOGW("Amoled_DisplayPanel", "Touch init failed on wakeup");
    }

    if (!initDisplay(colorOrder)) {
        ESP_LOGW("Amoled_DisplayPanel", "Display init failed on wakeup");
        return false;
    }

    if (hwConfig.default_rotation >= 0) {
        setRotation(hwConfig.default_rotation);
    } else if (panelType == DISPLAY_1_75_INCHES) {
        setRotation(hwConfig.rotation_175);
    } else {
        setRotation(0);
    }
    if (sleepBrightnessLevel > 0) {
        setBrightness(sleepBrightnessLevel);
    }
    return true;
}

uint8_t Amoled_DisplayPanel::getPoint(int16_t *x_array, int16_t *y_array, uint8_t get_point) {
    if (!_touchDrv || !_touchDrv->isPressed()) {
        return 0;
    }
    if (touchType == TOUCH_CST92XX) {
        return _touchDrv->getPoint(x_array, y_array, _touchDrv->getSupportTouchPoint());
    }

    uint8_t points = _touchDrv->getPoint(x_array, y_array, get_point);

    for (uint8_t i = 0; i < points; i++) {
        int16_t rawX = x_array[i] + hwConfig.lcd_gram_offset_x;
        int16_t rawY = y_array[i] + hwConfig.lcd_gram_offset_y;

        switch (_rotation) {
        case 1: // 90°
            x_array[i] = rawY;
            y_array[i] = width() - rawX;
            break;
        case 2: // 180°
            x_array[i] = width() - rawX;
            y_array[i] = height() - rawY;
            break;
        case 3: // 270°
            x_array[i] = height() - rawY;
            y_array[i] = rawX;
            break;
        default: // 0°
            x_array[i] = rawX;
            y_array[i] = rawY;
            break;
        }
    }

    return points;
}

bool Amoled_DisplayPanel::isPressed() {
    if (_touchDrv) {
        return _touchDrv->isPressed();
    }
    return 0;
}

uint16_t Amoled_DisplayPanel::getBattVoltage(void) {
    if (hwConfig.battery_voltage_adc_data == -1) {
        return 0;
    }
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adc_chars);

    const int number_of_samples = 20;
    uint32_t sum = 0;
    for (int i = 0; i < number_of_samples; i++) {
        sum += analogRead(hwConfig.battery_voltage_adc_data);
        waitMs(2);
    }
    sum = sum / number_of_samples;

    return esp_adc_cal_raw_to_voltage(sum, &adc_chars) * 2;
}

void Amoled_DisplayPanel::pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t *data) {
    if (displayBus && display) {
        display->draw16bitRGBBitmap(x, y, data, width, height);
    }
}

void Amoled_DisplayPanel::setRotation(uint8_t rotation) {
    _rotation = rotation;

    if (displayBus && display) {
        display->setRotation(rotation);
    }
}

bool Amoled_DisplayPanel::initTouch() {
    if (!g_touchWireStarted) {
        Wire.begin(hwConfig.i2c_sda, hwConfig.i2c_scl);
        g_touchWireStarted = true;
    }
    touchResetIfAvailable(hwConfig);
    touchPrepareIntPin(hwConfig);
    auto tryFt = [&](uint8_t addr) -> bool {
        TouchDrvFT6X36 *tmp2 = new TouchDrvFT6X36();
        tmp2->setPins(hwConfig.tp_rst, hwConfig.tp_int);
        if (tmp2->begin(Wire, addr, hwConfig.i2c_sda, hwConfig.i2c_scl)) {
            tmp2->interruptTrigger();
            _touchDrv = tmp2;
            ESP_LOGI("Amoled_DisplayPanel", "Touch FT init ok addr=0x%02X model=%s", addr,
                     _touchDrv->getModelName());
            Wire.setClock(100000);
            Wire.setTimeOut(50);
            touchType = TOUCH_FT3168;
            panelType = DISPLAY_1_43_INCHES;
            return true;
        }
        delete tmp2;
        return false;
    };

    auto tryCst = [&](uint8_t addr) -> bool {
        TouchDrvCSTXXX *tmp = new TouchDrvCSTXXX();
        tmp->setPins(hwConfig.tp_rst, hwConfig.tp_int);
        if (tmp->begin(Wire, addr, hwConfig.i2c_sda, hwConfig.i2c_scl)) {
            _touchDrv = tmp;
            ESP_LOGI("Amoled_DisplayPanel", "Touch CST init ok addr=0x%02X model=%s", addr,
                     _touchDrv->getModelName());
            Wire.setClock(100000);
            Wire.setTimeOut(50);
            tmp->setMaxCoordinates(466, 466);
            if (hwConfig.mirror_touch) {
                tmp->setMirrorXY(true, true);
            }
            touchType = TOUCH_CST92XX;
            panelType = DISPLAY_1_75_INCHES;
            return true;
        }
        delete tmp;
        return false;
    };

    if (tryFt(FT3168_DEVICE_ADDRESS)) {
        return true;
    }
    // CST controllers often use 0x15; keep 0x5A as fallback.
    if (tryCst(0x15)) {
        return true;
    }
    if (tryCst(CST92XX_DEVICE_ADDRESS)) {
        return true;
    }

    ESP_LOGE("Amoled_DisplayPanel", "Unable to find touch device.");
    return false;
}

bool Amoled_DisplayPanel::initDisplay(Amoled_Display_Panel_Color_Order colorOrder) {
    if (displayBus == nullptr) {
        displayBus =
            new Arduino_ESP32QSPI(hwConfig.lcd_cs /* CS */, hwConfig.lcd_sclk /* SCK */, hwConfig.lcd_sdio0 /* SDIO0 */,
                                  hwConfig.lcd_sdio1 /* SDIO1 */, hwConfig.lcd_sdio2 /* SDIO2 */, hwConfig.lcd_sdio3 /* SDIO3 */);

        display = new CO5300(displayBus, hwConfig.lcd_rst /* RST */, _rotation /* rotation */, false /* IPS */,
                             hwConfig.lcd_width, hwConfig.lcd_height, hwConfig.lcd_gram_offset_x /* col offset 1 */,
                             0 /* row offset 1 */, hwConfig.lcd_gram_offset_y /* col_offset2 */, 0 /* row_offset2 */, colorOrder);
    }

    pinMode(hwConfig.lcd_en, OUTPUT);
    digitalWrite(hwConfig.lcd_en, HIGH);

    bool success = display->begin(80000000);
    if (!success) {
        ESP_LOGE("Amoled_DisplayPanel", "Failed to initialize display");
        return false;
    }

    if (hwConfig.default_rotation >= 0) {
        setRotation(hwConfig.default_rotation);
    } else {
        switch (panelType) {
        case DISPLAY_1_75_INCHES:
            setRotation(hwConfig.rotation_175);
            break;
        case DISPLAY_1_43_INCHES:
        case DISPLAY_UNKNOWN:
        default:
            setRotation(0);
            break;
        }
    }

    // required for correct GRAM initialization
    displayBus->writeCommand(CO5300_C_PTLON);
    display->fillScreen(BLACK);

    return success;
}

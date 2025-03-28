
#include <Arduino.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#error "The current version is not supported for the time being, please use a version below Arduino ESP32 3.0"
#endif

/* Please make sure your touch IC model. */
#define TOUCH_USE_CAPACITIVE_TOUCH // uses same logic as examples/CapacitiveTouch.ino
// #define TOUCH_MODULES_CST_MUTUAL
// #define TOUCH_MODULES_CST_SELF

#if defined(TOUCH_MODULES_CST_SELF) || defined(TOUCH_MODULES_CST_SELF)
// #include "TouchLib.h"
#define TOUCH_READ_FROM_INTERRNUPT
#endif

/* The product now has two screens, and the initialization code needs a small change in the new version. The LCD_MODULE_CMD_1 is used to define the
 * switch macro. */
#define LCD_MODULE_CMD_1

#include "lv_conf.h"
#include "lvgl.h" /* https://github.com/lvgl/lvgl.git */
#include "Arduino.h"
#include "Wire.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "pin_config.h"
#include "lv_demo_widgets.h"

#ifdef TOUCH_USE_CAPACITIVE_TOUCH
#include <TouchDrvCSTXXX.hpp>
#define PIN_LCD_BL 38
#define PIN_LCD_D0 39
#define PIN_LCD_D1 40
#define PIN_LCD_D2 41
#define PIN_LCD_D3 42
#define PIN_LCD_D4 45
#define PIN_LCD_D5 46
#define PIN_LCD_D6 47
#define PIN_LCD_D7 48
#define PIN_POWER_ON 15
#define PIN_LCD_RES 5
#define PIN_LCD_CS 6
#define PIN_LCD_DC 7
#define PIN_LCD_WR 8
#define PIN_LCD_RD 9
#define PIN_BUTTON_1 0
#define PIN_BUTTON_2 14
#define PIN_BAT_VOLT 4
#define BOARD_I2C_SCL 17
#define BOARD_I2C_SDA 18
#define BOARD_TOUCH_IRQ 16
#define BOARD_TOUCH_RST 21

TouchDrvCSTXXX touch;
int16_t x[5], y[5];

#endif

esp_lcd_panel_io_handle_t io_handle = NULL;
static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
static lv_disp_drv_t disp_drv;      // contains callback functions
static lv_color_t *lv_disp_buf;
static bool is_initialized_lvgl = false;
#if defined(LCD_MODULE_CMD_1)
typedef struct
{
    uint8_t cmd;
    uint8_t data[14];
    uint8_t len;
} lcd_cmd_t;

lcd_cmd_t lcd_st7789v[] = {
    {0x11, {0}, 0 | 0x80},
    {0x3A, {0X05}, 1},
    {0xB2, {0X0B, 0X0B, 0X00, 0X33, 0X33}, 5},
    {0xB7, {0X75}, 1},
    {0xBB, {0X28}, 1},
    {0xC0, {0X2C}, 1},
    {0xC2, {0X01}, 1},
    {0xC3, {0X1F}, 1},
    {0xC6, {0X13}, 1},
    {0xD0, {0XA7}, 1},
    {0xD0, {0XA4, 0XA1}, 2},
    {0xD6, {0XA1}, 1},
    {0xE0, {0XF0, 0X05, 0X0A, 0X06, 0X06, 0X03, 0X2B, 0X32, 0X43, 0X36, 0X11, 0X10, 0X2B, 0X32}, 14},
    {0xE1, {0XF0, 0X08, 0X0C, 0X0B, 0X09, 0X24, 0X2B, 0X22, 0X43, 0X38, 0X15, 0X16, 0X2F, 0X37}, 14},

};
#endif

#if defined(TOUCH_MODULES_CST_SELF) || defined(TOUCH_MODULES_CST_SELF)
TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS328_SLAVE_ADDRESS, PIN_TOUCH_RES);
bool inited_touch = false;
#if defined(TOUCH_READ_FROM_INTERRNUPT)
bool get_int_signal = false;
#endif
#endif

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    if (is_initialized_lvgl)
    {
        lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
        lv_disp_flush_ready(disp_driver);
    }
    return false;
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

#if defined(TOUCH_MODULES_CST_SELF) || defined(TOUCH_MODULES_CST_SELF) || defined(TOUCH_USE_CAPACITIVE_TOUCH)
static void lv_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
#ifdef TOUCH_READ_FROM_INTERRNUPT
    if (get_int_signal)
    {
        get_int_signal = false;
        touch.read();
#else
#ifdef TOUCH_USE_CAPACITIVE_TOUCH
    if (touch.getPoint(x, y, touch.getSupportTouchPoint()))
    {
        data->point.x = *x;
        data->point.y = *y;
        data->state = LV_INDEV_STATE_PR;
    }
#else
    if (touch.read())
    {
#endif
#endif

        else data->state = LV_INDEV_STATE_REL;
    }
#endif

    void setup()
    {
        Serial.begin(115200);

        // Turn on display power
        pinMode(PIN_POWER_ON, OUTPUT);
        digitalWrite(PIN_POWER_ON, HIGH);

        pinMode(PIN_LCD_RD, OUTPUT);
        digitalWrite(PIN_LCD_RD, HIGH);
        esp_lcd_i80_bus_handle_t i80_bus = NULL;
        esp_lcd_i80_bus_config_t bus_config = {
            .dc_gpio_num = PIN_LCD_DC,
            .wr_gpio_num = PIN_LCD_WR,
            .clk_src = LCD_CLK_SRC_PLL160M,
            .data_gpio_nums =
                {
                    PIN_LCD_D0,
                    PIN_LCD_D1,
                    PIN_LCD_D2,
                    PIN_LCD_D3,
                    PIN_LCD_D4,
                    PIN_LCD_D5,
                    PIN_LCD_D6,
                    PIN_LCD_D7,
                },
            .bus_width = 8,
            .max_transfer_bytes = LVGL_LCD_BUF_SIZE * sizeof(uint16_t),
        };
        esp_lcd_new_i80_bus(&bus_config, &i80_bus);

        esp_lcd_panel_io_i80_config_t io_config = {
            .cs_gpio_num = PIN_LCD_CS,
            .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
            .trans_queue_depth = 20,
            .on_color_trans_done = example_notify_lvgl_flush_ready,
            .user_ctx = &disp_drv,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .dc_levels =
                {
                    .dc_idle_level = 0,
                    .dc_cmd_level = 0,
                    .dc_dummy_level = 0,
                    .dc_data_level = 1,
                },
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));
        esp_lcd_panel_handle_t panel_handle = NULL;
        esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = PIN_LCD_RES,
            .color_space = ESP_LCD_COLOR_SPACE_RGB,
            .bits_per_pixel = 16,
        };
        esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);
        esp_lcd_panel_reset(panel_handle);
        esp_lcd_panel_init(panel_handle);
        esp_lcd_panel_invert_color(panel_handle, true);

        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, true);
        // the gap is LCD panel specific, even panels with the same driver IC, can
        // have different gap value
        esp_lcd_panel_set_gap(panel_handle, 0, 35);
#if defined(LCD_MODULE_CMD_1)
        for (uint8_t i = 0; i < (sizeof(lcd_st7789v) / sizeof(lcd_cmd_t)); i++)
        {
            esp_lcd_panel_io_tx_param(io_handle, lcd_st7789v[i].cmd, lcd_st7789v[i].data, lcd_st7789v[i].len & 0x7f);
            if (lcd_st7789v[i].len & 0x80)
                delay(120);
        }
#endif
        /* Lighten the screen with gradient */
        ledcSetup(0, 10000, 8);
        ledcAttachPin(PIN_LCD_BL, 0);
        for (uint8_t i = 0; i < 0xFF; i++)
        {
            ledcWrite(0, i);
            delay(2);
        }

        lv_init();
        lv_disp_buf = (lv_color_t *)heap_caps_malloc(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

        lv_disp_draw_buf_init(&disp_buf, lv_disp_buf, NULL, LVGL_LCD_BUF_SIZE);
        /*Initialize the display*/
        lv_disp_drv_init(&disp_drv);
        /*Change the following line to your display resolution*/
        disp_drv.hor_res = EXAMPLE_LCD_H_RES;
        disp_drv.ver_res = EXAMPLE_LCD_V_RES;
        disp_drv.flush_cb = example_lvgl_flush_cb;
        disp_drv.draw_buf = &disp_buf;
        disp_drv.user_data = panel_handle;
        lv_disp_drv_register(&disp_drv);

#if defined(TOUCH_MODULES_CST_SELF) || defined(TOUCH_MODULES_CST_SELF)
        /* Register touch brush with LVGL */
        Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL, 800000);
        inited_touch = touch.init();
        if (inited_touch)
        {
            touch.setRotation(1);
            static lv_indev_drv_t indev_drv;
            lv_indev_drv_init(&indev_drv);
            indev_drv.type = LV_INDEV_TYPE_POINTER;
            indev_drv.read_cb = lv_touchpad_read;
            lv_indev_drv_register(&indev_drv);
        }
        is_initialized_lvgl = true;
#if defined(TOUCH_READ_FROM_INTERRNUPT)
        attachInterrupt(
            PIN_TOUCH_INT, []
            { get_int_signal = true; }, FALLING);
#endif
#endif

#ifdef TOUCH_USE_CAPACITIVE_TOUCH
        // Initialize capacitive touch
        touch.setPins(BOARD_TOUCH_RST, BOARD_TOUCH_IRQ);

        if (!touch.begin(Wire, CST328_SLAVE_ADDRESS, BOARD_I2C_SDA, BOARD_I2C_SCL))
        {
            Serial.println("Failed init CST328 Device!");
            if (!touch.begin(Wire, CST816_SLAVE_ADDRESS, BOARD_I2C_SDA, BOARD_I2C_SCL))
            {
                Serial.println("Failed init CST816 Device!");
                while (1)
                {
                    Serial.println("Not find touch device!");
                    delay(1000);
                }
            }
        }

        // fix orientation
        touch.setMaxCoordinates(320, 170);
        touch.setMirrorXY(true, false);
        touch.setSwapXY(true);

        // Press the circular touch button on the screen to get the coordinate point,
        // and set it as the coordinate of the touch button
        // T-Display-S3 CST328 touch panel, touch button coordinates are is 85 , 360
        touch.setCenterButtonCoordinate(85, 360);

        // Depending on the touch panel, not all touch panels have touch buttons.
        touch.setHomeButtonCallback([](void *user_data) {
            Serial.println("Home key pressed!");
            static uint32_t checkMs = 0;
            if (millis() > checkMs) {
                if (digitalRead(PIN_LCD_BL)) {
                    digitalWrite(PIN_LCD_BL, LOW);
                } else {
                    digitalWrite(PIN_LCD_BL, HIGH);
                }
            }
            checkMs = millis() + 200;
        }, NULL);

#endif

        // link
        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = lv_touchpad_read;
        lv_indev_drv_register(&indev_drv);
        is_initialized_lvgl = true;

        lv_demo_widgets();
    }

    void loop()
    {
        lv_timer_handler();
        delay(2);
    }

#include <zephyr/drivers/led.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>

#define TOUCH_LED_NODE DT_NODELABEL(led_touch)
#define TOUCH_LED_IDX DT_NODE_CHILD_IDX(TOUCH_LED_NODE)

static void input_cb(struct input_event *evt, void *user_data)
{
        static const struct device *dev = DEVICE_DT_GET(DT_PARENT(TOUCH_LED_NODE));

        if (evt->code == INPUT_BTN_TOUCH) {
                if (evt->value) {
                        led_on(dev, TOUCH_LED_IDX);
                } else {
                        led_off(dev, TOUCH_LED_IDX);
                }
        }
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(touch_sense)), input_cb, NULL);

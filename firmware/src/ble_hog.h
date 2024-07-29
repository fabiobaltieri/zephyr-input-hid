#include <zephyr/bluetooth/addr.h>
#include <zephyr/device.h>

void ble_hog_set_peer(const struct device *dev, const bt_addr_le_t *peer);

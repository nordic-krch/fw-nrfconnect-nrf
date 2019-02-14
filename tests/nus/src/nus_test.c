#include <zephyr.h>
#include <unity.h>
#include <bluetooth/services/nus.h>
#include "bluetooth/mock_gatt.h"

void nus_received_cb(const u8_t *const data, u16_t len)
{
	printf("asdsadas\n");
}

void nus_sent_cb(const u8_t *data, u16_t len)
{
	printf("asdsadas\n");

}

void test_nus_init(void)
{
//	u8_t buf[8];
	struct bt_nus_cb callbacks = {
		.received_cb = nus_received_cb,
		.sent_cb = nus_sent_cb
	};
	struct bt_gatt_service nus_svc;

	__wrap_bt_gatt_service_register_ExpectAndReturn(NULL, 0);
	__wrap_bt_gatt_service_register_IgnoreArg_svc();
	__wrap_bt_gatt_service_register_ReturnThruPtr_svc(&nus_svc);

	nus_init(&callbacks);

}

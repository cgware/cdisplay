#include "display_driver.h"

display_driver_t *display_driver_find(strv_t name)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != DISPLAY_DRIVER_TYPE) {
			continue;
		}

		display_driver_t *drv = i->data;
		if (drv != NULL && strv_eq(strv_cstr(drv->name), name)) {
			return drv;
		}
	}

	return NULL;
}

u32 display_driver_list(display_driver_t **drivers, u32 capacity)
{
	u32 count = 0;
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != DISPLAY_DRIVER_TYPE) {
			continue;
		}

		display_driver_t *drv = i->data;
		if (drivers != NULL && count < capacity) {
			drivers[count] = drv;
		}
		count++;
	}

	return count;
}

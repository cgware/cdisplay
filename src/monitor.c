#include "monitor.h"

#include "display_driver.h"

int display_monitors(display_t *display, arr_t *monitors)
{
	if (display == NULL || display->drv == NULL || display->drv->monitors == NULL || monitors == NULL ||
	    monitors->size != sizeof(display_monitor_t)) {
		return 1;
	}

	return display->drv->monitors(display, monitors);
}

size_t monitor_print(const display_monitor_t *monitor, dst_t dst)
{
	if (monitor == NULL) {
		return 0;
	}

	return dputf(dst,
		     "monitor id=%u name=\"%s\" pos=%d,%d size=%ux%u physical=%ux%u refresh=%u scale=%u primary=%d native=0x%llx\n",
		     monitor->id,
		     monitor->name,
		     monitor->x,
		     monitor->y,
		     monitor->width,
		     monitor->height,
		     monitor->physical_width,
		     monitor->physical_height,
		     monitor->refresh_rate,
		     monitor->scale,
		     monitor->primary,
		     (u64)(uintptr_t)monitor->native);
}

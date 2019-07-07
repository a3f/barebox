#ifndef __INCLUDE_STPMIC1_H
#define __INCLUDE_STPMIC1_H

#include <driver.h>
#include <i2c/i2c.h>
#include <regmap.h>

struct stpmic1 {
	struct regmap		*map;
	struct device_d		*dev;
	struct i2c_client	*client;
};

#endif

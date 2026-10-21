/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether expressed or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 */

#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/of.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/delay.h>

#include <linux/crc32.h>
#include <asm/byteorder.h>

#define GD3X_GPIO_NUM_PINS 32

#define GD3X_PIN_MODE_OUT	0
#define GD3X_PIN_MODE_IN	1
#define GD3X_PIN_MODE_ADC	2
#define GD3X_PIN_MODE_NONE	3

#define GD3X_INPUT_SV_VOLT_REG		0x1e0		// input sv voltage
#define GD3X_INTERNAL_TEMP_REG		0x1ea		// internal sv temperature
#define GD3X_FW_VER_PROJECT_REG		0x1ec		// ver project code
#define GD3X_FW_VER_BOARD_REG		0x1ed		// ver board code
#define GD3X_FW_VER_ADAPT_REG		0x1ee		// ver adaptation code
#define GD3X_FW_VER_REG				0x1ef		// ver firmware code
#define GD3X_FW_VER_TYPE_REG		0x1f0		// ver firmware type
#define GD3X_BL_VER_REG				0x1ff		// bootloader version

#define GD3X_TEMP_REG				0x504		// temperature
#define GD3X_INT_SWITCH_REG			0x200		// interrupt on-off mask
#define GD3X_INT_SUM_REG			0x201		// fixme: interrupt summary mask
#define GD3X_USB_CONTROL_REG		0x107		// USB power control (auto - 0, manual - 1)
#define GD3X_POEOUT_CONTROL_REG		0x10a		// PoE OUT power control (auto - 0, manual - 1)
#define GD3X_WDT_MARGIN_REG			0x101		// WD toggle period
#define GD3X_HEAT_END_TEMP_REG		0x102		// heater max temp 
#define GD3X_HEAT_END_TIME_REG		0x103		// heater max time 
#define GD3X_HEAT_HYST_REG			0x111		// heater temp hysteresis
#define GD3X_I2C_ERRORS_CNT_REG     0x112		// supervisi2c errorc counter
#define GD3X_VOLT_THRESHOLD_REG		0x110		// in_voltage threshold
#define GD3X_FW_UPGRADE_FLAG		0x1eb		// reboot to bootloader flag (volatile - 0, non volatile - 1)
#define GD3X_SYSTEM_REBOOT			0x1f3		// linux system reboot
#define GD3X_FW_UPGRADE_REG			0x1fc		// reboot to bootloader
#define GD3X_FW_UPGRADE_MAGIC		0xdddddddd	// GD3X_FW_UPGRADE_REG value
#define GD3X_WD_REBOOT_COUNTER		0x1df		// wd reboot counter

#define GD3X_ADC0_REG				0x700
#define GD3X_ADC1_REG				0x701
#define GD3X_ADC2_REG				0x702
#define GD3X_ADC3_REG				0x703
#define GD3X_ADC4_REG				0x704
#define GD3X_ADC5_REG				0x705
#define GD3X_ADC6_REG				0x706
#define GD3X_ADC7_REG				0x707
#define GD3X_ADC8_REG				0x708
#define GD3X_INPUT_VOLT_REG			0x709		// input voltage

#define GD3X_PU0_REG				0x300
#define GD3X_PU1_REG				0x301
#define GD3X_PU2_REG				0x302
#define GD3X_PU3_REG				0x303
#define GD3X_PU4_REG				0x304
#define GD3X_PU5_REG				0x305
#define GD3X_PU6_REG				0x306
#define GD3X_PU7_REG				0x307
#define GD3X_PU8_REG				0x308

#define GD3X_PD0_REG				0x400
#define GD3X_PD1_REG				0x401
#define GD3X_PD2_REG				0x402
#define GD3X_PD3_REG				0x403
#define GD3X_PD4_REG				0x404
#define GD3X_PD5_REG				0x405
#define GD3X_PD6_REG				0x406
#define GD3X_PD7_REG				0x407
#define GD3X_PD8_REG				0x408

#define GD3X_UPTIME					0x5ff

/* transfer frame: address flag of a write request, address of an error response */
#define GD3X_XFER_WRITE		BIT(31)
#define GD3X_XFER_ERROR		0xffff
/* data of an error response */
#define GD3X_XFER_ERR_READ	0	// read failed
#define GD3X_XFER_ERR_WRITE	1	// write failed
#define GD3X_XFER_ERR_CRC	2	// request checksum mismatch

#define GD3X_CHAN(_index) \
	{ \
		.type = IIO_VOLTAGE, \
		.indexed = 1, \
		.channel = _index, \
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) \
				| BIT(IIO_CHAN_INFO_SCALE), \
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SAMP_FREQ), \
	}

/**
 * struct gd3x - GPIO driver data
 * @chip: GPIO controller chip
 * @client: I2C device pointer
 * @lock: Protects write sequences
 */
struct gd3x {
	struct gpio_chip chip;
	struct i2c_client *client;
	struct mutex lock;
	u32 index;
	u32 err_count;
	const struct gd3x_variant *variant;
};

struct gd3x_pin_config {
    u32 addr;    // pin addr
    u8 mode;   // GPIO mode
    char*   name;   // GPIO name
    bool    hwirq;  // connected to hardware interrupt (only one pin can have true)
};

/**
 * struct gd3x_variant - board specific supervisor register map
 * @pins: GPIO lines, GD3X_GPIO_NUM_PINS entries
 * @adc: ADC channels, @num_adc entries
 * @channels: IIO channels, @num_adc entries
 * @num_adc: number of ADC channels
 * @group: sysfs attributes
 */
struct gd3x_variant {
	const struct gd3x_pin_config *pins;
	const struct gd3x_pin_config *adc;
	const struct iio_chan_spec *channels;
	unsigned int num_adc;
	const struct attribute_group *group;
};

/* LT70 */
static const struct gd3x_pin_config gd3x_lt70_pins[] =
{
    // addr         GPIO mode           GPIO name   hwirq
    { GD3X_PU0_REG, GD3X_PIN_MODE_OUT, "gpio0"    , 0 }, // used as PULL_UP0
    { GD3X_PU1_REG, GD3X_PIN_MODE_OUT, "gpio1"    , 0 }, // used as PULL_UP1
    { GD3X_PU2_REG, GD3X_PIN_MODE_OUT, "gpio2"    , 0 }, // used as PULL_UP2
    { GD3X_PU3_REG, GD3X_PIN_MODE_OUT, "gpio3"    , 0 }, // used as PULL_UP3
    { GD3X_PU4_REG, GD3X_PIN_MODE_OUT, "gpio4"    , 0 }, // used as PULL_UP4
    { GD3X_PU5_REG, GD3X_PIN_MODE_OUT, "gpio5"    , 0 }, // used as PULL_UP5
    { GD3X_PU6_REG, GD3X_PIN_MODE_OUT, "gpio6"    , 0 }, // used as PULL_UP6
    { GD3X_PU7_REG, GD3X_PIN_MODE_OUT, "gpio7"    , 0 }, // used as PULL_UP7
	{ GD3X_PU8_REG, GD3X_PIN_MODE_OUT, "gpio8"    , 0 }, // used as PULL_UP8
    { GD3X_PD0_REG, GD3X_PIN_MODE_OUT, "gpio9"    , 0 }, // used as PULL_DOWN0
    { GD3X_PD1_REG, GD3X_PIN_MODE_OUT, "gpio10"   , 0 }, // used as PULL_DOWN1
    { GD3X_PD2_REG, GD3X_PIN_MODE_OUT, "gpio11"   , 0 }, // used as PULL_DOWN2
    { GD3X_PD3_REG, GD3X_PIN_MODE_OUT, "gpio12"   , 0 }, // used as PULL_DOWN3
    { GD3X_PD4_REG, GD3X_PIN_MODE_OUT, "gpio13"   , 0 }, // used as PULL_DOWN4
    { GD3X_PD5_REG, GD3X_PIN_MODE_OUT, "gpio14"   , 0 }, // used as PULL_DOWN5
    { GD3X_PD6_REG, GD3X_PIN_MODE_OUT, "gpio15"   , 0 }, // used as PULL_DOWN6
    { GD3X_PD7_REG, GD3X_PIN_MODE_OUT, "gpio16"   , 0 }, // used as PULL_DOWN7
	{ GD3X_PD8_REG, GD3X_PIN_MODE_OUT, "gpio17"   , 0 }, // used as PULL_DOWN8
	{ 0x105, GD3X_PIN_MODE_OUT, "gpio18"   , 0 }, // used as 7.5V switch
	{ 0x106, GD3X_PIN_MODE_OUT, "gpio19"   , 0 }, // used as PoE Out power switch
	{ 0x203, GD3X_PIN_MODE_NONE, "gpio20"   , 0 }, // reserved
	{ 0x108, GD3X_PIN_MODE_OUT, "gpio21"   , 0 }, // used as USB power switch
	{ 0x109, GD3X_PIN_MODE_OUT, "gpio22"   , 0 }, // used as WIFI led
	{ 0x203, GD3X_PIN_MODE_NONE, "gpio23"   , 0 }, // reserved
	{ 0x502, GD3X_PIN_MODE_IN, "gpio24"   , 0 }, // used as PoE error INT
	{ 0x503, GD3X_PIN_MODE_IN, "gpio25"   , 0 }, // used as USB error INT
	{ 0x501, GD3X_PIN_MODE_IN, "gpio26"   , 0 }, // used as heating INT
	{ 0x505, GD3X_PIN_MODE_IN, "gpio27"   , 0 }, // used as wd INT
	{ 0x506, GD3X_PIN_MODE_IN, "gpio28"   , 0 }, // used as voltage INT
	{ 0x203, GD3X_PIN_MODE_NONE, "gpio29"   , 0 }, // reserved
	{ 0x203, GD3X_PIN_MODE_NONE, "gpio30"   , 0 }, // reserved
	{ 0x203, GD3X_PIN_MODE_NONE, "gpio31"   , 0 } // reserved
};
static_assert(ARRAY_SIZE(gd3x_lt70_pins) == GD3X_GPIO_NUM_PINS);

static const struct gd3x_pin_config gd3x_lt70_adc[] =
{
    // addr          GPIO mode           GPIO name   hwirq
    { GD3X_ADC0_REG,   GD3X_PIN_MODE_ADC , "adc0"    , 0 }, // used as adc0
    { GD3X_ADC1_REG,   GD3X_PIN_MODE_ADC , "adc1"    , 0 }, // used as adc1
    { GD3X_ADC2_REG,   GD3X_PIN_MODE_ADC , "adc2"    , 0 }, // used as adc2
    { GD3X_ADC3_REG,   GD3X_PIN_MODE_ADC , "adc3"    , 0 }, // used as adc3
    { GD3X_ADC4_REG,   GD3X_PIN_MODE_ADC , "adc4"    , 0 }, // used as adc4
    { GD3X_ADC5_REG,   GD3X_PIN_MODE_ADC , "adc5"    , 0 }, // used as adc5
    { GD3X_ADC6_REG,   GD3X_PIN_MODE_ADC , "adc6"    , 0 }, // used as adc6
    { GD3X_ADC7_REG,   GD3X_PIN_MODE_ADC , "adc7"    , 0 }, // used as adc7
    { GD3X_ADC8_REG,   GD3X_PIN_MODE_ADC , "adc8"    , 0 }  // used as adc8
};

/* RTU1068v5 */
static const struct gd3x_pin_config gd3x_rtu1068v5_pins[] =
{
	// addr         GPIO mode           GPIO name   hwirq
	{ GD3X_PU0_REG, GD3X_PIN_MODE_OUT, "gpio0"    , 0 }, // used as PULL_UP0
	{ GD3X_PU1_REG, GD3X_PIN_MODE_OUT, "gpio1"    , 0 }, // used as PULL_UP1
	{ GD3X_PU2_REG, GD3X_PIN_MODE_OUT, "gpio2"    , 0 }, // used as PULL_UP2
	{ GD3X_PU3_REG, GD3X_PIN_MODE_OUT, "gpio3"    , 0 }, // used as PULL_UP3
	{ GD3X_PU4_REG, GD3X_PIN_MODE_NONE, "gpio4"    , 0 }, // reserved
	{ GD3X_PU5_REG, GD3X_PIN_MODE_NONE, "gpio5"    , 0 }, // reserved
	{ GD3X_PU6_REG, GD3X_PIN_MODE_NONE, "gpio6"    , 0 }, // reserved
	{ GD3X_PU7_REG, GD3X_PIN_MODE_NONE, "gpio7"    , 0 }, // reserved
	{ GD3X_PU8_REG, GD3X_PIN_MODE_NONE, "gpio8"    , 0 }, // reserved
	{ GD3X_PD0_REG, GD3X_PIN_MODE_OUT, "gpio9"    , 0 }, // used as PULL_DOWN0
	{ GD3X_PD1_REG, GD3X_PIN_MODE_OUT, "gpio10"   , 0 }, // used as PULL_DOWN1
	{ GD3X_PD2_REG, GD3X_PIN_MODE_OUT, "gpio11"   , 0 }, // used as PULL_DOWN2
	{ GD3X_PD3_REG, GD3X_PIN_MODE_OUT, "gpio12"   , 0 }, // used as PULL_DOWN3
	{ GD3X_PD4_REG, GD3X_PIN_MODE_OUT, "gpio13"   , 0 }, // beeper
	{ GD3X_PD5_REG, GD3X_PIN_MODE_NONE, "gpio14"   , 0 }, // reserved
	{ GD3X_PD6_REG, GD3X_PIN_MODE_NONE, "gpio15"   , 0 }, // reserved
	{ GD3X_PD7_REG, GD3X_PIN_MODE_NONE, "gpio16"   , 0 }, // reserved
	{ GD3X_PD8_REG, GD3X_PIN_MODE_NONE, "gpio17"   , 0 }, // reserved
	{ 0x105, GD3X_PIN_MODE_NONE, "gpio18"   , 0 }, // reserved
	{ 0x106, GD3X_PIN_MODE_NONE, "gpio19"   , 0 }, // reserved
	{ 0x203, GD3X_PIN_MODE_NONE, "gpio20"   , 0 }, // reserved
	{ 0x108, GD3X_PIN_MODE_NONE, "gpio21"   , 0 }, // reserved
	{ 0x109, GD3X_PIN_MODE_NONE, "gpio22"   , 0 }, // reserved
	{ 0x203, GD3X_PIN_MODE_NONE, "gpio23"   , 0 }, // reserved
	{ 0x502, GD3X_PIN_MODE_NONE, "gpio24"   , 0 }, // reserved
	{ 0x503, GD3X_PIN_MODE_NONE, "gpio25"   , 0 }, // reserved
	{ 0x501, GD3X_PIN_MODE_NONE, "gpio26"   , 0 }, // reserved
	{ 0x505, GD3X_PIN_MODE_IN, "gpio27"   , 0 }, // used as wd INT
	{ 0x506, GD3X_PIN_MODE_IN, "gpio28"   , 0 }, // used as voltage INT
	{ 0x203, GD3X_PIN_MODE_NONE, "gpio29"   , 0 }, // reserved
	{ 0x203, GD3X_PIN_MODE_NONE, "gpio30"   , 0 }, // reserved
	{ 0x203, GD3X_PIN_MODE_NONE, "gpio31"   , 0 }  // reserved
};
static_assert(ARRAY_SIZE(gd3x_rtu1068v5_pins) == GD3X_GPIO_NUM_PINS);

/* Placeholder: copy of the LT70 ADC map, keep in sync with gd3x_rtu1068v5_channels */
static const struct gd3x_pin_config gd3x_rtu1068v5_adc[] =
{
	// addr          GPIO mode           GPIO name   hwirq
	{ GD3X_ADC0_REG,   GD3X_PIN_MODE_ADC , "adc0"    , 0 }, // used as adc0
	{ GD3X_ADC1_REG,   GD3X_PIN_MODE_ADC , "adc1"    , 0 }, // used as adc1
	{ GD3X_ADC2_REG,   GD3X_PIN_MODE_ADC , "adc2"    , 0 }, // used as adc2
	{ GD3X_ADC3_REG,   GD3X_PIN_MODE_ADC , "adc3"    , 0 }, // used as adc3
	{ GD3X_ADC4_REG,   GD3X_PIN_MODE_ADC , "adc4"    , 0 }, // used as adc4
	{ GD3X_ADC5_REG,   GD3X_PIN_MODE_ADC , "adc5"    , 0 }, // used as adc5
	{ GD3X_ADC6_REG,   GD3X_PIN_MODE_ADC , "adc6"    , 0 }, // used as adc6
	{ GD3X_ADC7_REG,   GD3X_PIN_MODE_ADC , "adc7"    , 0 }, // used as adc7
	{ GD3X_ADC8_REG,   GD3X_PIN_MODE_ADC , "adc8"    , 0 }  // used as adc8
};

/* Request and response frame, little-endian on the wire */
struct gd3x_frame {
	__le32 index;
	__le32 address;
	__le32 data;
	__le32 crc;
} __packed;

static int gd3x_xfer(struct gd3x *data, bool write, u32 reg, u32 word, bool log)
{
	struct i2c_client *client = data->client;
	struct gd3x_frame rx = { }, tx = { };
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(tx),
			.buf = (u8 *)&tx,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = sizeof(rx),
			.buf = (u8 *)&rx,
		}
	};
	u32 tx_index, index, address, value, crc;
	int ret;

	if (write)
		reg |= GD3X_XFER_WRITE;

	data->index += 1;
	tx_index = data->index;

	tx.index = cpu_to_le32(tx_index);
	tx.address = cpu_to_le32(reg);
	tx.data = cpu_to_le32(word);
	tx.crc = cpu_to_le32(crc32(0, &tx, offsetof(struct gd3x_frame, crc)));

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs)) {
		if (log)
			dev_err_ratelimited(&client->dev,
					    "transfer error %d when accessing the address 0x%x\n",
					    ret, reg);
		return -EIO;
	}

	index = le32_to_cpu(rx.index);
	address = le32_to_cpu(rx.address);
	value = le32_to_cpu(rx.data);
	crc = crc32(0, &rx, offsetof(struct gd3x_frame, crc));

	if (address == GD3X_XFER_ERROR) {
		if (log) {
			switch (value) {
			case GD3X_XFER_ERR_READ:
				dev_err_ratelimited(&client->dev,
						    "sv: cannot read from 0x%x address\n", reg);
				break;
			case GD3X_XFER_ERR_WRITE:
				dev_err_ratelimited(&client->dev,
						    "sv: cannot write to 0x%x address\n", reg);
				break;
			case GD3X_XFER_ERR_CRC:
				dev_err_ratelimited(&client->dev,
						    "sv: checksum does not match when accessing the address 0x%x\n",
						    reg);
				break;
			default:
				dev_err_ratelimited(&client->dev,
						    "sv: unknown error %u when accessing the address 0x%x\n",
						    value, reg);
				break;
			}
		}
		data->err_count += 1;
		return -EIO;
	}

	if (crc != le32_to_cpu(rx.crc)) {
		if (log)
			dev_err_ratelimited(&client->dev,
					    "wrong checksum: expected 0x%x, but received 0x%x (index: 0x%x; address: 0x%x; data: 0x%x)\n",
					    crc, le32_to_cpu(rx.crc), index, address, value);
		data->err_count += 1;
		return -EIO;
	}
	if (address != reg) {
		if (log)
			dev_err_ratelimited(&client->dev,
					    "wrong address: expected 0x%x, but received 0x%x\n",
					    reg, address);
		data->err_count += 1;
		return -EIO;
	}
	if (index != tx_index) {
		if (log)
			dev_err_ratelimited(&client->dev,
					    "wrong index: expected 0x%x, but received 0x%x\n",
					    tx_index, index);
		data->err_count += 1;
		return -EIO;
	}

	if (write)
		return 0;

	return value;
}

/* A failed transfer is retried once, only the retry is logged */
static int gd3x_write(struct gd3x *data, u32 reg, u32 word)
{
	int ret;

	ret = gd3x_xfer(data, true, reg, word, false);
	if (ret == -EIO)
		ret = gd3x_xfer(data, true, reg, word, true);
	return ret;
}

static int gd3x_read(struct gd3x *data, u32 reg)
{
	int ret;

	ret = gd3x_xfer(data, false, reg, 0, false);
	if (ret == -EIO)
		ret = gd3x_xfer(data, false, reg, 0, true);
	return ret;
}

static void gd3x_set(struct gpio_chip *chip, unsigned offset, int value){
	struct gd3x *gpio = gpiochip_get_data(chip);
	gd3x_write(gpio, gpio->variant->pins[offset].addr, value);
}

static int gd3x_get(struct gpio_chip *chip, unsigned offset)
{
	struct gd3x *gpio = gpiochip_get_data(chip);
	int		value;
	value = gd3x_read(gpio, gpio->variant->pins[offset].addr);
	return value;
}

static int gd3x_get_direction(struct gpio_chip *chip,
				  unsigned offset)
{
	struct gd3x *gpio = gpiochip_get_data(chip);

	if (gpio->variant->pins[offset].mode == GD3X_PIN_MODE_IN)
		return GPIO_LINE_DIRECTION_IN;

	return GPIO_LINE_DIRECTION_OUT;
}

static int gd3x_direction_input(struct gpio_chip *chip,
				    unsigned offset)
{
	struct gd3x *gpio = gpiochip_get_data(chip);

	if (gpio->variant->pins[offset].mode != GD3X_PIN_MODE_IN)
		return -EPERM;

	return 0;
}

static int gd3x_direction_output(struct gpio_chip *chip,
				     unsigned offset, int value)
{
	struct gd3x *gpio = gpiochip_get_data(chip);

	if (gpio->variant->pins[offset].mode != GD3X_PIN_MODE_OUT)
		return -EPERM;

	return gd3x_write(gpio, gpio->variant->pins[offset].addr, value);
}

static int gd3x_init_valid_mask(struct gpio_chip *chip,
				unsigned long *valid_mask,
				unsigned int ngpios)
{
	struct gd3x *gpio = gpiochip_get_data(chip);
	unsigned int i;

	for (i = 0; i < ngpios; i++)
		if (gpio->variant->pins[i].mode == GD3X_PIN_MODE_NONE)
			clear_bit(i, valid_mask);

	return 0;
}

static int gd3x_adc_read_raw(struct iio_dev *iio,
			struct iio_chan_spec const *channel, int *val,
			int *val2, long mask)
{
	struct gd3x *adc = iio_priv(iio);
	int ret;

	ret = gd3x_read(adc, adc->variant->adc[channel->channel].addr);
	if (ret < 0)
		return ret;

	*val = ret;
	return IIO_VAL_INT;
}

static const struct gpio_chip template_chip = {
	.label			= "gd3x",
	.owner			= THIS_MODULE,
	.get			= gd3x_get,
	.get_direction		= gd3x_get_direction,
	.direction_input	= gd3x_direction_input,
	.direction_output	= gd3x_direction_output,
	.set			= gd3x_set,
	.init_valid_mask	= gd3x_init_valid_mask,
	.base			= -1,
	.ngpio			= GD3X_GPIO_NUM_PINS,
	.can_sleep		= true,
};

static const struct iio_info gd3x_adc_info = {
	.read_raw = gd3x_adc_read_raw,
};

static const struct iio_chan_spec gd3x_lt70_channels[] = {
	GD3X_CHAN(0),
	GD3X_CHAN(1),
	GD3X_CHAN(2),
	GD3X_CHAN(3),
	GD3X_CHAN(4),
	GD3X_CHAN(5),
	GD3X_CHAN(6),
	GD3X_CHAN(7),
	GD3X_CHAN(8),
};
static_assert(ARRAY_SIZE(gd3x_lt70_channels) == ARRAY_SIZE(gd3x_lt70_adc));

/* Placeholder: copy of the LT70 channels, keep in sync with gd3x_rtu1068v5_adc */
static const struct iio_chan_spec gd3x_rtu1068v5_channels[] = {
	GD3X_CHAN(0),
	GD3X_CHAN(1),
	GD3X_CHAN(2),
	GD3X_CHAN(3),
	GD3X_CHAN(4),
	GD3X_CHAN(5),
	GD3X_CHAN(6),
	GD3X_CHAN(7),
	GD3X_CHAN(8),
};
static_assert(ARRAY_SIZE(gd3x_rtu1068v5_channels) == ARRAY_SIZE(gd3x_rtu1068v5_adc));

static ssize_t gd3x_attribute_show(struct device *dev, char *buf, u32 reg)
{
	struct gd3x *data = dev_get_drvdata(dev);
	int value;

	value = gd3x_read(data, reg);
	if (value < 0)
		return value;

	return sysfs_emit(buf, "%d\n", value);
}

static ssize_t gd3x_attribute_store(struct device *dev, const char *buf,
				    size_t count, u32 reg)
{
	struct gd3x *data = dev_get_drvdata(dev);
	u32 value;
	int err;

	err = kstrtou32(buf, 10, &value);
	if (err)
		return err;

	err = gd3x_write(data, reg, value);
	if (err)
		return err;

	return count;
}

/* Attributes that read or write a single supervisor register */
#define GD3X_SHOW(_name, _reg)						\
static ssize_t _name##_show(struct device *dev,				\
			    struct device_attribute *da, char *buf)	\
{									\
	return gd3x_attribute_show(dev, buf, _reg);			\
}

#define GD3X_STORE(_name, _reg)						\
static ssize_t _name##_store(struct device *dev,			\
			     struct device_attribute *da,		\
			     const char *buf, size_t count)		\
{									\
	return gd3x_attribute_store(dev, buf, count, _reg);		\
}

#define GD3X_ATTR_RO(_name, _reg)					\
	GD3X_SHOW(_name, _reg)						\
	static DEVICE_ATTR_RO(_name)

#define GD3X_ATTR_RW(_name, _reg)					\
	GD3X_SHOW(_name, _reg)						\
	GD3X_STORE(_name, _reg)						\
	static DEVICE_ATTR_RW(_name)

GD3X_ATTR_RO(input_voltage, GD3X_INPUT_VOLT_REG);
GD3X_ATTR_RO(int_sum, GD3X_INT_SUM_REG);
GD3X_ATTR_RO(sv_i2c_errors, GD3X_I2C_ERRORS_CNT_REG);
GD3X_ATTR_RO(sv_temp, GD3X_INTERNAL_TEMP_REG);
GD3X_ATTR_RO(sv_voltage, GD3X_INPUT_SV_VOLT_REG);
GD3X_ATTR_RO(temp, GD3X_TEMP_REG);
GD3X_ATTR_RO(uptime, GD3X_UPTIME);
GD3X_ATTR_RO(wd_reboot_counter, GD3X_WD_REBOOT_COUNTER);

GD3X_ATTR_RW(fw_upg_flag, GD3X_FW_UPGRADE_FLAG);
GD3X_ATTR_RW(heat_end_temp, GD3X_HEAT_END_TEMP_REG);
GD3X_ATTR_RW(heat_end_time, GD3X_HEAT_END_TIME_REG);
GD3X_ATTR_RW(heat_hyst, GD3X_HEAT_HYST_REG);
GD3X_ATTR_RW(int_switch, GD3X_INT_SWITCH_REG);
GD3X_ATTR_RW(poeout_control, GD3X_POEOUT_CONTROL_REG);
GD3X_ATTR_RW(system_reboot, GD3X_SYSTEM_REBOOT);
GD3X_ATTR_RW(usb_control, GD3X_USB_CONTROL_REG);
GD3X_ATTR_RW(voltage_threshold, GD3X_VOLT_THRESHOLD_REG);
GD3X_ATTR_RW(wdt_margin, GD3X_WDT_MARGIN_REG);

static const u32 gd3x_fw_version_regs[] = {
	GD3X_FW_VER_PROJECT_REG,
	GD3X_FW_VER_BOARD_REG,
	GD3X_FW_VER_ADAPT_REG,
	GD3X_FW_VER_REG,
	GD3X_FW_VER_TYPE_REG,
};

static int gd3x_read_fw_version(struct gd3x *data, int *ver)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gd3x_fw_version_regs); i++) {
		ver[i] = gd3x_read(data, gd3x_fw_version_regs[i]);
		if (ver[i] < 0)
			return ver[i];
	}

	return 0;
}

static ssize_t fw_version_show(struct device *dev, struct device_attribute *da,
			       char *buf)
{
	struct gd3x *data = dev_get_drvdata(dev);
	int ver[ARRAY_SIZE(gd3x_fw_version_regs)];

	/* "N/A" is shown as is by the board info scripts */
	if (gd3x_read_fw_version(data, ver))
		return sysfs_emit(buf, "N/A\n");

	return sysfs_emit(buf, "%d.%d.%d.%d.%d\n",
			  ver[0], ver[1], ver[2], ver[3], ver[4]);
}

static ssize_t bl_version_show(struct device *dev, struct device_attribute *da,
			       char *buf)
{
	struct gd3x *data = dev_get_drvdata(dev);
	int value;

	value = gd3x_read(data, GD3X_BL_VER_REG);
	if (value < 0)
		return value;

	return sysfs_emit(buf, "%d.%d.%d.%d\n", (value & 0xff000000) >> 24,
			  (value & 0xff0000) >> 16, (value & 0xff00) >> 8,
			  value & 0xff);
}

static ssize_t i2c_errors_show(struct device *dev, struct device_attribute *da,
			       char *buf)
{
	struct gd3x *data = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", data->err_count);
}

static ssize_t fw_upgrade_store(struct device *dev, struct device_attribute *da,
				const char *buf, size_t count)
{
	struct gd3x *data = dev_get_drvdata(dev);
	u32 value;
	int err;

	err = kstrtou32(buf, 10, &value);
	if (err)
		return err;
	if (value != 1)
		return -EINVAL;
	//fixme
	err = gd3x_write(data, GD3X_FW_UPGRADE_REG, GD3X_FW_UPGRADE_MAGIC);
	if (err)
		return err;

	return count;
}

static DEVICE_ATTR_RO(bl_version);
static DEVICE_ATTR_RO(fw_version);
static DEVICE_ATTR_RO(i2c_errors);
static DEVICE_ATTR_WO(fw_upgrade);

static void gd3x_print_fw_version(struct gd3x *data)
{
	int ver[ARRAY_SIZE(gd3x_fw_version_regs)];

	if (gd3x_read_fw_version(data, ver)) {
		dev_warn(&data->client->dev, "cannot read firmware version\n");
		return;
	}

	dev_info(&data->client->dev, "firmware version %d.%d.%d.%d.%d\n",
		 ver[0], ver[1], ver[2], ver[3], ver[4]);
}

/*-----------------------------------------------------------------------*/
/* sysfs attributes for gd3x */

static struct attribute *gd3x_lt70_attrs[] = {
	&dev_attr_bl_version.attr,
	&dev_attr_fw_upg_flag.attr,
	&dev_attr_fw_upgrade.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_heat_end_temp.attr,
	&dev_attr_heat_end_time.attr,
	&dev_attr_heat_hyst.attr,
	&dev_attr_i2c_errors.attr,
	&dev_attr_input_voltage.attr,
	&dev_attr_int_sum.attr,
	&dev_attr_int_switch.attr,
	&dev_attr_poeout_control.attr,
	&dev_attr_sv_i2c_errors.attr,
	&dev_attr_sv_temp.attr,
	&dev_attr_sv_voltage.attr,
	&dev_attr_system_reboot.attr,
	&dev_attr_temp.attr,
	&dev_attr_uptime.attr,
	&dev_attr_usb_control.attr,
	&dev_attr_voltage_threshold.attr,
	&dev_attr_wd_reboot_counter.attr,
	&dev_attr_wdt_margin.attr,
	NULL
};
static const struct attribute_group gd3x_lt70_group = {
	.attrs = gd3x_lt70_attrs,
};

/* Placeholder: copy of the LT70 attributes, to be filled in for RTU1068v5 */
static struct attribute *gd3x_rtu1068v5_attrs[] = {
	&dev_attr_bl_version.attr,
	&dev_attr_fw_upg_flag.attr,
	&dev_attr_fw_upgrade.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_i2c_errors.attr,
	&dev_attr_input_voltage.attr,
	&dev_attr_int_sum.attr,
	&dev_attr_int_switch.attr,
	&dev_attr_sv_i2c_errors.attr,
	&dev_attr_sv_temp.attr,
	&dev_attr_sv_voltage.attr,
	&dev_attr_system_reboot.attr,
	&dev_attr_temp.attr,
	&dev_attr_uptime.attr,
	&dev_attr_voltage_threshold.attr,
	&dev_attr_wd_reboot_counter.attr,
	&dev_attr_wdt_margin.attr,
	NULL
};
static const struct attribute_group gd3x_rtu1068v5_group = {
	.attrs = gd3x_rtu1068v5_attrs,
};
/*-----------------------------------------------------------------------*/

static const struct gd3x_variant gd3x_lt70 = {
	.pins		= gd3x_lt70_pins,
	.adc		= gd3x_lt70_adc,
	.channels	= gd3x_lt70_channels,
	.num_adc	= ARRAY_SIZE(gd3x_lt70_channels),
	.group		= &gd3x_lt70_group,
};

static const struct gd3x_variant gd3x_rtu1068v5 = {
	.pins		= gd3x_rtu1068v5_pins,
	.adc		= gd3x_rtu1068v5_adc,
	.channels	= gd3x_rtu1068v5_channels,
	.num_adc	= ARRAY_SIZE(gd3x_rtu1068v5_channels),
	.group		= &gd3x_rtu1068v5_group,
};

static const struct of_device_id gd3x_of_match_table[] = {
	{ .compatible = "teleofis,lt70-gd3x", .data = &gd3x_lt70 },
	{ .compatible = "teleofis,rtu1068v5-gd3x", .data = &gd3x_rtu1068v5 },
	{ .compatible = "sz,gd3x", .data = &gd3x_lt70 },	// legacy, LT70 map
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, gd3x_of_match_table);

static int gd3x_probe(struct i2c_client *client)
{
	const struct gd3x_variant *variant;
	struct gd3x *gpio;
	struct iio_dev *indio_dev;

	int ret;

	variant = i2c_get_match_data(client);
	if (!variant)
		return -ENODEV;

	/* driver data lives in the IIO device, freed after everything below */
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*gpio));
	if (!indio_dev)
		return -ENOMEM;
	gpio = iio_priv(indio_dev);

	i2c_set_clientdata(client, gpio);

	gpio->chip = template_chip;
	gpio->chip.parent = &client->dev;

	gpio->client = client;
	gpio->variant = variant;

	mutex_init(&gpio->lock);

	gd3x_print_fw_version(gpio);

	ret = devm_gpiochip_add_data(&client->dev, &gpio->chip, gpio);
	if (ret < 0) {
		dev_err(&client->dev, "Unable to register gpiochip\n");
		return ret;
	}
	ret = devm_device_add_group(&client->dev, variant->group);
	if (ret < 0) {
		dev_err(&client->dev, "Unable to create sysfs attributes\n");
		return ret;
	}

	indio_dev->name = dev_name(&client->dev);
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &gd3x_adc_info;
	indio_dev->channels = variant->channels;
	indio_dev->num_channels = variant->num_adc;
	ret = devm_iio_device_register(&client->dev, indio_dev);
	if (ret < 0)
		return ret;

	return 0;
}

static const struct i2c_device_id gd3x_id_table[] = {
	{ "gd3x", (kernel_ulong_t)&gd3x_lt70 },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, gd3x_id_table);

static struct i2c_driver gd3x_driver = {
	.driver = {
		.name = "gd3x",
		.of_match_table = gd3x_of_match_table,
	},
	.probe = gd3x_probe,
	.id_table = gd3x_id_table,
};
module_i2c_driver(gd3x_driver);

MODULE_DESCRIPTION("GD3X 32Bit Driver GPIO Driver");
MODULE_LICENSE("GPL v2");

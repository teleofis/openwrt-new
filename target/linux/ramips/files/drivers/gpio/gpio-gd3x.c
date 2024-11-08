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

#define GD3X_P48_ID 48

#define GD3X_GPIO_NUM_PINS 32
#define GD3X_ADC_NUM_PINS 9

#define GD3X_PIN_MODE_OUT 0
#define GD3X_PIN_MODE_IN  1
#define GD3X_PIN_MODE_ADC 2

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
#define GD3X_FOO					0x888

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
 * @buffer: Buffer for device register
 * @lock: Protects write sequences
 */
struct gd3x {
	struct gpio_chip chip;
	struct i2c_client *client;
	u16 buffer;
	struct mutex lock;
	u32 index;
	u32 err_count;
};

struct gd3x_pin_config {
    u32 addr;    // pin addr
    u8 mode;   // GPIO mode
    char*   name;   // GPIO name
    bool    hwirq;  // connected to hardware interrupt (only one pin can have true)
};

struct gd3x_pin_config gd3x_board_config[GD3X_GPIO_NUM_PINS] =
{
    // addr         GPIO mode           GPIO name   hwirq
    { GD3X_PU0_REG, GD3X_PIN_MODE_OUT , "gpio0"    , 0 }, // used as PULL_UP0
    { GD3X_PU1_REG, GD3X_PIN_MODE_OUT , "gpio1"    , 0 }, // used as PULL_UP1
    { GD3X_PU2_REG, GD3X_PIN_MODE_OUT , "gpio2"    , 0 }, // used as PULL_UP2
    { GD3X_PU3_REG, GD3X_PIN_MODE_OUT , "gpio3"    , 0 }, // used as PULL_UP3
    { GD3X_PU4_REG, GD3X_PIN_MODE_OUT , "gpio4"    , 0 }, // used as PULL_UP4
    { GD3X_PU5_REG, GD3X_PIN_MODE_OUT , "gpio5"    , 0 }, // used as PULL_UP5
    { GD3X_PU6_REG, GD3X_PIN_MODE_OUT , "gpio6"    , 0 }, // used as PULL_UP6
    { GD3X_PU7_REG, GD3X_PIN_MODE_OUT , "gpio7"    , 0 }, // used as PULL_UP7
	{ GD3X_PU8_REG, GD3X_PIN_MODE_OUT , "gpio8"    , 0 }, // used as PULL_UP8
    { GD3X_PD0_REG, GD3X_PIN_MODE_OUT , "gpio9"    , 0 }, // used as PULL_DOWN0
    { GD3X_PD1_REG, GD3X_PIN_MODE_OUT , "gpio10"   , 0 }, // used as PULL_DOWN1
    { GD3X_PD2_REG, GD3X_PIN_MODE_OUT , "gpio11"   , 0 }, // used as PULL_DOWN2
    { GD3X_PD3_REG, GD3X_PIN_MODE_OUT , "gpio12"   , 0 }, // used as PULL_DOWN3
    { GD3X_PD4_REG, GD3X_PIN_MODE_OUT , "gpio13"   , 0 }, // used as PULL_DOWN4
    { GD3X_PD5_REG, GD3X_PIN_MODE_OUT , "gpio14"   , 0 }, // used as PULL_DOWN5
    { GD3X_PD6_REG, GD3X_PIN_MODE_OUT , "gpio15"   , 0 }, // used as PULL_DOWN6
    { GD3X_PD7_REG, GD3X_PIN_MODE_OUT , "gpio16"   , 0 }, // used as PULL_DOWN7
	{ GD3X_PD8_REG, GD3X_PIN_MODE_OUT , "gpio17"   , 0 }, // used as PULL_DOWN8
	{ 0x105, GD3X_PIN_MODE_OUT , "gpio18"   , 0 }, // used as 7.5V switch
	{ 0x106, GD3X_PIN_MODE_OUT , "gpio19"   , 0 }, // used as PoE Out power switch
	//{ 0x107, GD3X_PIN_MODE_OUT , "gpio20"   , 0 }, // used as USB power management (auto - 0, manual - 1)
	{ 0x203, GD3X_PIN_MODE_IN  , "gpio20"   , 0 }, // reserved
	{ 0x108, GD3X_PIN_MODE_OUT , "gpio21"   , 0 }, // used as USB power switch
	{ 0x109, GD3X_PIN_MODE_OUT , "gpio22"   , 0 }, // used as WIFI led
	//{ 0x10A, GD3X_PIN_MODE_OUT , "gpio23"   , 0 }, // used as PoE Out power management (auto - 0, manual - 1)
	{ 0x203, GD3X_PIN_MODE_IN  , "gpio23"   , 0 }, // reserved
	{ 0x502, GD3X_PIN_MODE_IN  , "gpio24"   , 0 }, // used as PoE error INT
	{ 0x503, GD3X_PIN_MODE_IN  , "gpio25"   , 0 }, // used as USB error INT
	{ 0x501, GD3X_PIN_MODE_IN  , "gpio26"   , 0 }, // used as heating INT
	{ 0x505, GD3X_PIN_MODE_IN  , "gpio27"   , 0 }, // used as wd INT
	{ 0x506, GD3X_PIN_MODE_IN  , "gpio28"   , 0 }, // used as voltage INT
	{ 0x203, GD3X_PIN_MODE_IN  , "gpio29"   , 0 }, // reserved
	{ 0x203, GD3X_PIN_MODE_IN  , "gpio30"   , 0 }, // reserved
	{ 0x203, GD3X_PIN_MODE_IN  , "gpio31"   , 0 } // reserved
};

struct gd3x_pin_config gd3x_adc_config[GD3X_ADC_NUM_PINS] =
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

typedef struct __attribute__ ((packed)){  // 2k size
	uint32_t index;
	uint32_t address;
	uint32_t data;
	uint32_t crc;
}i2c_transfer_data;  // Data to write for writng data sequence

static int i2c_transfer_words(u32 op, struct i2c_client *client, u32 reg, unsigned word)
{
	struct i2c_adapter *adap = client->adapter;
	struct gd3x *data = i2c_get_clientdata(client);
	u32 index_r, index_rw, crc_r, crc_rw, crc_rx_result;
	i2c_transfer_data rx, tx;

	memset(&rx, 0, sizeof(i2c_transfer_data));
	memset(&tx, 0, sizeof(i2c_transfer_data));

	if(op == 0){
		reg |= 0x80000000;
	}

	data->index += 1;
	index_rw = data->index;

	tx.index	= index_rw;
	tx.address	= reg;
	tx.data		= word;
	crc_rw		= crc32(0, (unsigned char *)&tx, (sizeof(tx) - sizeof(uint32_t)));
	tx.crc		= crc_rw;

	rx.index	= 0;
	rx.address	= 0;
	rx.data		= 0;
	rx.crc		= 0;

	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(i2c_transfer_data),
			.buf = (u32)&tx,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = sizeof(i2c_transfer_data),
			.buf = (u32)&rx,
		},
	};

	/* read data registers */
	if(i2c_transfer(client->adapter, &msgs[0], 2) != 2 ) {
		dev_err(&client->dev, "%s: transfer error\n", __func__);
		return -EIO;
	}

	crc_rx_result = crc32(0, &rx, (sizeof(rx) - sizeof(uint32_t)));

	if(rx.address == 0xffff){
		switch(rx.data){
		case 0:
			dev_err(&client->dev,"sv: cannot read from 0x%0x address\n", tx.address);
			break;
		case 1:
			dev_err(&client->dev,"sv: cannot write to 0x%0x address\n", tx.address);
			break;
		case 2:
			dev_err(&client->dev,"sv: checksum does not match when accessing the address 0x%0x\n", tx.address);
			break;
		default:
			dev_err(&client->dev,"sv: unknown error %d\n when accessing the address 0x%0x\n", rx.data, tx.address);
			break;
		}

		data->err_count += 1;
		return -EIO;
	} else {
		if(crc_rx_result != rx.crc) {
			dev_info(&client->dev,"wrong checksum: expected 0x%0x, but received: 0x%0x (index: 0x%0x; adress: 0x%0x; data: 0x%0x) \n", crc_rx_result, rx.crc, rx.index, rx.address, rx.data);
			data->err_count += 1;
			return -EIO;
		}
		if(rx.address != tx.address) {
			dev_err(&client->dev,"wrong address: expected 0x%0x, but received 0x%0x \n", tx.address, rx.address);
			data->err_count += 1;
			return -EIO;
		}
		if(rx.index != tx.index) {
			dev_err(&client->dev,"wrong index: expected 0x%0x, but received 0x%0x \n", tx.index, rx.index);
			data->err_count += 1;
			return -EIO;
		}
	}

	if(op == 0){
		return 0;
	}

	return rx.data;
}

static int i2c_write_word(struct i2c_client *client, u32 reg, unsigned word)
{
	return i2c_transfer_words(0, client, reg, word);
}

static int i2c_read_word(struct i2c_client *client, u32 reg)
{
	return i2c_transfer_words(1, client, reg, 0);
}

static void gd3x_set(struct gpio_chip *chip, unsigned offset, int value){
	struct gd3x *gpio = gpiochip_get_data(chip);
	i2c_write_word(gpio->client, gd3x_board_config[offset].addr, value);
}

static int gd3x_get(struct gpio_chip *chip, unsigned offset)
{
	struct gd3x *gpio = gpiochip_get_data(chip);
	int		value;
	value = i2c_read_word(gpio->client,gd3x_board_config[offset].addr);
	return value;
}

static int gd3x_get_direction(struct gpio_chip *chip,
				  unsigned offset)
{
	if (gd3x_board_config[offset].mode == GD3X_PIN_MODE_IN)
		return GPIO_LINE_DIRECTION_IN;

	return GPIO_LINE_DIRECTION_OUT;
}

static int gd3x_direction_input(struct gpio_chip *chip,
				    unsigned offset)
{
	if (gd3x_board_config[offset].mode != GD3X_PIN_MODE_IN)
		return -EPERM;

	return 0;
}

static int gd3x_direction_output(struct gpio_chip *chip,
				     unsigned offset, int value)
{
	if (gd3x_board_config[offset].mode != GD3X_PIN_MODE_OUT)
		return -EPERM;

	gd3x_set(chip, offset, value);
	return 0;
}

static int gd3x_adc_read_raw(struct iio_dev *iio,
			struct iio_chan_spec const *channel, int *val,
			int *val2, long mask)
{
	struct gd3x *adc = iio_priv(iio);
	*val = i2c_read_word(adc->client,gd3x_adc_config[channel->channel].addr);
	if (val < 0)
		return -EINVAL;
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
	.base			= -1,
	.ngpio			= GD3X_GPIO_NUM_PINS,
	.can_sleep		= true,
};

static const struct of_device_id gd3x_of_match_table[] = {
	{ .compatible = "sz,gd3x" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, gd3x_of_match_table);

static const struct iio_info gd3x_adc_info = {
	.read_raw = gd3x_adc_read_raw,
};

static const struct iio_chan_spec gd3x_channels[] = {
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

static ssize_t gd3x_attribute_show(struct device *dev, struct device_attribute *da,
			 char *buf, u32 reg){
	struct i2c_client *client = to_i2c_client(dev);
	struct gd3x *data = i2c_get_clientdata(client);
	int value;

	value = i2c_read_word(data->client, reg);
	if(value<0)
		return value;

	return scnprintf(buf, PAGE_SIZE, "%d\n", value);
}

static ssize_t gd3x_attribute_store(struct device *dev, struct device_attribute *da,
			 const char *buf, size_t count, u32 reg)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gd3x *data = i2c_get_clientdata(client);
	unsigned long value;
	int err;

	err = kstrtoul(buf, 10, &value);
	if (err)
		return err;

	err = i2c_write_word(data->client, reg, value);
	if (err)
		return err;

	return count;
}

static ssize_t temp_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_TEMP_REG);
}

static ssize_t sv_temp_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_INTERNAL_TEMP_REG);
}

static ssize_t fw_version_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gd3x *data = i2c_get_clientdata(client);
	int project,board,adapt,ver,type;

	project = i2c_read_word(data->client,GD3X_FW_VER_PROJECT_REG);
	if(project<0)
		return "N/A";
	board = i2c_read_word(data->client,GD3X_FW_VER_BOARD_REG);
	if(board<0)
		return "N/A";
	adapt = i2c_read_word(data->client,GD3X_FW_VER_ADAPT_REG);
	if(adapt<0)
		return "N/A";
	ver = i2c_read_word(data->client,GD3X_FW_VER_REG);
	if(ver<0)
		return "N/A";
	type = i2c_read_word(data->client,GD3X_FW_VER_TYPE_REG);
	if(type<0)
		return "N/A";

	return scnprintf(buf, PAGE_SIZE, "%d.%d.%d.%d.%d\n", project, board, adapt, ver, type);
}

static ssize_t bl_version_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gd3x *data = i2c_get_clientdata(client);
	int value;

	value = i2c_read_word(data->client,GD3X_BL_VER_REG);
	if(value<0)
		return value;

	return scnprintf(buf, PAGE_SIZE, "%d.%d.%d.%d\n", (value & 0xff000000) >> 24, 
		(value & 0xff0000) >> 16, (value & 0xff00) >> 8, value & 0xff);
}

static ssize_t input_voltage_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_INPUT_VOLT_REG);
}

static ssize_t sv_voltage_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_INPUT_SV_VOLT_REG);
}

static ssize_t int_sum_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_INT_SUM_REG);
}

static ssize_t int_switch_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_INT_SWITCH_REG);
}

static ssize_t int_switch_store(struct device *dev, struct device_attribute *da,
			 const char *buf, size_t count)
{
	return gd3x_attribute_store(dev, da, buf, count, GD3X_INT_SWITCH_REG);
}

static ssize_t voltage_threshold_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_VOLT_THRESHOLD_REG);
}

static ssize_t voltage_threshold_store(struct device *dev, struct device_attribute *da,
			 const char *buf, size_t count)
{
	return gd3x_attribute_store(dev, da, buf, count, GD3X_VOLT_THRESHOLD_REG);
}

static ssize_t usb_control_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_USB_CONTROL_REG);
}

static ssize_t usb_control_store(struct device *dev, struct device_attribute *da,
			 const char *buf, size_t count)
{
	return gd3x_attribute_store(dev, da, buf, count, GD3X_USB_CONTROL_REG);
}

static ssize_t poeout_control_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_POEOUT_CONTROL_REG);
}

static ssize_t poeout_control_store(struct device *dev, struct device_attribute *da,
			 const char *buf, size_t count)
{
	return gd3x_attribute_store(dev, da, buf, count, GD3X_POEOUT_CONTROL_REG);
}

static ssize_t wdt_margin_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_WDT_MARGIN_REG);
}

static ssize_t wdt_margin_store(struct device *dev, struct device_attribute *da,
			 const char *buf, size_t count)
{
	return gd3x_attribute_store(dev, da, buf, count, GD3X_WDT_MARGIN_REG);
}

static ssize_t heat_end_temp_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_HEAT_END_TEMP_REG);
}

static ssize_t heat_end_temp_store(struct device *dev, struct device_attribute *da,
			 const char *buf, size_t count)
{
	return gd3x_attribute_store(dev, da, buf, count, GD3X_HEAT_END_TEMP_REG);
}

static ssize_t heat_end_time_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_HEAT_END_TIME_REG);
}

static ssize_t heat_end_time_store(struct device *dev, struct device_attribute *da,
			 const char *buf, size_t count)
{
	return gd3x_attribute_store(dev, da, buf, count, GD3X_HEAT_END_TIME_REG);
}

static ssize_t heat_hyst_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_HEAT_HYST_REG);
}

static ssize_t heat_hyst_store(struct device *dev, struct device_attribute *da,
			 const char *buf, size_t count)
{
	return gd3x_attribute_store(dev, da, buf, count, GD3X_HEAT_HYST_REG);
}

static ssize_t fw_upgrade_store(struct device *dev, struct device_attribute *da,
			 const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gd3x *data = i2c_get_clientdata(client);
	unsigned long value;
	int err;

	err = kstrtoul(buf, 10, &value);
	if (err)
		return err;
	//fixme
	err = i2c_write_word(data->client, GD3X_FW_UPGRADE_REG, 0xdddddddd);
	if (err)
		return err;

	return count;
}

static ssize_t system_reboot_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_SYSTEM_REBOOT);
}

static ssize_t system_reboot_store(struct device *dev, struct device_attribute *da,
			 const char *buf, size_t count)
{
	return gd3x_attribute_store(dev, da, buf, count, GD3X_SYSTEM_REBOOT);
}

static ssize_t fw_upg_flag_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_FW_UPGRADE_FLAG);
}

static ssize_t fw_upg_flag_store(struct device *dev, struct device_attribute *da,
			 const char *buf, size_t count)
{
	return gd3x_attribute_store(dev, da, buf, count, GD3X_FW_UPGRADE_FLAG);
}

static ssize_t uptime_show(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return gd3x_attribute_show(dev, da, buf, GD3X_UPTIME);
}

static ssize_t i2c_errors_show(struct device *dev, struct device_attribute *da,
			 char *buf){
	struct i2c_client *client = to_i2c_client(dev);
	struct gd3x *data = i2c_get_clientdata(client);
	return scnprintf(buf, PAGE_SIZE, "%d\n", data->err_count);
}

static ssize_t sv_i2c_errors_show(struct device *dev, struct device_attribute *da,
			 char *buf){
	return gd3x_attribute_show(dev, da, buf, GD3X_I2C_ERRORS_CNT_REG);
}

static ssize_t wd_reboot_counter_show(struct device *dev, struct device_attribute *da,
			 char *buf){
	return gd3x_attribute_show(dev, da, buf, GD3X_WD_REBOOT_COUNTER);
}

/*-----------------------------------------------------------------------*/
/* sysfs attributes for gd3x */

static DEVICE_ATTR_RO(bl_version);
static DEVICE_ATTR_RO(fw_version);
static DEVICE_ATTR_RO(i2c_errors);
static DEVICE_ATTR_RO(input_voltage);
static DEVICE_ATTR_RO(int_sum);
static DEVICE_ATTR_RO(sv_i2c_errors);
static DEVICE_ATTR_RO(sv_temp);
static DEVICE_ATTR_RO(sv_voltage);
static DEVICE_ATTR_RO(temp);
static DEVICE_ATTR_RO(uptime);
static DEVICE_ATTR_RO(wd_reboot_counter);

static DEVICE_ATTR_RW(fw_upg_flag);
static DEVICE_ATTR_RW(heat_end_temp);
static DEVICE_ATTR_RW(heat_end_time);
static DEVICE_ATTR_RW(heat_hyst);
static DEVICE_ATTR_RW(int_switch);
static DEVICE_ATTR_RW(poeout_control);
static DEVICE_ATTR_RW(system_reboot);
static DEVICE_ATTR_RW(usb_control);
static DEVICE_ATTR_RW(voltage_threshold);
static DEVICE_ATTR_RW(wdt_margin);

static DEVICE_ATTR_WO(fw_upgrade);

static struct attribute *gd3x_attrs[] = {
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
static const struct attribute_group gd3x_groups = {
	.attrs = gd3x_attrs,
};
/*-----------------------------------------------------------------------*/

static int gd3x_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct gd3x *gpio;
	struct iio_dev *indio_dev;

	int ret;

	gpio = devm_kzalloc(&client->dev, sizeof(*gpio), GFP_KERNEL);
	if (!gpio)
		return -ENOMEM;

	i2c_set_clientdata(client, gpio);

	gpio->chip = template_chip;
	gpio->chip.parent = &client->dev;

	gpio->client = client;

	mutex_init(&gpio->lock);

	ret = gpiochip_add_data(&gpio->chip, gpio);
	if (ret < 0) {
		dev_err(&client->dev, "Unable to register gpiochip\n");
		return ret;
	}
	ret = sysfs_create_group(&client->dev.kobj, &gd3x_groups);

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*gpio));
	if (!indio_dev)
		return -ENOMEM;
	gpio = iio_priv(indio_dev);
	gpio->client = client;
	gpio->index = 0;
	gpio->err_count = 0;

	indio_dev->dev.parent = &client->dev;
	indio_dev->dev.of_node = client->dev.of_node;
	indio_dev->name = dev_name(&client->dev);
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &gd3x_adc_info;
	indio_dev->channels = gd3x_channels;
	indio_dev->num_channels = ARRAY_SIZE(gd3x_channels);
	ret = devm_iio_device_register(&client->dev, indio_dev);
	if (ret < 0)
		return ret;

	return 0;
}

static int gd3x_remove(struct i2c_client *client)
{
	struct gd3x *gpio = i2c_get_clientdata(client);

	gpiochip_remove(&gpio->chip);

	return 0;
}

static const struct i2c_device_id gd3x_id_table[] = {
	{ "gd3x", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, gd3x_id_table);

static struct i2c_driver gd3x_driver = {
	.driver = {
		.name = "gd3x",
		.of_match_table = gd3x_of_match_table,
	},
	.probe = gd3x_probe,
	.remove = gd3x_remove,
	.id_table = gd3x_id_table,
};
module_i2c_driver(gd3x_driver);

MODULE_DESCRIPTION("GD3X 32Bit Driver GPIO Driver");
MODULE_LICENSE("GPL v2");

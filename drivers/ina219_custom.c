#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/regmap.h>
#include <linux/util_macros.h>
#include <linux/property.h>
#include <linux/module.h>
#include <linux/bits.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>


#define DRIVER_NAME "ina219_custom"

#define SLAVE_ADDR 0x40
#define CONFIG_REG 0x00
#define SHUNTV_REG 0x01
#define BUSV_REG 0x02
#define POWER_REG 0x03
#define CURRENT_REG 0x04
#define CALIBRATION_REG 0x05

#define ina219_MAX_REGISTERS 0x07
#define INA219_CONFIG_DEFAULT	0x399F	/* PGA=8 */
#define ina219_RSHUNT_DEFAULT		10000


static bool ina219_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CALIBRATION_REG:
	case CONFIG_REG:
        return true;
	default:
		return false;
	}
}

static bool ina219_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SHUNTV_REG:
	case BUSV_REG:
	case POWER_REG:
	case CURRENT_REG:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config ina219_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.use_single_write = true,
	.use_single_read = true,
	.max_register = ina219_MAX_REGISTERS,
	.cache_type = REGCACHE_MAPLE,
	.volatile_reg = ina219_volatile_reg,
	.writeable_reg = ina219_writeable_reg,
};

struct ina219_data{
    char name[15];
    int busv_shift;
    int busv_lsb;

    long rshunt;
	long current_lsb_uA;
	long power_lsb_uW;

    struct i2c_client *ina219_client;
    struct miscdevice ina219_miscdev;
    struct regmap *regmap;
    const struct ina219_config *config;
};

static int ina219_get_value(struct ina219_data *data, u8 reg, unsigned int regval);
static int ina219_set_shunt(struct ina219_data *data, unsigned long val);
static int ina219_init(struct device *dev, struct ina219_data *data);

static int ina219_get_value(struct ina219_data *data, u8 reg, unsigned int regval){
    int val = (regval >> data->busv_shift) * data->busv_lsb;
    val = DIV_ROUND_CLOSEST(val, 1000);
    return val;
}

// File operations: write function
static ssize_t ina219_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    printk("writing is not enabled for this ina219 driver, it only reads values from bus voltage register");
    return 0;
}

static ssize_t ina219_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct ina219_data* data = file->private_data;
    u8 voltage_reg = BUSV_REG;
	struct regmap *regmap = data->regmap;
	unsigned int regval;
	int ret;
    unsigned long val;

    ret = regmap_read(regmap, voltage_reg, &regval);
    if (ret)
        return ret;
    val = ina219_get_value(data, voltage_reg, regval);

    if(copy_to_user(buf, &val, sizeof(val)))
        return -EFAULT;

    // *ppos += sizeof(val);
    return sizeof(val);
}

static int ina219_open(struct inode *inode, struct file *file)
{
    struct miscdevice *misc = file->private_data;
    struct ina219_data *data = container_of(misc, struct ina219_data, ina219_miscdev);
    file->private_data = data;
    return 0;
}

static int ina219_release(struct inode *inode, struct file *file)
{
    struct ina219_data *data = file->private_data;
    dev_info(&data->ina219_client->dev, "ina219: device closed for ina219");
    return 0;
}


struct ina219_config {
		u16 config_default;
		int calibration_value;
		int shunt_div;
		int bus_voltage_shift;
		int bus_voltage_lsb;
		int power_lsb_factor;
		bool has_alerts;
		bool has_ishunt;
		bool has_power_average;
};


static const struct ina219_config ina219_config = {
		.config_default = INA219_CONFIG_DEFAULT,
		.calibration_value = 4096,
		.shunt_div = 100,
		.bus_voltage_shift = 3,
		.bus_voltage_lsb = 689,
		.power_lsb_factor = 20,
		.has_alerts = false,
		.has_ishunt = false,
		.has_power_average = false,
};

// File operations structure
static const struct file_operations ina_fops = {
    .owner = THIS_MODULE,
    .open = ina219_open,
    .release = ina219_release,
    .write = ina219_write,
    .read = ina219_read,
    // .unlocked_ioctl = ina219_ioctl,
};

static int ina219_set_shunt(struct ina219_data *data, unsigned long val)
{
	unsigned int dividend = DIV_ROUND_CLOSEST(1000000000,
						  data->config->shunt_div);
	if (!val || val > dividend)
		return -EINVAL;

	data->rshunt = val;
	data->current_lsb_uA = DIV_ROUND_CLOSEST(dividend, val);
	data->power_lsb_uW = data->config->power_lsb_factor *
			     data->current_lsb_uA;

	return 0;
}

static int ina219_init(struct device *dev, struct ina219_data *data){
	struct regmap *regmap = data->regmap;
	u32 shunt;
	int ret;

	if (device_property_read_u32(dev, "shunt-resistor", &shunt) < 0)
		shunt = ina219_RSHUNT_DEFAULT;

    printk(KERN_INFO "INA219 attempting to set shunt voltage register \n");	
	ret = ina219_set_shunt(data, shunt);
	if (ret < 0)
		return ret;

    printk(KERN_INFO "INA219 attempting to set configuration register \n");	

	ret = regmap_write(regmap, CONFIG_REG, data->config->config_default);
	if (ret < 0)
		return ret;

	if (data->config->has_ishunt)
		return 0;

    printk(KERN_INFO "INA219 attempting to write to calibration register \n");	
	/*
	 * Calibration register is set to the best value, which eliminates
	 * truncation errors on calculating current register in hardware.
	 * According to datasheet (eq. 3) the best values are 2048 for
	 * ina226 and 4096 for ina219. They are hardcoded as calibration_value.
	 */
	return regmap_write(regmap, CALIBRATION_REG,
			    data->config->calibration_value);
}

static int ina219_probe(struct i2c_client *client)
{
    printk(KERN_INFO "INA219 attempting to probe \n");	
    dev_info(&client->dev, "ina219 I2C driver probing\n");

    struct ina219_data * data;
    int ret;

    // Allocate private structure 
    data = devm_kzalloc(&client->dev, sizeof(struct ina219_data), GFP_KERNEL);
    if(!data){
        dev_info(&client->dev, "ina219 unable to allocate memory");
        return -ENOMEM;
    }

    printk(KERN_INFO "INA219 attempting to set client data\n");	
    // store pointer to the device structure in bus device context
    i2c_set_clientdata(client, data);


    // store pointer to I2C client
    data->ina219_client = client;
    data->config = &ina219_config;
    data->busv_shift = data->config->bus_voltage_shift;
    data->busv_lsb   = data->config->bus_voltage_lsb;

    strncpy(data->name, DRIVER_NAME , sizeof(data->name) - 1);
    data->ina219_miscdev.name = data->name;
    data->ina219_miscdev.minor = MISC_DYNAMIC_MINOR;
    data->ina219_miscdev.fops = &ina_fops;
    data->ina219_miscdev.parent = &client->dev;
    data->ina219_miscdev.mode = 0666;

    printk(KERN_INFO "INA219 attempting to init regmap \n");	
    data->regmap = devm_regmap_init_i2c(client, &ina219_regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(&client->dev, "failed to allocate register map\n");
		return PTR_ERR(data->regmap);
	}

    printk(KERN_INFO "INA219 attempting device init \n");	
    ret = ina219_init(&client->dev, data);
	if (ret < 0)
		return dev_err_probe(&client->dev, ret, "failed to configure device\n");

    printk(KERN_INFO "INA219 attempting device registration\n");	
    ret = misc_register(&data->ina219_miscdev);
    if(ret < 0){
        dev_info(&client->dev, "device registration failed");
        return -EFAULT;
    }

    printk(KERN_INFO "INA219 driver loaded successfully\n");	
    return 0;
}

static void ina219_remove(struct i2c_client *client)
{
    struct ina219_data * data;

    // Get device structure from device bus context
    data = i2c_get_clientdata(client);

    dev_info(&client->dev, "entered remove function for ina219");

    // Deregister misc device
    misc_deregister(&data->ina219_miscdev);

    dev_info(&client->dev,  "Exiting remove function for ina219");
}

static const struct of_device_id ina_ids[] = {
    { .compatible = "ti,ina219_custom" },
    { }
};
MODULE_DEVICE_TABLE(of, ina_ids);

static const struct i2c_device_id ina219_id[] = {
    { .name = "ina219_custom"},
    { }
};
MODULE_DEVICE_TABLE(i2c, ina219_id);

static struct i2c_driver ina219_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = ina_ids,
    },
    .probe = ina219_probe,
    .remove = ina219_remove,
    .id_table = ina219_id,
};

module_i2c_driver(ina219_driver);

MODULE_AUTHOR("Mwesigwa Guma");
MODULE_DESCRIPTION("I2C Driver for QAPASS 1602A ina");
MODULE_LICENSE("GPL");

// ina219_driver.c

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/ioctl.h>

#define DRIVER_NAME "ina219_custom"

struct ina219_dev{
    struct i2c_client *ina219_client;
    struct miscdevice ina219_miscdev;
    char name[15];
};

// File operations: write function
static ssize_t ina219_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    printk(KERN_INFO "Writing to ina219 device");	
	return 0;
}

static ssize_t ina219_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    printk(KERN_INFO "Reading ina219 device");	
	return 0;
}

static int ina219_open(struct inode *inode, struct file *file)
{
    struct miscdevice *misc = file->private_data;
    struct ina219_dev *ina219 = container_of(misc, struct ina219_dev, ina219_miscdev);
    file->private_data = ina219;
    return 0;
}

static int ina219_release(struct inode *inode, struct file *file)
{
    struct ina219_dev *ina219 = file->private_data;
    dev_info(&ina219->ina219_client->dev, "ina219: device closed for ina219");
    return 0;
}

// static long ina219_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
// {
//     // struct ina219_dev *ina219 = file->private_data;
//     //
//     // switch (cmd) {
//     //  case 0x01: // Clear display
//     //  case 0x80: // Set cursor to first line position 0
//     //
//     //  case 0xC0: // Set cursor to second line position 0
//     //  default:
//     //   return -EINVAL;
//     // }
//     return -EINVAL;
// }

// File operations structure
static const struct file_operations ina_fops = {
    .owner = THIS_MODULE,
    .open = ina219_open,
    .release = ina219_release,
    .write = ina219_write,
    .read = ina219_read,
    // .unlocked_ioctl = ina219_ioctl,
};

static int ina219_probe(struct i2c_client *client)
{
    dev_info(&client->dev, "ina219 I2C driver probing\n");

    struct ina219_dev * ina219;

    // Allocate private structure 
    ina219 = devm_kzalloc(&client->dev, sizeof(struct ina219_dev), GFP_KERNEL);
    if(!ina219){
        dev_info(&client->dev, "ina219 unable to allocate memory");
        return -ENOMEM;
    }

    // store pointer to the device structure in bus device context
    i2c_set_clientdata(client, ina219);

    // store pointer to I2C client
    ina219->ina219_client = client;

    // initialize the misc device, ina219 is incremented after each probe call
    // sprintf(ina219->name, "ina219%02d", counter++);
    strncpy(ina219->name, DRIVER_NAME , sizeof(ina219->name) - 1);

    ina219->ina219_miscdev.name = ina219->name;
    ina219->ina219_miscdev.minor = MISC_DYNAMIC_MINOR;
    ina219->ina219_miscdev.fops = &ina_fops;
    ina219->ina219_miscdev.parent = &client->dev;

    int ret = misc_register(&ina219->ina219_miscdev);
    if(ret < 0){
        dev_info(&client->dev, "device registration failed");
        return ret;
    }


    // initialize ina device


    printk(KERN_INFO "INA219 driver loaded successfully\n");	
    return 0;
}

static void ina219_remove(struct i2c_client *client)
{
    struct ina219_dev * ina219;

    // Get device structure from device bus context
    ina219 = i2c_get_clientdata(client);

    dev_info(&client->dev, "entered remove function for ina219");

    // Deregister misc device
    misc_deregister(&ina219->ina219_miscdev);

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

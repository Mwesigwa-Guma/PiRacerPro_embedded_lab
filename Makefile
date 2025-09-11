KDIR := /lib/modules/$(shell uname -r)/build

MYSRC := $(PWD)/drivers
DTSRC := $(PWD)/dtoverlays

builddrivers:
	$(MAKE) -C $(KDIR) M=$(MYSRC) modules

builddtrees:
	dtc -@ -I dts -O dtb -o $(DTSRC)/ina219_custom.dtbo $(DTSRC)/ina219_custom.dts

clean:
	$(MAKE) -C $(KDIR) M=$(MYSRC) clean
	rm $(DTSRC)/*.dtbo
	sudo rm /boot/firmware/overlays/ina219_custom*

cpoverlays:
	sudo cp $(DTSRC)/*.dtbo /boot/firmware/overlays/

load:
	sudo insmod $(MYSRC)/ina219_custom.ko

dependencies:
	sudo modprobe regmap-i2c

unload:
	sudo rmmod ina219_custom
	sudo rmmod ina2xx

verifyloadeddrivers:
	lsmod | grep ina

dmesg:
	sudo dmesg | tail

livedts:
	sudo dtc -I fs -O dts -o $(DTSRC)/live.dts /sys/firmware/devicetree/base

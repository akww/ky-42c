# Makefile for the Linux sound card driver
#

# This software is contributed or developed by KYOCERA Corporation.
# (C) 2022 KYOCERA Corporation

subdir-ccflags-y += -Werror
subdir-ccflags-y += -DCONFIG_KYOCERA_MSND

obj-$(CONFIG_SOUND) += soundcore.o
obj-$(CONFIG_SOUND_PRIME) += oss/
obj-$(CONFIG_DMASOUND) += oss/
obj-$(CONFIG_SND) += core/ i2c/ drivers/ isa/ pci/ ppc/ arm/ sh/ synth/ usb/ \
	firewire/ sparc/ spi/ parisc/ pcmcia/ mips/ soc/ atmel/ hda/
obj-$(CONFIG_SND_AOA) += aoa/

# This one must be compilable even if sound is configured out
obj-$(CONFIG_AC97_BUS) += ac97_bus.o

ifeq ($(CONFIG_SND),y)
  obj-y += last.o
endif

soundcore-objs  := sound_core.o

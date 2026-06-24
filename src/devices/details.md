# Overview of `devfs` and `sysfs`:

`/dev/` aka `devfs`, and `/sys/` aka `sysfs`, are virtual filesystems that collectively make up the device and driver trees. `sysfs` houses physical devices, while `devfs` houses endpoints provided by drivers.

## `sysfs` Structure:

### `sysfs/root`:

This contains the ground truth for the physical device tree of the system. Every root bus and device is a directory here (ex: `sysfs/root/pci0000:00`), and any sub-devices (ex: `sysfs/devices/pci0000:00/00:0)
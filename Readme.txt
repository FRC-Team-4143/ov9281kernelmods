Jetpack 6.1 Innomaker ov9281 mipi camera driver
download and extract https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v4.0/sources/public_sources.tbz2
copy these files into tree

make modules
make dtbs

sudo cp nvidia-oot/drivers/media/platform/tegra/camera/tegra-camera.ko /lib/modules/5.15.148-tegra/updates/drivers/media/platform/tegra/camera
sudo cp nvidia-oot/drivers/media/i2c/nv_ov9281.ko /lib/modules/5.15.148-tegra/updates/drivers/media/i2c
sudo cp kernel-devicetree/generic-dts/dtbs/tegra234-p3767-camera-p3768-ov9281-dual.dtbo /boot
sudo depmod -A
sudo /opt/nvidia/jetson-io/jetson-io.py  # create boot option with ov9281 overlay

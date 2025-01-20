#!/bin/bash

release_dir=./ov9281_release

# setup the release directory
if [[ -d $release_dir  ]]; then
	rm -rf $release_dir
fi
mkdir $release_dir

# move the sources into the release
cp nvidia-oot/drivers/media/platform/tegra/camera/tegra-camera.ko $release_dir
cp nvidia-oot/drivers/media/i2c/nv_ov9281.ko $release_dir
cp kernel-devicetree/generic-dts/dtbs/tegra234-p3767-camera-p3768-ov9281-dual.dtbo $release_dir

# create the installer script
/bin/cat <<EOF > $release_dir/install_release.sh
sudo cp ./tegra-camera.ko /lib/modules/5.15.148-tegra/updates/drivers/media/platform/tegra/camera
sudo cp ./nv_ov9281.ko /lib/modules/5.15.148-tegra/updates/drivers/media/i2c
sudo cp ./tegra234-p3767-camera-p3768-ov9281-dual.dtbo /boot
sudo depmod -A
sudo /opt/nvidia/jetson-io/jetson-io.py  # create boot option with ov9281 overlay

EOF
chmod +x $release_dir/install_release.sh

# package the release
tar -czf ov_9281_release.tgz $release_dir
mv ov_9281_release.tgz ~/Downloads


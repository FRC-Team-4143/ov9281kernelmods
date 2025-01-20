# Jetpack 6.2 Innomaker ov9281 mipi camera driver

## Installing the drivers

1. Download the appropriate `.tgz` release from this repository 
2. Extract the release tarfile `tar -xvf ov_9281_release.tgz`
3. Run the install script `./install_release.sh`
4. When prompted, select the following options:
   - Configure 24 pin CSI
   - Configure for compatible hardware
   - Camera OV9281 Dual
   - Save pin changes
   - any of the save and exit options

## Building the driver
1. Find the public sources for the intended jetpack version: https://developer.nvidia.com/embedded/jetson-linux-r3643
2. Download the BSP sources package: https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v4.3/sources/public_sources.tbz2
3. Extract the BSP sources via `tar -xf ./public-sources.tbz2`
4. Extract `kernel_src.tbz2`, `kernel_oot_modules_src.tbz2` and `nvidia_kernel_display_driver_source.tbz2`
5. Copy the files from this repository into kernel tree

6. Run `make modules`
7. Run `make dtbs`

8. Run the packaging script `./package_release.sh`
9. Upload `ov_9281_release.tgz` to a new release
10. Profit :)


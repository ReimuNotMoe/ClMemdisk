# ClMemdisk

## Introduction
An elegant way to make use of the spare memory of your OpenCL-compatible graphics card.
- Maps GPU memory to a block device.
- Uses OpenCL API, (technically) works on any recent GPU.


## Build
### Dependencies
- OpenCL
- [BUSE](https://github.com/ReimuNotMoe/BUSE)
### Compile
Nearly all my projects use CMake. It's very simple:

    mkdir build
    cd build
    cmake ..
    make -j `nproc`


## How to use
```
Usage: clmemdisk <operation> [/dev/nbdX] [size] [platform_id:device_id] [[platform_id:device_id]...]

Operations:
        list        List available OpenCL platforms and devices
        single      Use memory from single device
        stripe      Use memory from multiple devices with striping (WIP)

Example:
    # clmemdisk list
    # clmemdisk single /dev/nbd0 512M 0:0
    # clmemdisk stripe /dev/nbd233 1G 0:0 0:1 0:2
```


## Caveats
- Only known to work in Linux.
- You need to load the `nbd` kernel module first.

## Screenshot
![ClMemDisk_Demo](https://raw.githubusercontent.com/ReimuNotMoe/ReimuNotMoe.github.io/master/images/ClMemDisk_Demo_.png)

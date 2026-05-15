# mOS

I'm trying to invent a wheel: a microkernel(hibrid) OS for RISC-V.

I just play with real HW and my unprofessional code.

Can be built on board under Linux, Windows and Linux with crosstool.
Copy to /boot/ and run from U-Boot. Or dowload form tftp.

## Supported boards
- VisionFive2
- Banana Pi BPI-F3
- QEMU (MEMIO is NOT allowed in user space. I use it for test of my ideas.)

## Building:

Steps to replace OpenSBI on Spacemit K1
<br>
https://gist.github.com/cyyself/a07096e6e99c949ed13f8fa16d884402
<br>

```
meson setup build -Dboard=bpi-f3 --cross-file=cross.txt
ninja -C build
```

## Run
### BPI-F3
```
dhcp
tftpboot 0x200000 192.168.1.1:img.bin
go 0x200000
```
### VisionFive 2
```
dhcp
tftpboot 0x40200000 192.168.1.1:img.bin
go 0x40200000
```

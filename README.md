# stlink-tool

libusb tool for flashing ST-Link dongles. Please note that similarly to ST's updater, the uploaded firmware won't replace the bootloader (meaning that you should be able to reflash the original afterwards using [ST's firmware update utility](http://www.st.com/en/development-tools/stsw-link007.html)).

```
Usage: stlink-tool.exe [options] [firmware.bin]
Options:
  -h, --help            Show help
  -p, --probe           Probe the ST-Link adapter
  -d, --decrypt KEY     Decrypt Firmware using KEY. Pass "" to use internal key.
  -sd, --save_dec       Save decripted firmware as filename + .dec
  -t, --st_type TYPE    Change STLink type to TYPE.
                          A for "STM32 Debugger+Audio"
                          B for "STM32 Debug+Mass storage+VCP"
                          E for "STM32 Debug+Mass storage+VCP"
                          F for "STM8/STM32 Debug+Mass storage+VCP+Bridge" (for STLink V3)
                          G for "STM8 Debug+Mass storage+VCP"
                          J for "STM32 Debugger"
                          M for "STM8/STM32 Debugger"
                          S for "STM8 Debugger"
  -v, --ver S.J.X       Change reported STLink sersion.
                          S is STLink version, J is JTAG version,
                          X is SWIM or MSD version.

Options for Modifying Device Config (Only for STLink v2 and up):
  --usb_cur CURRENT     Set the MaxPower reported in USB Descriptor
                        to CURRENT(mA)
  --msd_name VOLUME     Set the volsume name of the MSD drive to VOLUME.
  --mbed_name NAME      Set the MBED board name to NAME.
  --dfu_opt OPT         Set DFU Options to OPT.
                        OPT is the Decimal value of Bit Field:
                          bit1: "No Power Off"
                          bit2: "Autostart"
  --dynamic_opt OPT     Set Dynamic Option to OPT.
                          'V': MSD Off
                          'M': MSD On
                          'W': MSD Always Off
  --mco_out OPT         Set MCO Output to OPT. OPT is the Hex value of:
                          Lower Nybble(MCO Source):
                            0: None
                            1: HSI
                            2: HSE
                            3: PLL
                          Upper Nybble (Divider):
                            Divider - 1 (Valid Divider 1-5)
  --startup OPT         Set Startup Preferences to OPT.
                          0: High Power
                          1: Balanced
                          2: Low Power
                          3: Default
  To remove a configuration you can use the "" argument with the option
  (Ex. --usb_cur "") or prefix the option with rm_ (Ex. --rm_usb_cur).

Application in Flash is started when called without argument, after firmware
load or configuration change.
```

stlink-tool has been tested under Debian based Linux and Windows x86/x64.

## New stlink-tool Features

* show ST-Link dongle information
* can show and modify device configuration (show is only for ST-Link V2.1)
* can modify STLink type and reported firmware version
* can add "Anti-Clone" Tag and "Firmware Flashed/EOF" Tag (to make flashed firmware bootable without needing to exit DFU on V2.1)
* can decrypt and flash firmwares taken from `STLinkUpgrade.jar`

Examples:

STLinkV2

```
STLinkV2 Bootloader Found
STLink Type: M [STM8/STM32 Debugger]
Firmware Version: V2J38S7

Bootloader PID: 3748
Reported Flash Size: 64KB

STLink ID: ************************
Firmware Encryption Key: ********************************
Anti-Clone Key: ********************************
Current Mode: 1
```

STLinkV2-1

```
Trying to switch STLINK/Application to bootloader
STLinkV2-1 Bootloader Found
STLink Type: B [STM32 Debug+Mass storage+VCP]
Firmware Version: V2J38M27

Current Device Configuration:
USB Current: [300mA]

Bootloader PID: 3748
HW Version: V2.0   Flags: 0x000000
Reported Flash Size: 128KB

STLink ID: ************************
Firmware Encryption Key: ********************************
Anti-Clone Key: ********************************
Current Mode: 2
```

This changes were done by referencing:
[GMMan/st-link-hack](https://github.com/GMMan/st-link-hack)
[sakana280's fork](https://github.com/sakana280/stlink-tool)

## Compiling

Required dependencies :

* C compiler (both clang and gcc seems to work great)
* libusb1
* git

```
git clone -b stlinkv21 https://github.com/GabyPCgeeK/stlink-tool.git
cd stlink-tool
git submodule init
git submodule update
make
```

## [Writing firmwares for ST-Link dongles](docs/writing-firmware.md)

## Firmware upload protocol

ST's firmware upload protocol is USB's DFU protocol with some twists. Every DFU command is issued with the 0xF3 prefix, and the command set does not exactly match USB's.

Some documentation :
* http://www.st.com/content/ccc/resource/technical/document/application_note/6a/17/92/02/58/98/45/0c/CD00264379.pdf/files/CD00264379.pdf/jcr:content/translations/en.CD00264379.pdf
* http://www.usb.org/developers/docs/devclass_docs/DFU_1.1.pdf

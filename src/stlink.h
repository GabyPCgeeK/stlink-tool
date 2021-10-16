/*
  Copyright (c) 2018 Jean THOMAS.
  
  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the Software
  is furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
  OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef _STLINK_H
#define _STLINK_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libusb.h>

#ifdef WINDOWS
  #ifndef bool
    #define bool unsigned char
  #endif
  #ifndef true
    #define true 1
  #endif

  #ifndef false
    #define false 0
  #endif
#else
  #include <stdbool.h>
#endif

enum DeviceStatus {
  OK = 0x00,
  errTARGET = 0x01,
  errFILE = 0x02,
  errWRITE = 0x03,
  errERASE = 0x04,
  errCHECK_ERASED = 0x05,
  errPROG = 0x06,
  errVERIFY = 0x07,
  errADDRESS = 0x08,
  errNOTDONE = 0x09,
  errFIRMWARE = 0x0A,
  errVENDOR = 0x0B,
  errUSBR = 0x0C,
  errPOR = 0x0D,
  errUNKNOWN = 0x0E,
  errSTALLEDPKT = 0x0F
};

enum DeviceState {
  appIDLE = 0,
  appDETACH = 1,
  dfuIDLE = 2,
  dfuDNLOAD_SYNC = 3,
  dfuDNBUSY = 4,
  dfuDNLOAD_IDLE = 5,
  dfuMANIFEST_SYNC = 6,
  dfuMANIFEST = 7,
  dfuMANIFEST_WAIT_RESET = 8,
  dfuUPLOAD_IDLE = 9,
  dfuERROR = 10
};

enum BlTypes {
    STLINK_BL_V2 = 0,
    STLINK_BL_V21,
    STLINK_BL_V3
};

enum ConfigTypes {
  confVERSION = 0,
  confST_TYPE,
  confUSB_CUR,
  confMSD_NAME,
  confMBED_NAME,
  confDFU_OPT,
  confDYN_OPT,
  confMCO_OUT,
  confSTARTUP
};

enum ModifyAction {
  modCOPY = 0,
  modADD,
  modREMOVE
};

struct STLinkConfig {
  enum ModifyAction modify[9];
  uint8_t raw_config[0x40];
  uint16_t soft_version;
  char stlink_type;
  uint16_t usb_current;
  char volume[12];
  char mbed_name[5];
  uint8_t dfu_option;
  char dynamic_option;
  uint8_t mco_output;
  uint8_t startup_pref;
};

struct DFUStatus {
  enum DeviceStatus bStatus : 8;
  unsigned int bwPollTimeout : 24;
  enum DeviceState bState : 8;
  unsigned char iString : 8;
};

struct STLinkInfo {
  uint8_t firmware_key[16];
  uint8_t anti_clone[16];
  uint8_t id[12];
  struct STLinkConfig config;

  union {
    struct {
      uint16_t swim_version: 6;
      uint16_t jtag_version: 6;
      uint16_t stlink_version: 4;
    };
    uint16_t software_version;
  } __attribute__((packed));

  uint16_t bootloader_pid;
  union {
    struct {
      uint32_t hardware_flags: 24;
      uint32_t hardware_minor: 4;
      uint32_t hardware_mayor: 4;
    };
    uint32_t hardware_version;
  } __attribute__((packed));
  uint8_t flash_size;
  uint8_t reported_flash_size;
  uint8_t reserved_flash;
  uint8_t mode;
  libusb_context *stinfo_usb_ctx;
  libusb_device_handle *stinfo_dev_handle;
  unsigned char stinfo_ep_in;
  unsigned char stinfo_ep_out;
  enum BlTypes stinfo_bl_type;
  char* decrypt_key;
};

extern char* st_types[];

int stlink_flash_config_area(struct STLinkInfo *info, struct STLinkConfig *config);
char* stlink_get_dev_config(struct STLinkConfig *config, enum ConfigTypes config_type);

int stlink_dfu_mode(libusb_device_handle *dev_handle, int trigger);
int stlink_read_info(struct STLinkInfo *info);
int stlink_current_mode(struct STLinkInfo *info);
int stlink_dfu_download(struct STLinkInfo *stlink_info,
			unsigned char *data,
			const size_t data_len,
			const uint16_t wBlockNum);
int stlink_flash(struct STLinkInfo *stlink_info, const char *filename, bool decrypt, bool save);
int stlink_exit_dfu(struct STLinkInfo *info);

#endif //_STLINK_H

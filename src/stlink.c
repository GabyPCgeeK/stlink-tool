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

#include <fcntl.h>
#include <sys/types.h> 
#include <sys/stat.h>

#ifdef WINDOWS
  #include <Winsock2.h>
#else
  #include <arpa/inet.h>
#endif

#include <string.h>
#include <ctype.h>

#include "crypto.h"
#include "stlink.h"

#define USB_TIMEOUT 5000

#define DFU_DETACH 0x00
#define DFU_DNLOAD 0x01
#define DFU_UPLOAD 0x02
#define DFU_GETSTATUS 0x03
#define DFU_CLRSTATUS 0x04
#define DFU_GETSTATE 0x05
#define DFU_ABORT 0x06
#define DFU_EXIT  0x07
#define ST_DFU_INFO   0xF1
#define ST_DFU_MAGIC  0xF3

#define GET_COMMAND 0x00
#define SET_ADDRESS_POINTER_COMMAND 0x21
#define ERASE_COMMAND 0x41
#define ERASE_SECTOR_COMMAND 0x42
#define READ_UNPROTECT_COMMAND 0x92

char typeA[]  = "STM32 Debugger+Audio";
char typeB1[] = "STM32 Debug+Mass storage+VCP";
char typeB2[] = "STM32 Debug+VCP";
char typeE[]  = "STM32 Debug+Mass storage+VCP";
char typeF1[] = "STM8/STM32 Debug+Mass storage+VCP+Bridge";
char typeF2[] = "STM8/STM32 Debug+2VCP+Bridge";
char typeG1[]  = "STM8 Debug+Mass storage+VCP";
char typeG2[]  = "STM8 Debug+VCP";
char typeJ[]  = "STM32 Debugger";
char typeM[]  = "STM8/STM32 Debugger";
char typeS[]  = "STM8 Debugger";

char* st_types[] = {[0] = "Error", ['A'] = typeA, ['B'] = typeB1, ['E'] = typeE, ['F'] = typeF1, ['G'] = typeG1, ['J'] = typeJ, ['M'] = typeM, ['S'] = typeS, [0xFF] = "Not Set", ['B' - 'A'] = typeB2, ['F' - 'A'] = typeF2, ['G' - 'A'] = typeG2};

static int stlink_erase(struct STLinkInfo *info,  uint32_t address);
static int stlink_set_address(struct STLinkInfo *info, uint32_t address);
static int stlink_dfu_status(struct STLinkInfo *info, struct DFUStatus *status);

char* stlink_get_dev_config(struct STLinkConfig *config, enum ConfigTypes config_type) {
  switch (config_type) {
    case confDFU_OPT:
      switch (config->dfu_option) {
        case 1:
          return "No Power Off";
        case 2:
          return "Autostart";
        case 3:
          return "No Power Off, Autostart";
      }
      break;
    case confDYN_OPT:
      switch (config->dynamic_option) {
        case 'V':
          return "MSD Off";
        case 'W':
          return "MSD Always Off";
        case 'M':
          return "MSD On";
      }
      break;
    case confSTARTUP:
      switch (config->startup_pref) {
        case 0:
          return "High Power";
        case 1:
          return "Balanced";
        case 2:
          return "Low Power";
        case 3:
          return "Default";
      }
    default:
      break;
  }
  return "";
}

int stlink_flash_anticlone_tag(struct STLinkInfo *info) {
  int res;

  res = stlink_set_address(info, 0x08003C00);
  if (res) {
    fprintf(stderr, "Set Address Error at 0x%08x\n", 0x08003C00);
    return res;
  }
  res = stlink_dfu_download(info, info->anti_clone, 16, 2);
  if (res) {
    fprintf(stderr, "Download Error at 0x%08x\n", 0x08003C00);
    return res;
  }

  printf("Downloaded Anticlone Tag\n");
  return 0;
}

int stlink_flash_stlink_type(struct STLinkInfo *info, char st_type) {
  uint8_t data[16];
  int res;

  memset(data, 0xFF, sizeof(data));
  data[0] = st_type;

  res = stlink_set_address(info, 0x08003C20);
  if (res) {
    fprintf(stderr, "Set Address Error at 0x%08x\n", 0x08003C20);
    return res;
  }
  res = stlink_dfu_download(info, data, 16, 2);
  if (res) {
    fprintf(stderr, "Download Error at 0x%08x\n", 0x08003C20);
    return res;
  }

  printf("Downloaded STLink Type\n");
  return 0;
}

int stlink_flash_dev_config(struct STLinkInfo *info, struct STLinkConfig *config) {
  int res;
  if (config) {
    switch (config->modify[confUSB_CUR]) {
      case modADD:
        info->config.raw_config[0] = 'P';
        info->config.raw_config[1] = config->usb_current / 2;
        break;
      case modREMOVE:
        *(uint16_t*)(info->config.raw_config) = 0xFFFF;
      case modCOPY:
        break;
    }
    switch (config->modify[confMSD_NAME]) {
      case modADD:
        info->config.raw_config[2] = 'V';
        memset(info->config.raw_config+3, 0x20, 11);
        memcpy(info->config.raw_config+3, config->volume, strlen(config->volume));
        break;
      case modREMOVE:
        memset(info->config.raw_config+2, 0xFF, 12);
      case modCOPY:
        break;
    }
    switch (config->modify[confMBED_NAME]) {
      case modADD:
        info->config.raw_config[15] = 'B';
        memset(info->config.raw_config+16, 0xFF, 4);
        memcpy(info->config.raw_config+16, config->mbed_name, strlen(config->mbed_name));
        break;
      case modREMOVE:
        memset(info->config.raw_config+15, 0xFF, 5);
      case modCOPY:
        break;
    }
    switch (config->modify[confDFU_OPT]) {
      case modADD:
        info->config.raw_config[20] = 'F';
        info->config.raw_config[21] = config->dfu_option;
        break;
      case modREMOVE:
        *(uint16_t*)(info->config.raw_config+20) = 0xFFFF;
      case modCOPY:
        break;
    }
    switch (config->modify[confDYN_OPT]) {
      case modADD:
        info->config.raw_config[22] = 'D';
        info->config.raw_config[23] = (uint8_t)config->dynamic_option;
        break;
      case modREMOVE:
        *(uint16_t*)(info->config.raw_config+22) = 0xFFFF;
      case modCOPY:
        break;
    }
    switch (config->modify[confMCO_OUT]) {
      case modADD:
        info->config.raw_config[26] = 'O';
        info->config.raw_config[27] = config->mco_output;
        break;
      case modREMOVE:
        *(uint16_t*)(info->config.raw_config+26) = 0xFFFF;
      case modCOPY:
        break;
    }
    switch (config->modify[confSTARTUP]) {
      case modADD:
        info->config.raw_config[32] = 'C';
        info->config.raw_config[33] = config->startup_pref;
        break;
      case modREMOVE:
        *(uint16_t*)(info->config.raw_config+32) = 0xFFFF;
      case modCOPY:
        break;
    }
  }

  res = stlink_set_address(info, 0x08003C30);
  if (res) {
    fprintf(stderr, "Set Address Error at 0x%08x\n", 0x08003C30);
    return res;
  }
  res = stlink_dfu_download(info, info->config.raw_config, 0x40, 2);
  if (res) {
    fprintf(stderr, "Download Error at 0x%08x\n", 0x08003C30);
    return res;
  }

  printf("Downloaded Device Configuration\n");
  return 0;
}

int stlink_flash_software_version(struct STLinkInfo *info, uint16_t version) {
  uint8_t data[16];
  int res;

  memset(data, 0xFF, sizeof(data));
  *(uint16_t*)(data+14) = version;
  //data[14] = 0x20;
  //data[15] = 0x41;

  res = stlink_set_address(info, 0x08003FF0);
  if (res) {
    fprintf(stderr, "Set Address Error at 0x%08x\n", 0x08003FF0);
    return res;
  }
  res = stlink_dfu_download(info, data, 16, 2);
  if (res) {
    fprintf(stderr, "Download Error at 0x%08x\n", 0x08003FF0);
    return res;
  }

  printf("Downloaded Software Version\n");
  return 0;
}

int stlink_flash_firmware_exists_flag(struct STLinkInfo *info) {
  uint8_t data[16];
  int res;

  uint32_t address = 0x08000000 | (((info->hardware_flags & 0x000001 ? 128 : info->flash_size) << 10) - 16);
  memset(data, 0xFF, sizeof(data));
  *(uint32_t*)(data+12) = 0xA50027D3;

  res = stlink_erase(info, address);
  if (res) {
    fprintf(stderr, "Erase Error at 0x%08x\n", address);
    return res;
  }

  res = stlink_set_address(info, address);
  if (res) {
    fprintf(stderr, "Set Address Error at 0x%08x\n", address);
    return res;
  }
  res = stlink_dfu_download(info, data, 16, 2);
  if (res) {
    fprintf(stderr, "Download Error at 0x%08x\n", address);
    return res;
  }

  printf("Downloaded Firmware Exists Tag\n");
  return 0;
}

int stlink_flash_config_area(struct STLinkInfo *info, struct STLinkConfig *config) {
  int res;
  char st_type = info->config.stlink_type;
  uint16_t version = htons(info->software_version);
  if (config){
    if (config->modify[confST_TYPE] == modADD)
      st_type = config->stlink_type;
    if (config->modify[confVERSION] == modADD) {
      version = htons(config->soft_version);
    }
  }

  res = stlink_erase(info, 0x08003C00);
  if (res) {
    fprintf(stderr, "Erase error at 0x%08x\n", 0x08003C00);
    return res;
  }
  res = stlink_flash_anticlone_tag(info);
  if (res) {
    fprintf(stderr, "Error Downloading Anticlone Tag\n");
    return res;
  }
  res = stlink_flash_stlink_type(info, st_type);
  if (res) {
    fprintf(stderr, "Error Downloading STLink Type\n");
    return res;
  }
  res = stlink_flash_dev_config(info, config);
  if (res) {
    fprintf(stderr, "Error Downloading Device Configuration\n");
    return res;
  }
  res = stlink_flash_software_version(info, version);
  if (res) {
    fprintf(stderr, "Error Downloading Software Version\n");
    return res;
  }
  res = stlink_flash_firmware_exists_flag(info);
  if (res) {
    fprintf(stderr, "Error Downloading Firmware Exists Tag\n");
    return res;
  }

  return 0;
}

int stlink_dfu_mode(libusb_device_handle *dev_handle, int trigger) {
  unsigned char data[16];
  int rw_bytes, res;

  memset(data, 0, sizeof(data));

  data[0] = 0xF9;
  if (trigger) data[1] = DFU_DNLOAD;
  /* Write */
  res = libusb_bulk_transfer(dev_handle,
                              1 | LIBUSB_ENDPOINT_OUT,
                              data,
                              sizeof(data),
                              &rw_bytes,
                              USB_TIMEOUT);
  if (res) {
    fprintf(stderr, "USB transfer failure\n");
    return -1;
  }
  if (!trigger) {
    /* Read */
    libusb_bulk_transfer(dev_handle,
                          1 | LIBUSB_ENDPOINT_IN,
                          data,
                          2,
                          &rw_bytes,
                          USB_TIMEOUT);
    if (res) {
      fprintf(stderr, "stlink_read_info() failure\n");
      return -1;
    }
  }
  return data[0] << 8 | data[1];
}

int stlink_read_info(struct STLinkInfo *info) {
  unsigned char data[0x40];
  int res, rw_bytes;

  memset(data, 0, sizeof(data));

  data[0] = ST_DFU_INFO;
  data[1] = 0x80;

  /* Write */
  res = libusb_bulk_transfer(info->stinfo_dev_handle,
           info->stinfo_ep_out,
           data,
           16,
           &rw_bytes,
           USB_TIMEOUT);
  if (res) {
    fprintf(stderr, "stlink_read_info out transfer failure\n");
    return -1;
  }

  /* Read */
  res = libusb_bulk_transfer(info->stinfo_dev_handle,
           info->stinfo_ep_in,
           data,
           6,
           &rw_bytes,
           USB_TIMEOUT);
  if (res) {
    fprintf(stderr, "stlink_read_info in transfer failure\n");
    return -1;
  }

  info->software_version = ntohs(*(uint16_t*)data);
  info->bootloader_pid = data[5] << 8 | data[4];

  memset(data, 0, sizeof(data));

  data[0] = ST_DFU_MAGIC;
  data[1] = 0x08;

  /* Write */
  res = libusb_bulk_transfer(info->stinfo_dev_handle,
           info->stinfo_ep_out,
           data,
           16,
           &rw_bytes,
           USB_TIMEOUT);
  if (res) {
    fprintf(stderr, "USB transfer failure\n");
    return -1;
  }

  /* Read */
  res = libusb_bulk_transfer(info->stinfo_dev_handle,
           info->stinfo_ep_in,
           data,
           20,
           &rw_bytes,
           USB_TIMEOUT);
  if (res) {
    fprintf(stderr, "USB transfer failure\n");
    return -1;
  }

  info->reported_flash_size = data[1] << 8 | data[0];
  info->flash_size = info->reported_flash_size;
  info->reserved_flash = 0;
  info->config.stlink_type = data[4];

  res = stlink_current_mode(info);
  if (res < 0) {
    return -1;
  }

  if (info->stinfo_bl_type == STLINK_BL_V2) {
    switch(info->mode) {
    case 0:
      info->stinfo_bl_type = STLINK_BL_V2; //TODO
      break;
    case 1:
      info->stinfo_bl_type = STLINK_BL_V2;
      break;
    case 2:
      info->stinfo_bl_type = STLINK_BL_V21;
      break;
    default:
      info->stinfo_bl_type = STLINK_BL_V3;
      break;
    }
  }
  memcpy(info->id, data+8, 12);

  /* Firmware encryption key generation */
  memcpy(info->firmware_key, data, 4);
  memcpy(info->firmware_key+4, data+8, 12);
  my_encrypt((unsigned char*)"I am key, wawawa", info->firmware_key, 16);
  /* Anti-Clone Tag generation */
  memcpy(info->anti_clone, data, 4);
  memcpy(info->anti_clone+4, data+8, 12);
  my_encrypt((unsigned char*)"What are you doing", info->anti_clone, 16);

  if (info->mode > 1) {
    memset(data, 0, sizeof(data));

    data[0] = ST_DFU_MAGIC;
    data[1] = 0x09;
    *(uint16_t*)(data+2) = 0x40;

    // Write //
    res = libusb_bulk_transfer(info->stinfo_dev_handle,
            info->stinfo_ep_out,
            data,
            16,
            &rw_bytes,
            USB_TIMEOUT);
    if (res) {
      fprintf(stderr, "USB transfer failureW\n");
      return -1;
    }

    // Read //
    res = libusb_bulk_transfer(info->stinfo_dev_handle,
            info->stinfo_ep_in,
            data,
            0x40,
            &rw_bytes,
            USB_TIMEOUT);
    if (res && (res != -9)) {
      fprintf(stderr, "USB transfer failureR %d\n", res);
      return -1;
    } else if (res == -9) {
      printf("Bootloader DFU doesn't support 'get device config' command.\n");
    } else {
      memcpy(info->config.raw_config, data, 0x40);
      /* printf("Info3: ");
      for (int i = 0; i < 0x40; i++) {
        printf("%02X ", info->config.raw_config[i]);
      }
      printf("\n"); */

      if (data[0] == 'P') {
        info->config.modify[confUSB_CUR] = modADD;
        info->config.usb_current = data[1] * 2;
      }
      if (data[2] == 'V') {
        info->config.modify[confMSD_NAME] = modADD;
        memcpy(info->config.volume, data+3, 11);
      }
      if (data[15] == 'B') {
        info->config.modify[confMBED_NAME] = modADD;
        memcpy(info->config.mbed_name, data+16, 4);
      }
      if (data[20] == 'F') {
        info->config.modify[confDFU_OPT] = modADD;
        info->config.dfu_option = data[21];
      }
      if (data[22] == 'D') {
        info->config.modify[confDYN_OPT] = modADD;
        info->config.dynamic_option = (char)data[23];
      }
      if (data[26] == 'O') {
        info->config.modify[confMCO_OUT] = modADD;
        info->config.mco_output = data[27];
      }
      if (data[32] == 'C') {
        info->config.modify[confSTARTUP] = modADD;
        info->config.startup_pref = data[33];
      }
    }

    memset(data, 0, sizeof(data));

    data[0] = ST_DFU_MAGIC;
    data[1] = 0x0A;

    // Write //
    res = libusb_bulk_transfer(info->stinfo_dev_handle,
            info->stinfo_ep_out,
            data,
            16,
            &rw_bytes,
            USB_TIMEOUT);
    if (res) {
      fprintf(stderr, "USB transfer failureW\n");
      return -1;
    }

    // Read //
    res = libusb_bulk_transfer(info->stinfo_dev_handle,
            info->stinfo_ep_in,
            data,
            16,
            &rw_bytes,
            USB_TIMEOUT);
    if (res && (res != -9)) {
      fprintf(stderr, "USB transfer failureR %d\n", res);
      return -1;
    } else if (res == -9) {
      printf("Bootloader DFU doesn't support 'get hardware version' command.\n");
    } else {
      info->hardware_version = data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0];
      if (info->hardware_flags & 0x000001)
        info->flash_size = 128;
      if (info->hardware_flags & 0x000002)
        info->reserved_flash = 20;
    }
  }

  return 0;
}

int stlink_current_mode(struct STLinkInfo *info) {
  unsigned char data[16];
  int rw_bytes, res;

  memset(data, 0, sizeof(data));

  data[0] = 0xF5;

  /* Write */
  res = libusb_bulk_transfer(info->stinfo_dev_handle,
           info->stinfo_ep_out,
           data,
           sizeof(data),
           &rw_bytes,
           USB_TIMEOUT);
  if (res) {
    fprintf(stderr, "USB transfer failure\n");
    return -1;
  }

  /* Read */
  libusb_bulk_transfer(info->stinfo_dev_handle,
           info->stinfo_ep_in,
           data,
           2,
           &rw_bytes,
           USB_TIMEOUT);
  if (res) {
    fprintf(stderr, "stlink_read_info() failure\n");
    return -1;
  }

  if (data[0] == 0)
    info->mode = data[1];

  return data[0] << 8 | data[1];
}

uint16_t stlink_checksum(const unsigned char *firmware,
       size_t len) {
  unsigned int i;
  int ret = 0;

  for (i = 0; i < len; i++) {
    ret += firmware[i];
  }

  return (uint16_t)ret & 0xFFFF;
}

int stlink_dfu_download(struct STLinkInfo *info,
      unsigned char *data,
      const size_t data_len,
      const uint16_t wBlockNum) {
  unsigned char download_request[16];
  struct DFUStatus dfu_status;
  int rw_bytes, res;
  memset(download_request, 0, sizeof(download_request));

  download_request[0] = ST_DFU_MAGIC;
  download_request[1] = DFU_DNLOAD;
  *(uint16_t*)(download_request+2) = wBlockNum; /* wValue */
  *(uint16_t*)(download_request+4) = stlink_checksum(data, data_len); /* wIndex */
  *(uint16_t*)(download_request+6) = data_len; /* wLength */

  if (wBlockNum >= 2) {
    my_encrypt(info->firmware_key, data, data_len);
  }

  res = libusb_bulk_transfer(info->stinfo_dev_handle,
             info->stinfo_ep_out,
             download_request,
             sizeof(download_request),
             &rw_bytes,
             USB_TIMEOUT);
  if (res || rw_bytes != sizeof(download_request)) {
    fprintf(stderr, "USB transfer failure\n");
    return -1;
  }
  res = libusb_bulk_transfer(info->stinfo_dev_handle,
           info->stinfo_ep_out,
           data,
           data_len,
           &rw_bytes,
           USB_TIMEOUT);
  if (res || rw_bytes != (int)data_len) {
    fprintf(stderr, "USB transfer failure\n");
    return -1;
  }

  if (stlink_dfu_status(info, &dfu_status)) {
    return -1;
  }

  if (dfu_status.bState != dfuDNBUSY) {
    fprintf(stderr, "Unexpected DFU state : %d\n", dfu_status.bState);
    return -2;
  }

  if (dfu_status.bStatus != OK) {
    fprintf(stderr, "Unexpected DFU status : %d\n", dfu_status.bStatus);
    return -3;
  }

  usleep(dfu_status.bwPollTimeout * 1000);

  if (stlink_dfu_status(info, &dfu_status)) {
    return -1;
  }

  if (dfu_status.bState != dfuDNLOAD_IDLE) {
    if (dfu_status.bStatus == errVENDOR) {
      fprintf(stderr, "Read-only protection active\n");
      return -3;
    } else if (dfu_status.bStatus == errTARGET) {
      fprintf(stderr, "Invalid Address Error\n");
      return -3;
    } else {
      fprintf(stderr, "Unknown Error : %d\n", dfu_status.bStatus);
      return -3;
    }
  }

  return 0;
}

int stlink_dfu_status(struct STLinkInfo *info, struct DFUStatus *status) {
  unsigned char data[16];
  int rw_bytes, res;

  memset(data, 0, sizeof(data));

  data[0] = ST_DFU_MAGIC;
  data[1] = DFU_GETSTATUS;
  data[6] = 0x06; /* wLength */

  res = libusb_bulk_transfer(info->stinfo_dev_handle,
           info->stinfo_ep_out,
           data,
           16,
           &rw_bytes,
           USB_TIMEOUT);
  if (res || rw_bytes != 16) {
    fprintf(stderr, "USB transfer failure\n");
    return -1;
  }
  res = libusb_bulk_transfer(info->stinfo_dev_handle,
            info->stinfo_ep_in,
           data,
           6,
           &rw_bytes,
           USB_TIMEOUT);
  if (res || rw_bytes != 6) {
    fprintf(stderr, "USB transfer failure\n");
    return -1;
  }

  status->bStatus = data[0];
  status->bwPollTimeout = data[1] | data[2] << 8 | data[3] << 16;
  status->bState = data[4];
  status->iString = data[5];
  
  return 0;
}

int stlink_erase(struct STLinkInfo *info, uint32_t address) {
  unsigned char command[5];
  int res;

  command[0] = ERASE_COMMAND;
  command[1] = address & 0xFF;
  command[2] = (address >> 8) & 0xFF;
  command[3] = (address >> 16) & 0xFF;
  command[4] = (address >> 24) & 0xFF;

  res =  stlink_dfu_download(info, command, sizeof(command), 0);

  return res;
}

int stlink_sector_erase(struct STLinkInfo *info, uint32_t sector) {
  unsigned char command[5];
  int res;

  command[0] = ERASE_SECTOR_COMMAND;
  command[1] = sector & 0xFF;
  command[2] = 0;
  command[3] = 0;
  command[4] = 0;

  res =  stlink_dfu_download(info, command, sizeof(command), 0);

  return res;
}

int stlink_set_address(struct STLinkInfo *info,  uint32_t address) {
  unsigned char set_address_command[5];
  int res;

  set_address_command[0] = SET_ADDRESS_POINTER_COMMAND;
  set_address_command[1] = address & 0xFF;
  set_address_command[2] = (address >> 8) & 0xFF;
  set_address_command[3] = (address >> 16) & 0xFF;
  set_address_command[4] = (address >> 24) & 0xFF;

  res = stlink_dfu_download(info, set_address_command, sizeof(set_address_command), 0);
  return res;
}

int stlink_flash(struct STLinkInfo *info, const char *filename, bool decrypt, bool save) {
  uint32_t file_size, file_read_size;
  
  FILE *fd;
  int res;
  struct stat firmware_stat;
  uint8_t* firmware;

  fd = fopen(filename, "rb");
  if (fd == NULL) {
    fprintf(stderr, "Opening File %s Failed\n", filename);
    return -1;
  }
  
  stat(filename, &firmware_stat);

  file_size = firmware_stat.st_size;
  if (!file_size)
      return -1;

  printf("Loaded firmware : %s, size : %d bytes\n", filename, (int)file_size);
  if (file_size > ((uint32_t)(info->flash_size - 1 - 16 - info->reserved_flash) << 10)) {
    printf("Firmware Size is larger than Flash Size. Continue? [Y/n]: ");
    while (1) {
      int c = getchar();
      if (c != '\n')
        while ((getchar()) != '\n');
      if (c > 0)
        c = tolower(c);
      if (c == 'n')
        return -1;
      if (c == 'y' || c == '\n')
        break;
    }
  }

  int chunk_size = (2 << 10);
  int padding = (16 - (file_size % 16)) % 16;
  firmware = malloc(file_size + padding);
  memset(firmware, 0xFF, file_size + padding);
  file_read_size = fread(firmware, sizeof(unsigned char), file_size, fd);
  if (file_read_size <= 0) {
    fprintf(stderr, "File Read Failed\n");
    free(firmware);
    return file_read_size;
  }

  if (decrypt) {
    if (info->decrypt_key)
      printf("Decrypting Firmware Using Key \"%s\"\n", info->decrypt_key);
    else {
      info->decrypt_key = "best performance";
    }

    for(unsigned int i = 0; i < file_size; i += 0xC00) {
      my_decrypt((unsigned char*)info->decrypt_key, firmware + i, (i + 0xC00) < file_size ? 0xC00 : file_size - i);
    }
    printf("Decrypted Firmware\n");

    if (save) {
      char* dec_filename = malloc(strlen(filename) + 5);
      sprintf(dec_filename, "%s.dec", filename);
      printf("Saving Decrypted Firmware as %s\n", dec_filename);

      FILE* fdw = fopen(dec_filename, "wb");
      if (fdw) {
        fwrite(firmware, sizeof(unsigned char), file_size, fdw);
        fclose(fdw);
      }
      free(dec_filename);
    }
  }

  file_size += padding;

  printf("Firmware Type %s\n\n",  (info->stinfo_bl_type == STLINK_BL_V3) ? "V3" : "V2");
  unsigned int base_offset;
  base_offset =  (info->stinfo_bl_type == STLINK_BL_V3) ? 0x08020000 : 0x08004000;
  
  unsigned int flashed_bytes = 0;
  while (flashed_bytes < file_size) {
    unsigned int cur_chunk_size;
    if ((flashed_bytes + chunk_size) > file_size) {
        cur_chunk_size = file_size - flashed_bytes;
    } else {
        cur_chunk_size = chunk_size;
    }
    int wdl = 2;
    if (info->stinfo_bl_type == STLINK_BL_V3) {
      if (((base_offset + flashed_bytes) & ((1 << 14) - 1)) == 0) {
        uint32_t sector_start[8] = {0x08000000, 0x08004000, 0x08008000, 0x0800C000, 0x08010000, 0x08020000, 0x08040000, 0x08060000};
        int sector = -1;
        int i = 0;
        for (; i < 8; i++) {
          if (sector_start[i] == base_offset + flashed_bytes) {
              sector = i;
              break;
          }
        }
        if (i < 0) {
          fprintf(stderr, "No sector match for address %08x\n", base_offset + flashed_bytes);
          return i;
        }
        printf("Erase Sector %d\n", sector);
        res = stlink_sector_erase(info, sector);
        if (res) {
          fprintf(stderr, "Erase Sector %d failed\n", sector);
          return res;
        }
        printf("Erase Sector %d done\n", sector);
      } else {
          wdl = 3;
      }
    } else {
      res = stlink_erase(info, base_offset + flashed_bytes);
      if (res) {
        fprintf(stderr, "Erase Error at 0x%08x\n", base_offset + flashed_bytes);
        return res;
      }
    }
    res = stlink_set_address(info, base_offset+flashed_bytes);
    if (res) {
      fprintf(stderr, "Set Address Error at 0x%08x\n", base_offset + flashed_bytes);
      return res;
    } else {
      //printf("set address to 0x%08x done\n", base_offset + flashed_bytes);
    }
    res = stlink_dfu_download(info, firmware + flashed_bytes, cur_chunk_size, wdl);
    if (res) {
      fprintf(stderr, "Download Error at 0x%08x\n", base_offset + flashed_bytes);
      return res;
    } else {
      printf("Download at 0x%08x done. %.1f%%\r", base_offset + flashed_bytes, ((float)(flashed_bytes + cur_chunk_size) / file_size) * 100.0);
    }

    fflush(stdout); /* Flush stdout buffer */

    flashed_bytes += cur_chunk_size;
  }

  free(firmware);
  fclose(fd);

  printf("Downloaded Firmware File            \n");

  return 0;
}

int stlink_exit_dfu(struct STLinkInfo *info) {
  unsigned char data[16];
  int rw_bytes, res;
  
  memset(data, 0, sizeof(data));
  
  data[0] = ST_DFU_MAGIC;
  data[1] = DFU_EXIT;
  
  res = libusb_bulk_transfer(info->stinfo_dev_handle,
           info->stinfo_ep_out,
           data,
           16,
           &rw_bytes,
           USB_TIMEOUT);
  if (res || rw_bytes != 16) {
    fprintf(stderr, "USB transfer failure\n");
    return -1;
  }
  return 0;
}

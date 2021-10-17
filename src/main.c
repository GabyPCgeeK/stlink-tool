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

#ifdef WINDOWS
  #include "getopt.h"
#else
  #include <getopt.h>
#endif

#include "stlink.h"

#ifndef min
  #define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define max_per_line(x) (++x % 2 == 0 ? '\n' : '\0')

#define STLINK_VID        0x0483
#define STLINK_PID        0x3748
#define STLINK_PIDV21     0x374b
#define STLINK_PIDV21_MSD 0x3752
#define STLINK_PIDV3      0x374f
#define STLINK_PIDV3_BL   0x374d

#define OPENMOKO_VID      0x1d50
#define BMP_APPL_PID      0x6018
#define BMP_DFU_IF        4

enum OptionsVal {
  optHELP = 0,
  optPROBE,
  optDECRYPT,
  optSAVE_DEC,
  optST_TYPE,
  optVERSION,
  optFIX,
  optUSB_CUR,
  optMSD_NAME,
  optMBED_NAME,
  optDFU_OPT,
  optDYN_OPT,
  optMCO_OUT,
  optSTARTUP
};

static struct option long_options[] = {
  {"help",           0, 0,  optHELP},
  {"h",              0, 0,  optHELP},
   
  {"probe",          0, 0,  optPROBE},
  {"p",              0, 0,  optPROBE},
   
  {"decrypt",        1, 0,  optDECRYPT},
  {"d",              1, 0,  optDECRYPT},
   
  {"save_dec",       0, 0,  optSAVE_DEC},
  {"sd",             0, 0,  optSAVE_DEC},
   
  {"st_type",        1, 0,  optST_TYPE},
  {"t",              1, 0,  optST_TYPE},

  {"ver",            1, 0,  optVERSION},
  {"v",              1, 0,  optVERSION},

  {"fix",            0, 0,  optFIX},
  {"f",              0, 0,  optFIX},
   
  {"usb_cur",        1, 0,  optUSB_CUR},
  {"rm_usb_cur",     0, 0,  optUSB_CUR},
   
  {"msd_name",       1, 0,  optMSD_NAME},
  {"rm_msd_name",    0, 0,  optMSD_NAME},
   
  {"mbed_name",      1, 0,  optMBED_NAME},
  {"rm_mbed_name",   0, 0,  optMBED_NAME},
   
  {"dfu_opt",        1, 0,  optDFU_OPT},
  {"rm_dfu_opt",     0, 0,  optDFU_OPT},
   
  {"dynamic_opt",    1, 0,  optDYN_OPT},
  {"rm_dynamic_opt", 0, 0,  optDYN_OPT},
   
  {"mco_out",        1, 0,  optMCO_OUT},
  {"rm_mco_out",     0, 0,  optMCO_OUT},
   
  {"startup",        1, 0,  optSTARTUP},
  {"rm_startup",     0, 0,  optSTARTUP},
  
  {0 ,0, 0, 0}
};

void print_help(char *argv[]) {
  printf("Usage: %s [options] [firmware.bin]\n", argv[0]);
  printf("Options:\n");
  printf("  -h, --help\t\tShow help\n");
  printf("  -p, --probe\t\tProbe the ST-Link adapter\n");
  printf("  -d, --decrypt KEY\tDecrypt Firmware using KEY. Pass \"\" to use internal key.\n");
  printf("  -sd, --save_dec\tSave decripted firmware as filename + .dec\n");
  printf("  -t, --st_type TYPE\tChange STLink type to TYPE.\n");
  for (int i = 'A'; i <= 'Z'; i++) {
    if (st_types[i])
      printf("\t\t\t  %c for \"%s\"\n", (char)i, st_types[i]);
  }
  printf("  -v, --ver S.J.X\tChange reported STLink sersion.\n\t\t\t  S is STLink version, J is JTAG version,\n\t\t\t  X is SWIM or MSD version.\n");
  printf("  -f, --fix\t\tFlash Anti-Clone Tag and Firmware Exists/EOF Tag\n\n");
  printf("Options for Modifying Device Config (Only for STLink v2 and up):\n");
  printf("  --usb_cur CURRENT\tSet the MaxPower reported in USB Descriptor\n\t\t\tto CURRENT(mA)\n");
  printf("  --msd_name VOLUME\tSet the volsume name of the MSD drive to VOLUME.\n");
  printf("  --mbed_name NAME\tSet the MBED board name to NAME.\n");
  printf("  --dfu_opt OPT\t\tSet DFU Options to OPT.\n\t\t\tOPT is the Decimal value of Bit Field:\n\t\t\t  bit1: \"No Power Off\"\n\t\t\t  bit2: \"Autostart\"\n");
  printf("  --dynamic_opt OPT\tSet Dynamic Option to OPT.\n\t\t\t  'V': MSD Off\n\t\t\t  'M': MSD On\n\t\t\t  'W': MSD Always Off\n");
  printf("  --mco_out OPT\t\tSet MCO Output to OPT. OPT is the Hex value of:\n\t\t\t  Lower Nybble(MCO Source):\n\t\t\t    0: None\n\t\t\t    1: HSI\n\t\t\t    2: HSE\n\t\t\t    3: PLL\n\t\t\t  Upper Nybble (Divider):\n\t\t\t    Divider - 1 (Valid Divider 1-5)\n");
  printf("  --startup OPT\t\tSet Startup Preferences to OPT.\n\t\t\t  0: High Power\n\t\t\t  1: Balanced\n\t\t\t  2: Low Power\n\t\t\t  3: Default\n");
  printf("  To remove a configuration you can use the \"\" argument with the option\n  (Ex. --usb_cur \"\") or prefix the option with rm_ (Ex. --rm_usb_cur).\n\n");
  printf("Application in Flash is started when called without argument, after firmware\nload or configuration change.\n\n");
}

#include <string.h>
int main(int argc, char *argv[]) {
  struct STLinkInfo info;
  struct STLinkConfig config;
  int res = EXIT_FAILURE, i, opt;
  bool probe = false, decrypt = false, save_decrypted = false, flash_config = false, fix_config = false;
  char* boot_ver = "";
  char ver_type = 'S';

  memset(&info.config, 0, sizeof(info.config));
  memset(info.config.raw_config, 0xFF, sizeof(info.config.raw_config));
  memset(&config, 0, sizeof(config));
  memset(config.raw_config, 0xFF, sizeof(config.raw_config));

  while ((opt = getopt_long_only(argc, argv, ":", long_options, NULL)) != -1) {
    //printf("%d/%d %d %c %s", optind, argc, opt, optopt, optarg);
    switch (opt) {
      case optPROBE: /* Probe mode */
        probe = true;
        break;
      case optDECRYPT:
        decrypt = true;
        if (optarg && strlen(optarg) > 0) {
          info.decrypt_key = optarg;
        } else {
          info.decrypt_key = NULL;
        }
        break;
      case optSAVE_DEC:
        save_decrypted = true;
        break;
      case optST_TYPE:
        if (optarg && strlen(optarg) > 0) {
          config.modify[confST_TYPE] = modADD;
          config.stlink_type = optarg[0];
        }
        break;
      case optVERSION:
        if (optarg && strlen(optarg) > 0) {
          char* st_v = strtok(optarg, ".");
          char* jt_v = strtok(NULL, ".");
          char* sw_v = strtok(NULL, ".");

          config.modify[confVERSION] = modADD;
          config.soft_version = (atoi(st_v) & 0xF) << 12 | (atoi(jt_v) & 0x3F) << 6 | (atoi(sw_v) & 0x3F);
        }
        break;
      case optFIX:
        fix_config = true;
        break;
      case optUSB_CUR:
        if (optarg && strlen(optarg) > 0) {
          config.modify[confUSB_CUR] = modADD;
          config.usb_current = atoi(optarg);
        } else {
          config.modify[confUSB_CUR] = modREMOVE;
        }
        break;
      case optMSD_NAME:
        if (optarg && strlen(optarg) > 0) {
          config.modify[confMSD_NAME] = modADD;
          memcpy(config.volume, optarg, min(sizeof(config.volume) - 1, strlen(optarg)));
          config.volume[11] = '\0';
        } else {
          config.modify[confMSD_NAME] = modREMOVE;
        }
        break;
      case optMBED_NAME:
        if (optarg && strlen(optarg) > 0) {
          config.modify[confMBED_NAME] = modADD;
          memcpy(config.mbed_name, optarg, min(sizeof(config.mbed_name) - 1, strlen(optarg)));
          config.mbed_name[4] = '\0';
        } else {
          config.modify[confMBED_NAME] = modREMOVE;
        }
        break;
      case optDFU_OPT:
        if (optarg && strlen(optarg) > 0) {
          config.modify[confDFU_OPT] = modADD;
          config.dfu_option = atoi(optarg);
        } else {
          config.modify[confDFU_OPT] = modREMOVE;
        }
        break;
      case optDYN_OPT:
        if (optarg && strlen(optarg) > 0) {
          config.modify[confDYN_OPT] = modADD;
          config.dynamic_option = optarg[0];
        } else {
          config.modify[confDYN_OPT] = modREMOVE;
        }
        break;
      case optMCO_OUT:
        if (optarg && strlen(optarg) > 0) {
          config.modify[confMCO_OUT] = modADD;
          config.mco_output = strtol(optarg, NULL, 16);
        } else {
          config.modify[confMCO_OUT] = modREMOVE;
        }
        break;
      case optSTARTUP:
        if (optarg && strlen(optarg) > 0) {
          config.modify[confSTARTUP] = modADD;
          config.startup_pref = atoi(optarg);
        } else {
          config.modify[confSTARTUP] = modREMOVE;
        }
        break;
      case optHELP: /* Help */
        print_help(argv);
        return EXIT_SUCCESS;
        break;
      default:
        print_help(argv);
        return EXIT_FAILURE;
        break;
    }
  }

  int do_load = (optind < argc);

  res = libusb_init(&info.stinfo_usb_ctx);
rescan:
  info.stinfo_dev_handle = NULL;
  libusb_device **devs;
  int n_devs = libusb_get_device_list(info.stinfo_usb_ctx, &devs);
  if (n_devs < 0)
    goto exit_libusb;
  for (int i = 0;  devs[i]; i++) {
    libusb_device *dev =  devs[i];
    struct libusb_device_descriptor desc;
    int res = libusb_get_device_descriptor(dev, &desc);
    if (res < 0)
      continue;
    if ((desc.idVendor == OPENMOKO_VID) && (desc.idProduct == BMP_APPL_PID)) {
      res = libusb_open(dev, &info.stinfo_dev_handle);
      if (res < 0) {
          fprintf(stderr, "Can not open BMP/Application!\n");
          continue;
      }
      libusb_claim_interface(info.stinfo_dev_handle, BMP_DFU_IF);
      res = libusb_control_transfer(info.stinfo_dev_handle,
                                    /* bmRequestType */ LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                    /* bRequest      */ 0, /*DFU_DETACH,*/
                                    /* wValue        */ 1000,
                                    /* wIndex        */ BMP_DFU_IF,
                                    /* Data          */ NULL,
                                    /* wLength       */ 0,
                                    5000 );
      libusb_release_interface(info.stinfo_dev_handle, 0);
      if (res < 0) {
        fprintf(stderr, "BMP Switch failed\n");
        continue;
      }
      libusb_free_device_list(devs, 1);
      usleep(2000000);
      goto rescan;
      break;
    }
    if (desc.idVendor != STLINK_VID)
      continue;
    switch (desc.idProduct) {
    case STLINK_PID:
      res = libusb_open(dev, &info.stinfo_dev_handle);
      if (res < 0) {
        fprintf(stderr, "Can not open STLINK/Bootloader!\n");
        continue;
      }
      info.stinfo_ep_in  = 1 | LIBUSB_ENDPOINT_IN;
      info.stinfo_ep_out = 2 | LIBUSB_ENDPOINT_OUT;
      info.stinfo_bl_type = STLINK_BL_V2;
      //fprintf(stderr, "STLinkV2, STLinkV2-1 Bootloader found\n");
      break;
    case STLINK_PIDV3_BL:
      res = libusb_open(dev,  &info.stinfo_dev_handle);
      if (res < 0) {
          fprintf(stderr, "Can not open STLINK-V3/Bootloader!\n");
          continue;
      }
      info.stinfo_ep_in  = 1 | LIBUSB_ENDPOINT_IN;
      info.stinfo_ep_out = 1 | LIBUSB_ENDPOINT_OUT;
      info.stinfo_bl_type = STLINK_BL_V3;
      //fprintf(stderr, "StlinkV3 Bootloader found\n");
      break;
    case STLINK_PIDV21:
    case STLINK_PIDV21_MSD:
    case STLINK_PIDV3:
      res = libusb_open(dev, &info.stinfo_dev_handle);
      if (res < 0) {
        fprintf(stderr, "Can not open STLINK/Application!\n");
        continue;
      }
      if (libusb_claim_interface(info.stinfo_dev_handle, 0)) {
        fprintf(stderr, "Unable to claim USB interface ! Please close all programs that "
                "may communicate with an ST-Link dongle.\n");
        continue;
      }
      res = stlink_dfu_mode(info.stinfo_dev_handle, 0);
      if (res != 0x8000) {
        libusb_release_interface(info.stinfo_dev_handle, 0);
        return 0;
      }
      stlink_dfu_mode(info.stinfo_dev_handle, 1);
      libusb_release_interface(info.stinfo_dev_handle, 0);
      libusb_free_device_list(devs, 1);
      fprintf(stderr, "Trying to switch STLINK/Application to bootloader\n");
      usleep(3000000);
      goto rescan;
      break;
    }
    if (info.stinfo_dev_handle)
      break;
  }
  libusb_free_device_list(devs, 1);
  if (!info.stinfo_dev_handle) {
    fprintf(stderr, "No ST-Link in DFU mode found. Replug ST-Link to flash!\n");
    return EXIT_FAILURE;
  }

  if (libusb_claim_interface(info.stinfo_dev_handle, 0)) {
    fprintf(stderr, "Unable to claim USB interface ! Please close all programs that "
            "may communicate with an ST-Link dongle.\n");
    return EXIT_FAILURE;
  }

  if (stlink_read_info(&info)) {
    libusb_release_interface(info.stinfo_dev_handle, 0);
    return EXIT_FAILURE;
  }

  switch (info.stinfo_bl_type) {
  case STLINK_BL_V2:
    boot_ver = "2";
    break;
  case STLINK_BL_V21:
    boot_ver = "2-1";
    ver_type = 'M';
    break;
  case STLINK_BL_V3:
    boot_ver = "3";
    break;
  }

  printf("STLinkV%s Bootloader Found\n", boot_ver);
  char msd_opt = info.config.dynamic_option == 'V' ? 'A' : 0;
  printf("STLink Type: %c [%s]\n", info.config.stlink_type, st_types[(uint8_t)info.config.stlink_type - msd_opt]);
  printf("Firmware Version: V%uJ%u%c%u\n\n", info.stlink_version,
        info.jtag_version, ver_type, info.swim_version);

  if (info.mode > 1) {
    i = 0;
    printf("Current Device Configuration:\n");
    if (info.config.modify[confUSB_CUR] == modADD)
      printf("USB Current: [%umA] %c", info.config.usb_current, max_per_line(i));
    if (info.config.modify[confMSD_NAME] == modADD)
      printf("MSD Volume: [%s] %c", info.config.volume, max_per_line(i));
    if (info.config.modify[confMBED_NAME] == modADD)
      printf("MBED Board Name: [%s] %c", info.config.mbed_name, max_per_line(i));
    if (info.config.modify[confDFU_OPT] == modADD)
      printf("DFU Options: [%s] %c", stlink_get_dev_config(&info.config, confDFU_OPT), max_per_line(i));
    if (info.config.modify[confDYN_OPT] == modADD)
      printf("Dynamic Options: [%s] %c", stlink_get_dev_config(&info.config, confDYN_OPT), max_per_line(i));
    if (info.config.modify[confMCO_OUT] == modADD)
      printf("MCO Output: [%u] %c", info.config.mco_output, max_per_line(i));
    if (info.config.modify[confSTARTUP] == modADD)
      printf("Startup Pref: [%s]", stlink_get_dev_config(&info.config, confSTARTUP));
    printf("\n\n");
  }

  printf("Bootloader PID: %04X\n", info.bootloader_pid);
  if (info.mode > 1)
    printf("HW Version: V%u.%u   Flags: 0x%06X\n", info.hardware_mayor, info.hardware_minor, info.hardware_flags);
  printf("Reported Flash Size: %uKB%s%s\n\n", info.reported_flash_size, info.hardware_flags & 0x0001 ? " (128KB Overwrite Flag)" : "", info.hardware_flags & 0x0002 ? " (20KB Reserved Flash)" : "");
  
  printf("STLink ID: ");
  for (i = 0; i < 12; i += 4) {
    printf("%02X", info.id[i + 3]);
    printf("%02X", info.id[i + 2]);
    printf("%02X", info.id[i + 1]);
    printf("%02X", info.id[i + 0]);
  } 
  printf("\n");

  printf("Firmware Encryption Key: ");
  for (i = 0; i < 16; i++) {
    printf("%02X", info.firmware_key[i]);
  }
  printf("\n");

  printf("Anti-Clone Key: ");
  for (i = 0; i < 16; i++) {
    printf("%02X", info.anti_clone[i]);
  }
  printf("\n");

  res = stlink_current_mode(&info);
  if (res < 0) {
    libusb_release_interface(info.stinfo_dev_handle, 0);
    return EXIT_FAILURE;
  }
  printf("Current Mode: %d\n\n", res);

  if (res & 0xfffc) {
    printf("ST-Link dongle is not in the correct mode. Please unplug and plug the dongle again.\n");
    libusb_release_interface(info.stinfo_dev_handle, 0);
    return EXIT_SUCCESS;
  }

  if (!probe) {
    for (int i = 0; i < 9; i++) {
      if (config.modify[i] != modCOPY) {
        flash_config = true;
        break;
      }
    }

    if (do_load)
      if (stlink_flash(&info, argv[optind], decrypt, save_decrypted))
        flash_config = fix_config = false;

    if (flash_config || fix_config) {
      stlink_flash_config_area(&info, &config);
    }
    stlink_exit_dfu(&info);
  }
  libusb_release_interface(info.stinfo_dev_handle, 0);
exit_libusb:
  libusb_exit(info.stinfo_usb_ctx);

  return EXIT_SUCCESS;
}

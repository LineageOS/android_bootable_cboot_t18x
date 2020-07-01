/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_EEPROM_LAYOUT_H
#define TEGRABL_EEPROM_LAYOUT_H

#include <stdint.h>

/**
 * @brief The Product Part Number structure that is embedded into
 * EEPROM layout structure
 *
 * @param leading - 699 or 600 sticker info
 * @param separator_0 - separator, always '-'
 * @param class - Board Class, always 8 for mobile
 * @param id - Board ID (Quill Product 3310)
 * @param separator_1 - separator, always '-'
 * @param sku - Board SKU
 * @param separator_2 - separator, always '-'
 * @param fab - FAB value, Eg: 100, 200, 300
 * @param separator_3 - separator, space ' '
 * @param rev - Manufacturing Major revision
 * @param separator_4 - separator, always '.'
 * @param ending - Always "0"
 * @param pad - 0x00
 */
TEGRABL_PACKED(
struct part_no_t {				/* 20 - 49 */
	uint8_t	leading[3];			/* 20 */
	uint8_t	separator_0;		/* 23 */
	uint8_t	class;				/* 24 */
	uint8_t	id[4];				/* 25 */
	uint8_t	separator_1;		/* 29 */
	uint8_t	sku[4];				/* 30 */
	uint8_t	separator_2;		/* 34 */
	uint8_t	fab[3];				/* 35 */
	uint8_t	separator_3;		/* 38 */
	uint8_t	rev;				/* 39 */
	uint8_t	separator_4;		/* 40 */
	uint8_t	ending;				/* 41 */
	uint8_t	pad[8];				/* 42 */
}
);

/**
 * @brief The layout of data in EEPROMS
 *
 * @param version - Version of Board ID contents
 * @param size - Size of Board ID data that follows this address
 * @param board_no - ID of the board on which EEPROM in mounted
 * @param sku - Always matches Board SKU on sticker
 * @param fab - fabrication ID of the Board
 * @param rev - revision of the Board
 * @param minor_rev - Minor revision
 * @param mem_type - Memory type
 * @param powrr_config - Power cfgs like PM Stuff, DC-DC, VF, Max * Curr Limits
 * @param misc_config - Defines spl reworks, mech. Changes. Its a bitwise field
 * @param modem_config - Modem, eg: Icera Modem fuse/unfuse, Antenna bands
 * @param touch_config - Reworks related to touch
 * @param disp_config - Reflects any spl reworks/changes related to Display
 * @param rework_level - Syseng Rework Level
 * @param reserved_0 - Reserved bytes
 * @param part_no - asset_tracker_field_1 - 699 or 600 BOM Number
 * @param wifi_mac_addr - MAC address for primary wifi chip
 * @param bt_mac_addr - MAC address for bluetooth chip
 * @param sec_wifi_mac_addr - MAC address for secondary wifi chip
 * @param eth_mac_addr - MAC address for ethernet port
 * @param serial_no - asset_field_tracker_2 - Serial number on sticker
 * @param reserved_1 - Reserved bytes
 * @param cust_blocksig - 'NVCB' - NV Config Block
 * @param cust_blocklen - Length from Block Signature to end of EEPROM
 * @param cust_typesig - 'M1' - MAC Address Struc Type 1
 * @param cust_ver - 0x0000
 * @param cust_wifi_mac_addr - Customer usable field
 * @param cust_bt_mac_addr - Customer usable field
 * @param cust_eth_mac_addr - Customer usable field
 * @param reserved_2 - Reserved for future use
 * @param checksum - CRC-8 computed for bytes 0 through 254
 */
TEGRABL_PACKED(
struct eeprom_layout {
	uint16_t	version;				/* 00 */
	uint16_t	size;					/* 02 */
	uint16_t	board_no;				/* 04 */
	uint16_t	sku;					/* 06 */
	uint8_t		fab;					/* 08 */
	uint8_t		rev;					/* 09 */
	uint8_t		rev_minor;				/* 10 */
	uint8_t		mem_type;				/* 11 */
	uint8_t		power_config;			/* 12 */
	uint8_t		misc_config;			/* 13 */
	uint8_t		modem_config;			/* 14 */
	uint8_t		touch_config;			/* 15 */
	uint8_t		display_config;			/* 16 */
	uint8_t		rework_level;			/* 17 */
	uint8_t		reserved_0[2];			/* 18 */
	struct part_no_t part_no;			/* 20 - 49 */
	uint8_t		wifi_mac_addr[6];		/* 50 */
	uint8_t		bt_mac_addr[6];			/* 56 */
	uint8_t		sec_wifi_mac_addr[6];	/* 62 */
	uint8_t		eth_mac_addr[6];		/* 68 */
	uint8_t		serial_no[15];			/* 74 */
	uint8_t		reserved_1[61];			/* 89 */
	uint8_t		cust_blocksig[4];		/* 150 */
	uint16_t	cust_blocklen;			/* 154 */
	uint8_t		cust_typesig[2];		/* 156 */
	uint16_t	cust_ver;				/* 158 */
	uint8_t		cust_wifi_mac_addr[6];	/* 160 */
	uint8_t		cust_bt_mac_addr[6];	/* 166 */
	uint8_t		cust_eth_mac_addr[6];	/* 172 */
	uint8_t		reserved_2[77];			/* 178 */
	uint8_t		checksum;				/* 255 */
}
);

#define EEPROM_SZ		sizeof(struct eeprom_layout)
#define EEPROM_BDID_SZ	sizeof(((struct part_no_t *)0)->id)
#define EEPROM_SKU_SZ	sizeof(((struct part_no_t *)0)->sku)
#define EEPROM_FAB_SZ	sizeof(((struct part_no_t *)0)->fab)
#define EEPROM_REV_SZ	sizeof(((struct part_no_t *)0)->rev)
#define EEPROM_FULL_BDID_LEN	(EEPROM_BDID_SZ + 1 + EEPROM_SKU_SZ + 1 \
									+ EEPROM_FAB_SZ + 1 + EEPROM_REV_SZ + 1)
#define EEPROM_CUST_SIG_SIZE   4
#define EEPROM_CUST_TYPE_SIZE  2

#endif /* TEGRABL_EEPROM_LAYOUT_H */

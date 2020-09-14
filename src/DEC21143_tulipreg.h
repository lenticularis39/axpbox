/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://sourceforge.net/projects/es40
 * E-mail : camiel@camicom.com
 *
 * This file is based upon NetBsd.
 *
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 * Contains the definitions for the registers for the emulated DEC 21143
 * NIC device.
 *
 * $Id: DEC21143_tulipreg.h,v 1.4 2008/03/14 15:30:51 iamcamiel Exp $
 *
 * X-1.3        Camiel Vanderhoeven                             02-JAN-2008
 *      Cleanup.
 *
 * X-1.2        Camiel Vanderhoeven                             15-NOV-2007
 *      Added newline at end to avoid warnings.
 *
 * X-1.1        Camiel Vanderhoeven                             14-NOV-2007
 *      Initial version for ES40 emulator.
 *
 * \author Camiel Vanderhoeven (camiel@camicom.com / http://www.camicom.com)
 **/
#ifndef __volatile
#define __volatile
#endif
#ifndef _DEV_IC_TULIPREG_H_
#define _DEV_IC_TULIPREG_H_

/*
 * Register description for the Digital Semiconductor ``Tulip'' (21x4x)
 * Ethernet controller family.
 */

/*
 * Descriptor Status bits common to transmit and receive.
 */
#define TDSTAT_OWN 0x80000000 /* Tulip owns descriptor */
#define TDSTAT_ES 0x00008000  /* Error Summary */

/*
 * Descriptor Status bits for Receive Descriptor.
 */
#define TDSTAT_Rx_FF 0x40000000  /* Filtering Fail */
#define TDSTAT_Rx_FL 0x3fff0000  /* Frame Length including CRC */
#define TDSTAT_Rx_DE 0x00004000  /* Descriptor Error */
#define TDSTAT_Rx_DT 0x00003000  /* Data Type */
#define TDSTAT_Rx_RF 0x00000800  /* Runt Frame */
#define TDSTAT_Rx_MF 0x00000400  /* Multicast Frame */
#define TDSTAT_Rx_FS 0x00000200  /* First Descriptor */
#define TDSTAT_Rx_LS 0x00000100  /* Last Descriptor */
#define TDSTAT_Rx_TL 0x00000080  /* Frame Too Long */
#define TDSTAT_Rx_CS 0x00000040  /* Collision Seen */
#define TDSTAT_Rx_RT 0x00000020  /* Frame Type */
#define TDSTAT_Rx_RW 0x00000010  /* Receive Watchdog */
#define TDSTAT_Rx_RE 0x00000008  /* Report on MII Error */
#define TDSTAT_Rx_DB 0x00000004  /* Dribbling Bit */
#define TDSTAT_Rx_CE 0x00000002  /* CRC Error */
#define TDSTAT_Rx_ZER 0x00000001 /* Zero (always 0) */

#define TDSTAT_Rx_LENGTH(x) (((x)&TDSTAT_Rx_FL) >> 16)
#define TDSTAT_Rx_DT_SR 0x00000000 /* Serial Received Frame */
#define TDSTAT_Rx_DT_IL 0x00001000 /* Internal Loopback Frame */
#define TDSTAT_Rx_DT_EL 0x00002000 /* External Loopback Frame */
#define TDSTAT_Rx_DT_r 0x00003000  /* Reserved */

/*
 * Descriptor Status bits for Transmit Descriptor.
 */
#define TDSTAT_Tx_TO 0x00004000 /* Transmit Jabber Timeout */
#define TDSTAT_Tx_LO 0x00000800 /* Loss of Carrier */
#define TDSTAT_Tx_NC 0x00000400 /* No Carrier */
#define TDSTAT_Tx_LC 0x00000200 /* Late Collision */
#define TDSTAT_Tx_EC 0x00000100 /* Excessive Collisions */
#define TDSTAT_Tx_HF 0x00000080 /* Heartbeat Fail */
#define TDSTAT_Tx_CC 0x00000078 /* Collision Count */
#define TDSTAT_Tx_LF 0x00000004 /* Link Fail */
#define TDSTAT_Tx_UF 0x00000002 /* Underflow Error */
#define TDSTAT_Tx_DE 0x00000001 /* Deferred */

#define TDSTAT_Tx_COLLISIONS(x) (((x)&TDSTAT_Tx_CC) >> 3)

/*
 * Descriptor Control bits common to transmit and receive.
 */
#define TDCTL_SIZE1 0x000007ff /* Size of buffer 1 */
#define TDCTL_SIZE1_SHIFT 0

#define TDCTL_SIZE2 0x003ff800 /* Size of buffer 2 */
#define TDCTL_SIZE2_SHIFT 11

#define TDCTL_ER 0x02000000 /* End of Ring */
#define TDCTL_CH 0x01000000 /* Second Address Chained */

/*
 * Descriptor Control bits for Transmit Descriptor.
 */
#define TDCTL_Tx_IC 0x80000000  /* Interrupt on Completion */
#define TDCTL_Tx_LS 0x40000000  /* Last Segment */
#define TDCTL_Tx_FS 0x20000000  /* First Segment */
#define TDCTL_Tx_FT1 0x10000000 /* Filtering Type 1 */
#define TDCTL_Tx_SET 0x08000000 /* Setup Packet */
#define TDCTL_Tx_AC 0x04000000  /* Add CRC Disable */
#define TDCTL_Tx_DPD 0x00800000 /* Disabled Padding */
#define TDCTL_Tx_FT0 0x00400000 /* Filtering Type 0 */

/*
 * The Tulip filter is programmed by "transmitting" a Setup Packet
 * (indicated by TDCTL_Tx_SET).  The filtering type is indicated
 * as follows:
 *
 *	FT1	FT0	Description
 *	---	---	-----------
 *	0	0	Perfect Filtering: The Tulip interprets the
 *			descriptor buffer as a table of 16 MAC addresses
 *			that the Tulip should receive.
 *
 *	0	1	Hash Filtering: The Tulip interprets the
 *			descriptor buffer as a 512-bit hash table
 *			plus one perfect address.  If the incoming
 *			address is Multicast, the hash table filters
 *			the address, else the address is filtered by
 *			the perfect address.
 *
 *	1	0	Inverse Filtering: Like Perfect Filtering, except
 *			the table is addresses that the Tulip does NOT
 *			receive.
 *
 *	1	1	Hash-only Filtering: Like Hash Filtering, but
 *			physical addresses are matched by the hash table
 *			as well, and not by matching a single perfect
 *			address.
 *
 * A Setup Packet must always be 192 bytes long.  The Tulip can store
 * 16 MAC addresses.  If not all 16 are specified in Perfect Filtering
 * or Inverse Filtering mode,  then unused entries should duplicate
 * one of the valid entries.
 */
#define TDCTL_Tx_FT_PERFECT 0
#define TDCTL_Tx_FT_HASH TDCTL_Tx_FT0
#define TDCTL_Tx_FT_INVERSE TDCTL_Tx_FT1
#define TDCTL_Tx_FT_HASHONLY (TDCTL_Tx_FT1 | TDCTL_Tx_FT0)
#define TULIP_SETUP_PACKET_LEN 192
#define TULIP_MAXADDRS 16
#define TULIP_MCHASHSIZE 512

/*
 * Maximum size of a Tulip Ethernet Address ROM or SROM.
 */
#define TULIP_ROM_SIZE(bits) (2 << (bits))
#define TULIP_MAX_ROM_SIZE 512

/*
 * Format of the standard Tulip SROM information:
 *
 *	Byte offset	Size	Usage
 *	0		18	reserved
 *	18		1	SROM Format Version
 *	19		1	Chip Count
 *	20		6	IEEE Network Address
 *	26		1	Chip 0 Device Number
 *	27		2	Chip 0 Info Leaf Offset
 *	29		1	Chip 1 Device Number
 *	30		2	Chip 1 Info Leaf Offset
 *	32		1	Chip 2 Device Number
 *	33		2	Chip 2 Info Leaf Offset
 *	...		1	Chip n Device Number
 *	...		2	Chip n Info Leaf Offset
 *	...		...	...
 *	Chip Info Leaf Information
 *	...
 *	...
 *	...
 *	126		2	CRC32 checksum
 */
#define TULIP_ROM_SROM_FORMAT_VERION 18 /* B */
#define TULIP_ROM_CHIP_COUNT 19         /* B */
#define TULIP_ROM_IEEE_NETWORK_ADDRESS 20
#define TULIP_ROM_CHIPn_DEVICE_NUMBER(n) (26 + ((n)*3))    /* B */
#define TULIP_ROM_CHIPn_INFO_LEAF_OFFSET(n) (27 + ((n)*3)) /* W */
#define TULIP_ROM_CRC32_CHECKSUM 126                       /* W */
#define TULIP_ROM_CRC32_CHECKSUM1 94                       /* W */

#define TULIP_ROM_IL_SELECT_CONN_TYPE 0 /* W */
#define TULIP_ROM_IL_MEDIA_COUNT 2      /* B */
#define TULIP_ROM_IL_MEDIAn_BLOCK_BASE 3

#define SELECT_CONN_TYPE_TP 0x0000
#define SELECT_CONN_TYPE_BNC 0x0001
#define SELECT_CONN_TYPE_AUI 0x0002
#define SELECT_CONN_TYPE_100TX 0x0003
#define SELECT_CONN_TYPE_100T4 0x0006
#define SELECT_CONN_TYPE_100FX 0x0007
#define SELECT_CONN_TYPE MII_10T 0x0009
#define SELECT_CONN_TYPE_MII_100TX 0x000d
#define SELECT_CONN_TYPE_MII_100T4 0x000f
#define SELECT_CONN_TYPE_MII_100FX 0x0010
#define SELECT_CONN_TYPE_TP_AUTONEG 0x0100
#define SELECT_CONN_TYPE_TP_FDX 0x0204
#define SELECT_CONN_TYPE_MII_10T_FDX 0x020a
#define SELECT_CONN_TYPE_100TX_FDX 0x020e
#define SELECT_CONN_TYPE_MII_100TX_FDX 0x0211
#define SELECT_CONN_TYPE_TP_NOLINKPASS 0x0400
#define SELECT_CONN_TYPE_ASENSE 0x0800
#define SELECT_CONN_TYPE_ASENSE_POWERUP 0x8800
#define SELECT_CONN_TYPE_ASENSE_AUTONEG 0x0900

#define TULIP_ROM_MB_MEDIA_CODE 0x3f
#define TULIP_ROM_MB_MEDIA_TP 0x00
#define TULIP_ROM_MB_MEDIA_BNC 0x01
#define TULIP_ROM_MB_MEDIA_AUI 0x02
#define TULIP_ROM_MB_MEDIA_100TX 0x03
#define TULIP_ROM_MB_MEDIA_TP_FDX 0x04
#define TULIP_ROM_MB_MEDIA_100TX_FDX 0x05
#define TULIP_ROM_MB_MEDIA_100T4 0x06
#define TULIP_ROM_MB_MEDIA_100FX 0x07
#define TULIP_ROM_MB_MEDIA_100FX_FDX 0x08

#define TULIP_ROM_MB_EXT 0x40

#define TULIP_ROM_MB_CSR13 1 /* W */
#define TULIP_ROM_MB_CSR14 3 /* W */
#define TULIP_ROM_MB_CSR15 5 /* W */

#define TULIP_ROM_MB_SIZE(mc) (((mc)&TULIP_ROM_MB_EXT) ? 7 : 1)
#define TULIP_ROM_MB_NOINDICATOR 0x8000
#define TULIP_ROM_MB_DEFAULT 0x4000
#define TULIP_ROM_MB_POLARITY 0x0080
#define TULIP_ROM_MB_OPMODE(x) (((x)&0x71) << 18)
#define TULIP_ROM_MB_BITPOS(x) (1 << (((x)&0x0e) >> 1))
#define TULIP_ROM_MB_21140_GPR 0   /* 21140[A] GPR block */
#define TULIP_ROM_MB_21140_MII 1   /* 21140[A] MII block */
#define TULIP_ROM_MB_21142_SIA 2   /* 2114[23] SIA block */
#define TULIP_ROM_MB_21142_MII 3   /* 2114[23] MII block */
#define TULIP_ROM_MB_21143_SYM 4   /* 21143 SYM block */
#define TULIP_ROM_MB_21143_RESET 5 /* 21143 reset block */

#define TULIP_ROM_GETW(data, off)                                              \
  ((uint32_t)(data)[(off)] | (uint32_t)((data)[(off) + 1]) << 8)

/*
 * Tulip control registers.
 */
#define TULIP_CSR0 0x00
#define TULIP_CSR1 0x08
#define TULIP_CSR2 0x10
#define TULIP_CSR3 0x18
#define TULIP_CSR4 0x20
#define TULIP_CSR5 0x28
#define TULIP_CSR6 0x30
#define TULIP_CSR7 0x38
#define TULIP_CSR8 0x40
#define TULIP_CSR9 0x48
#define TULIP_CSR10 0x50
#define TULIP_CSR11 0x58
#define TULIP_CSR12 0x60
#define TULIP_CSR13 0x68
#define TULIP_CSR14 0x70
#define TULIP_CSR15 0x78
#define TULIP_CSR16 0x80
#define TULIP_CSR17 0x88
#define TULIP_CSR18 0x90
#define TULIP_CSR19 0x98
#define TULIP_CSR20 0xa0
#define TULIP_CSR21 0xa8
#define TULIP_CSR22 0xb0
#define TULIP_CSR23 0xb8
#define TULIP_CSR24 0xc0
#define TULIP_CSR25 0xc8
#define TULIP_CSR26 0xd0
#define TULIP_CSR27 0xd8
#define TULIP_CSR28 0xe0
#define TULIP_CSR29 0xe8
#define TULIP_CSR30 0xf0
#define TULIP_CSR31 0xf8

#define TULIP_CSR_INDEX(csr) ((csr) >> 3)

/* CSR0 - Bus Mode */
#define CSR_BUSMODE TULIP_CSR0
#define BUSMODE_SWR 0x00000001 /* software reset */
#define BUSMODE_BAR 0x00000002 /* bus arbitration */
#define BUSMODE_DSL 0x0000007c /* descriptor skip length */
#define BUSMODE_BLE 0x00000080 /* big endian */

/* programmable burst length */
#define BUSMODE_PBL_DEFAULT 0x00000000 /*     default value */
#define BUSMODE_PBL_1LW 0x00000100     /*     1 longword */
#define BUSMODE_PBL_2LW 0x00000200     /*     2 longwords */
#define BUSMODE_PBL_4LW 0x00000400     /*     4 longwords */
#define BUSMODE_PBL_8LW 0x00000800     /*     8 longwords */
#define BUSMODE_PBL_16LW 0x00001000    /*    16 longwords */
#define BUSMODE_PBL_32LW 0x00002000    /*    32 longwords */

/* cache alignment */
#define BUSMODE_CAL_NONE 0x00000000 /*     no alignment */
#define BUSMODE_CAL_8LW 0x00004000  /*     8 longwords */
#define BUSMODE_CAL_16LW 0x00008000 /*    16 longwords */
#define BUSMODE_CAL_32LW 0x0000c000 /*    32 longwords */
#define BUSMODE_DAS 0x00010000      /* diagnostic address space */

/*   must be zero on most */

/* transmit auto-poll */
#define BUSMODE_TAP_NONE 0x00000000    /*     no auto-polling */
#define BUSMODE_TAP_200us 0x00020000   /*   200 uS */
#define BUSMODE_TAP_800us 0x00040000   /*   400 uS */
#define BUSMODE_TAP_1_6ms 0x00060000   /*   1.6 mS */
#define BUSMODE_TAP_12_8us 0x00080000  /*  12.8 uS (21041+) */
#define BUSMODE_TAP_25_6us 0x000a0000  /*  25.6 uS (21041+) */
#define BUSMODE_TAP_51_2us 0x000c0000  /*  51.2 uS (21041+) */
#define BUSMODE_TAP_102_4us 0x000e0000 /* 102.4 uS (21041+) */
#define BUSMODE_DBO 0x00100000         /* desc-only b/e (21041+) */
#define BUSMODE_RME 0x00200000         /* rd/mult enab (21140+) */
#define BUSMODE_RLE 0x00800000         /* rd/line enab (21140+) */
#define BUSMODE_WLE 0x01000000         /* wt/line enab (21140+) */

/* CSR1 - Transmit Poll Demand */
#define CSR_TXPOLL TULIP_CSR1
#define TXPOLL_TPD 0x00000001 /* transmit poll demand */

/* CSR2 - Receive Poll Demand */
#define CSR_RXPOLL TULIP_CSR2
#define RXPOLL_RPD 0x00000001 /* receive poll demand */

/* CSR3 - Receive List Base Address */
#define CSR_RXLIST TULIP_CSR3

/* CSR4 - Transmit List Base Address */
#define CSR_TXLIST TULIP_CSR4

/* CSR5 - Status */
#define CSR_STATUS TULIP_CSR5
#define STATUS_TI 0x00000001     /* transmit interrupt */
#define STATUS_TPS 0x00000002    /* transmit process stopped */
#define STATUS_TU 0x00000004     /* transmit buffer unavail */
#define STATUS_TJT 0x00000008    /* transmit jabber timeout */
#define STATUS_LNPANC 0x00000010 /* link pass (21041) */
#define STATUS_UNF 0x00000020    /* transmit underflow */
#define STATUS_RI 0x00000040     /* receive interrupt */
#define STATUS_RU 0x00000080     /* receive buffer unavail */
#define STATUS_RPS 0x00000100    /* receive process stopped */
#define STATUS_RWT 0x00000200    /* receive watchdog timeout */
#define STATUS_AT                                                              \
  0x00000400 /* SIA AUI/TP pin changed                                         \
                      (21040) */
#define STATUS_ETI                                                             \
  0x00000400 /* early transmit interrupt                                       \
                      (21142) */
#define STATUS_FD                                                              \
  0x00000800                         /* full duplex short frame                \
                                              received (21040) */
#define STATUS_TM 0x00000800         /* timer expired (21041) */
#define STATUS_LNF 0x00001000        /* link fail (21040) */
#define STATUS_SE 0x00002000         /* system error */
#define STATUS_ER 0x00004000         /* early receive (21041) */
#define STATUS_AIS 0x00008000        /* abnormal interrupt summary */
#define STATUS_NIS 0x00010000        /* normal interrupt summary */
#define STATUS_RS 0x000e0000         /* receive process state */
#define STATUS_RS_STOPPED 0x00000000 /* Stopped */
#define STATUS_RS_FETCH                                                        \
  0x00020000 /* Running - fetch receive                                        \
                      descriptor */
#define STATUS_RS_CHECK                                                        \
  0x00040000                           /* Running - check for end              \
                                                of receive */
#define STATUS_RS_WAIT 0x00060000      /* Running - wait for packet */
#define STATUS_RS_SUSPENDED 0x00080000 /* Suspended */
#define STATUS_RS_CLOSE                                                        \
  0x000a0000 /* Running - close receive                                        \
                      descriptor */
#define STATUS_RS_FLUSH                                                        \
  0x000c0000 /* Running - flush current                                        \
                      frame from FIFO */
#define STATUS_RS_QUEUE                                                        \
  0x000e0000                         /* Running - queue current                \
                                              frame from FIFO into             \
                                              buffer */
#define STATUS_TS 0x00700000         /* transmit process state */
#define STATUS_TS_STOPPED 0x00000000 /* Stopped */
#define STATUS_TS_FETCH                                                        \
  0x00100000 /* Running - fetch transmit                                       \
                      descriptor */
#define STATUS_TS_WAIT                                                         \
  0x00200000 /* Running - wait for end                                         \
                      of transmission */
#define STATUS_TS_READING                                                      \
  0x00300000                           /* Running - read buffer from           \
                                                memory and queue into          \
                                                FIFO */
#define STATUS_TS_RESERVED 0x00400000  /* RESERVED */
#define STATUS_TS_SETUP 0x00500000     /* Running - Setup packet */
#define STATUS_TS_SUSPENDED 0x00600000 /* Suspended */
#define STATUS_TS_CLOSE 0x00700000     /* Running - close transmit descriptor */
#define STATUS_EB 0x03800000           /* error bits */
#define STATUS_EB_PARITY 0x00000000    /* parity errror */
#define STATUS_EB_MABT 0x00800000      /* master abort */
#define STATUS_EB_TABT 0x01000000      /* target abort */
#define STATUS_GPPI 0x04000000         /* GPIO interrupt (21142) */
#define STATUS_LC 0x08000000           /* 100baseTX link change (21142) */
#define STATUS_X3201_PMEIS                                                     \
  0x10000000 /* power management event interrupt summary */
#define STATUS_X3201_SFIS                                                      \
  0x80000000 /* second function (Modem) interrupt status */

/* CSR6 - Operation Mode */
#define CSR_OPMODE TULIP_CSR6
#define OPMODE_HP 0x00000001         /* hash/perfect mode (ro) */
#define OPMODE_SR 0x00000002         /* start receive */
#define OPMODE_HO 0x00000004         /* hash only mode (ro) */
#define OPMODE_PB 0x00000008         /* pass bad frames */
#define OPMODE_IF 0x00000010         /* inverse filter mode (ro) */
#define OPMODE_SB 0x00000020         /* start backoff counter */
#define OPMODE_PR 0x00000040         /* promiscuous mode */
#define OPMODE_PM 0x00000080         /* pass all multicast */
#define OPMODE_FKD 0x00000100        /* flaky oscillator disable */
#define OPMODE_FD 0x00000200         /* full-duplex mode */
#define OPMODE_OM 0x00000c00         /* operating mode */
#define OPMODE_OM_NORMAL 0x00000000  /*     normal mode */
#define OPMODE_OM_INTLOOP 0x00000400 /*     internal loopback */
#define OPMODE_OM_EXTLOOP 0x00000800 /*     external loopback */
#define OPMODE_FC 0x00001000         /* force collision */
#define OPMODE_ST 0x00002000         /* start transmitter */
#define OPMODE_TR 0x0000c000         /* threshold control */
#define OPMODE_TR_72 0x00000000      /*     72 bytes */
#define OPMODE_TR_96 0x00004000      /*     96 bytes */
#define OPMODE_TR_128 0x00008000     /*    128 bytes */
#define OPMODE_TR_160 0x0000c000     /*    160 bytes */
#define OPMODE_BP 0x00010000         /* backpressure enable */
#define OPMODE_CA 0x00020000         /* capture effect enable */
#define OPMODE_PS 0x00040000 /* port select: 1 = MII/SYM, 0 = SRL (21140) */
#define OPMODE_HBD                                                             \
  0x00080000 /* heartbeat disable: set in MII/SYM 100mbps, set according to    \
                PHY in MII 10mbps mode (21140) */
#define OPMODE_SF 0x00200000 /* store and forward mode (21140) */
#define OPMODE_TTM                                                             \
  0x00400000 /* Transmit Threshold Mode: 1 = 10mbps, 0 = 100mbps (21140) */
#define OPMODE_PCS 0x00800000    /* PCS function (21140) */
#define OPMODE_SCR 0x01000000    /* scrambler mode (21140) */
#define OPMODE_MBO 0x02000000    /* must be one (21140) */
#define OPMODE_IDAMSB 0x04000000 /* ignore dest addr MSB (21142) */
#define OPMODE_RA 0x40000000     /* receive all (21140) */
#define OPMODE_SC 0x80000000     /* special capture effect enable (21041+) */

/* Shorthand for media-related OPMODE bits */
#define OPMODE_MEDIA_BITS                                                      \
  (OPMODE_FD | OPMODE_PS | OPMODE_TTM | OPMODE_PCS | OPMODE_SCR)

/* CSR7 - Interrupt Enable */
#define CSR_INTEN TULIP_CSR7

/* See bits for CSR5 -- Status */

/* CSR8 - Missed Frames */
#define CSR_MISSED TULIP_CSR8
#define MISSED_MFC 0x0000ffff /* missed packet count */
#define MISSED_MFO                                                             \
  0x00010000 /* missed packet count                                            \
                      overflowed */
#define MISSED_FOC                                                             \
  0x0ffe0000 /* fifo overflow counter                                          \
                      (21140) */
#define MISSED_OCO                                                             \
  0x10000000 /* overflow counter overflowed                                    \
                      (21140) */

#define MISSED_GETMFC(x) ((x)&MISSED_MFC)
#define MISSED_GETFOC(x) (((x)&MISSED_FOC) >> 17)

/* CSR9 - MII, SROM, Boot ROM, Ethernet Address ROM register. */
#define CSR_MIIROM TULIP_CSR9
#define MIIROM_DATA 0x000000ff   /* byte of data to/from Boot ROM (21041+) */
#define MIIROM_SROMCS 0x00000001 /* SROM chip select */
#define MIIROM_SROMSK 0x00000002 /* SROM clock */
#define MIIROM_SROMDI 0x00000004 /* SROM data in (to) */
#define MIIROM_SROMDO 0x00000008 /* SROM data out (from) */
#define MIIROM_REG 0x00000400    /* external register select */
#define MIIROM_SR 0x00000800     /* SROM select */
#define MIIROM_BR 0x00001000     /* boot ROM select */
#define MIIROM_WR 0x00002000     /* write to boot ROM */
#define MIIROM_RD 0x00004000     /* read from boot ROM */
#define MIIROM_MOD 0x00008000    /* mode select (ro) (21041) */
#define MIIROM_MDC 0x00010000    /* MII clock */
#define MIIROM_MDO 0x00020000    /* MII data out */
#define MIIROM_MIIDIR                                                          \
  0x00040000                  /* MII direction mode                            \
                                       1 = PHY in read,                        \
                                       0 = PHY in write */
#define MIIROM_MDI 0x00080000 /* MII data in */
#define MIIROM_DN 0x80000000  /* data not valid (21040) */

/* SROM opcodes */
#define TULIP_SROM_OPC_ERASE 0x04
#define TULIP_SROM_OPC_WRITE 0x05
#define TULIP_SROM_OPC_READ 0x06

/* CSR10 - Boot ROM address register (21041+). */
#define CSR_ROMADDR TULIP_CSR10
#define ROMADDR_MASK 0x000003ff /* boot rom address */

/* CSR11 - General Purpose Timer (21041+). */
#define CSR_GPT TULIP_CSR11
#define GPT_VALUE 0x0000ffff /* timer value */
#define GPT_CON 0x00010000   /* continuous mode */

/* 21143-PD and 21143-TD Interrupt Mitigation bits */
#define GPT_NRX 0x000e0000   /* number of Rx packets */
#define GPT_RXT 0x00f00000   /* Rx timer */
#define GPT_NTX 0x07000000   /* number of Tx packets */
#define GPT_TXT 0x78000000   /* Tx timer */
#define GPT_CYCLE 0x80000000 /* cycle size */

/* CSR12 - SIA Status Register. */
#define CSR_SIASTAT TULIP_CSR12
#define SIASTAT_PAUI 0x00000001  /* pin AUI/TP indication (21040) */
#define SIASTAT_MRA 0x00000001   /* MII receive activity (21142) */
#define SIASTAT_NCR 0x00000002   /* network connection error */
#define SIASTAT_LS100 0x00000002 /* 100baseT link status 0 == pass (21142) */
#define SIASTAT_LKF 0x00000004   /* link fail status */
#define SIASTAT_LS10 0x00000004  /* 10baseT link status 0 == pass (21142) */
#define SIASTAT_APS 0x00000008   /* auto polarity status */
#define SIASTAT_DSD 0x00000010   /* PLL self test done */
#define SIASTAT_DSP 0x00000020   /* PLL self test pass */
#define SIASTAT_DAZ 0x00000040   /* PLL all zero */
#define SIASTAT_DAO 0x00000080   /* PLL all one */
#define SIASTAT_SRA 0x00000100   /* selected port receive activity (21041) */
#define SIASTAT_ARA 0x00000100   /* AUI receive activity (21142) */
#define SIASTAT_NRA                                                            \
  0x00000200                   /* non-selected port receive activity (21041)   \
                                */
#define SIASTAT_TRA 0x00000200 /* 10base-T receive activity (21142) */
#define SIASTAT_NSN 0x00000400 /* non-stable NLPs detected (21041) */
#define SIASTAT_TRF 0x00000800 /* transmit remote fault (21041) */
#define SIASTAT_ANS 0x00007000 /* autonegotiation state (21041) */
#define SIASTAT_ANS_DIS 0x00000000       /*     disabled */
#define SIASTAT_ANS_TXDIS 0x00001000     /*     transmit disabled */
#define SIASTAT_ANS_START 0x00001000     /*     (MX98715AEC) */
#define SIASTAT_ANS_ABD 0x00002000       /*     ability detect */
#define SIASTAT_ANS_ACKD 0x00003000      /*     acknowledge detect */
#define SIASTAT_ANS_ACKC 0x00004000      /*     complete acknowledge */
#define SIASTAT_ANS_FLPGOOD 0x00005000   /*     FLP link good */
#define SIASTAT_ANS_LINKCHECK 0x00006000 /*     link check */
#define SIASTAT_LPN 0x00008000           /* link partner negotiable (21041) */
#define SIASTAT_LPC 0xffff0000           /* link partner code word */
#define SIASTAT_GETLPC(x) (((x)&SIASTAT_LPC) >> 16)

/* CSR13 - SIA Connectivity Register. */
#define CSR_SIACONN TULIP_CSR13
#define SIACONN_SRL 0x00000001 /* SIA reset (0 == reset) */
#define SIACONN_PS 0x00000002  /* pin AUI/TP selection (21040) */
#define SIACONN_CAC 0x00000004 /* CSR autoconfiguration */
#define SIACONN_AUI 0x00000008 /* select AUI (0 = TP) */
#define SIACONN_EDP 0x00000010 /* SIA PLL external input enable (21040) */
#define SIACONN_ENI 0x00000020 /* encoder input multiplexer (21040) */
#define SIACONN_SIM                                                            \
  0x00000040                   /* serial interface input multiplexer (21040)   \
                                */
#define SIACONN_ASE 0x00000080 /* APLL start enable (21040) */
#define SIACONN_SEL                                                            \
  0x00000f00 /* external port output multiplexer select (21040) */
#define SIACONN_IE 0x00001000      /* input enable (21040) */
#define SIACONN_OE1_3 0x00002000   /* output enable 1, 3 (21040) */
#define SIACONN_OE2_4 0x00004000   /* output enable 2, 4 (21040) */
#define SIACONN_OE5_6_7 0x00008000 /* output enable 5, 6, 7 (21040) */
#define SIACONN_SDM                                                            \
  0x0000ef00 /* SIA diagnostic mode; always set to this value for normal       \
                operation (21041) */

/* CSR14 - SIA Transmit Receive Register. */
#define CSR_SIATXRX TULIP_CSR14
#define SIATXRX_ECEN 0x00000001         /* encoder enable */
#define SIATXRX_LBK 0x00000002          /* loopback enable */
#define SIATXRX_DREN 0x00000004         /* driver enable */
#define SIATXRX_LSE 0x00000008          /* link pulse send enable */
#define SIATXRX_CPEN 0x00000030         /* compensation enable */
#define SIATXRX_CPEN_DIS0 0x00000000    /*     disabled */
#define SIATXRX_CPEN_DIS1 0x00000010    /*     disabled */
#define SIATXRX_CPEN_HIGHPWR 0x00000020 /*     high power */
#define SIATXRX_CPEN_NORMAL 0x00000030  /*     normal */
#define SIATXRX_MBO 0x00000040          /* must be one (21041 pass 2) */
#define SIATXRX_TH 0x00000040           /* 10baseT HDX enable (21142) */
#define SIATXRX_ANE 0x00000080 /* autonegotiation enable (21041/21142) */
#define SIATXRX_RSQ 0x00000100 /* receive squelch enable */
#define SIATXRX_CSQ 0x00000200 /* collision squelch enable */
#define SIATXRX_CLD 0x00000400 /* collision detect enable */
#define SIATXRX_SQE 0x00000800 /* signal quality generation enable */
#define SIATXRX_LTE 0x00001000 /* link test enable */
#define SIATXRX_APE 0x00002000 /* auto-polarity enable */
#define SIATXRX_SPP 0x00004000 /* set polarity plus */
#define SIATXRX_TAS                                                            \
  0x00008000 /* 10base-T/AUI autosensing enable (21041/21142) */
#define SIATXRX_THX 0x00010000 /* 100baseTX-HDX (21142) */
#define SIATXRX_TXF 0x00020000 /* 100baseTX-FDX (21142) */
#define SIATXRX_T4 0x00040000  /* 100baseT4 (21142) */

/* CSR15 - SIA General Register. */
#define CSR_SIAGEN TULIP_CSR15
#define SIAGEN_JBD 0x00000001  /* jabber disable */
#define SIAGEN_HUJ 0x00000002  /* host unjab */
#define SIAGEN_JCK 0x00000004  /* jabber clock */
#define SIAGEN_ABM 0x00000008  /* BNC select (21041) */
#define SIAGEN_RWD 0x00000010  /* receive watchdog disable */
#define SIAGEN_RWR 0x00000020  /* receive watchdog release */
#define SIAGEN_LE1 0x00000040  /* LED 1 enable (21041) */
#define SIAGEN_LV1 0x00000080  /* LED 1 value (21041) */
#define SIAGEN_TSCK 0x00000100 /* test clock */
#define SIAGEN_FUSQ 0x00000200 /* force unsquelch */
#define SIAGEN_FLF 0x00000400  /* force link fail */
#define SIAGEN_LSD 0x00000800  /* LED stretch disable (21041) */
#define SIAGEN_LEE 0x00000800  /* Link extend enable (21142) */
#define SIAGEN_DPST 0x00001000 /* PLL self-test start */
#define SIAGEN_FRL 0x00002000  /* force receiver low */
#define SIAGEN_LE2 0x00004000  /* LED 2 enable (21041) */
#define SIAGEN_RMP 0x00004000  /* received magic packet (21143) */
#define SIAGEN_LV2 0x00008000  /* LED 2 value (21041) */
#define SIAGEN_HCKR 0x00008000 /* hacker (21143) */
#define SIAGEN_MD 0x000f0000   /* general purpose mode/data */
#define SIAGEN_LGS0 0x00100000 /* LED/GEP 0 select */
#define SIAGEN_LGS1 0x00200000 /* LED/GEP 1 select */
#define SIAGEN_LGS2 0x00400000 /* LED/GEP 2 select */
#define SIAGEN_LGS3 0x00800000 /* LED/GEP 3 select */
#define SIAGEN_GEI0 0x01000000 /* GEP pin 0 intr enable */
#define SIAGEN_GEI1 0x02000000 /* GEP pin 1 intr enable */
#define SIAGEN_RME 0x04000000  /* receive match enable */
#define SIAGEN_CWE 0x08000000  /* control write enable */
#define SIAGEN_GI0 0x10000000  /* GEP pin 0 interrupt */
#define SIAGEN_GI1 0x20000000  /* GEP pin 1 interrupt */
#define SIAGEN_RMI 0x40000000  /* receive match interrupt */

/* CSR12 - General Purpose Port (21140+). */
#define CSR_GPP TULIP_CSR12
#define GPP_MD 0x000000ff  /* general purpose mode/data */
#define GPP_GPC 0x00000100 /* general purpose control */

/*
 * Digital Semiconductor 21142/21143 registers.
 */

/* SIA configuration for 10baseT (from the 21143 manual) */
#define SIACONN_21142_10BASET 0x00000001
#define SIATXRX_21142_10BASET 0x00007f3f
#define SIAGEN_21142_10BASET 0x00000008

/* SIA configuration for 10baseT full-duplex (from the 21143 manual) */
#define SIACONN_21142_10BASET_FDX 0x00000001
#define SIATXRX_21142_10BASET_FDX 0x00007f3d
#define SIAGEN_21142_10BASET_FDX 0x00000008

/* SIA configuration for 10base5 (from the 21143 manual) */
#define SIACONN_21142_AUI 0x00000009
#define SIATXRX_21142_AUI 0x00004705
#define SIAGEN_21142_AUI 0x0000000e

/* SIA configuration for 10base2 (from the 21143 manual) */
#define SIACONN_21142_BNC 0x00000009
#define SIATXRX_21142_BNC 0x00004705
#define SIAGEN_21142_BNC 0x00000006
#endif /* _DEV_IC_TULIPREG_H_ */

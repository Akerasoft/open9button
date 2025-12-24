/*  Extenmote : NES, SNES, N64 and Gamecube to Wii remote adapter firmware
 *  Copyright (C) 2012-2014  Raphael Assenat <raph@raphnet.net>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <string.h>

#include "classic.h"
#include "eeprom.h"
#include "analog.h"

#if WITH_8_BUTTONS
#include "tripleclick.h"
#elif WITH_12_BUTTONS
#include "tripleclick.h"
#endif

#define C_DEFLECTION	100

/*       |                 Bit                                |
 * Byte  |   7   |   6   |   5   |  4  |  3 |  2 |  1  |  0   |
 * ------+---------------+-------+----------------------------+
 *  0    |    RX<4:3>    |            LX<5:0>                 |
 *  1    |    RX<2:1>    |           LY<5:0>                  |
 *  2    | RX<0> |    LT<4:3>    |      RY<4:0>               |
 *  3    |        LT<2:0>        |      RT<4:0>               |
 *  4    | BDR   |  BDD  |  BLT  |  B- | BH | B+  | BRT | 1   |
 *  5    | BZL   |   BB  |  BY   | BA  | BX | BZR | BDL | BDU |
 *
 *  6    | 0x52 ('R')                                         |
 *  7    | Controller ID byte 0                               |
 *  8    | Controller ID byte 1                               |
 *
 *  9    | Controller specific data 0                         |
 *  10   | Controller specific data 1                         |
 *  11   | Controller specific data 2                         |
 *  12   | Controller specific data 3                         |
 *  13   | Controller specific data 4                         |
 *  14   | Controller specific data 5                         |
 *  15   | Controller specific data 6                         |
 *  16   | Controller specific data 7                         |
 *
 *  Controller ID:
 *
 *   0   |  1
 *  -----+-----
 *   '6' | '4'    N64 controller
 *   'G' | 'C'    Gamecube controller
 *   'S' | 'F'    SNES
 *   'F' | 'C'    NES
 *
 * Writing at byte 6 might one day control the rumble motor. Rumbles on when non-zero.
 */
void pack_classic_data_mode1(classic_pad_data *src, unsigned char dst[PACKED_CLASSIC_DATA_SIZE], int analog_style)
{
	unsigned char rx,ry,lx=0x20,ly=0x20; // down sized
	unsigned char shoulder_left=0, shoulder_right=0; // lower 5 bits only

	memset(dst, 0x00, PACKED_CLASSIC_DATA_SIZE);

	lx = ((0x80 + src->lx) >> 2) & 0x3F;
	ly = ((0x80 + src->ly) >> 2) & 0x3F;

	rx = ((0x80 + src->rx) >> 3) & 0x1F;
	ry = ((0x80 + src->ry) >> 3) & 0x1F;

	shoulder_left = (src->lt >> 3);
	shoulder_right = (src->rt >> 3);

	dst[0] = ((rx<<3) & 0xC0) | lx;
	dst[1] = ((rx<<5) & 0xC0) | ly;
	dst[2] = rx<<7 | ry | (shoulder_left & 0x18) << 2;

	dst[3] = (shoulder_left << 5) | (shoulder_right & 0x1F);
	dst[4] = (src->buttons >> 8) ^ 0xFF;
	dst[5] = (src->buttons) ^ 0xFF;

	dst[6] = 'R';
	memcpy(dst+7, src->controller_id, 2);
	memcpy(dst+9, src->controller_raw_data, 8);
}

void pack_classic_data_mode2(classic_pad_data *src, unsigned char dst[PACKED_CLASSIC_DATA_SIZE], int analog_style)
{
	unsigned char rx=0x80,ry=0x80,lx=0x80,ly=0x80;
	unsigned char shoulder_left=0, shoulder_right=0;

	memset(dst, 0x00, PACKED_CLASSIC_DATA_SIZE);


	lx = 0x80 + src->lx;
	ly = 0x80 + src->ly;
	rx = 0x80 + src->rx;
	ry = 0x80 + src->ry;
	shoulder_left = src->lt;
	shoulder_right = src->rt;

	dst[0] = lx;
	dst[1] = rx;
	dst[2] = ly;
	dst[3] = ry;
	dst[4] = 0; // I think this can hold extra bits for the axes
	dst[5] = shoulder_left;
	dst[6] = shoulder_right;
	dst[7] = (src->buttons >> 8) ^ 0xFF;
	dst[8] = (src->buttons) ^ 0xFF;
}

void pack_classic_data_mode3(classic_pad_data *src, unsigned char dst[PACKED_CLASSIC_DATA_SIZE], int analog_style)
{
	unsigned char rx=0x80,ry=0x80,lx=0x80,ly=0x80;
	unsigned char shoulder_left=0, shoulder_right=0;

	memset(dst, 0x00, PACKED_CLASSIC_DATA_SIZE);

	lx = 0x80 + src->lx;
	ly = 0x80 + src->ly;
	rx = 0x80 + src->rx;
	ry = 0x80 + src->ry;

	shoulder_left = src->lt;
	shoulder_right = src->rt;

	dst[0] = lx;
	dst[1] = rx;
	dst[2] = ly;
	dst[3] = ry;
	dst[4] = shoulder_left;
	dst[5] = shoulder_right;
	dst[6] = (src->buttons >> 8) ^ 0xFF;
	dst[7] = (src->buttons) ^ 0xFF;
}


void pack_classic_data(classic_pad_data *src, unsigned char dst[PACKED_CLASSIC_DATA_SIZE], int analog_style, int mode)
{
	switch (mode)
	{
		default:
		case CLASSIC_MODE_1:
			pack_classic_data_mode1(src, dst, analog_style);
			break;
		case CLASSIC_MODE_2:
			pack_classic_data_mode2(src, dst, analog_style);
			break;
		case CLASSIC_MODE_3:
			pack_classic_data_mode3(src, dst, analog_style);
			break;
	}
}


void dataToClassic(const gamepad_data *src, classic_pad_data *dst, char first_read)
{
	static char test_x = 0, test_y = 0;
	//unsigned short buttons_zl_zr = CPAD_BTN_ZR;
	static char waiting_release = 0;

	// Prior to version 2.1.1, whenever the button mapping called for Z being pressed
	// on the classic controller, ZL and ZR were used (i.e. They were pushed down together
	// on the virtual classic controller).
	//
	// But in Super Smash Bros Ultimate on the Switch through the 8BitDo GBros.,
	// it has been observed that one cannot roll when both Z triggers are down. So starting
	// with version 2.1.1, only ZR is used.
	//if (g_current_config.merge_zl_zr) {
	//	buttons_zl_zr |= CPAD_BTN_ZL;
	//}

	memset(dst, 0, sizeof(classic_pad_data));
	
#if WITH_15_BUTTONS
            // try CL for classic, if it does not work try GC instead.
			dst->controller_id[0] = 'C';
			dst->controller_id[1] = 'L';
			memcpy(dst->controller_raw_data, src->classic.controller_raw_data, 8);

			if (src->classic.buttons & SNES_BTN_B) { dst->buttons |= CPAD_BTN_B; }
			if (src->classic.buttons & SNES_BTN_Y) { dst->buttons |= CPAD_BTN_Y; }
			if (src->classic.buttons & SNES_BTN_A) { dst->buttons |= CPAD_BTN_A; }
			if (src->classic.buttons & SNES_BTN_X) { dst->buttons |= CPAD_BTN_X; }

			// analog sticks
			dst->ly = src->classic.ly;
			dst->lx = src->classic.lx;
			dst->ry = src->classic.ry;
			dst->rx = src->classic.rx;

			// dpad
			if (src->classic.buttons & SNES_BTN_DPAD_UP) { dst->buttons |= CPAD_BTN_DPAD_UP; }
			if (src->classic.buttons & SNES_BTN_DPAD_DOWN) { dst->buttons |= CPAD_BTN_DPAD_DOWN; }
			if (src->classic.buttons & SNES_BTN_DPAD_LEFT) { dst->buttons |= CPAD_BTN_DPAD_LEFT; }
			if (src->classic.buttons & SNES_BTN_DPAD_RIGHT) { dst->buttons |= CPAD_BTN_DPAD_RIGHT; }

			if (src->classic.buttons & SNES_BTN_SELECT) { dst->buttons |= CPAD_BTN_MINUS; }
			if (src->classic.buttons & SNES_BTN_START) { dst->buttons |= CPAD_BTN_PLUS; }
			if (src->classic.buttons & SNES_BTN_L) { dst->buttons |= CPAD_BTN_TRIG_LEFT; }
			if (src->classic.buttons & SNES_BTN_R) { dst->buttons |= CPAD_BTN_TRIG_RIGHT; }
			
			// fake mapping
			if (src->classic.buttons & SNES_BTN_ZL) { dst->buttons |= CPAD_BTN_ZL; }
			if (src->classic.buttons & SNES_BTN_ZR) { dst->buttons |= CPAD_BTN_ZR; }

			if (src->snes.buttons & SNES_BTN_HOME) { dst->buttons |= CPAD_BTN_HOME; }

#if WITH_ANALOG_TRIGGERS
			dst->lt = src->classic.lt;
			dst->rt = src->classic.rt;
#else
			// Simulate L/R fully pressed values (like the analogue-less classic controller pro does)
			if (dst->buttons & CPAD_BTN_TRIG_LEFT) {
				dst->lt = 0xff;
			}
			if (dst->buttons & CPAD_BTN_TRIG_RIGHT) {
				dst->rt = 0xff;
			}
#endif
#elif WITH_13_BUTTONS
			dst->controller_id[0] = 'S';
			dst->controller_id[1] = 'F';
			memcpy(dst->controller_raw_data, src->snes.raw_data, SNES_RAW_SIZE);

			if (g_current_config.g_snes_nes_mode) {
				if (src->snes.buttons & SNES_BTN_Y) { dst->buttons |= CPAD_BTN_B; }
				if (src->snes.buttons & SNES_BTN_B) { dst->buttons |= CPAD_BTN_A; }
			} else {
				if (src->snes.buttons & SNES_BTN_B) { dst->buttons |= CPAD_BTN_B; }
				if (src->snes.buttons & SNES_BTN_Y) { dst->buttons |= CPAD_BTN_Y; }
				if (src->snes.buttons & SNES_BTN_A) { dst->buttons |= CPAD_BTN_A; }
				if (src->snes.buttons & SNES_BTN_X) { dst->buttons |= CPAD_BTN_X; }
			}

			if (g_current_config.g_snes_analog_dpad) {
				if (src->snes.buttons & SNES_BTN_DPAD_UP) { dst->ly = 100; }
				if (src->snes.buttons & SNES_BTN_DPAD_DOWN) { dst->ly = -100; }
				if (src->snes.buttons & SNES_BTN_DPAD_LEFT) { dst->lx = -100; }
				if (src->snes.buttons & SNES_BTN_DPAD_RIGHT) { dst->lx = 100; }
			} else {
				if (src->snes.buttons & SNES_BTN_DPAD_UP) { dst->buttons |= CPAD_BTN_DPAD_UP; }
				if (src->snes.buttons & SNES_BTN_DPAD_DOWN) { dst->buttons |= CPAD_BTN_DPAD_DOWN; }
				if (src->snes.buttons & SNES_BTN_DPAD_LEFT) { dst->buttons |= CPAD_BTN_DPAD_LEFT; }
				if (src->snes.buttons & SNES_BTN_DPAD_RIGHT) { dst->buttons |= CPAD_BTN_DPAD_RIGHT; }
			}

			if (src->snes.buttons & SNES_BTN_SELECT) { dst->buttons |= CPAD_BTN_MINUS; }
			if (src->snes.buttons & SNES_BTN_START) { dst->buttons |= CPAD_BTN_PLUS; }
			if (src->snes.buttons & SNES_BTN_L) { dst->buttons |= CPAD_BTN_TRIG_LEFT; }
			if (src->snes.buttons & SNES_BTN_R) { dst->buttons |= CPAD_BTN_TRIG_RIGHT; }

			if (IS_SIMULTANEOUS(src->snes.buttons, SNES_BTN_START|SNES_BTN_SELECT|SNES_BTN_L|SNES_BTN_R|SNES_BTN_DPAD_UP)) {
				g_current_config.g_snes_nes_mode = 0;
				g_current_config.g_snes_analog_dpad = 0;
				sync_config();
			}
			if (IS_SIMULTANEOUS(src->snes.buttons, SNES_BTN_START|SNES_BTN_SELECT|SNES_BTN_L|SNES_BTN_R|SNES_BTN_DPAD_DOWN)) {
				g_current_config.g_snes_nes_mode = 1;
				g_current_config.g_snes_analog_dpad = 0;
				sync_config();
			}
			if (IS_SIMULTANEOUS(src->snes.buttons, SNES_BTN_START|SNES_BTN_SELECT|SNES_BTN_L|SNES_BTN_R|SNES_BTN_DPAD_LEFT)) {
				g_current_config.g_snes_analog_dpad = 1;
				g_current_config.g_snes_nes_mode = 0;
				sync_config();
			}

			if (src->snes.buttons & SNES_BTN_HOME) { dst->buttons |= CPAD_BTN_HOME; }

			// Simulate L/R fully pressed values (like the analogue-less classic controller pro does)
			if (dst->buttons & CPAD_BTN_TRIG_LEFT) {
				dst->lt = 0xff;
			}
			if (dst->buttons & CPAD_BTN_TRIG_RIGHT) {
				dst->rt = 0xff;
			}
#elif WITH_12_BUTTONS
			dst->controller_id[0] = 'S';
			dst->controller_id[1] = 'F';
			memcpy(dst->controller_raw_data, src->snes.raw_data, SNES_RAW_SIZE);

			if (g_current_config.g_snes_nes_mode) {
				if (src->snes.buttons & SNES_BTN_Y) { dst->buttons |= CPAD_BTN_B; }
				if (src->snes.buttons & SNES_BTN_B) { dst->buttons |= CPAD_BTN_A; }
			} else {
				if (src->snes.buttons & SNES_BTN_B) { dst->buttons |= CPAD_BTN_B; }
				if (src->snes.buttons & SNES_BTN_Y) { dst->buttons |= CPAD_BTN_Y; }
				if (src->snes.buttons & SNES_BTN_A) { dst->buttons |= CPAD_BTN_A; }
				if (src->snes.buttons & SNES_BTN_X) { dst->buttons |= CPAD_BTN_X; }
			}

			if (g_current_config.g_snes_analog_dpad) {
				if (src->snes.buttons & SNES_BTN_DPAD_UP) { dst->ly = 100; }
				if (src->snes.buttons & SNES_BTN_DPAD_DOWN) { dst->ly = -100; }
				if (src->snes.buttons & SNES_BTN_DPAD_LEFT) { dst->lx = -100; }
				if (src->snes.buttons & SNES_BTN_DPAD_RIGHT) { dst->lx = 100; }
			} else {
				if (src->snes.buttons & SNES_BTN_DPAD_UP) { dst->buttons |= CPAD_BTN_DPAD_UP; }
				if (src->snes.buttons & SNES_BTN_DPAD_DOWN) { dst->buttons |= CPAD_BTN_DPAD_DOWN; }
				if (src->snes.buttons & SNES_BTN_DPAD_LEFT) { dst->buttons |= CPAD_BTN_DPAD_LEFT; }
				if (src->snes.buttons & SNES_BTN_DPAD_RIGHT) { dst->buttons |= CPAD_BTN_DPAD_RIGHT; }
			}

			if (src->snes.buttons & SNES_BTN_SELECT) { dst->buttons |= CPAD_BTN_MINUS; }
			if (src->snes.buttons & SNES_BTN_START) { dst->buttons |= CPAD_BTN_PLUS; }
			if (src->snes.buttons & SNES_BTN_L) { dst->buttons |= CPAD_BTN_TRIG_LEFT; }
			if (src->snes.buttons & SNES_BTN_R) { dst->buttons |= CPAD_BTN_TRIG_RIGHT; }

			if (IS_SIMULTANEOUS(src->snes.buttons, SNES_BTN_START|SNES_BTN_SELECT|SNES_BTN_L|SNES_BTN_R|SNES_BTN_DPAD_UP)) {
				g_current_config.g_snes_nes_mode = 0;
				g_current_config.g_snes_analog_dpad = 0;
				sync_config();
			}
			if (IS_SIMULTANEOUS(src->snes.buttons, SNES_BTN_START|SNES_BTN_SELECT|SNES_BTN_L|SNES_BTN_R|SNES_BTN_DPAD_DOWN)) {
				g_current_config.g_snes_nes_mode = 1;
				g_current_config.g_snes_analog_dpad = 0;
				sync_config();
			}
			if (IS_SIMULTANEOUS(src->snes.buttons, SNES_BTN_START|SNES_BTN_SELECT|SNES_BTN_L|SNES_BTN_R|SNES_BTN_DPAD_LEFT)) {
				g_current_config.g_snes_analog_dpad = 1;
				g_current_config.g_snes_nes_mode = 0;
				sync_config();
			}

			if (isTripleClick(src->snes.buttons & SNES_BTN_START)) {
				dst->buttons |= CPAD_BTN_HOME;
			}

			// Simulate L/R fully pressed values (like the analogue-less classic controller pro does)
			if (dst->buttons & CPAD_BTN_TRIG_LEFT) {
				dst->lt = 0xff;
			}
			if (dst->buttons & CPAD_BTN_TRIG_RIGHT) {
				dst->rt = 0xff;
			}
#elif WITH_9_BUTTONS
			dst->controller_id[0] = 'F';
			dst->controller_id[1] = 'C';
			memcpy(dst->controller_raw_data, src->nes.raw_data, NES_RAW_SIZE);

			if (src->nes.buttons & NES_BTN_A) { dst->buttons |= CPAD_BTN_A; }
			if (src->nes.buttons & NES_BTN_B) { dst->buttons |= CPAD_BTN_B; }
			if (src->nes.buttons & NES_BTN_SELECT) { dst->buttons |= CPAD_BTN_MINUS; }
			if (src->nes.buttons & NES_BTN_START) { dst->buttons |= CPAD_BTN_PLUS; }
			if (src->nes.buttons & NES_BTN_DPAD_UP) { dst->buttons |= CPAD_BTN_DPAD_UP; }
			if (src->nes.buttons & NES_BTN_DPAD_DOWN) { dst->buttons |= CPAD_BTN_DPAD_DOWN; }
			if (src->nes.buttons & NES_BTN_DPAD_LEFT) { dst->buttons |= CPAD_BTN_DPAD_LEFT; }
			if (src->nes.buttons & NES_BTN_DPAD_RIGHT) { dst->buttons |= CPAD_BTN_DPAD_RIGHT; }

			if (src->nes.buttons & NES_BTN_HOME) { dst->buttons |= CPAD_BTN_HOME; }
#elif WITH_8_BUTTONS
			// 8 buttons
			dst->controller_id[0] = 'F';
			dst->controller_id[1] = 'C';
			memcpy(dst->controller_raw_data, src->nes.raw_data, NES_RAW_SIZE);

			if (src->nes.buttons & NES_BTN_A) { dst->buttons |= CPAD_BTN_A; }
			if (src->nes.buttons & NES_BTN_B) { dst->buttons |= CPAD_BTN_B; }
			if (src->nes.buttons & NES_BTN_SELECT) { dst->buttons |= CPAD_BTN_MINUS; }
			if (src->nes.buttons & NES_BTN_START) { dst->buttons |= CPAD_BTN_PLUS; }
			if (src->nes.buttons & NES_BTN_DPAD_UP) { dst->buttons |= CPAD_BTN_DPAD_UP; }
			if (src->nes.buttons & NES_BTN_DPAD_DOWN) { dst->buttons |= CPAD_BTN_DPAD_DOWN; }
			if (src->nes.buttons & NES_BTN_DPAD_LEFT) { dst->buttons |= CPAD_BTN_DPAD_LEFT; }
			if (src->nes.buttons & NES_BTN_DPAD_RIGHT) { dst->buttons |= CPAD_BTN_DPAD_RIGHT; }

			if (isTripleClick(src->nes.buttons & NES_BTN_START)) {
				dst->buttons |= CPAD_BTN_HOME;
			}
#endif
}

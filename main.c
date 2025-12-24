/*  Extenmote : NES, SNES, N64 and Gamecube to Wii remote adapter firmware
 *  Copyright (C) 2012-2015  Raphael Assenat <raph@raphnet.net>
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

#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <math.h>
#include "wiimote.h"
#if WITH_15_BUTTONS
#include "clsc.h"
#else
#include "snes.h"
#endif
#include "eeprom.h"
#include "classic.h"
#include "analog.h"

static unsigned char classic_id[6] = { 0x00, 0x00, 0xA4, 0x20, 0x01, 0x01 };
static unsigned char adapter_snes_id[6] = { 0x00, 0x00, 0xA4, 0x20, 0x52, 0x10 };
static unsigned char adapter_nes_id[6] = { 0x00, 0x00, 0xA4, 0x20, 0x52, 0x08 };

// Classic controller (1)
// e1 15 82  e3 1a 7e  e3 1d 82  e4 1a 81  1a 18  7b d0
// Classic controller (2)
// e1 1b 7e  ea 1b 83  e5 1b 82  e4 16 80  26 22  9b f0
//

static unsigned char cal_data[32] = { 
		0xE0, 0x20, 0x80, // Left stick: Max X, Min X, Center X
		0xE0, 0x20, 0x80, // Left stick: Max Y, Min Y, Center Y
		0xE0, 0x20, 0x80, // Right stick: Max X, Min X, Center X
		0xE0, 0x20, 0x80, // Right stick: Max Y, Min Y, Center Y
		0x00, 0x00, 0, 0, 	// Shoulder Max? Min? checksum?

		0xE0, 0x20, 0x80, // Left stick: Max X, Min X, Center X
		0xE0, 0x20, 0x80, // Left stick: Max Y, Min Y, Center Y
		0xE0, 0x20, 0x80, // Right stick: Max X, Min X, Center X
		0xE0, 0x20, 0x80, // Right stick: Max Y, Min Y, Center Y
		0x00, 0x00, 0, 0,	// Shoulder Max? Min? checksum?
};

static volatile char performupdate;

static void hwInit(void)
{
	/* PORTD
	 * input for 8 buttons
	 */
	PORTD = 0xff;
	DDRD = 0x00;
	
	PORTC = 0x01;
	DDRC = 0xc0;

	/* PORTB
	 *
	 * 0: in1 - SNES uses
	 * 1: in1 - SNES uses
	 * 2: in1 - SNES uses
	 * 3: in1 - SNES uses
	 * 4: in1 - always - used for home
	 * 5: in1 - ignored - different design could use this one
	 * 6: in1 - biggerpad
	 * 7: in1 - biggerpad
	 *
	 */
	//PORTB = 0x3f;
	//DDRB = 0x00;
	
	// change to no crystal
	PORTB = 0xff;
	DDRB = 0x00;
}

static void pollfunc(void)
{
	performupdate = 1;
}

#define ERROR_THRESHOLD			10

#define STATE_NO_CONTROLLER		0
#define STATE_CONTROLLER_ACTIVE	1

int main(void)
{
#if WITH_15_BUTTONS
	Gamepad *clsc_gamepad = NULL;
#else
	Gamepad *snes_gamepad = NULL;
#endif
	unsigned char analog_style = ANALOG_STYLE_DEFAULT;
	gamepad_data lastReadData;
	classic_pad_data classicData;
	unsigned char current_report[PACKED_CLASSIC_DATA_SIZE];
	char first_controller_read=0;

	hwInit();
#if WITH_EEPROM
	init_config();
#endif

#if WITH_15_BUTTONS
	clsc_gamepad = clscGetGamepad();
#else
	snes_gamepad = snesGetGamepad();
#endif

	dataToClassic(NULL, &classicData, 0);
	pack_classic_data(&classicData, current_report, ANALOG_STYLE_DEFAULT, CLASSIC_MODE_1);

#if WITH_15_BUTTONS
 		// no alt id
#elif WITH_13_BUTTONS
		wm_setAltId(adapter_snes_id);
#elif WITH_12_BUTTONS
		wm_setAltId(adapter_snes_id);
#elif WITH_9_BUTTONS
		wm_setAltId(adapter_nes_id);
#elif WITH_8_BUTTONS
		wm_setAltId(adapter_nes_id);
#endif

	wm_init(classic_id, current_report, PACKED_CLASSIC_DATA_SIZE, cal_data, pollfunc);
	wm_start();
	sei();

	while(1)
	{
		// Adapter without sleep: 4mA
		// Adapter with sleep: 1.6mA

		set_sleep_mode(SLEEP_MODE_EXT_STANDBY);
		sleep_enable();
		sleep_cpu();
		sleep_disable();

		while (!performupdate) { }
		performupdate = 0;

		// With this delay, the controller read is postponed until just before
		// the next I2C read from the wiimote. This is to reduce latency to a
		// minimum (2.2ms at the moment).
		//
		// The N64 or Gamecube controller read will be corrupted if interrupted
		// by an I2C interrupt. The timing of the I2C read varies (menu vs in-game)
		// so we have to be careful not to let the N64/GC transaction overlap
		// with the I2C communication..
		//
		// This is why I chose to maintain a margin.
		//
		_delay_ms(2.35); // delay A

		//                                        |<----------- E ----------->|
		//                               C  -->|  |<--
		//         ____________________________    ________  / ______________________ ...
		// N64/GC:                             ||||         /
		//         _____  __    __    ___________________  / ____  __    __    _____ ...
		// I2C:         ||  ||||  ||||                    /      ||  ||||  ||||
		//                                               /
		//              |<---- D --->|
		//                      |<----- A ---->|
		//              |<-------------------B------------------>|
		// A = 2.35ms (Configured above)
		// B = 5ms (Wiimote classic controller poll rate)
		// C = 0.4ms (GC controller poll time)
		// D = 1.2ms, 1.1ms, 1.5ms (Wiimote I2C communication time. Varies [menu/game])
		// E = 2.34ms (menu), 2.84ms (in game)
		//

#if WITH_15_BUTTONS
		clsc_gamepad = clscGetGamepad();
		clsc_gamepad->update();
		clsc_gamepad->getReport(&lastReadData);
#else
		snes_gamepad = snesGetGamepad();
		snes_gamepad->update();
		snes_gamepad->getReport(&lastReadData);
#endif

		if (!wm_altIdEnabled())
		{
			unsigned char mode;

			switch(wm_getReg(0xFE))
			{
				default:
				case 0x01: mode = CLASSIC_MODE_1; break;
				case 0x03: mode = CLASSIC_MODE_3; break;
				case 0x02: mode = CLASSIC_MODE_2; break;
			}

			dataToClassic(&lastReadData, &classicData, first_controller_read);
			pack_classic_data(&classicData, current_report, analog_style, mode);
			wm_newaction(current_report, PACKED_CLASSIC_DATA_SIZE);
		}
		else
		{
			unsigned char raw[8];

#if WITH_15_BUTTONS
				memcpy(raw, lastReadData.classic.controller_raw_data, sizeof(lastReadData.classic.controller_raw_data));
				wm_newaction(raw, sizeof(lastReadData.classic.controller_raw_data));
#elif WITH_13_BUTTONS
				memcpy(raw, lastReadData.snes.raw_data, sizeof(lastReadData.snes.raw_data));
				wm_newaction(raw, sizeof(lastReadData.snes.raw_data));
#elif WITH_12_BUTTONS
				memcpy(raw, lastReadData.snes.raw_data, sizeof(lastReadData.snes.raw_data));
				wm_newaction(raw, sizeof(lastReadData.snes.raw_data));
#elif WITH_9_BUTTONS
				memcpy(raw, lastReadData.nes.raw_data, sizeof(lastReadData.nes.raw_data));
				wm_newaction(raw, sizeof(lastReadData.nes.raw_data));
#elif WITH_8_BUTTONS
				memcpy(raw, lastReadData.nes.raw_data, sizeof(lastReadData.nes.raw_data));
				wm_newaction(raw, sizeof(lastReadData.nes.raw_data));
#endif
		}
	}

	return 0;
}

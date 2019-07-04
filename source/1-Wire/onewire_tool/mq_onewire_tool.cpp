// Copyright: (c) Jaromir Veber 2017-2018
// Version: 24102018
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// This dirver contains controll interface onewire device - it lists all devices connected to 1-wire
#include <stdexcept>                // exceptions
#include "../../config.h"
#if defined pigpio_FOUND
    #include "MaximInterface/Platforms/pigpio/I2CMaster.hpp"
    #include "MaximInterface/Platforms/pigpio/Sleep.hpp"
#elif defined gpiocxx_FOUND
    #include "MaximInterface/Platforms/i2cxx/I2CMaster.hpp"
    #include "MaximInterface/Platforms/i2cxx/Sleep.hpp"
#endif
#include "MaximInterface/Devices/DS2482_DS2484.hpp"
#include "MaximInterface/Links/RomCommands.hpp"
#include "MaximInterface/Utilities/HexConversions.hpp"

#include "MaximInterface/Devices/DS18B20.hpp"
#include "spdlog/sinks/stdout_sinks.h"


// This code is defigned to report devices on 1-Wire allowing the user to prepare adress of devices for MQ Sytem
// Well right now I support only one master (DS2482_100) over I2C on address 0x18.
// but it is also possible to change the adress and also support other masters (DS2482_800, DS2484)
// Also I support oly devices I have on 1-Wire and that is DS18B20

void PrintInformationByRomFamily(MaximInterface::RomId::value_type family, uint_least8_t romID[8]) {
	printf("----------------------------------------------------\n"); 
	printf("Device ID: %s\n", MaximInterface::byteArrayToHexString(romID, 8).c_str());
	switch (family) {
		case 0x28:
			printf("Detected family 0x28 probably device DS18B20 or compatible\n");			
			printf("Provides values: Temperature (read only)\n");
			break;
		case 0x10:
			printf("Detected family 0x10 probably device DS1920 or compatible\n");
			printf("Provides values: Temperature (read only)\n");
			break;
		case 0x3A:
			printf("Detected family 0x3A probably device DS2413 or compatible\n");
			printf("Device known - DS2413 but not supported\n");
			//_logger->info("Provides values: DO1, DO2 (write only)\n");
			break;
		case 0x2D:
			printf("Detected family 0x2D probably device DS2431 or compatible\n");
			printf("Device known - DS2431 but not supported\n");
			break;
		default:
			printf("Unsupported device type - TODO\n");
			break;
	}
}

int main() {
	auto logger = spdlog::stdout_logger_mt("console");
#if defined pigpio_FOUND
    MaximInterface::piI2CMaster i2c;
    MaximInterface::pigpio::Sleep Sleep;
#elif defined gpiocxx_FOUND
    MaximInterface::xxI2CMaster i2c("/dev/i2c-1", logger);
    MaximInterface::pigpio::Sleep Sleep;
#endif
	i2c.start(0x18);
	MaximInterface::DS2482_100 device(i2c, 0x18);
	auto result = device.initialize();
	if (!result.value()){
		printf("Device init OK\n");
	} else {
		printf("Device error %s\n", result.message().c_str());
		return 1;
	}
	MaximInterface::SearchRomState searchState;
	do {
		auto result = MaximInterface::searchRom(device, searchState);
		if (result) {
			printf("Device error %s\n", result.message().c_str());
			break;
		}		
		if (MaximInterface::valid(searchState.romId)) {
			uint_least8_t romID[8];
			for (size_t i = 0; i < 8; ++i)
				romID[i] = searchState.romId[i];	
			PrintInformationByRomFamily(MaximInterface::familyCode(searchState.romId), romID);
			
			MaximInterface::SelectMatchRom rom(searchState.romId);
			
			printf("Init DS18B20\n");
			MaximInterface::DS18B20 dev(Sleep, device, rom);
			int temp = 0;
			printf("Read temp DS18B20\n");
			auto result = MaximInterface::readTemperature(dev, temp);
			if (result)
				printf("Read temp failed with error %s\n", result.message().c_str());
			else
				printf("DS18B20 result %f\n", temp/16.f);
		}
	} while (searchState.lastDevice == false);
	printf("----------------------------------------------------\n"); 
	printf("Rom Search Finshed\n");
	i2c.stop();
	return 0;
}

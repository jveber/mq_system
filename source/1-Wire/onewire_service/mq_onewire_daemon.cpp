// Copyright: (c) Jaromir Veber 2017-2019
// Version: 04062019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// This dirver contains controll interface onewire devices (master + slaves)

#include "mq_lib.h"

#include <json-c/json_object.h>     // JSON format for communication
#include <json-c/json_tokener.h>    // JSON format translation (reading)
#include <libconfig.h++>            // loading configuration data

#include <chrono>                   // for elapsed time measurement
#include <stdexcept>                // exceptions
#include <thread>                   // sleep_for
#include <limits>                   // for limit
#include <algorithm>                // to write some common algos faster
#if defined pigpio_FOUND
    #include "MaximInterface/Platforms/pigpio/I2CMaster.hpp"
    #include "MaximInterface/Platforms/pigpio/Sleep.hpp"
#elif defined gpiocxx_FOUND
    #include "MaximInterface/Platforms/i2cxx/I2CMaster.hpp"
    #include "MaximInterface/Platforms/i2cxx/Sleep.hpp"
#endif
#include "MaximInterface/Devices/DS2482_DS2484.hpp"
#include "MaximInterface/Devices/DS18B20.hpp"
#include "MaximInterface/Links/RomCommands.hpp"
#include "MaximInterface/Utilities/HexConversions.hpp"

using namespace MQ_System;
using namespace libconfig;

class OneWire_Service : public Daemon {
 public:
    OneWire_Service();
    virtual ~OneWire_Service() noexcept;
    void main();
    //virtual void CallBack(const std::string& topic, const std::string& message) override;
 private:
    void ProcessRomDevice(MaximInterface::SearchRomState& searchState);
    void load_daemon_configuration();

    class OWDevice {
     public:
        OWDevice(const std::string& name, const std::string& id, int interval) : _found(false), _name(name), _interval(interval) {
             auto rom_id = MaximInterface::hexStringToByteArray(id);
             std::move(rom_id.begin(), rom_id.begin() + 8, _rom_id.begin());
        }
        decltype(std::chrono::steady_clock::now()) _last_refresh;
        bool _found;
        MaximInterface::RomId _rom_id;
        std::string _name;
        int _interval;
    };
    void ReadAndProcessDS18B20(OWDevice& device);

    std::vector<OWDevice> _devices;
    int _ow_driver_address;
    std::string _sensor_name;
    struct json_tokener* _tokener;
#if defined pigpio_FOUND
    MaximInterface::piI2CMaster* _i2c;
#elif defined gpiocxx_FOUND
    MaximInterface::xxI2CMaster* _i2c;
#endif
    MaximInterface::DS2482_DS2484* _master;
    MaximInterface::Sleep* _sleep;
};

OneWire_Service::~OneWire_Service() noexcept {
    if (_master != nullptr)
        delete _master;
    _i2c->stop();
    delete _i2c;
    delete _sleep;
    json_tokener_free(_tokener);
}

OneWire_Service::OneWire_Service(): Daemon("mq_onewire_daemon", "/var/run/mq_onewire_daemon.pid", true), _tokener(json_tokener_new()), _master(nullptr) {}

void OneWire_Service::load_daemon_configuration() {
    static const char* config_file = "/etc/mq_system/mq_onewire_daemon.conf";
    constexpr int kDefaultDriverAddress = 0x18;

    Config cfg;	
    _logger->trace("Load conf");
    try
    {
        cfg.readFile(config_file);
    }
    catch(const FileIOException &fioex)
    {
        throw std::runtime_error(std::string("I/O error while reading configuration file"));
    }
    catch(const ParseException &pex)
    {
        throw std::runtime_error(std::string("Parse error at ") + pex.getFile() + ":" + std::to_string(pex.getLine()) + " - " + pex.getError() );
    }
    try {
        if (!cfg.exists("driver_address"))
            _ow_driver_address = kDefaultDriverAddress;
        else
            cfg.lookupValue("driver_address", _ow_driver_address);

        Setting& sensors = cfg.lookup("sensors");
        for (int i = 0; i < sensors.getLength(); i++) {
            const auto& sensor = sensors[i];
            std::string sensor_name = sensor.lookup("name");
            std::string device_id = sensor.lookup("dev_id");
            int sensor_interval = 60;
            if (sensor.exists("interval"))
                sensor_interval = sensor.lookup("interval");
            _devices.emplace_back(sensor_name, device_id, sensor_interval);
        }
    } catch(const SettingNotFoundException &nfex) {
        throw std::runtime_error(std::string("Setting \"sensors\" not found in configuration file - running daemon without registered sensors is wast of your resources terminating"));
    } catch (const SettingTypeException &nfex) {
        throw std::runtime_error(std::string("Seting type error at: ") + nfex.getPath());
    }
}

void OneWire_Service::ProcessRomDevice(MaximInterface::SearchRomState& searchState) {
    auto iterator = std::find_if(_devices.begin(), _devices.end(), [searchState](const OWDevice& dev){ return std::equal(&searchState.romId[0], &searchState.romId[7], dev._rom_id.cbegin());});
    if (iterator == _devices.end()) {
        _logger->info("Found additional device that is not rquested by config file - such device can't be reported - ID {}", MaximInterface::byteArrayToHexString(&searchState.romId[0], 8) );
        return;
    }
    auto family = MaximInterface::familyCode(searchState.romId);
    switch (family) {
        case 0x28:
            _logger->trace("Detected family 0x28 probably device DS18B20 or compatible");
            break;
        default:
            _logger->warn("Founf unsupported device type: {}; device will be removed from report list!", family);
            _devices.erase(iterator);
            break;
    }
    iterator->_found = true;
}

void OneWire_Service::ReadAndProcessDS18B20(OWDevice& device) {
    _logger->debug("Init DS18B20");
    MaximInterface::SelectMatchRom rom(device._rom_id);
    MaximInterface::DS18B20 dev(*_sleep, *_master, rom);
    auto result = dev.initialize();
    if (result) {
        _logger->error("DS18B20 device init error {}", result.message());
        return;
    }
    int measurement = 0;
    _logger->debug("Read temp DS18B20");
    result = MaximInterface::readTemperature(dev, measurement);
    if (result) {
        _logger->error("DS18B20 device error {}", result.message());
        return;
    }
    double temperature = measurement / 16.0;
    auto temp = json_object_new_array();
    json_object_array_add(temp, json_object_new_double(temperature));
    json_object_array_add(temp, json_object_new_string("°C"));
    struct json_object* j_object = json_object_new_object();
    json_object_object_add(j_object, "Temperature", temp);
    const std::string json_string = json_object_to_json_string_ext(j_object, JSON_C_TO_STRING_PLAIN);
    Publish(std::string("status/") + device._name, json_string);
    json_object_put(j_object);
}

void OneWire_Service::main() {
    load_daemon_configuration();
    _logger->debug("Entered main");
#if defined pigpio_FOUND
    _i2c = new MaximInterface::piI2CMaster();
#elif defined gpiocxx_FOUND
    _i2c = new MaximInterface::xxI2CMaster("/dev/i2c-1", _logger);
#endif
    _i2c->start(_ow_driver_address);
    // right now we support only DS2482_100 but library (Maxim interface is able to support also DS2482_800 & DS2484
    // unfortunatly I don't have such devices so I decided not to write code to support them bacuse I cant't verify if it works..
    // if someone have such devics we cand prepare working version to support also the other masters
    _master	= new MaximInterface::DS2482_100(*_i2c, _ow_driver_address);
    auto result = _master->initialize();
    if (result) {
        _logger->critical("Device init failed with error {}", result.message());
        return;
    }
    _logger->trace("Device init OK");
    MaximInterface::SearchRomState searchState;
    do {
        auto result = MaximInterface::searchRom(*_master, searchState);
        if (result) {
            _logger->error("Device error {}", result.message());
            break;
        }
        if (MaximInterface::valid(searchState.romId)) {
            ProcessRomDevice(searchState);
        }
    } while (searchState.lastDevice == false);
    _logger->trace("Rom Search Finshed");
    for (auto iterator = _devices.begin(); iterator != _devices.end(); ++iterator)
        if (!iterator->_found) {
            _logger->warn("Device {} not found on I2C - removing it from list", MaximInterface::byteArrayToHexString(iterator->_rom_id.data(), 8) );
            iterator = _devices.erase(iterator);
        }
    if (!_devices.size()) {
        _logger->warn("No device in devices list - terminating daemon - no reason to run");
        return;
    }
    _sleep = new MaximInterface::pigpio::Sleep();

    while(true) {
        auto now = std::chrono::steady_clock::now();
        for (auto&& device : _devices) {
            auto diff = now - device._last_refresh;
            double seconds_since_last_refresh = diff.count() / 1000000000.0;
            _logger->debug("Last refresh {} s", seconds_since_last_refresh);
            if (seconds_since_last_refresh > device._interval) {
                switch (MaximInterface::familyCode(device._rom_id)) {
                    case 0x28: {
                        device._last_refresh = now;
                        ReadAndProcessDS18B20(device);  // this takes some time ~ 1s
                        now = std::chrono::steady_clock::now();  // so we need to refresh now status, because it may be 1 sec ahead
                        break;
                    }
                    default: 
                        _logger->critical("Unexpected device family!");  // this should never happen
                        return;
                }
            }
        }
        double next_refresh = std::numeric_limits<double>::max();
        for (const auto& device : _devices) {
            double seconds_since_last_refresh = (now - device._last_refresh).count() / 1000000000.0;
            if (next_refresh > (device._interval - seconds_since_last_refresh))
                next_refresh = device._interval - seconds_since_last_refresh;
        }
        if (next_refresh > 0.5) {
            _logger->debug("Refresh time {} s", next_refresh);
            std::this_thread::sleep_for(std::chrono::duration<float>(next_refresh));
        }
    }
}

int main() {
    try {
        OneWire_Service* d = new OneWire_Service();
        d->main();
        delete d;
    } catch (const std::runtime_error& error) {
        return -2;
    }
    return 0;
}

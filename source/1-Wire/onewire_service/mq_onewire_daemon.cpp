// Copyright: (c) Jaromir Veber 2017-2019
// Version: 09122019
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

#include "Platforms/Sleep.hpp"
#if defined pigpio_FOUND
    #include "Platforms/pigpio/I2CMaster.hpp"
#elif defined gpiocxx_FOUND
    #include "Platforms/i2cxx/I2CMaster.hpp"
#endif
#include "MaximInterfaceDevices/DS2482_DS2484.hpp"
#include "MaximInterfaceDevices/DS18B20.hpp"
#include "MaximInterfaceCore/RomCommands.hpp"
#include "MaximInterfaceCore/HexString.hpp"

using namespace MQ_System;
using namespace libconfig;

class OneWire_Service : public Daemon {
 public:
    OneWire_Service();
    virtual ~OneWire_Service() noexcept;
    void main();
    //virtual void CallBack(const std::string& topic, const std::string& message) override;
 private:
    void ProcessRomDevice(MaximInterfaceCore::SearchRomState& searchState);
    void load_daemon_configuration();

    class OWDevice {
     public:
        OWDevice(const std::string& name, const std::string& id, int interval) : _found(false), _name(name), _interval(interval) {
             auto rom_id = MaximInterfaceCore::fromHexString(MaximInterfaceCore::span<const char>(id.c_str(), id.size()));
             if (rom_id) // rom_id is optional (it may or may not contain vector) this way we check if it has value
                _rom_id = std::move(*rom_id);
        }
        decltype(std::chrono::steady_clock::now()) _last_refresh;
        bool _found;
        // MaximInterfaceCore::RomId _rom_id;
        std::vector<uint_least8_t> _rom_id;
        std::string _name;
        int _interval;
    };
    void ReadAndProcessDS18B20(OWDevice& device);

    std::vector<OWDevice> _devices;
    int _ow_driver_address;
    std::string _sensor_name;
    struct json_tokener* const _tokener;
#if defined pigpio_FOUND
    std::unique_ptr<MaximInterfaceCore::piI2CMaster> _i2c;
#elif defined gpiocxx_FOUND
    std::unique_ptr<MaximInterfaceCore::xxI2CMaster> _i2c;
#endif
    std::unique_ptr<MaximInterfaceDevices::DS2482_DS2484> _master;
    std::unique_ptr<MaximInterface::Sleep> _sleep;
};

OneWire_Service::~OneWire_Service() noexcept {
    _master.reset(nullptr);
    if (_i2c) {
        _i2c->stop();
    }
    if (_tokener)
        json_tokener_free(_tokener);
}

OneWire_Service::OneWire_Service(): Daemon("mq_onewire_daemon", "/var/run/mq_onewire_daemon.pid"), _tokener(json_tokener_new()) {}

void OneWire_Service::load_daemon_configuration() {
    static const char* config_file = "/etc/mq_system/mq_onewire_daemon.conf";
    static constexpr int kDefaultDriverAddress = 0x18;

    Config cfg;
    _logger->trace("Load conf");
    try {
        cfg.readFile(config_file);
    } catch(const FileIOException &fioex) {
        _logger->error("I/O error while reading configuration file");
        throw std::runtime_error("");
    } catch(const ParseException &pex) {
        _logger->error("Parse error at {}:{} - {}", pex.getFile(), pex.getLine(), pex.getError());
        throw std::runtime_error("");
    }
    try {
        if (!cfg.exists("driver_address"))
            _ow_driver_address = kDefaultDriverAddress;
        else
            cfg.lookupValue("driver_address", _ow_driver_address);

        const Setting& sensors = cfg.lookup("sensors");
        for (int i = 0; i < sensors.getLength(); i++) {
            const auto& sensor = sensors[i];
            const std::string sensor_name = sensor.lookup("name");
            const std::string device_id = sensor.lookup("dev_id");
            int sensor_interval = 60;
            if (sensor.exists("interval"))
                sensor_interval = sensor.lookup("interval");
            _devices.emplace_back(sensor_name, device_id, sensor_interval);
        }
        if  (cfg.exists("log_level")) {
            int level;
            cfg.lookupValue("log_level", level);
            _logger->set_level(static_cast<spdlog::level::level_enum>(level));		
        }
    } catch(const SettingNotFoundException &nfex) {
        _logger->error("Setting \"sensors\" not found in configuration file - running daemon without registered sensors is wast of your resources terminating");
        throw std::runtime_error("");
    } catch (const SettingTypeException &nfex) {
        _logger->error("Seting type error at: {}", nfex.getPath());
    }
}

void OneWire_Service::ProcessRomDevice(MaximInterfaceCore::SearchRomState& searchState) {
    auto iterator = std::find_if(_devices.begin(), _devices.end(), [searchState](const OWDevice& dev){ return std::equal(&searchState.romId[0], &searchState.romId[7], dev._rom_id.cbegin());});
    if (iterator == _devices.end()) {
        _logger->info("Found additional device that is not rquested by config file - such device can't be reported - ID {}", MaximInterfaceCore::toHexString(searchState.romId));
        return;
    }
    auto family_code = searchState.romId.front();
    switch (family_code) {
        case 0x28:
            _logger->trace("Detected family 0x28 probably device DS18B20 or compatible");
            break;
        default:
            _logger->warn("Founf unsupported device type: {}; device will be removed from report list!", family_code);
            _devices.erase(iterator);
            break;
    }
    iterator->_found = true;
}

void OneWire_Service::ReadAndProcessDS18B20(OWDevice& device) {
    _logger->debug("Init DS18B20");
    MaximInterfaceCore::SelectMatchRom rom(device._rom_id);
    MaximInterfaceDevices::DS18B20 dev(*_sleep.get(), *_master.get() , rom);
    const auto result_init_dev = dev.initialize();
    if (!result_init_dev) {
        _logger->error("DS18B20 device init error {}", result_init_dev.error().message());
        return;
    }
    _logger->debug("Read temp DS18B20");
    const auto result_read_temp = MaximInterfaceDevices::readTemperature(dev);
    if (!result_read_temp) {
        _logger->error("DS18B20 device error {}", result_read_temp.error().message());
        return;
    }
    const double temperature = result_read_temp.value() / 16.0;
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
    _i2c.reset(new MaximInterfaceCore::piI2CMaster());
#elif defined gpiocxx_FOUND
    _i2c.reset(new MaximInterfaceCore::xxI2CMaster("/dev/i2c-1", _logger));
#endif
    _i2c->start(_ow_driver_address);
    // right now we support only DS2482_100 but library (Maxim interface is able to support also DS2482_800 & DS2484
    // unfortunatly I don't have such devices so I decided not to write code to support them because I cant't verify if it works..
    // if someone have such devics we cand prepare working version to support also the other masters
    _master.reset(new MaximInterfaceDevices::DS2482_100(*_i2c.get(), _ow_driver_address));
    const auto result_init_master = _master->initialize();
    if (!result_init_master) {
        _logger->critical("Device init failed with error {}", result_init_master.error().message());
        return;
    }
    _logger->trace("Device init OK");
    MaximInterfaceCore::SearchRomState searchState;
    do {
        const auto result_search_rom = MaximInterfaceCore::searchRom(*_master.get(), searchState);
        if (!result_search_rom) {
            _logger->error("Device error {}", result_search_rom.error().message());
            break;
        }
        if (MaximInterfaceCore::valid(searchState.romId)) {
            ProcessRomDevice(searchState);
        }
    } while (!searchState.lastDevice);
    _logger->trace("Rom Search Finshed");
    for (auto iterator = _devices.begin(); iterator != _devices.end(); ++iterator)
        if (!iterator->_found) {
            _logger->warn("Device {} not found on I2C - removing it from list", MaximInterfaceCore::toHexString(iterator->_rom_id));
            iterator = _devices.erase(iterator);
        }
    if (!_devices.size()) {
        _logger->warn("No device in devices list - terminating daemon - no reason to run");
        return;
    }
    _sleep.reset(new MaximInterface::Sleep());

    while(true) {
        auto now = std::chrono::steady_clock::now();
        for (auto&& device : _devices) {
            auto diff = now - device._last_refresh;
            const double seconds_since_last_refresh = diff.count() / 1000000000.0;
            _logger->debug("Last refresh {} s", seconds_since_last_refresh);
            if (seconds_since_last_refresh > device._interval) {
                // TODO move this logic to proxy function
                auto family_code = device._rom_id.front();
                switch (family_code) {
                    case 0x28: {
                        device._last_refresh = now;
                        ReadAndProcessDS18B20(device);  // this takes some time ~ 1s
                        now = std::chrono::steady_clock::now();  // so we need to refresh now status, because it may be 1 sec ahead
                        break;
                    }
                    default: 
                        _logger->critical("Unexpected device family! {}", family_code);  // this should never happen
                        return;
                }
            }
        }
        double next_refresh = std::numeric_limits<double>::max();
        for (const auto& device : _devices) {
            const double seconds_since_last_refresh = (now - device._last_refresh).count() / 1000000000.0;
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
        OneWire_Service d;
        d.main();
    } catch (const std::runtime_error& error) {
        return -1;
    }
    return 0;
}

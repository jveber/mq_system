// Copyright: (c) Jaromir Veber 2017-2019
// Version: 12122019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// This dirver contains controll interface for almost all relevant devices on UniPI board.
//
// MCP23008 (port expander for relays)
// MCP3422 (ADC - analog input)
// 24AA00/24C02 (EEPROM)
// DI - Special Digital input ports
// AO - Special analog output
//
/////////////////////////////////////////////////////////////
//
// Some devices are removed or resolved by other driver:
// DS2482-100 -    since it adds another network with undefined number of devices and also the logic is 
//                 rather complicated so I decided to move 1-Wire to special module that just controll the
//                 1-Wire network and if possible also adds support for communication with connected 1-Wire 
//                 devices.
// MCP79410 - Real Time Clock (this device is handled by kernel driver - ds1307 on Linux; thus I decided it may be ommitted here); through it 
//              provides HW alarms that might be useful. Maybe in later versions.
// I2C  (controlled through I2C drivers  - only comunication bus not a device reporting values)
// UART (controlled through UART drivers - only comunication bus not a device reporting values)
//
// So this driver accepts:
//      relay1 ... relay8 - boolean value (on/off) 	   - switch specified relay 
//      AO                - double value (0 ... 10)    - set analog output to defined value of volts
// and this driver reports:
//      I1 ... I14       - boolean value (high/low)    - of digital input; reported on-change (but there may be some lag caused by minimal 5ms time of switch and also device reporting system and also broker communication system)
//      AI1 ... AI2      - double value (-10 ... 10)   - value of analog input in volts 
//
// EEPROM even throug reading and writing is fully supported by API; it is not yet accepted/reported by module (maybe in future versions)


// TODO - current version not finished (may not ork well)!

#include "mq_lib.h"

#include <limits.h>                 // techically could be switched to <limits> but well
#include <i2c/i2cxx.hpp>            // I2C connector
#include <gpio/gpioxx.hpp>          // GPIO connector
#include <json-c/json_object.h>     // JSON format for communication
#include <json-c/json_tokener.h>    // JSON format translation (reading)
#include <json-c/linkhash.h>        // access JSON-C dictionary object (for cycle json_object_object_foreach)
#include <libconfig.h++>            // loading configuration data


#include <chrono>                   // for elapsed time measurement
#include <stdexcept>                // exceptions
#include <thread>                   // sleep_for
#include <bitset>                   // bit operations
#include <cmath>                    // for NAN

using namespace MQ_System;
using namespace libconfig;

class EEPROM;
class MCP23008;
class MCP3422;
//class DigitalInputs;
//class AnalogOutput;

class UniPi_Service : public Daemon {
 public:
    UniPi_Service();
    virtual ~UniPi_Service() noexcept;
    void main();
    virtual void CallBack(const std::string& topic, const std::string& message) override;
    void read_thread_loop();
    //DigitalInputs* _digital_input;
 private:
    void load_daemon_configuration();

    static const char* pid_file_name;
    std::string _sensor_name;
    EEPROM* _eeprom;
    MCP23008* _relays;
    MCP3422* _analog_input;
    std::atomic<bool> _read_thread_run;
    //AnalogOutput* _analog_output;
    struct json_tokener* _tokener;
    std::thread _read_thread;
    double _analog_input_report_time;
    std::chrono::system_clock::time_point _last_ai_report_time;
};

UniPi_Service::UniPi_Service(): Daemon("mq_unipi_daemon", "/var/run/mq_unipi_daemon.pid"), _eeprom(nullptr), _relays(nullptr), _analog_input(nullptr), 
            _read_thread_run(true), //_digital_input(nullptr), _analog_output(nullptr),
             _tokener(json_tokener_new()), _analog_input_report_time(120) {}

void UniPi_Service::load_daemon_configuration() {
    static const char* config_file = "/etc/mq_system/mq_unipi_daemon.conf";
    Config cfg;	
    _logger->trace("Load conf");
    try {
        cfg.readFile(config_file);
    } catch(const FileIOException &fioex) {
        _logger->error("I/O error while reading system configuration file: {}", config_file);
        throw std::runtime_error("" );
    } catch(const ParseException &pex) {
        _logger->error("Parse error at {} : {} - {}", pex.getFile(), pex.getLine(), pex.getError());
        throw std::runtime_error("");
    }
    try {
        cfg.getRoot().lookupValue("name", _sensor_name);
        cfg.getRoot().lookupValue("AI", _analog_input_report_time);
    } catch(const SettingNotFoundException &nfex) {
        _logger->error("Required setting not found in system configuration file");
        throw std::runtime_error("");
    } catch (const SettingTypeException &nfex) {
        _logger->error("Seting type error (at system configuaration) at: {}", nfex.getPath());
        throw std::runtime_error("");
    }
}

class EEPROM {
public:
    EEPROM (const std::string& device, const std::shared_ptr<spdlog::logger>& log): _logger(log) {
        _eeprom_handle = new i2cxx(device, EEPROM_ADDRESS, _logger);
        _logger->trace("24C02 open ... OK");
    }

    ~EEPROM() noexcept {
        delete _eeprom_handle;
    }

    uint8_t read_byte(unsigned address) {
        if (address >= 256) {
            _logger->error("Error EEPROM address out of range");
            throw std::runtime_error("");
        }
        return _eeprom_handle->read_byte_data(address);
    }

    void write_byte(unsigned address, uint8_t byte) {
        if (address >= 0xE0){  // 0xE0-0xFF is reserved for settings so we won't write it..
            _logger->error("Error EEPROM address out of range");
            throw std::runtime_error("");
        }
        _eeprom_handle->write_byte_data(address, byte);
    }

    float read_coef(bool second) {
        float coef;
        uint8_t* p_coef = reinterpret_cast<uint8_t*>(&coef);
        unsigned address = second ? 0xf4 : 0xf0;
        p_coef[3] = read_byte(address);
        p_coef[2] = read_byte(address + 1);
        p_coef[1] = read_byte(address + 2);
        p_coef[0] = read_byte(address + 3);
        if (6.f < coef || coef < 4.8f) { // well this resolve Unipi 1.0 & possible error or EEPROM rewrite
            _logger->info("Coeficient was not in range so using 5.56 - Analog input may not be precise");
            coef = 5.564920867;
        }
        return coef;
    }
private:
    static constexpr unsigned EEPROM_ADDRESS = 0x50;
    i2cxx* _eeprom_handle;
    const std::shared_ptr<spdlog::logger> _logger;
};

class MCP23008 {
public:
    MCP23008(const std::string& device, const std::shared_ptr<spdlog::logger>& log):  _logger(log) {
        _mcp_handle = new i2cxx(device, MCP23008_ADDRESS, log);
        _logger->trace("MCP23008 open ... OK");
        _mcp_handle->write_byte_data(MCP23008_IODIR, 0x00);  // since there are on all I/O pins relays we need them all to be output.
        read_state();
    }

    ~MCP23008() noexcept {
        delete _mcp_handle;
    }

    bool get_relay_value(size_t pos) {
        return _state.test(7 - pos);  // techically this will throw out_of_bounds exception if pos < 0 or pos > 7 ...
    }

    void set_relay_value(size_t pos, bool val) {
        _logger->trace("Set relay {} to {}", pos, val);
		if (pos > 7) {
            _logger->error("MCP23008 set_relay_value error - index out of bounds");
            throw std::runtime_error("");
        }
        _state[7 - pos] = val;  // this does not check boundary so the previous check is needed!
        _mcp_handle->write_byte_data(MCP23008_GPIO, static_cast<uint8_t>(_state.to_ulong()));
        read_state();
    }

private:
    void read_state() {
        _state = _mcp_handle->read_byte_data(MCP23008_OLAT);
    }

    static constexpr unsigned MCP23008_ADDRESS = 0x20;
    static constexpr unsigned MCP23008_IODIR = 0x00;     // direction register 1=inp, 0=out
    static constexpr unsigned MCP23008_GPIO = 0x09;      // input/output
    static constexpr unsigned MCP23008_OLAT = 0x0A;      // latch output status
    i2cxx* _mcp_handle;
    std::bitset<8> _state;
    const std::shared_ptr<spdlog::logger> _logger;
};


class MCP3422 {
public:
    MCP3422(const std::string& device, const std::shared_ptr<spdlog::logger>& log, float coef1, float coef2):  _logger(log), coef{coef1, coef2} {
        _mcp_handle.reset(new i2cxx(device, MCP3422_ADDRESS, log));
        _logger->trace("MCP3422 open ... OK");
    }

    // this method takes ~300ms to execute! so it may stall the thread a bit!
    double read_channel_code(bool channel) {
        auto defined_config = configure(channel);
        uint8_t data[4] = {0,0,0,0};
        uint8_t returned_config = 0;
        uint8_t max_tries = 5;
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));  // device does on 18bit precision like 3.75 reads per second so it's ~ 250ms for one read
            _mcp_handle->read((char*) data, 4);			
            returned_config = data[3];
            if (!(--max_tries)) 
                break;
        } while (returned_config & 0x80 || returned_config != (defined_config & 0x7F));
        if (!max_tries) {
            _logger->error("Error MCP3422 reading device: max_tries exceeded");
            throw std::runtime_error("");
        }
        uint8_t sign_byte = (data[0] & 0x03) ? 0xFF : 0x00;  // sign bit
        int32_t digital_output_code = (static_cast<int32_t>(sign_byte) << 24) | (static_cast<int32_t>(data[0]) << 16) | (static_cast<int32_t>(data[1]) << 8) | data[2];	
        double result = digital_output_code * (2.048 / 0b11111111111111111) * (channel ? coef[1] : coef[0]);
        return result;
    }

private:
    uint8_t configure(bool channel) {
        // PGA Gain = 1x
        _config[0] = 0;
        _config[1] = 0;
        // Sample Rate 18 bits
        _config[2] = 1;
        _config[3] = 1;
        // Conversion - Oneshot
        _config[4] = 0;
        // Channel
        _config[5] = channel;
        _config[6] = 0;
        // R/W bit - initiate one-shot conversion
        _config[7] = 1;
        auto config = static_cast<uint8_t>(_config.to_ulong());
        _mcp_handle->write_byte(config);
        return config;
    }

    static constexpr unsigned MCP3422_ADDRESS = 0x68;
    std::unique_ptr<i2cxx> _mcp_handle;
    std::bitset<8> _config;
    const std::shared_ptr<spdlog::logger> _logger;
    float coef[2];
};

#if 0
class DigitalInputs {
    public:
        DigitalInputs(const std::string& device, std::shared_ptr<spdlog::logger> log, struct mosquitto *mosq, const std::string& sensor_name) :  _logger(log), _mosquitto_object(mosq), _sensor_name(sensor_name) {
            _gpio = new gpiocxx(device, _logger);
            for (size_t i = 0; i < sizeof(di_map); ++i) {
                _gpio->watch_event(di_map[i], gpiocxx::event_req::BOTH_EDGES);
            }
            _logger->trace("Digital Inputs open ... OK");
        }

        std::vector<uint32_t> wait_events(uint32_t sec = 86400) {
            struct timespec timeout;
            timeout.tv_sec = sec; // 1 day
            timeout.tv_nsec = 0;
            std::vector<uint32_t> v;
            for (size_t i = 0; i < sizeof(di_map); ++i) {
                v.emplace_back(di_map[i]);
            }
            return _gpio->wait_events(v, &timeout);
        }

        void parse_event(uint32_t gpio) noexcept {
            struct gpioevent_data evdata;
            try {
                _gpio->read_event(gpio, evdata);
                //
                unsigned index = 0;
                for ( ; index < sizeof(di_map); ++index)
                    if (di_map[index] == gpio)
                        break;
                auto value_name = std::string("I") + std::to_string(index);	
                struct json_object* j_object = json_object_new_object();
                json_object_object_add(j_object, value_name.c_str(), json_object_new_boolean(evdata.id == GPIOEVENT_EVENT_RISING_EDGE ? true : false));
                const char* json_string = json_object_to_json_string_ext(j_object, JSON_C_TO_STRING_PLAIN);
                int mosqresult = mosquitto_publish(_mosquitto_object, NULL, (std::string("status/") + _sensor_name).c_str(), strlen(json_string), json_string, 2, false);
                if (mosqresult != MOSQ_ERR_SUCCESS)
                    _logger->warn ("Publish error: {}", mosqresult);
                json_object_put(j_object);
            } catch (const std::runtime_error& error) {}
        }

    private:
        gpiocxx* _gpio;
        const std::shared_ptr<spdlog::logger> _logger;
        struct mosquitto * _mosquitto_object;
        const std::string& _sensor_name;
        static const uint8_t di_map[14];	
};

const uint8_t DigitalInputs::di_map[14] = { 4, 17, 27, 23, 22, 24, 11, 7, 8, 9, 25, 10, 31, 30 };
#endif
// Analog autout is kinda problematic.. it uses PWM; however sotware PWM is not "stable" and "hw" PWM is not supported by standard kernel - pigpio or another RPi (driver) is needed for that 
// So for now we keep pigpio version and we got "nothing" if pigpio is not present.
// I might add sw driven pwm to controll it but It would need additional testing and I do not use AO at all so... 
/*
class AnalogOutput {
    public:
        AnalogOutput(int pigpio_handle, const std::shared_ptr<spdlog::logger> log, uint8_t version_minor) : _pigpio_handle(pigpio_handle), _logger(log), _version_minor(version_minor) {
            set_PWM_frequency(_pigpio_handle, _pin, 400);
            set_PWM_range(_pigpio_handle, _pin, 1000);
            set(0.0);
            _logger->trace("Analog Output open ... OK");
        }

        void set(double voltage) {	
            if (voltage > 10.0) voltage = 10.0;
            if (voltage < 0.0) voltage = 0.0;			
            unsigned freq = static_cast<unsigned>(std::round(((_version_minor == 0) ? 10.0 - voltage : voltage) * 100.0));
            set_PWM_dutycycle(_pigpio_handle, _pin, freq);
        }

    private:
        static constexpr unsigned _pin = 18;
        int _pigpio_handle;
        const std::shared_ptr<spdlog::logger> _logger;
        uint8_t _version_minor;
};
*/


void read_thread_start(UniPi_Service *u) {
    u->read_thread_loop();
}

void UniPi_Service::read_thread_loop() {
    while (_read_thread_run) {
        auto now = std::chrono::system_clock::now();
        std::chrono::duration<double> diff = now - _last_ai_report_time;
        if (diff.count() > _analog_input_report_time) {
            _logger->debug("Report Analog input");
            double AI2_value = _analog_input->read_channel_code(true);
            double AI1_value = _analog_input->read_channel_code(false);
            _last_ai_report_time = now;
            struct json_object* j_object = json_object_new_object();
            struct json_object* AI1_obj = json_object_new_array();
            json_object_array_add(AI1_obj, json_object_new_double(AI1_value));
            json_object_array_add(AI1_obj, json_object_new_string("V"));
            json_object_object_add(j_object, "AI1", AI1_obj);
            struct json_object* AI2_obj = json_object_new_array();
            json_object_array_add(AI2_obj, json_object_new_double(AI2_value));
            json_object_array_add(AI2_obj, json_object_new_string("V"));
            json_object_object_add(j_object, "AI2", AI2_obj);
            const std::string json_string = json_object_to_json_string_ext(j_object, JSON_C_TO_STRING_PLAIN);
            Publish(std::string("status/") + _sensor_name, json_string);
            json_object_put(j_object);
            auto now = std::chrono::system_clock::now();
            diff = now - _last_ai_report_time;
        }
        while (_read_thread_run) {
            double sleep_time = _analog_input_report_time - diff.count();
            if (sleep_time < 0.0)
                break;
            if (sleep_time >= 1.0) {
                double intpart = 0;
                double rest = std::modf(sleep_time, &intpart);
                /* TODO
                auto result = _digital_input->wait_events(static_cast<uint32_t>(intpart) > 10 ? 10 : static_cast<uint32_t>(intpart)); // we wait no longer than 10 secs
                if (result.size()) {
                    for (const auto& x : result) {
                        _digital_input->parse_event(x);
                    }
                }
                */
                auto now = std::chrono::system_clock::now();
                diff = now - _last_ai_report_time;
            } else if (sleep_time > 0.0) {
                _logger->debug("Wait report {}s", sleep_time);
                std::chrono::duration<double> sleep_duration(sleep_time);
                std::this_thread::sleep_for(sleep_duration);
            }
        }
    }
}

void UniPi_Service::main() {
    _logger->debug("Entered main");
    load_daemon_configuration();
    std::string dev = "/dev/i2c-1";
    std::string gpio_dev = "/dev/gpiochip0";
    // OK now initialize all the devices on UniPi
    _eeprom = new EEPROM(dev,_logger);
    _relays = new MCP23008(dev,_logger);
    _analog_input = new MCP3422(dev, _logger, _eeprom->read_coef(false), _eeprom->read_coef(true));
    //_digital_input = new DigitalInputs(gpio_dev, _logger, _mosquitto_object, _sensor_name); //TODO
    //_analog_output = new AnalogOutput(_pigpio_handle, _logger, _eeprom->read_byte(0xe3));
    _logger->info("Unipi version {}.{}", _eeprom->read_byte(0xe2), _eeprom->read_byte(0xe3));
    Subscribe(std::string("set/") + _sensor_name);
    _read_thread = std::move(std::thread(read_thread_start, this));
    SleepForever();  // do thread instead of sleeping and running other thread?
}

void UniPi_Service::CallBack(const std::string& topic, const std::string& message) {
    struct json_object* o = json_tokener_parse_ex(_tokener, message.c_str(), message.size());
    if (json_object_get_type(o) != json_type_object) {
        _logger->warn("Did not receive object as initial json type - bad json format: {}", message);
        return;
    }
    json_object_object_foreach(o, k, object) {
        auto object_type = json_object_get_type(object);
        // it may be an array - [value, unit]; we don't care about unit much here; so extract only the value
        if (object_type == json_type_array) {
            object = json_object_array_get_idx(object, 0);
            if (object == NULL)
                continue;
            object_type = json_object_get_type(object);
        }
        std::string key = k;
        if (key == "AO") {
            /* 
            double value = NAN;
            if (object_type == json_type_double)
                value = json_object_get_double(object);
            else if (object_type == json_type_int)
                value = json_object_get_int(object);
            if (std::isnan(value) || value > 10.0 || value < 0.0)
                _logger->warn("{}:AO value not in expected range - got: {}", topic.substr(4), value);	
            else
                 _analog_output->set(value);
            */
        } else {
            if (key.length() == 6 && key.substr(0,5) == "relay") {
                    auto relay_num = key[5] - '0';
                    if (relay_num < 1 || relay_num > 8)
                        _logger->warn("Relay ID out of bounds {} on sensor {}", key, topic.substr(4));
                    else {
                        if (object_type != json_type_boolean)
                            _logger->warn("{} value on sensor {} not of expected type boolean", key, topic.substr(4));
                        else
                            _relays->set_relay_value(--relay_num, json_object_get_boolean(object));
                    }
            } else
                _logger->warn("Unexpected value name {} on sensor {}", key, topic.substr(4));
        }
    }
}

UniPi_Service::~UniPi_Service() noexcept {
    _read_thread_run = false;
    delete _eeprom;
    delete _relays;
    _read_thread.join();  // this may take up to 10 secs :/
    delete _analog_input;
    //delete _digital_input;
    //delete _analog_output;
    json_tokener_free(_tokener);
    
}

int main() 
{
    try {
        UniPi_Service d;
        d.main();
    } catch (const std::runtime_error& error) {
        return -1;
    }
}

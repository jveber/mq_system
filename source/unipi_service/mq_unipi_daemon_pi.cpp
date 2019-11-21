// Copyright: (c) Jaromir Veber 2017-2019
// Version: 25062019
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
//        provides HW alarms that might be useful. Maybe in later versions.
// I2C  (controlled through pigpio - only comunication bus not a device reporting values)
// UART (controlled through pigpio - only comunication bus not a device reporting values)
//
// So this driver accepts:
//      relay1 ... relay8 - boolean value (on/off) 	   - switch specified relay 
//      AO                - double value (0 ... 10)    - set analog output to defined value of volts
// and this driver reports:
//      I1 ... I14       - boolean value (high/low)    - of digital input; reported on-change (but there may be some lag caused by minimal 5ms tim of switch and also pigpio reporting system and also mosquitto communication system)
//      AI1 ... AI2      - double value (-10 ... 10)   - value of analog input in volts 
//
// EEPROM even throug reading and writing is fully supported by API; it is not yet accepted/reported by module (maybe in future versions)

#include "mq_lib.h"

#include <limits.h>                 // techically could be switched to <limits> but well
#include <pigpiod_if2.h>            // GPIO connector
#include <json-c/json_object.h>     // JSON format for communication
#include <json-c/json_tokener.h>    // JSON format translation (reading)
#include <json-c/linkhash.h>        // access JSON-C dictionary object (for cycle json_object_object_foreach)
#include <libconfig.h++>            // loading configuration data


#include <chrono>                   // for elapsed time measurement
#include <stdexcept>                // exceptions
#include <thread>                   // sleep_for
#include <mutex>                    // for std::mutex
#include <condition_variable>       // for condition variable
#include <bitset>                   // bit operations
#include <cmath>                    // for NAN
#include <array>                    // for std::array
#include <iterator>                 // for std::distance


using namespace MQ_System;
using namespace libconfig;

class EEPROM;
class MCP23008;
class MCP3422;
class DigitalInputs;
class AnalogOutput;

class UniPi_Service : public Daemon {
    friend void read_thread_start(UniPi_Service *u) noexcept;
    friend class DigitalInputs;

 public:
    UniPi_Service();
    virtual ~UniPi_Service() noexcept;
    void main();
    void CallBack(const std::string& topic , const std::string& message) final;
 private:
    void read_thread_loop();
    void load_daemon_configuration();

    int _pigpio_handle;
    std::string _sensor_name;
    std::unique_ptr<EEPROM> _eeprom;
    std::unique_ptr<MCP23008> _relays;
    std::unique_ptr<MCP3422> _analog_input;
    std::unique_ptr<DigitalInputs> _digital_input;
    std::unique_ptr<AnalogOutput> _analog_output;
    struct json_tokener* _tokener;
    std::thread _read_thread;
    double _analog_input_report_time;
    std::chrono::system_clock::time_point _last_ai_report_time;
    std::atomic<bool> _read_thread_run;
    std::mutex mymutex;
    std::condition_variable mycond;
};

UniPi_Service::UniPi_Service(): Daemon("mq_unipi_daemon", "/var/run/mq_unipi_daemon.pid"), _pigpio_handle(-1), _eeprom(nullptr), _relays(nullptr), _analog_input(nullptr), 
            _digital_input(nullptr), _analog_output(nullptr), _tokener(json_tokener_new()), _analog_input_report_time(120), _read_thread_run(true) {}

void UniPi_Service::load_daemon_configuration() {
    static const char* config_file = "/etc/mq_system/mq_unipi_daemon.conf";
    Config cfg;
    _logger->trace("Load conf");
    try {
        cfg.readFile(config_file);
    } catch(const FileIOException &fioex) {
        _logger->error("I/O error while reading configuration file");
        throw std::runtime_error("");
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

constexpr unsigned I2CBUS_ID = 1;

static void i2c_open_error_handling(const char* device_name, int result, const std::shared_ptr<spdlog::logger>& log) {
    if (result < 0) {
        switch (result) {
            case PI_BAD_I2C_BUS:
                log->error("Error opening I2C device {} bad bus", device_name);
                throw std::runtime_error("");
            case PI_BAD_I2C_ADDR:
                log->error("Error opening I2C device {} bad address", device_name); 
                throw std::runtime_error("");
            case PI_BAD_FLAGS:
                log->error("Error opening I2C device {} bad flags", device_name);
                throw std::runtime_error("");
            case PI_NO_HANDLE:
                log->error("Error opening I2C device {} bad lib handle", device_name);
                throw std::runtime_error("");
            case PI_I2C_OPEN_FAILED:
                log->error("Error opening I2C device {} failed open", device_name); 
                throw std::runtime_error("");
            default:
                log->error("Error opening I2C device {} unexpected error", device_name); 
                throw std::runtime_error("");
            }
    }
}

class EEPROM {
public:
    EEPROM (int pigpio_handle, const std::shared_ptr<spdlog::logger>& log): _pigpio_handle(pigpio_handle), _logger(log) {
        int result = i2c_open(pigpio_handle, I2CBUS_ID, EEPROM_ADDRESS, 0);
        i2c_open_error_handling("EEPROM", result, _logger);
        _logger->trace("24C02 open ... OK");
        _eeprom_handle = static_cast<unsigned>(result);
    }
    
    uint8_t read_byte(unsigned address) {
        if (address >= 256) {
            _logger->error("Error EEPROM read address out of range");
            throw std::runtime_error("");
        }
        int result = i2c_read_byte_data(_pigpio_handle, _eeprom_handle, address);
        if (result < 0) {
          _logger->error("EEPROM read_byte() i2c_read_byte_data error");
          throw std::runtime_error("");
        }
        return static_cast<uint8_t>(result);
    }

    void write_byte(unsigned address, uint8_t byte) {
        if (address >= 0xE0) {  // 0xE0-0xFF is reserved for settings so we won't write it..
            _logger->error("Error EEPROM write address out of range");
            throw std::runtime_error("");
        }
        if (0 != i2c_write_byte_data(_pigpio_handle, _eeprom_handle, address, byte)) {  //all output !
            _logger->error("EEPROM write_byte() set relay output i2c_write_byte_data error");
            throw std::runtime_error("");
        }
    }

    float read_coef(bool second) {
        float coef = 0.0f;
        uint8_t* p_coef = reinterpret_cast<uint8_t*>(&coef);
        unsigned address = second ? 0xf4 : 0xf0;
        p_coef[3] = read_byte(address);
        p_coef[2] = read_byte(address + 1);
        p_coef[1] = read_byte(address + 2);
        p_coef[0] = read_byte(address + 3);
        if (6.0f < coef || coef < 4.8f) { // well this resolve Unipi 1.0 & possible read error or EEPROM rewrite
            _logger->info("Coeficient was not in range so using 5.56 - Analog input may not be precise");
            coef = 5.564920867f;
        }
        return coef;
    }

private:
    static constexpr unsigned EEPROM_ADDRESS = 0x50;
    unsigned _eeprom_handle;
    int _pigpio_handle;
    const std::shared_ptr<spdlog::logger> _logger;
};

class MCP23008 {
public:
    MCP23008(int pigpio_handle, const std::shared_ptr<spdlog::logger>& log): _pigpio_handle(pigpio_handle), _logger(log) {
        int result = i2c_open(pigpio_handle, I2CBUS_ID, MCP23008_ADDRESS, 0);
        i2c_open_error_handling("MCP23008", result, _logger);
        _logger->trace("MCP23008 open ... OK");
        _mcp_handle = static_cast<unsigned>(result);
        // since there are on all I/O pins relays we need them all to be output.
        if (0 != i2c_write_byte_data(pigpio_handle, _mcp_handle, MCP23008_IODIR, 0x00)) {  //all output !
            _logger->error("MCP23008 init() set all output i2c_write_byte_data error");
            throw std::runtime_error("");
        }
        read_state();
    }
    
    bool get_relay_value(size_t pos) {
        return _state.test(7 - pos);  // techically this will throw out_of_bounds exception if pos < 0 or pos > 7 ...
    }

    void set_relay_value(size_t pos, bool val) {
        if (pos > 7) {
            _logger->error("MCP23008 set_relay_value error - index out of bounds {}", pos);
            throw std::runtime_error("");
        }
        _state[7 - pos] = val;  // this does not check boundary so the previous check is needed!
        if (0 != i2c_write_byte_data(_pigpio_handle, _mcp_handle, MCP23008_GPIO, static_cast<uint8_t>(_state.to_ulong()))) {  //all output !
            _logger->error("MCP23008 set_relay_value() set relay output i2c_write_byte_data error");
            throw std::runtime_error("");
        }
        read_state();
    }

private:
    void read_state() {
        int result = i2c_read_byte_data(_pigpio_handle, _mcp_handle, MCP23008_OLAT);
        if (result < 0) {
            _logger->error("MCP23008 read_state() i2c_read_byte_data error");
            throw std::runtime_error("");
        }
        _state = std::bitset<8>(static_cast<uint8_t>(result));
    }

    static constexpr unsigned MCP23008_ADDRESS = 0x20;
    static constexpr unsigned MCP23008_IODIR = 0x00;     // direction register 1=inp, 0=out
    static constexpr unsigned MCP23008_GPIO = 0x09;      // input/output
    static constexpr unsigned MCP23008_OLAT = 0x0A;      // latch output status
    unsigned _mcp_handle;
    int _pigpio_handle;
    std::bitset<8> _state;
    const std::shared_ptr<spdlog::logger> _logger;
};

class MCP3422 {
public:
    MCP3422(int pigpio_handle, const std::shared_ptr<spdlog::logger>& log, float coef1, float coef2): _pigpio_handle{pigpio_handle}, coef{coef1, coef2}, _logger{log} {
        
        int result = i2c_open(pigpio_handle, I2CBUS_ID, MCP3422_ADDRESS, 0);
        i2c_open_error_handling("MCP3422", result, _logger);
        _logger->trace("MCP3422 open ... OK");
        _mcp_handle = static_cast<unsigned>(result);
    }

    // this method takes ~300ms to execute! so it may stall the thread a bit!
    double read_channel_code(bool channel) {
        auto defined_config = configure(channel);
        uint8_t data[4] = {0,0,0,0};
        uint8_t returned_config = 0;
        uint8_t max_tries = 5;
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));  // device does on 18bit precision like 3.75 reads per second so it's ~ 250ms for one read
            i2c_read_device(_pigpio_handle, _mcp_handle,(char*) data, 4);
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
        if (0 != i2c_write_byte(_pigpio_handle, _mcp_handle, config)) {
            _logger->error("MCP3422 configure() i2c_wrie_byte error");
            throw std::runtime_error("");
        }
        return config;
    }

    static constexpr unsigned MCP3422_ADDRESS = 0x68;
    unsigned _mcp_handle;
    int _pigpio_handle;
    float coef[2];
    std::bitset<8> _config;
    const std::shared_ptr<spdlog::logger> _logger;
};

static void callback_rise_func(int pi __attribute__((unused)), unsigned user_gpio, unsigned level, uint32_t tick, void * user) noexcept;

class DigitalInputs {
    public:
        DigitalInputs(UniPi_Service* parent, const std::string& sensor_name) : _parent(parent), _sensor_name(sensor_name) {
            for (const auto pin : digital_input_pin_map) {
                set_pull_up_down(_parent->_pigpio_handle, pin, PI_PUD_UP);
                callback_ex(_parent->_pigpio_handle, pin, EITHER_EDGE, callback_rise_func, this);
            }
            _parent->_logger->trace("Digital Inputs setup ... OK");
        }

        void event(unsigned user_gpio, unsigned level, uint32_t tick) noexcept {
            if (level == 2)
                return;
            auto pin_map_iterator = std::find(digital_input_pin_map.begin(), digital_input_pin_map.end(), user_gpio);
            if (pin_map_iterator == digital_input_pin_map.end()) {
                _parent->_logger->warn("DigitalInputs::event - ignoring signal on unregistered pin!? {}", user_gpio);  // this should not happen since we register only pins in map but..
                return;
            }
            auto value_name = std::string("I") + std::to_string((pin_map_iterator - digital_input_pin_map.begin()) + 1); //it's I01 .. I14
            struct json_object* j_object = json_object_new_object();
            json_object_object_add(j_object, value_name.c_str(), json_object_new_boolean(level));
            const char* json_string = json_object_to_json_string_ext(j_object, JSON_C_TO_STRING_PLAIN);
            _parent->Publish(std::string("status/") + _sensor_name, std::string(json_string));
            json_object_put(j_object);
        }

    private:
        UniPi_Service* _parent;
        const std::string& _sensor_name;
        static const std::array<uint8_t, 14> digital_input_pin_map;
};

const std::array<uint8_t, 14> DigitalInputs::digital_input_pin_map = {4, 17, 27, 23, 22, 24, 11, 7, 8, 9, 25, 10, 31, 30};

static void callback_rise_func(int pi __attribute__((unused)), unsigned user_gpio, unsigned level, uint32_t tick, void * user) noexcept {
        reinterpret_cast<DigitalInputs*>(user)->event(user_gpio, level, tick);
}

class AnalogOutput {
    public:
        AnalogOutput(int pigpio_handle, const std::shared_ptr<spdlog::logger>& log, uint8_t version_minor) : _pigpio_handle(pigpio_handle), _logger(log), _version_minor(version_minor) {
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

void read_thread_start(UniPi_Service *u) noexcept {
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
            const char* json_string = json_object_to_json_string_ext(j_object, JSON_C_TO_STRING_PLAIN);
            Publish(std::string("status/") + _sensor_name, json_string);
            json_object_put(j_object);
            auto now = std::chrono::system_clock::now();
            diff = now - _last_ai_report_time;
        }
        double sleep_time = _analog_input_report_time - diff.count();
        if (sleep_time > 0.0) {
            _logger->debug("Wait report {}s", sleep_time);
            std::chrono::duration<double> sleep_duration(sleep_time);
            std::unique_lock<std::mutex> lock(mymutex);
            mycond.wait_for( lock, sleep_duration);  //  wakeable alternative to std::this_thread::sleep_for(sleep_duration); We need to be able to wake the thread up anytime in case of deamon termination request.
        }
    }
}

void UniPi_Service::main() {
    _logger->debug("Entered main");
    load_daemon_configuration();
    _pigpio_handle = pigpio_start(NULL, NULL);  // technically we also could support GPIO read from another RPI but well not yet needed TODO?
    if (_pigpio_handle < 0) {
            _logger->error("Failed to connect to GPIO daemon (pigpiod)");
            throw std::runtime_error("");
    }
    // OK now initialize all the devices on UniPi
    _eeprom = std::make_unique<EEPROM>(_pigpio_handle, _logger); // new EEPROM(_pigpio_handle, _logger);
    _relays = std::make_unique<MCP23008>(_pigpio_handle, _logger);
    _analog_input = std::make_unique<MCP3422>(_pigpio_handle, _logger, _eeprom->read_coef(false), _eeprom->read_coef(true));
    _digital_input = std::make_unique<DigitalInputs>(this, _sensor_name);
    _analog_output = std::make_unique<AnalogOutput>(_pigpio_handle, _logger, _eeprom->read_byte(0xe3));
    _logger->info("Unipi version {}.{}", _eeprom->read_byte(0xe2), _eeprom->read_byte(0xe3));
    Subscribe(std::string("set/") + _sensor_name);
    _read_thread = std::move(std::thread(read_thread_start, this));
    SleepForever();
}

void UniPi_Service::CallBack(const std::string& topic , const std::string& message) {
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
            double value = NAN;
            if (object_type == json_type_double)
                value = json_object_get_double(object);
            else if (object_type == json_type_int)
                value = json_object_get_int(object);
            if (std::isnan(value) || value > 10.0 || value < 0.0)
                _logger->warn("{}:AO value not in expected range - got: {}", topic.substr(4), value);
            else
                _analog_output->set(value);
        } else {
            if (key.length() == 6 && key.substr(0,4) == "relay") {
                auto relay_num = key[5] - '0';
                    if (relay_num < 1 || relay_num > 8)
                        _logger->warn("unexpected value name {} on sensor {}", key, topic.substr(4));
                    else {
                        if (object_type != json_type_boolean)
                            _logger->warn("{} value on sensor {} not of expected type boolean", key, topic.substr(4));
                        else
                            _relays->set_relay_value(--relay_num, json_object_get_boolean(object));
                    }
            } else
                _logger->warn("unexpected value name {} on sensor {}", key, topic.substr(4));
        }
        // EEPROM - does anyone need EEPROM theese days, on RPi? There is always storage (microSD; and also may be external flash) so I do not see any reason to use this EEPROM except of it's current use 
        // (read only for UniPi verison an coeficients)
    }
}

UniPi_Service::~UniPi_Service() noexcept {
    _read_thread_run = false;  // ensure thread termination
    mycond.notify_one();  // this should wake sleeping thread
    _read_thread.join();  // this may take up to 10 secs :/
    json_tokener_free(_tokener);
    if (_pigpio_handle > 0) {
        pigpio_stop(_pigpio_handle);
        _pigpio_handle = -1;
    }
    // smart pointers destroyed now...
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

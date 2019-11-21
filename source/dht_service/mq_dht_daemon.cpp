// Copyright: (c) Jaromir Veber 2017-2019
// Version: 29032019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// Code to handle DHT sensor (DHT22 for now)
// This code is not based on origial adafruit (or another) code.
#include "mq_lib.h"

#if defined pigpio_FOUND
    #include <pigpiod_if2.h>            // GPIO connector
#elif defined gpiocxx_FOUND
    #include "gpio/gpioxx.hpp"
#else
    #error "no GPIO library present inf the system!"
#endif

#include <json-c/json_object.h>     // JSON format for communication
#include <libconfig.h++>            // loading configuration data

#include <cmath>                    // dew point equation
#include <limits>                   // techically could be switched to <limits> but well
#include <chrono>                   // for elapsed time measurement
#include <vector>                   // for storing sensor information
#include <stdexcept>                // exceptions
#include <thread>                   // sleep_for
#include <memory>

using namespace MQ_System;
using namespace libconfig;

class DHT_Service : public Daemon {
 public:
    DHT_Service();
    ~DHT_Service() noexcept;
    void main();
 private:
    struct MyConfig {
        MyConfig(std::string n, int p, int i): 
            pin(p), interval(i), name(n) {}
        int pin;
        int interval;
        std::string name;
        std::chrono::time_point<std::chrono::steady_clock> last_refresh;
    };

    void load_daemon_configuration();
    void read_sensors();
    void read_sensor(MyConfig &c) noexcept;
    int  read_sensor_data(int id, float& humidity, float& temperature) noexcept;
#ifdef pigpio_FOUND
    void check_result(int result);
#endif
    std::vector<MyConfig> _pin_config;
#ifdef pigpio_FOUND
    int _pigpio_handle;
#else
    std::unique_ptr<gpiocxx> _chip;
#endif
};


DHT_Service::DHT_Service(): Daemon("mq_dht_daemon", "/var/run/mq_dht_daemon.pid")
#ifdef pigpio_FOUND 
    , _pigpio_handle(-1)
#endif
{}


void DHT_Service::load_daemon_configuration() {
    static const char* config_file = "/etc/mq_system/mq_dht_daemon.conf";	
    Config cfg;	
    try
    {
        cfg.readFile(config_file);
    }
    catch(const FileIOException &fioex)
    {
        _logger->error("I/O error while reading system configuration file: {}", config_file);
        throw std::runtime_error("");
    }
    catch(const ParseException &pex)
    {
        _logger->error("Parse error at {} : {} - {}", pex.getFile(), pex.getLine(), pex.getError());
        throw std::runtime_error("");
    }
    try {
        const auto& sensors = cfg.getRoot()["sensors"];
        for (auto sensor = sensors.begin(); sensor != sensors.end(); ++sensor) {
            std::string sensor_name = sensor->lookup("name");
            int sensor_pin = sensor->lookup("pin");
            int sensor_interval = 60;
            if (sensor->exists("interval"))
                sensor_interval = sensor->lookup("interval");
            _pin_config.emplace_back(std::string("status/") + sensor_name, sensor_pin, sensor_interval);
        }
        if  (cfg.exists("log_level")) {
            int level;
            cfg.lookupValue("log_level", level);
            _logger->set_level(static_cast<spdlog::level::level_enum>(level));		
        }
    } catch(const SettingNotFoundException &nfex) {
        _logger->error("Required setting not found in system configuration file");
        throw std::runtime_error("");
    } catch (const SettingTypeException &nfex) {
        _logger->error("Seting type error (at system configuaration) at: {}", nfex.getPath());
        throw std::runtime_error("");
    }
}

void DHT_Service::main()
{
    load_daemon_configuration();
    read_sensors();  // Main Cycle
}

typedef struct my_data {
   uint8_t bit_counter;
   uint32_t time;
   uint64_t bits;
} MyData;

void DHT_Service::read_sensors()
{
    while (true) {
        int interval = std::numeric_limits<decltype(interval)>::max();
#ifdef pigpio_FOUND
        if (_pigpio_handle < 0)
            _pigpio_handle = pigpio_start(NULL, NULL);	// technically we also could support GPIO read from another RPI but well not yet needed TODO?

        if (_pigpio_handle < 0) {
            _logger->error("Failed to connect to GPIO daemon (pigpiod)");
            throw std::runtime_error("");
        }
#else
        if (!_chip)
            _chip.reset(new gpiocxx("/dev/gpiochip0", _logger));  // C++11 does not offer "std::make_unique<gpiocxx>("/dev/gpiochip0", _logger);" ...
        _logger->debug("Chip initialized");
#endif

        for (auto&& x : _pin_config) {
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<float> diff = now - x.last_refresh;
            int next_wake = 0;
            if (diff.count() >= x.interval) {
                read_sensor(x);
                x.last_refresh = now;
                next_wake = x.interval;
            } else {
                next_wake = x.interval - diff.count();
            }
            if (next_wake < interval)
                interval = next_wake;
        }
        if (interval != 0) {
#ifdef pigpio_FOUND
            pigpio_stop(_pigpio_handle);  // consider whether we need to close connection every time?
            _pigpio_handle = -1;
#endif
            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }
        _logger->flush();
    }
}

void DHT_Service::read_sensor(MyConfig &c)  noexcept {
    float humidity_[3], temperature_[3];
    bool got_measure[3] = {false, false, false};
    //_logger->debug("Read sensor {}", c.pin);
    // prepare GPIO
#ifdef pigpio_FOUND
    set_pull_up_down(_pigpio_handle, c.pin, PI_PUD_OFF);
    set_pull_up_down(_pigpio_handle, c.pin, PI_PUD_DOWN);
    set_noise_filter(_pigpio_handle, c.pin, 0, 0);
#else
#endif
    // we actually do 3 measures and calculate the average
    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 10; i++) {
            if (0 != read_sensor_data(c.pin, humidity_[j], temperature_[j])) {
                _logger->trace("Error reading sensor {}", c.pin);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            if (humidity_[j] > 100.f ||  humidity_[j] < 0.f) {
                _logger->trace("Humidity out of bounds: {}", humidity_[j]);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            if (temperature_[j] > 55.f || temperature_[j] < -30.f) {
                _logger->trace("Temperature out of bounds: {}", temperature_[j]);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            // all is ok finally
            got_measure[j] = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1)); // wait 1 sec be4 next measure
    }
    
    if (!got_measure[0] && !got_measure[1] && !got_measure[2]) {
        _logger->warn("Unable to read sensor: {} at all", c.pin);
        return;
    }

    // calculate the average
    double divisor = (got_measure[0] ? 1.0 : 0.0) + (got_measure[1] ? 1.0 : 0.0) + (got_measure[2] ? 1.0 : 0.0);
    double humidity = ((got_measure[0] ? humidity_[0] : 0.0) + (got_measure[1] ? humidity_[1] : 0.0) + (got_measure[2] ? humidity_[2] : 0.0)) / divisor;
    double temperature = ((got_measure[0] ? temperature_[0] : 0.0) + (got_measure[1] ? temperature_[1] : 0.0) + (got_measure[2] ? temperature_[2] : 0.0)) / divisor;
    // Dew point equation " Arden Buck equation"
    // https://en.wikipedia.org/wiki/Dew_point
    // using close approximation with error around 1%
    double dew_point_LN = std::log((humidity / 100.0) * std::exp((17.62 - (temperature / 243.5))*(temperature / 243.12)));
    double dew_point = (243.12 * dew_point_LN) / (17.62 - dew_point_LN);
    // parse result into JSON string
    // format (JSON): array [ dict {id_string : double }, dict {id_string : double } ]
    // example [ { "temperature" : 21.3} , { "humidity" : 53 } ] */

    struct json_object* j = json_object_new_object();
    //temperature
    {
        struct json_object* arr = json_object_new_array();
        json_object_array_add(arr, json_object_new_double(temperature));
        json_object_array_add(arr, json_object_new_string("°C"));
        json_object_object_add(j, "Temperature", arr);
    }
    //humidity
    {
        struct json_object* arr = json_object_new_array();
        json_object_array_add(arr, json_object_new_double(humidity));
        json_object_array_add(arr, json_object_new_string("%"));
        json_object_object_add(j, "RH", arr);
    }
    //dew_point
    {
        struct json_object* arr = json_object_new_array();
        json_object_array_add(arr, json_object_new_double(dew_point));
        json_object_array_add(arr, json_object_new_string("°C"));
        json_object_object_add(j, "Dew-point", arr);
    }

    //format JSON string
    std::string json_string = json_object_to_json_string(j);
    Publish(c.name, json_string);  // send JSON message with our measures to MQTT broker
    json_object_put(j); //free the object - is this enough? TODO check memory consumption after few days running (basically it seems to be safe).
}



#ifdef pigpio_FOUND

static void callback_rise_func(int pi __attribute__((unused)), unsigned user_gpio __attribute__((unused)), unsigned level, uint32_t tick, void * user) noexcept {
    MyData *T = (MyData*) user;
    uint32_t time_dif = tick - T->time;
    if (time_dif > std::numeric_limits<int32_t>::max()) //FIX for clock wrap-around (once per hour & 12 minutes) - does it "really" work?
          time_dif = tick + (std::numeric_limits<uint32_t>::max() - T->time);
    T->time = tick;
    if (level == 0 && time_dif > 15) {
        ++T->bit_counter;
        T->bits <<= 1;
        if (time_dif  > 60)
            T->bits |= 1;
    }
}

int DHT_Service::read_sensor_data(int id, float& humidity, float& temperature)  noexcept {
    MyData T;
    T.time = get_current_tick(_pigpio_handle);
    T.bit_counter = 0;
    T.bits = 0;
    decltype(std::chrono::steady_clock::now()) begin;

    check_result(set_mode(_pigpio_handle, id, PI_OUTPUT));    // first of all send DHT pulse
    check_result(gpio_write(_pigpio_handle, id, 0));          // write 0
    begin = std::chrono::steady_clock::now();
    int callback_id = callback_ex(_pigpio_handle, id, EITHER_EDGE, callback_rise_func, &T);
    time_sleep(0.018);                                        // wait at least 1ms (we wait ~18ms)
    check_result(gpio_write(_pigpio_handle, id, 1));          // write 1
    check_result(set_mode(_pigpio_handle, id, PI_INPUT));     // set it to read mode (let DHT communicate)
    for (uint64_t time_dif = 0; time_dif < 8000; ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // let it communicate some time it may actually get woke up sooner so we control time if it's at least ~8ms
        auto now = std::chrono::steady_clock::now();
        time_dif = (now - begin).count();
    }
    callback_cancel(callback_id);

    if (T.bit_counter < 40) {
        _logger->trace("Error - not enough bits providied by sensor!");
        return -2;
    }

    // now process bits sensor provided us; well bit work could be much more... simple but this is enough I guess...
    T.bits &= 0xFFFFFFFFFF; // discard additional bits
    uint8_t data[5];
    data[4] = T.bits & 0xFF;
    T.bits >>= 8;
    data[3] = T.bits & 0xFF;
    T.bits >>= 8;
    data[2] = T.bits & 0xFF;
    T.bits >>= 8;
    data[1] = T.bits & 0xFF;
    T.bits >>= 8;
    data[0] = T.bits & 0xFF;

    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        _logger->trace("Data CRC failed");
        return -1;
    } else {
        humidity = (((uint16_t) data[0]) << 8 | data[1]) / 10.f;
        temperature = ((((uint16_t)(data[2] & 0x7F)) << 8) | data[3]) / 10.f;
        if (data[2] & 0x80) temperature = -temperature;
        _logger->debug("Humidity = {}% | Temperature = {} *C", humidity, temperature );
        return 0;
    }
}

void DHT_Service::check_result(int result) {
    switch (result) {
        case PI_BAD_GPIO:
            _logger->warn("Error bad GPIO");
            break;
        case PI_BAD_MODE:
            _logger->warn("Error bad mode");
            break;
        case PI_NOT_PERMITTED:
            _logger->warn("Error not permitted: dhtdaemon is missing rights to access GPIO?");
            break;
        case 0:
            break;
        default:
            _logger->warn("Unknown error");
            break;
    }
}

DHT_Service::~DHT_Service() noexcept {
    if (_pigpio_handle > 0) {
        pigpio_stop(_pigpio_handle);	// consider whether we need to close connection every time?
        _pigpio_handle = -1;
    }
}
#else /* gpioxx */

int DHT_Service::read_sensor_data(int id, float& humidity, float& temperature) noexcept {
    std::vector<std::pair<decltype(std::chrono::steady_clock::now()), decltype(_chip->get_value(id))>> pairs;
    static constexpr decltype(std::chrono::steady_clock::now().time_since_epoch().count()) kMaxTime = 200 + 40 * 130 + 500; // start (200ms) + 40 (ms) * 1-bit (130ms) + reserve (500ms)

    _chip->set_value(id, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(18));
    const auto begin = std::chrono::steady_clock::now();
    pairs.reserve(1500);
    pairs.emplace_back(begin, true);

    _chip->set_value(id, 1);
    // I really wanted to use event interface of GPIO, but setting pin from "output" to "input watching events" took on my RPi3 ~450us. 
    // Since I have ~200us before sensor sends required data to input... I was loosing 2-3 bits of data; thus I was
    // forced to use following approach... it may not work well if the system is under heavy load..
    for (int i = 0; i < 20000; ++i) {
        const auto now = std::chrono::steady_clock::now();
        pairs.emplace_back(now, _chip->get_value(id)); // first value request gonna request change of pin direction, that might take some time.~100 - 200us. Rest is taking 3-10us so the accuracy is sufficent. 
        if (((now - begin).count() / 1000) > kMaxTime)
            break;
    }
    _chip->set_value(id, 1);
    _chip->reset(id);   // free resources including kernel ones...
    
    // well start is like this (we set 1 and that let the device input data):
    //   --- (20-40us) ---            --- (80us) ---
    //                    |           |             |
    //                    |           |
    //                    ---(80us)---
    //  ~180-200us in total (if we do not catch the first falling edge)

    // 0 is
    //               --- (26-28us) ---
    //               |                |
    //   |           |
    //   ---(50us)---
    //  ~ 76-78us in total


    // 1 is
    //               --- (70us) ---
    //               |             |
    //  |            |
    //   ---(50us)---
    // ~ 120us in total

    std::vector<decltype(std::chrono::steady_clock::now())> edges;
    for (decltype(pairs.size()) i = 1; i < pairs.size(); ++i) {
        if (pairs[i-1].second && !pairs[i].second) {  // falling-edge detected
            edges.emplace_back(pairs[i-1].first);
        }
    }
    uint64_t bits = 0;
    int bit_counter = 0;
    _logger->debug("Detected {} edges",  edges.size());
    if (!edges.size()) {
        _logger->debug("No pulse from DHT detected");  // this may happen if system is under heavy load... if your system is under heavy load all the time this daemon may not work at all 
                                                       // so you might need to set higher priority (nice) or if your kernel support real-time sheduling you might set it's rt priority temporary...
        return -1;
    }
    auto last_edge_time = *edges.begin();
    for (auto&& edge : edges) {
        const auto interval = (edge - last_edge_time).count() / 1000;
        _logger->debug("Falling adge - Time {} us", interval);
        last_edge_time = edge;
        if (interval != 0) {
            if (interval > 160) {
                if (bit_counter) {
                    _logger->debug("Detected long pulse interval that is not on the start! length {} position {}", interval, bit_counter);   
                    break;
                }
                // otherwise ignore that pulse (it is initial pulse)
            } else {
                ++bit_counter;
                bits <<= 1;
                if (interval >= 105 )
                    bits |= 1;
            }
        }
    }
    if (bit_counter < 40) {
        _logger->debug("Got less than 40 bits from sensor! {}", bit_counter);
        return -1;
    }
    _logger->debug("Got {} bits", bit_counter);
    // now process bits sensor provided us; well bit work could be much more... simple but this is enough I guess...
    bits &= 0xFFFFFFFFFF; // discard additional bits
    uint8_t data[5];
    data[4] = bits & 0xFF;
    bits >>= 8;
    data[3] = bits & 0xFF;
    bits >>= 8;
    data[2] = bits & 0xFF;
    bits >>= 8;
    data[1] = bits & 0xFF;
    bits >>= 8;
    data[0] = bits & 0xFF;

    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        _logger->debug("Data CRC failed");
        return -1;
    } else {
        humidity = (((uint16_t) data[0]) << 8 | data[1]) / 10.f;
        temperature = ((((uint16_t)(data[2] & 0x7F)) << 8) | data[3]) / 10.f;
        if (data[2] & 0x80) temperature = -temperature;
        _logger->debug("Humidity = {}% | Temperature = {} *C", humidity, temperature );
        return 0;
    }
}

DHT_Service::~DHT_Service() noexcept {
    _logger->debug("~DHT_Service()");
}
#endif /* pigpio_FOUND */

int main() {
    try {
        DHT_Service d;  //explicit destruction on signal is possible but it does not delete pointer now...!
        d.main();
    } catch (const std::runtime_error& error) {
        return -1;
    }
    return 0;
}

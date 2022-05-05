// Copyright: (c) Jaromir Veber 2019
// Version: 10122019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// gpiocxx controlls GPIO defice with easy-to-use interface and efficent internal overhead

#include <gpioxx.hpp>

#include <type_traits>  //for std::underlying_type
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <linux/gpio.h>

gpiocxx::gpiocxx(const std::string& path, const std::shared_ptr<spdlog::logger>& logger): _logger(logger), _chip_fd(-1) {
    auto fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        _logger->error("GPIOcxx unable to open chip device {} : {}", path, strerror(errno));
        throw std::runtime_error("");
    }
    struct stat statbuf;
    int rv = lstat(path.c_str(), &statbuf);
    if (rv < 0) {
        _logger->error("GPIOcxx unable to stat chip device {}: {}", path, strerror(errno));
        close(fd);
        throw std::runtime_error("");
    }
    if (!S_ISCHR(statbuf.st_mode)) {
        close(fd);
        _logger->error("GPIOcxx chip device {} if not char device!", path);
        throw std::runtime_error("");
    }
    // TODO better check
    struct gpiochip_info info;
    rv = ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info);
    if (rv < 0) {
        _logger->error("GPIOcxx unable to get chip info {}: {}", path, strerror(errno));
        close(fd);
        throw std::runtime_error("");
    }
    _chip_fd = fd;
    _lines.resize(info.lines, nullptr);
}

gpiocxx::~gpiocxx() {
    for (decltype(_lines.size()) i = 0; i < _lines.size(); ++i)
        reset(i);
    close(_chip_fd);
}

inline void gpiocxx::check_offset(uint32_t gpio) {
    if (_lines.size() <= gpio) {
        _logger->error("GPIOcxx GPIO {} out of possible offsets (max {})", gpio, _lines.size());
        throw std::runtime_error("");
    }
}

bool gpiocxx::is_output(uint32_t gpio) {
    check_offset(gpio);
    if (_lines[gpio] == nullptr || _lines[gpio]->event)
        return false;
    if (_lines[gpio]->handle_fd == -1 || _lines[gpio]->input)
        return false;
    return true;
}

bool gpiocxx::is_input(uint32_t gpio) {
    check_offset(gpio);
    if (_lines[gpio] == nullptr || _lines[gpio]->event)
        return false;
    if (_lines[gpio]->handle_fd == -1 || !_lines[gpio]->input)
        return false;
    return true;
}

void gpiocxx::request(uint32_t gpio, bool input) {
    if (_lines[gpio] != nullptr) {
        if (_lines[gpio]->handle_fd != -1) {
            close(_lines[gpio]->handle_fd);
            _lines[gpio]->handle_fd = -1;
        }
        if (_lines[gpio]->event) {
            if (_lines[gpio]->handle_fd != -1) {
                close(_lines[gpio]->handle_fd);
                _lines[gpio]->handle_fd = -1;
            }
            _lines[gpio]->event = false;
        }
    }
    struct gpiohandle_request req = {};
    if (input)
        req.flags |= GPIOHANDLE_REQUEST_INPUT;
    else
        req.flags |= GPIOHANDLE_REQUEST_OUTPUT;
    req.lines = 1;
    req.lineoffsets[0] = gpio;
    req.default_values[0] = 1;
    strncpy(req.consumer_label, "mq_dht_daemon", sizeof(req.consumer_label) - 1);
    int rv = ioctl(_chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (rv < 0) {
        _logger->error("GPIOcxx unable to request line {}: {}", gpio, strerror(errno));
        throw std::runtime_error("");
    }
    if (_lines[gpio] == nullptr)
        _lines[gpio] = new line();
    _lines[gpio]->handle_fd = req.fd;
    _lines[gpio]->input = input;
}

void gpiocxx::set_input(uint32_t gpio) {
    if (is_input(gpio))
        return;
    if (_lines[gpio] == nullptr)
        _lines[gpio] = new line();
    request(gpio, true);
}

void gpiocxx::set_output(uint32_t gpio) {
    if (is_output(gpio))
        return;    
    if (_lines[gpio] == nullptr)
        _lines[gpio] = new line();
    request(gpio, false);
}

bool gpiocxx::get_value(uint32_t gpio) {
    if (!is_input(gpio))
        set_input(gpio);
    struct gpiohandle_data data = {};
    set_input(gpio);
    int rv = ioctl(_lines[gpio]->handle_fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
    if (rv < 0) {
        _logger->error("GPIOcxx unable to get value on line {}: {}", gpio, strerror(errno));
        throw std::runtime_error("");
    }
    return data.values[0]; 
}

void gpiocxx::set_value(uint32_t gpio, bool value) {
    if (!is_output(gpio))
        set_output(gpio);
    struct gpiohandle_data data = {};
    data.values[0] = value;
    set_output(gpio);
    int rv = ioctl(_lines[gpio]->handle_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
    if (rv < 0) {
        _logger->error("GPIOcxx unable to set value on line {}: {}", gpio, strerror(errno));
        throw std::runtime_error("");
    }
}

void gpiocxx::reset(uint32_t gpio) {
    check_offset(gpio);
    if (_lines[gpio] != nullptr) {
        if (_lines[gpio]->handle_fd != -1)
            close(_lines[gpio]->handle_fd);
        delete _lines[gpio];
        _lines[gpio] = nullptr;
    }
}

void gpiocxx::watch_event(uint32_t gpio, event_req event) {
    struct gpioevent_request req = {};
    gpiocxx::reset(gpio);   // free handle if it's already allocated
    _lines[gpio] = _lines[gpio] = new line();  //TODO - not exactly resolved.. ideal way
    strncpy(req.consumer_label, "mq_dht_daemon", sizeof(req.consumer_label) - 1);
    req.lineoffset = gpio;
    req.handleflags |= GPIOHANDLE_REQUEST_INPUT;
    switch(event) {
        case event_req::BOTH_EDGES:
            req.eventflags = GPIOEVENT_REQUEST_FALLING_EDGE | GPIOEVENT_REQUEST_RISING_EDGE;
            break;
        case event_req::RISING_EDGE:
            req.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
            break;
        case event_req::FALLING_EDGE:
            req.eventflags = GPIOEVENT_REQUEST_FALLING_EDGE;
            break;
    }
    auto rv = ioctl(_chip_fd, GPIO_GET_LINEEVENT_IOCTL, &req);
    if (rv < 0) {
        _logger->error("GPIOcxx unable to watch events on line {}: {}", gpio, strerror(errno));
        throw std::runtime_error("");
    }
    _lines[gpio]->handle_fd = req.fd;
    _lines[gpio]->event = true;
}

int gpiocxx::wait_event(uint32_t gpio, const struct timespec *timeout) {
    struct pollfd fds[1] = {};
    check_offset(gpio);
    if (_lines[gpio] == nullptr || !(_lines[gpio]->event)) {
        _logger->error("GPIOcxx wait for event but events not requested (watched) on line {}", gpio);
        throw std::runtime_error("");
    }
    fds[0].fd = _lines[gpio]->handle_fd;
    fds[0].events = POLLIN | POLLPRI;
    int rv = ppoll(fds, 1, timeout, NULL);
    if (rv < 0) {
        _logger->error("GPIOcxx unable to pool for events on line {}: {}", gpio, strerror(errno));
        throw std::runtime_error("");
    } else if (rv == 0) {
        return 0;
    }
    return 1;
}

std::vector<uint32_t> gpiocxx::wait_events(const std::vector<uint32_t>& l, const struct timespec *timeout) {
    std::vector<struct pollfd> fds;
    for (auto it = l.cbegin(); it != l.cend(); ++it) {
        check_offset(*it);
        if (_lines[*it] == nullptr || !(_lines[*it]->event)) {
            _logger->error("GPIOcxx wait for event but events not requested (watched) on line {}", *it);
            throw std::runtime_error("");
        }
        struct pollfd fd = {};
        fd.fd = _lines[*it]->handle_fd;
        fd.events = POLLIN | POLLPRI;
        fds.emplace_back(fd);
    }
    std::vector<uint32_t> result;
    int rv = ppoll(fds.data(), fds.size(), timeout, NULL);
    if (rv < 0) {
        _logger->error("GPIOcxx unable to pool for events on lines: {}", strerror(errno));
        throw std::runtime_error("");
    } else if (rv == 0) {
        return result;
    }
    for(decltype(fds.size()) i = 0; i < fds.size(); ++i) {
        if (fds[i].revents) {
            result.emplace_back(l[i]);
            if (!--rv)
                break;
        }
    }
    return result;
}

void gpiocxx::read_event(uint32_t gpio, struct gpioevent_data& evdata) {
    check_offset(gpio);
    if (_lines[gpio] != nullptr || !_lines[gpio]->event) {
        _logger->error("GPIOcxx wait for event but events not requested (watched) on line {}", gpio);
        throw std::runtime_error("");
    }
    evdata = {};
    ssize_t rd = read(_lines[gpio]->handle_fd, &evdata, sizeof(evdata));
    if (rd < 0) {
        _logger->error("GPIOcxx unable to read event on line {}: {}", gpio, strerror(errno));
        throw std::runtime_error("");
    } else if (rd != sizeof(evdata)) {
        _logger->error("GPIOcxx unable to read event on line {}: {}", gpio, strerror(errno));
        throw std::runtime_error("");
    }
}
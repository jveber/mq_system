// Copyright: (c) Jaromir Veber 2019
// Version: 10052019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
//

#include <sys/stat.h>
#include <unistd.h>

int main () {
    const char* gpio0 = "/dev/gpiochip0";
    const char* gpio1 = "/dev/gpiochip1";

    struct stat statbuf;
    auto result = stat(gpio0, &statbuf);
    if (result < 0) {
        auto result = stat(gpio1, &statbuf);
        if (result < 0){
            return 0;
        }
    }
    return 1;
}
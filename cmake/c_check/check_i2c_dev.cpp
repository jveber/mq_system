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
    const char* i2c0 = "/dev/i2c-0";
    const char* i2c1 = "/dev/i2c-1";

    struct stat statbuf;
    auto result = stat(i2c0, &statbuf);
    if (result < 0) {
        auto result = stat(i2c1, &statbuf);
        if (result < 0){
            return 0;
        }
    }
    return 1;
}
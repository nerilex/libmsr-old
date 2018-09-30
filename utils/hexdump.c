/* hexdump.c */
/*
    This file is part of libmsr.
    Copyright (C) 2018 bg nerilex (bg@nerilex.org)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>

void hexdumpf(FILE *f, const void *data, size_t length) {
    uint8_t b;
    const uint8_t *d = (const uint8_t*)data;
    size_t x, j;
    for (x = 0; x < length; x++) {
            b = d[x];
            if (x % 16 == 0) {
                printf("\n    <%04x>: ", x);
            }
            printf("%02x ", b);
            if ((x + 1) % 16 == 0) {
                printf("   ");
                for (j = x - 15; j <= x; ++j) {
                    b = d[j];
                    printf("%c", isprint(b) ? b : '.');
                }
            }
    }
    if (length > 0) {
        for(; (x % 16) != 0; ++x) {
            printf("   ");
        }
        printf("   ");
        for (j = x - 15; j <= x; ++j) {
            b = d[j];
            printf("%c", isprint(b) ? b : '.');
        }
    }
}

void hexdump(const void *data, size_t length) {
    hexdumpf(stdout, data, length);
}

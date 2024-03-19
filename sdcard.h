/*
 * sdcard.h
 *
 *  Created on: 2024/3/19/
 *      Author: ch.wang
 */

#ifndef SDCARD_H_
#define SDCARD_H_
#include "ff.h"

#define MAX_READ_BUF_LEN 4096

void fatfs_sdcard_init(void);
FIL* sdcard_get_wfile(void);
void sdcard_ends(void);
void dmesg(FRESULT fres);
FRESULT sdcard_write(const void* data, const unsigned int len);

#endif /* SDCARD_H_ */

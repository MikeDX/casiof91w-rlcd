#pragma once

#include <time.h>

void f91w_rtc_init(void);
void f91w_rtc_read_to_system(void);
void f91w_rtc_write_from_system(void);
struct tm f91w_rtc_read(void);
bool f91w_rtc_present(void);

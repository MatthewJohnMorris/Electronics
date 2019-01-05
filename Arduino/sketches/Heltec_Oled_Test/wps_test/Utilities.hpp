#ifndef IN_UTILITIES_HPP
#define IN_UTILITIES_HPP

void u8x8_init();
void u8x8_writeRow(int row, const char* msg);

const char* descForStatusCode(int status);
void showConnectionStatus(int status);

void led_init();
void led_setColour(int colour);

void wps_init();
void wps_disableAndReEnable();

#endif // IN_UTILITIES_HPP

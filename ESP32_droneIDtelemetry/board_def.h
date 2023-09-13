#ifndef BOARD_DEF_H
#define BOARD_DEF_H


#define ENABLE_SERIAL


//SERIAL
#ifdef ENABLE_SERIAL
#include <TinyGPS++.h>
#define UBLOX_GPS_OBJECT()  TinyGPSPlus gps
#define GPS_BAUD_RATE 115200

#define GPS_RX_PIN 16 //yellow
#define GPS_TX_PIN 17 //green
#else
UBLOX_GPS_OBJECT()
#endif


#endif /*BOARD_DEF_H*/

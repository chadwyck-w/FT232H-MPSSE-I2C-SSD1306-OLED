#include <stdio.h>
//#include <stdbool.h>
#include "ftd2xx.h"

// I2C FUNCTIONS

// ####################################################################################################################
// Function to read 1 byte from the I2C slave
//     Clock in one byte from the I2C Slave which is the actual data to be read
//     Clock out one bit to the I2C Slave which is the ACK/NAK bit
//     Put lines back to the idle state (idle between start and stop is clock low, data high (open-drain)
// This function reads only one byte from the I2C Slave. It therefore sends a '1' as the ACK/NAK bit. This is NAKing 
// the first byte of data, to tell the slave we dont want to read any more bytes. 
// The one byte of data read from the I2C Slave is put into ByteDataRead[0]
// ####################################################################################################################

BOOL ReadByteAndSendNAK(void);

// ##############################################################################################################
// Function to write 1 byte, and check if it returns an ACK or NACK by clocking in one bit
//     We clock one byte out to the I2C Slave
//     We then clock in one bit from the Slave which is the ACK/NAK bit
//     Put lines back to the idle state (idle between start and stop is clock low, data high (open-drain)
// Returns TRUE if the write was ACKed
// ##############################################################################################################

BOOL SendByteAndCheckACK(BYTE dwDataSend);

void SendByte(BYTE dwDataSend);

// ##############################################################################################################
// Function to write 1 byte, and check if it returns an ACK or NACK by clocking in one bit
// This function combines the data and the Read/Write bit to make a single 8-bit value
//     We clock one byte out to the I2C Slave
//     We then clock in one bit from the Slave which is the ACK/NAK bit
//     Put lines back to the idle state (idle between start and stop is clock low, data high (open-drain)
// Returns TRUE if the write was ACKed by the slave
// ##############################################################################################################

BOOL SendAddrAndCheckACK(BYTE dwDataSend, BOOL Read);

void SendAddr(BYTE dwDataSend);

// ##############################################################################################################
// Function to set all lines to idle states
// For I2C lines, it releases the I2C clock and data lines to be pulled high externally
// For the remainder of port AD, it sets AD3/4/5/6/7 as inputs as they are unused in this application
// For the LED control, it sets AC6 as an output with initial state high (LED off)
// For the remainder of port AC, it sets AC0/1/2/3/4/5/7 as inputs as they are unused in this application
// ##############################################################################################################

void SetI2CLinesIdle(void);

// ##############################################################################################################
// Function to set the I2C Start state on the I2C clock and data lines
// It pulls the data line low and then pulls the clock line low to produce the start condition
// It also sends a GPIO command to set bit 6 of ACbus low to turn on the LED. This acts as an activity indicator
// Turns on (low) during the I2C Start and off (high) during the I2C stop condition, giving a short blink.  
// ##############################################################################################################

void SetI2CStart(void);

// ##############################################################################################################
// Function to set the I2C Stop state on the I2C clock and data lines
// It takes the clock line high whilst keeping data low, and then takes both lines high
// It also sends a GPIO command to set bit 6 of ACbus high to turn off the LED. This acts as an activity indicator
// Turns on (low) during the I2C Start and off (high) during the I2C stop condition, giving a short blink.  
// ##############################################################################################################

void SetI2CStop(void);

// OPENING DEVICE AND MPSSE CONFIGURATION

void setupMPSSE(char *deviceName);

void scanDevicesAndQuit(void);

void shutdownFTDI(void);

//#include <stdio.h>
#include <stdlib.h>
#include "i2c_lib.h"
//#include "ftd2xx.h"

// #########################################################################################
// FT232H I2C BUS SCAN
//
// Scan the I2C bus of an FT232H and display the address of any devices found
// It simply calls the address and either gets an ACK or doesn't. 
// Requires FTDI D2XX drivers
// ######################################################################################### 

// #include <stdio.h>
// #include "ftd2xx.h"

    int bCommandEchod = 0;

    FT_STATUS ftStatus;         // Status defined in D2XX to indicate operation result
    FT_HANDLE ftHandle;         // Handle of FT232H device port 

    BYTE OutputBuffer[1024];        // Buffer to hold MPSSE commands and data to be sent to FT232H
    BYTE InputBuffer[1024];         // Buffer to hold Data bytes read from FT232H
    
    //DWORD dwClockDivisor = 0x32;
    //faster one crashes device or something
    DWORD dwClockDivisor = 0xC8;      // 100khz -- 60mhz / ((1+dwClockDivisor)*2) = clock speed in mhz
    //DWORD dwClockDivisor = 0x012B;
    
    DWORD dwNumBytesToSend = 0;         // Counter used to hold number of bytes to be sent
    DWORD dwNumBytesSent = 0;       // Holds number of bytes actually sent (returned by the read function)

    DWORD dwNumInputBuffer = 0;     // Number of bytes which we want to read
    DWORD dwNumBytesRead = 0;       // Number of bytes actually read
    DWORD ReadTimeoutCounter = 0;       // Used as a software timeout counter when the code checks the Queue Status
    DWORD dwCount = 0;

    BYTE ByteDataRead[15];          // Array for storing the data which was read from the I2C Slave
    BOOL DataInBuffer  = 0;         // Flag which code sets when the GetNumBytesAvailable returned is > 0 
    BYTE DataByte = 0;          // Used to store data bytes read from and written to the I2C Slave

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

BOOL ReadByteAndSendNAK(void)
{
    dwNumBytesToSend = 0;                           // Clear output buffer
    
    // Clock one byte of data in...
    OutputBuffer[dwNumBytesToSend++] = 0x20;        // Command to clock data byte in on the clock rising edge
    OutputBuffer[dwNumBytesToSend++] = 0x00;        // Length (low)
    OutputBuffer[dwNumBytesToSend++] = 0x00;        // Length (high)   Length 0x0000 means clock ONE byte in 

    // Now clock out one bit (the ACK/NAK bit). This bit has value '1' to send a NAK to the I2C Slave
    OutputBuffer[dwNumBytesToSend++] = 0x13;        // Command to clock data bits out on clock falling edge
    OutputBuffer[dwNumBytesToSend++] = 0x00;        // Length of 0x00 means clock out ONE bit
    OutputBuffer[dwNumBytesToSend++] = 0xFF;        // Command will send bit 7 of this byte, we send a '1' here

    // Put I2C line back to idle (during transfer) state... Clock line driven low, Data line high (open drain)
    OutputBuffer[dwNumBytesToSend++] = 0x80;        // Command to set lower 8 bits of port (ADbus 0-7 on the FT232H)
    OutputBuffer[dwNumBytesToSend++] = 0x1A;        // gpo1, rst, and data high 00111011 
    OutputBuffer[dwNumBytesToSend++] = 0x3B;        // 00111011
    
    // AD0 (SCL) is output driven low
    // AD1 (DATA OUT) is output high (open drain)
    // AD2 (DATA IN) is input (therefore the output value specified is ignored)
    // AD3 to AD7 are inputs driven high (not used in this application)

    // This command then tells the MPSSE to send any results gathered back immediately
    OutputBuffer[dwNumBytesToSend++] = 0x87;        // Send answer back immediate command

    ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);     // Send off the commands to the FT232H

    // ===============================================================
    // Now wait for the byte which we read to come back to the host PC
    // ===============================================================

    dwNumInputBuffer = 0;
    ReadTimeoutCounter = 0;

    ftStatus = FT_GetQueueStatus(ftHandle, &dwNumInputBuffer);  // Get number of bytes in the input buffer

    while ((dwNumInputBuffer < 1) && (ftStatus == FT_OK) && (ReadTimeoutCounter < 500))
    {
        // Sit in this loop until
        // (1) we receive the one byte expected
        // or (2) a hardware error occurs causing the GetQueueStatus to return an error code
        // or (3) we have checked 500 times and the expected byte is not coming 
        ftStatus = FT_GetQueueStatus(ftHandle, &dwNumInputBuffer);  // Get number of bytes in the input buffer
        ReadTimeoutCounter ++;
    }

    // If the loop above exited due to the byte coming back (not an error code and not a timeout)
    // then read the byte available and return True to indicate success
    if ((ftStatus == FT_OK) && (ReadTimeoutCounter < 500))
    {
        ftStatus = FT_Read(ftHandle, &InputBuffer, dwNumInputBuffer, &dwNumBytesRead); // Now read the data
        ByteDataRead[0] = InputBuffer[0];               // return the data read in the global array ByteDataRead
        return TRUE;                            // Indicate success
    }
    else
    {
        return FALSE;                           // Failed to get any data back or got an error code back
    }
}

// ##############################################################################################################
// Function to write 1 byte, and check if it returns an ACK or NACK by clocking in one bit
//     We clock one byte out to the I2C Slave
//     We then clock in one bit from the Slave which is the ACK/NAK bit
//     Put lines back to the idle state (idle between start and stop is clock low, data high (open-drain)
// Returns TRUE if the write was ACKed
// ##############################################################################################################

BOOL SendByteAndCheckACK(BYTE dwDataSend)
{
    dwNumBytesToSend = 0;           // Clear output buffer
    FT_STATUS ftStatus = FT_OK;

    OutputBuffer[dwNumBytesToSend++] = 0x11;        // command to clock data bytes out MSB first on clock falling edge
    OutputBuffer[dwNumBytesToSend++] = 0x00;        // 
    OutputBuffer[dwNumBytesToSend++] = 0x00;        // Data length of 0x0000 means 1 byte data to clock out
    OutputBuffer[dwNumBytesToSend++] = dwDataSend;  // Actual byte to clock out

    // Put I2C line back to idle (during transfer) state... Clock line driven low, Data line high (open drain)
    OutputBuffer[dwNumBytesToSend++] = 0x80;        // Command to set lower 8 bits of port (ADbus 0-7 on the FT232H)
    OutputBuffer[dwNumBytesToSend++] = 0x1A;        // Set the value of the pins (only affects those set as output)
    OutputBuffer[dwNumBytesToSend++] = 0x3B;        // Set the directions - all pins as output except Bit2(data_in)
    
    // AD0 (SCL) is output driven low
    // AD1 (DATA OUT) is output high (open drain)
    // AD2 (DATA IN) is input (therefore the output value specified is ignored)
    // AD3 to AD7 are inputs driven high (not used in this application)

    OutputBuffer[dwNumBytesToSend++] = 0x22;    // Command to clock in bits MSB first on clock rising edge
    OutputBuffer[dwNumBytesToSend++] = 0x00;    // Length of 0x00 means to scan in 1 bit

    // This command then tells the MPSSE to send any results gathered back immediately
    OutputBuffer[dwNumBytesToSend++] = 0x87;    //Send answer back immediate command

    ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);     //Send off the commands
    //printf("Byte Sent, waiting for ACK...\n");    
    // ===============================================================
    // Now wait for the byte which we read to come back to the host PC
    // ===============================================================

    dwNumInputBuffer = 0;
    ReadTimeoutCounter = 0;

    ftStatus = FT_GetQueueStatus(ftHandle, &dwNumInputBuffer);  // Get number of bytes in the input buffer

    while ((dwNumInputBuffer < 1) && (ftStatus == FT_OK) && (ReadTimeoutCounter < 5500))
    {
        // Sit in this loop until
        // (1) we receive the one byte expected
        // or (2) a hardware error occurs causing the GetQueueStatus to return an error code
        // or (3) we have checked 500 times and the expected byte is not coming 
        ftStatus = FT_GetQueueStatus(ftHandle, &dwNumInputBuffer);  // Get number of bytes in the input buffer
        //printf("counter: %d, bytes: %d, ftStatus: %d\n", ReadTimeoutCounter, dwNumInputBuffer, ftStatus);
        ReadTimeoutCounter ++;
    }

    // If the loop above exited due to the byte coming back (not an error code and not a timeout)

    if ((ftStatus == FT_OK) && (ReadTimeoutCounter < 2500))
    {
        ftStatus = FT_Read(ftHandle, &InputBuffer, dwNumInputBuffer, &dwNumBytesRead); // Now read the data
        //printf("status was %d, input was 0x%X\n", ftStatus, InputBuffer[0]);  
        if (((InputBuffer[0] & 0x01) == 0x0))       //Check ACK bit 0 on data byte read out
        {   
            //printf("received ACK.\n");
            return TRUE;    // Return True if the ACK was received
        }
        else
            printf("Failed to get ACK from I2C Slave when sending %X\n Received %X\n",dwDataSend,InputBuffer[0]);
            return FALSE; //Error, can't get the ACK bit 
        }
    else
    {
        printf("Error: %d status\n", ftStatus);
        return FALSE;   // Failed to get any data back or got an error code back
    }

}

//my function
void SendByte(BYTE dwDataSend)
{
    dwNumBytesToSend = 0;           // Clear output buffer
    FT_STATUS ftStatus = FT_OK;

    OutputBuffer[dwNumBytesToSend++] = 0x11;        // command to clock data bytes out MSB first on clock falling edge
    OutputBuffer[dwNumBytesToSend++] = 0x00;        // 
    OutputBuffer[dwNumBytesToSend++] = 0x00;        // Data length of 0x0000 means 1 byte data to clock out
    OutputBuffer[dwNumBytesToSend++] = dwDataSend;  // Actual byte to clock out

    // Put I2C line back to idle (during transfer) state... Clock line driven low, Data line high (open drain)
    OutputBuffer[dwNumBytesToSend++] = 0x80;        // Command to set lower 8 bits of port (ADbus 0-7 on the FT232H)
    OutputBuffer[dwNumBytesToSend++] = 0x1A;        // Set the value of the pins (only affects those set as output)
    OutputBuffer[dwNumBytesToSend++] = 0x3B;        // Set the directions - all pins as output except Bit2(data_in)
    
    // AD0 (SCL) is output driven low
    // AD1 (DATA OUT) is output high (open drain)
    // AD2 (DATA IN) is input (therefore the output value specified is ignored)
    // AD3 to AD7 are inputs driven high (not used in this application)

    OutputBuffer[dwNumBytesToSend++] = 0x22;    // Command to clock in bits MSB first on clock rising edge
    OutputBuffer[dwNumBytesToSend++] = 0x00;    // Length of 0x00 means to scan in 1 bit

    // This command then tells the MPSSE to send any results gathered back immediately
    OutputBuffer[dwNumBytesToSend++] = 0x87;    //Send answer back immediate command

    ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);     //Send off the commands

}



// ##############################################################################################################
// Function to write 1 byte, and check if it returns an ACK or NACK by clocking in one bit
// This function combines the data and the Read/Write bit to make a single 8-bit value
//     We clock one byte out to the I2C Slave
//     We then clock in one bit from the Slave which is the ACK/NAK bit
//     Put lines back to the idle state (idle between start and stop is clock low, data high (open-drain)
// Returns TRUE if the write was ACKed by the slave
// ##############################################################################################################

BOOL SendAddrAndCheckACK(BYTE dwDataSend, BOOL Read)
{
    dwNumBytesToSend = 0;           // Clear output buffer
    FT_STATUS ftStatus = FT_OK;

    // Combine the Read/Write bit and the actual data to make a single byte with 7 data bits and the R/W in the LSB
    if(Read == TRUE)
    {
        dwDataSend = ((dwDataSend << 1) | 0x01);
    }
    else
    {
        dwDataSend = ((dwDataSend << 1) & 0xFE);
    }

    OutputBuffer[dwNumBytesToSend++] = 0x11;        // command to clock data bytes out MSB first on clock falling edge
    OutputBuffer[dwNumBytesToSend++] = 0x00;        // 
    OutputBuffer[dwNumBytesToSend++] = 0x00;        // Data length of 0x0000 means 1 byte data to clock out
    OutputBuffer[dwNumBytesToSend++] = dwDataSend;  // Actual byte to clock out

    // Put I2C line back to idle (during transfer) state... Clock line driven low, Data line high (open drain)
    OutputBuffer[dwNumBytesToSend++] = 0x80;        // Command to set lower 8 bits of port (ADbus 0-7 on the FT232H)
    OutputBuffer[dwNumBytesToSend++] = 0x1A;        // 00011010
    OutputBuffer[dwNumBytesToSend++] = 0x3B;        // 00111011
    
    // AD0 (SCL) is output driven low
    // AD1 (DATA OUT) is output high (open drain)
    // AD2 (DATA IN) is input (therefore the output value specified is ignored)
    // AD3 to AD7 are inputs driven high (not used in this application)

    OutputBuffer[dwNumBytesToSend++] = 0x22;    // Command to clock in bits MSB first on clock rising edge
    OutputBuffer[dwNumBytesToSend++] = 0x00;    // Length of 0x00 means to scan in 1 bit

    // This command then tells the MPSSE to send any results gathered back immediately
    OutputBuffer[dwNumBytesToSend++] = 0x87;    //Send answer back immediate command

    ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);     //Send off the commands
    
    //Check if ACK bit received by reading the byte sent back from the FT232H containing the ACK bit
    ftStatus = FT_Read(ftHandle, InputBuffer, 1, &dwNumBytesRead);      //Read one byte from device receive buffer
    
    if ((ftStatus != FT_OK) || (dwNumBytesRead == 0))
    {
        printf("Failed to get ACK from I2C Slave - 0 bytes read\n");
        return FALSE; //Error, can't get the ACK bit
    }
    else 
    {
        if (((InputBuffer[0] & 0x01)  != 0x00))     //Check ACK bit 0 on data byte read out
        {   
            //printf("Failed to get ACK from I2C Slave - Response was 0x%X\n", InputBuffer[0]);
            return 1; //Error, can't get the ACK bit 
        }
        
    }
    //printf("Received ACK bit from Address 0x%X - 0x%X\n", dwDataSend, InputBuffer[0]);
    return 0;       // Return True if the ACK was received
}

//my function
void SendAddr(BYTE dwDataSend)
{
    dwNumBytesToSend = 0;           // Clear output buffer
    FT_STATUS ftStatus = FT_OK;

    // // Combine the Read/Write bit and the actual data to make a single byte with 7 data bits and the R/W in the LSB
    // if(Read == TRUE)
    // {
    //     dwDataSend = ((dwDataSend << 1) | 0x01);
    // }
    // else
    // {
    //     dwDataSend = ((dwDataSend << 1) & 0xFE);
    // }

    OutputBuffer[dwNumBytesToSend++] = 0x11;        // command to clock data bytes out MSB first on clock falling edge
    OutputBuffer[dwNumBytesToSend++] = 0x00;        // 
    OutputBuffer[dwNumBytesToSend++] = 0x00;        // Data length of 0x0000 means 1 byte data to clock out
    OutputBuffer[dwNumBytesToSend++] = dwDataSend;  // Actual byte to clock out

    // Put I2C line back to idle (during transfer) state... Clock line driven low, Data line high (open drain)
    OutputBuffer[dwNumBytesToSend++] = 0x80;        // Command to set lower 8 bits of port (ADbus 0-7 on the FT232H)
    OutputBuffer[dwNumBytesToSend++] = 0x1A;        // 00011010
    OutputBuffer[dwNumBytesToSend++] = 0x3B;        // 00111011
    
    // AD0 (SCL) is output driven low
    // AD1 (DATA OUT) is output high (open drain)
    // AD2 (DATA IN) is input (therefore the output value specified is ignored)
    // AD3 to AD7 are inputs driven high (not used in this application)

    OutputBuffer[dwNumBytesToSend++] = 0x22;    // Command to clock in bits MSB first on clock rising edge
    OutputBuffer[dwNumBytesToSend++] = 0x00;    // Length of 0x00 means to scan in 1 bit

    // This command then tells the MPSSE to send any results gathered back immediately
    OutputBuffer[dwNumBytesToSend++] = 0x87;    //Send answer back immediate command

    ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);     //Send off the commands
    
}

// ##############################################################################################################
// Function to set all lines to idle states
// For I2C lines, it releases the I2C clock and data lines to be pulled high externally
// For the remainder of port AD, it sets AD3/4/5/6/7 as inputs as they are unused in this application
// For the LED control, it sets AC6 as an output with initial state high (LED off)
// For the remainder of port AC, it sets AC0/1/2/3/4/5/7 as inputs as they are unused in this application
// ##############################################################################################################

void SetI2CLinesIdle(void)
{
    dwNumBytesToSend = 0;           //Clear output buffer

    // Set the idle states for the AD lines
    OutputBuffer[dwNumBytesToSend++] = 0x80;    // Command to set directions of ADbus and data values for pins set as o/p
    OutputBuffer[dwNumBytesToSend++] = 0x1B;        // 00011011
    OutputBuffer[dwNumBytesToSend++] = 0x3B;    // 00111011

    // IDLE line states are ...
    // AD0 (SCL) is output high (open drain, pulled up externally)
    // AD1 (DATA OUT) is output high (open drain, pulled up externally)
    // AD2 (DATA IN) is input (therefore the output value specified is ignored)

    // Set the idle states for the AC lines
    OutputBuffer[dwNumBytesToSend++] = 0x82;    // Command to set directions of ACbus and data values for pins set as o/p
    OutputBuffer[dwNumBytesToSend++] = 0xFF;    // 11111111
    OutputBuffer[dwNumBytesToSend++] = 0x40;    // 01000000
    //printf("i2c lines set to idle\n");

    // IDLE line states are ...
    // AC6 (LED) is output driving high
    // AC0/1/2/3/4/5/7 are inputs (not used in this application)

    ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);     //Send off the commands
}


// ##############################################################################################################
// Function to set the I2C Start state on the I2C clock and data lines
// It pulls the data line low and then pulls the clock line low to produce the start condition
// It also sends a GPIO command to set bit 6 of ACbus low to turn on the LED. This acts as an activity indicator
// Turns on (low) during the I2C Start and off (high) during the I2C stop condition, giving a short blink.  
// ##############################################################################################################
void SetI2CStart(void)
{
    dwNumBytesToSend = 0;           //Clear output buffer
    DWORD dwCount;
    
    // Pull Data line low, leaving clock high (open-drain)
    for(dwCount=0; dwCount < 5; dwCount++)  // Repeat commands to ensure the minimum period of the start hold time is achieved
    {
        OutputBuffer[dwNumBytesToSend++] = 0x80;    // Command to set directions of ADbus and data values for pins set as o/p
        OutputBuffer[dwNumBytesToSend++] = 0x19;    // Bring data out low (bit 1)
        OutputBuffer[dwNumBytesToSend++] = 0x3B;    // Set all pins as output except bit 2 which is the data_in
    }
    
    // Pull Clock line low now, making both clock and data low
    for(dwCount=0; dwCount < 5; dwCount++)  // Repeat commands to ensure the minimum period of the start setup time is achieved
    {
        OutputBuffer[dwNumBytesToSend++] = 0x80;    // Command to set directions of ADbus and data values for pins set as o/p
        OutputBuffer[dwNumBytesToSend++] = 0x18;    // Bring clock line low too to make clock and data low
        OutputBuffer[dwNumBytesToSend++] = 0x3B;    // Set all pins as output except bit 2 which is the data_in
    }

    // Turn the LED on by setting port AC6 low.
    OutputBuffer[dwNumBytesToSend++] = 0x82;    // Command to set directions of upper 8 pins and force value on bits set as output
    OutputBuffer[dwNumBytesToSend++] = 0xBF;    // 10111111
    OutputBuffer[dwNumBytesToSend++] = 0x40;    // 01000000

    ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);     //Send off the commands
    //printf("i2c lines set to start\n");
}



// ##############################################################################################################
// Function to set the I2C Stop state on the I2C clock and data lines
// It takes the clock line high whilst keeping data low, and then takes both lines high
// It also sends a GPIO command to set bit 6 of ACbus high to turn off the LED. This acts as an activity indicator
// Turns on (low) during the I2C Start and off (high) during the I2C stop condition, giving a short blink.  
// ##############################################################################################################

void SetI2CStop(void)
{
    dwNumBytesToSend = 0;           //Clear output buffer
    DWORD dwCount;

    // Initial condition for the I2C Stop - Pull data low (Clock will already be low and is kept low)
    for(dwCount=0; dwCount<4; dwCount++)        // Repeat commands to ensure the minimum period of the stop setup time is achieved
    {
        OutputBuffer[dwNumBytesToSend++] = 0x80;    // Command to set directions of ADbus and data values for pins set as o/p
        OutputBuffer[dwNumBytesToSend++] = 0x18;    // put data and clock low
        OutputBuffer[dwNumBytesToSend++] = 0x3B;    // Set all pins as output except bit 2 which is the data_in
    }

    // Clock now goes high (open drain)
    for(dwCount=0; dwCount<4; dwCount++)        // Repeat commands to ensure the minimum period of the stop setup time is achieved
    {
        OutputBuffer[dwNumBytesToSend++] = 0x80;    // Command to set directions of ADbus and data values for pins set as o/p
        OutputBuffer[dwNumBytesToSend++] = 0x19;    // put data low, clock remains high (open drain, pulled up externally)
        OutputBuffer[dwNumBytesToSend++] = 0x3B;    // Set all pins as output except bit 2 which is the data_in
    }

    // Data now goes high too (both clock and data now high / open drain)
    for(dwCount=0; dwCount<4; dwCount++)    // Repeat commands to ensure the minimum period of the stop hold time is achieved
    {
        OutputBuffer[dwNumBytesToSend++] = 0x80;    // Command to set directions of ADbus and data values for pins set as o/p
        OutputBuffer[dwNumBytesToSend++] = 0x1B;    // both clock and data now high (open drain, pulled up externally)
        OutputBuffer[dwNumBytesToSend++] = 0x3B;    // Set all pins as output except bit 2 which is the data_in
    }
        
    // Turn the LED off by setting port AC6 high.
        OutputBuffer[dwNumBytesToSend++] = 0x82;    // Command to set directions of upper 8 pins and force value on bits set as output
        OutputBuffer[dwNumBytesToSend++] = 0xFF;    // 11111111
        OutputBuffer[dwNumBytesToSend++] = 0x40;    // 01000000

    ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);     //Send off the commands
    //printf("i2c stop\n");
}




// OPENING DEVICE AND MPSSE CONFIGURATION

void setupMPSSE(char *deviceName)
{
    DWORD dwCount;
    //DWORD devIndex = 0;
    //char Buf[64];
        
    // Open the FT232H module by it's description in the EEPROM
    // Note: See FT_OpenEX in the D2xx Programmers Guide for other options available
    ftStatus = FT_OpenEx(deviceName, FT_OPEN_BY_SERIAL_NUMBER, &ftHandle);
    
    // Check if Open was successful
    if (ftStatus != FT_OK)
    {
        printf("Can't open %s device! \n", deviceName);
        getchar();
        exit(1);
    }
    else
    {   
        // #########################################################################################
        // After opening the device, Put it into MPSSE mode
        // #########################################################################################
                
        // Print message to show port opened successfully
        //printf("Successfully opened FT232H device. \n");

        // Reset the FT232H
        ftStatus |= FT_ResetDevice(ftHandle);   
        
        // Purge USB receive buffer ... Get the number of bytes in the FT232H receive buffer and then read them
        ftStatus |= FT_GetQueueStatus(ftHandle, &dwNumInputBuffer);  
        if ((ftStatus == FT_OK) && (dwNumInputBuffer > 0))
        {
            FT_Read(ftHandle, &InputBuffer, dwNumInputBuffer, &dwNumBytesRead);     
        }
        //printf("Purged receieve buffer.\n");

        ftStatus |= FT_SetUSBParameters(ftHandle, 65536, 65535);        // Set USB request transfer sizes
        ftStatus |= FT_SetChars(ftHandle, 0, 0, 0, 0);          // Disable event and error characters
        ftStatus |= FT_SetTimeouts(ftHandle, 5000, 5000);           // Set the read and write timeouts to 5 seconds
        ftStatus |= FT_SetLatencyTimer(ftHandle, 16);               // Keep the latency timer at default of 16ms
        ftStatus |= FT_SetBitMode(ftHandle, 0x0, 0x00);             // Reset the mode to whatever is set in EEPROM
        ftStatus |= FT_SetBitMode(ftHandle, 0x0, 0x02);             // Enable MPSSE mode
        
        // Inform the user if any errors were encountered
        if (ftStatus != FT_OK)
        {
            printf("failure to initialize FTYPX46A device! \n");
            getchar();
            exit(1);
            //return 1;
        }
        //printf("MPSSE initialized.\n");

        // #########################################################################################
        // Synchronise the MPSSE by sending bad command AA to it
        // #########################################################################################

        dwNumBytesToSend = 0;
        
        OutputBuffer[dwNumBytesToSend++] = 0x84;
                                           // Enable internal loopback
        ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
        if (ftStatus != FT_OK) { printf("failed"); }

        dwNumBytesToSend = 0;

        ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesRead);
        //printf("\n");

        if (dwNumBytesRead != 0)
        {
                printf("Error - MPSSE receive buffer should be empty\n");
                FT_SetBitMode(ftHandle, 0x0, 0x00);
                FT_Close(ftHandle);
                exit(1);
        }

        dwNumBytesToSend = 0;
        OutputBuffer[dwNumBytesToSend++] = 0xAA;
                                           // Bogus command added to queue
        ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
        //printf("sent.\n");
        dwNumBytesToSend = 0;
        
        do
        {
                ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesRead);
                                        // Get the number of bytes in the device input buffer
        } while ((dwNumBytesRead == 0) && (ftStatus == FT_OK));
                                        // Or timeout


        bCommandEchod = 0;      
        ftStatus = FT_Read(ftHandle, &InputBuffer, dwNumBytesRead, &dwNumBytesRead);
                                        // Read out the input buffer
        for (dwCount = 0; dwCount < dwNumBytesRead - 1; dwCount++)
                                        // Check if bad command and echo command are received
        {
                if ((InputBuffer[dwCount] == 0xFA) && (InputBuffer[dwCount+1] == 0xAA))
                {
                        //printf("Success. Input buffer contained 0x%X and 0x%X\n", InputBuffer[dwCount], InputBuffer[dwCount+1]);
                        bCommandEchod = 1;
                        break;
                }       
        }       
        if (bCommandEchod == 0)
        {
                printf("failed to synchronize MPSSE with command 0xAA \n");
                printf("0x%X, 0x%X\n", InputBuffer[dwCount], InputBuffer[dwCount+1]);
                FT_Close(ftHandle);
                exit(1);
        }

    

        // #########################################################################################
        // Synchronise the MPSSE by sending bad command AB to it
        // #########################################################################################

        dwNumBytesToSend = 0;
        //printf("\n");
        //printf("Sending bogus command 0xAB...");
        OutputBuffer[dwNumBytesToSend++] = 0xAB;
                                           // Bogus command added to queue
        ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
        dwNumBytesToSend = 0;
        
        do
        {
                ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesRead);
                                        // Get the number of bytes in the device input buffer
        } while ((dwNumBytesRead == 0) && (ftStatus == FT_OK));
                                        // Or timeout


        bCommandEchod = 0;
        ftStatus = FT_Read(ftHandle, &InputBuffer, dwNumBytesRead, &dwNumBytesRead);
                                        // Read out the input buffer

        for (dwCount = 0; dwCount < dwNumBytesRead - 1; dwCount++)
                                        // Check if bad command and echo command are received
        {
                if ((InputBuffer[dwCount] == 0xFA) && (InputBuffer[dwCount+1] == 0xAB))
                {
                        bCommandEchod = 1;
                        //printf("Success. Input buffer contained 0x%X and 0x%X\n", InputBuffer[dwCount], InputBuffer[dwCount+1]);
                        break;
                }
        }
        if (bCommandEchod == 0)
        {
                printf("failed to synchronize MPSSE with command 0xAB \n");
                printf("0x%X, 0x%X\n", InputBuffer[dwCount], InputBuffer[dwCount+1]);
                FT_Close(ftHandle);
                exit(1);
        }

        dwNumBytesToSend = 0;
        //printf("Disabling internal loopback...");
        OutputBuffer[dwNumBytesToSend++] = 0x85;
                                           // Disable loopback
        ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
        if (ftStatus != FT_OK ) {
                printf("command failed");
        } else {
                //printf("disabled.\n");
        }

        // #########################################################################################
        // Configure the MPSSE settings
        // #########################################################################################

        dwNumBytesToSend = 0;                   // Clear index to zero
        OutputBuffer[dwNumBytesToSend++] = 0x8A;        // Disable clock divide-by-5 for 60Mhz master clock
        OutputBuffer[dwNumBytesToSend++] = 0x97;        // Ensure adaptive clocking is off
        OutputBuffer[dwNumBytesToSend++] = 0x8C;        // Enable 3 phase data clocking, data valid on both clock edges for I2C

        OutputBuffer[dwNumBytesToSend++] = 0x9E;        // Enable the FT232H's drive-zero mode on the lines used for I2C ...
        OutputBuffer[dwNumBytesToSend++] = 0x07;        // ... on the bits 0, 1 and 2 of the lower port (AD0, AD1, AD2)...
        OutputBuffer[dwNumBytesToSend++] = 0x00;        // ...not required on the upper port AC 0-7

        OutputBuffer[dwNumBytesToSend++] = 0x85;        // Ensure internal loopback is off

        ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent); // Send off the commands

        // Now configure the dividers to set the SCLK frequency which we will use
        // The SCLK clock frequency can be worked out by the algorithm (when divide-by-5 is off)
        // SCLK frequency  = 60MHz /((1 +  [(1 +0xValueH*256) OR 0xValueL])*2)
        dwNumBytesToSend = 0;                               // Clear index to zero
        OutputBuffer[dwNumBytesToSend++] = 0x86;                    // Command to set clock divisor
        OutputBuffer[dwNumBytesToSend++] = dwClockDivisor & 0xFF;           // Set 0xValueL of clock divisor
        OutputBuffer[dwNumBytesToSend++] = (dwClockDivisor >> 8) & 0xFF;        // Set 0xValueH of clock divisor
        ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent); // Send off the commands
        if (ftStatus == FT_OK ) {
            //printf("Clock set to three-phase, drive-zero mode set, loopback off.\n");
        } else {
            printf("Clock and pin mode set failed.\n");
        }

                
        // #########################################################################################
        // Configure the I/O pins of the MPSSE
        // #########################################################################################

        // Call the I2C function to set the lines of port AD to their required states
        SetI2CLinesIdle();

        // Also set the required states of port AC0-7. Bit 6 is used as an active-low LED, the others are unused
        // After this instruction, bit 6 will drive out high (LED off)
        //dwNumBytesToSend = 0;             // Clear index to zero
        OutputBuffer[dwNumBytesToSend++] = '\x82';  // Command to set directions of upper 8 pins and force value on bits set as output
        OutputBuffer[dwNumBytesToSend++] = '\xFF';      // Write 1's to all bits, only affects those set as output
        OutputBuffer[dwNumBytesToSend++] = '\x40';  // Set bit 6 as an output
        ftStatus = FT_Write(ftHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent); // Send off the commands
    }
}

void shutdownFTDI(void) {
    printf("\n");
    FT_SetBitMode(ftHandle, 0x0, 0x00);
    FT_Close(ftHandle);
}

void scanDevicesAndQuit(void) {

    printf("Scanning I2C bus:\n");
    BYTE error, address;
    int numDevices;

    numDevices = 0;
    for (address = 0; address < 127; address++)
    {
        SetI2CLinesIdle();
        SetI2CStart();
        error = SendAddrAndCheckACK(address,0);
        SetI2CStop();
        if (error == 0)
        {
        //printf("I2C device found at address 0x");
        printf("0x");
           if (address < 16)
               {
           printf("0");
           }
            printf("%X ", address);
        //printf(".\n");
        numDevices++;
        }
        else
        {
        printf(". ");
        }   
    }

    printf("\n");

    if (numDevices == 0)
    {
        printf("No I2C devices found.\n");
        } else {
        if (numDevices == 1)
         {
          printf("One device found.\n");
          } else {
          if (numDevices > 1)
          {
          printf("%d devices found.\n", numDevices);
         }
            }
    }

    printf("\n");
           FT_SetBitMode(ftHandle, 0x0, 0x00);
           FT_Close(ftHandle);
} 
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
//#include <linux/types.h>
#include <unistd.h>

#pragma pack(push, 1)
typedef struct tagBITMAPFILEHEADER
{
  short bfType;         //specifies the file type
  int bfSize;         //specifies the size in bytes of the bitmap file
    short bfReserved1;      //reserved; must be 0
    short bfReserved2;      //reserved; must be 0
    int bOffBits;       //specifies the offset in bytes from the bitmapfileheader to the bitmap bits
}BITMAPFILEHEADER;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct tagBITMAPINFOHEADER
{
    int biSize;     //specifies the number of bytes required by the struct
    int biWidth;      //specifies width in pixels
    int biHeight;     //species height in pixels
    short biPlanes;     //specifies the number of color planes, must be 1
    short biBitCount;   //specifies the number of bit per pixel
    int biCompression;  //specifies the type of compression
    int biSizeImage;    //size of image in bytes
    int biXPelsPerMeter;  //number of pixels per meter in x axis
    int biYPelsPerMeter;  //number of pixels per meter in y axis
    int biClrUsed;      //number of colors used by the bitmap
    int biClrImportant; //number of colors that are important
}BITMAPINFOHEADER;
#pragma pack(pop)

unsigned char *LoadBitmapFile(char *filename, BITMAPINFOHEADER *bitmapInfoHeader);

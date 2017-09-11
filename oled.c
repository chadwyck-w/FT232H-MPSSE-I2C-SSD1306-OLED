//compile me like this
// gcc -lftd2xx  i2c_lib.c oled_lib.c oled.c bmp_lib.c  -o oled
//run me like this from the command line
//./oled 1 image1.bmp

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>  // for INT_MAX
#include <dirent.h>  // for directories

#include "oled_lib.h"
#include "i2c_lib.h"
#include "bmp_lib.h"

#include <math.h>

void test(int timeout, char *filename) {
  clearDisplay();
  BITMAPINFOHEADER bitmapInfoHeader;
  unsigned char *bitmapImage = LoadBitmapFile(filename,&bitmapInfoHeader);
  drawBitmap(0, 0, bitmapImage, SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT, WHITE);
  display();
  sleep(timeout);
  clearDisplay();
  display();
}

void oneframe(int timeout, char *filename) {
  BITMAPINFOHEADER bitmapInfoHeader;
  unsigned char *bitmapImage = LoadBitmapFile(filename,&bitmapInfoHeader);
  drawBitmap(0, 0, bitmapImage, SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT, WHITE);

  display();
  nanosleep((const struct timespec[]){{timeout / 1000, (timeout % 1000) * pow(10,6)}}, NULL);
}

void setup(char *deviceName)   {                
  setupMPSSE(deviceName);
  //device address is 0x3C according to the scanner
  begin(SSD1306_SWITCHCAPVCC,0x3C);
}

BOOL has_bmp_extension(char const *name)
{
    size_t len = strlen(name);
    return len > 4 && strcmp(name + len - 4, ".bmp") == 0;
}

int main(int argc, char **argv)
{

  //this is the new style way to get arguments on the command line
  char *image = NULL;

  //char *deviceName = "FTYPX46A";
  char *deviceName = "FT0EARPI";
  
  //loop or not
  int loopFlag = 0;

  //timeout
  int timeoutValue = 3;

  //delay
  int delayValue = 100;

  int index;
  int c;

  opterr = 0;

  while ((c = getopt (argc, argv, "i:lt:d:D:")) != -1)
    switch (c)
      {
      case 'i':
        image = optarg;
        break;
      case 'l':
        loopFlag = 1;
        break;
      case 't':
        timeoutValue = atoi(optarg);
        break;
      case 'd':
        delayValue = atoi(optarg);
        break;
      case 'D':
        deviceName = optarg;
        break;
      default:
        abort ();
      }

  printf ("image = %s, loopFlag = %d, timeoutValue = %d, delayValue = %d, deviceName = %s\n",
          image, loopFlag, timeoutValue, delayValue, deviceName );

  for (index = optind; index < argc; index++)
    printf ("Non-option argument %s\n", argv[index]);

  //the delay, default is 3
  int timeout = 3;
  char *filename = "image.bmp";
  setup(deviceName);
  
  if(loopFlag == 1){
    DIR           *d;
    struct dirent *dir;
    d = opendir("./images");
      if (d){
        while ((dir = readdir(d)) != NULL)
          {
            if( has_bmp_extension(dir->d_name) ) {
              char str[80];
              strcpy(str, "./images/");
              strcat(str, dir->d_name);
              //printf("filename %s\n", str);
              oneframe(timeoutValue, str);
            }
          }
      closedir(d);
    }
  }
  
  clearDisplay();
  //drawCircle(64,24,20, WHITE);
  display();

  nanosleep((const struct timespec[]){{delayValue / 1000, (delayValue % 1000) * pow(10,6)}}, NULL);

  clearDisplay();
  display();
  shutdownOLED();
  return 0;
}






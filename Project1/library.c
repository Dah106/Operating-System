/*
	Author: Danchen Huang
	Date Created: 09/15/2015
	This file is dedicated to resemble a small graphics library for project 1 of CS1550 
*/
#include <time.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "iso_font.h"
#include <stdio.h>

typedef unsigned short color_t;
unsigned short *frameBuffer;
unsigned long virtualResolutionX;
unsigned long virtualResolutionY;
unsigned long lineLength;
int fileDescriptor;
void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t color);
void fill_rect(int x1, int y1, int width, int height, color_t color);
void fill_circle(int x, int y, int radius, color_t color);
void draw_text(int x, int y, const char *text, color_t color);
void draw_line(int x1, int y1, int x2, int y2, color_t color);
void draw_char(int x, int y, unsigned char *ch, color_t color);

void init_graphics()
{
	struct fb_var_screeninfo sInfo;
	struct fb_fix_screeninfo bitDepth;
	struct termios terminalSetting;
	int fDesc = open("/dev/fb0", O_RDWR);
	ioctl(fDesc, FBIOGET_VSCREENINFO, &sInfo);
	ioctl(fDesc, FBIOGET_FSCREENINFO, &bitDepth);
	fileDescriptor = fDesc;
	virtualResolutionX = sInfo.xres_virtual;
	virtualResolutionY = sInfo.yres_virtual;
	lineLength = bitDepth.line_length;
	frameBuffer = (unsigned short *)mmap( NULL, virtualResolutionY * lineLength, PROT_WRITE, MAP_SHARED, fDesc, 0);
	ioctl(STDIN_FILENO, TCGETS, &terminalSetting);
	terminalSetting.c_lflag &= ~ICANON;
	terminalSetting.c_lflag &= ~ECHO;
	ioctl(STDIN_FILENO, TCSETS, &terminalSetting);
	clear_screen();	
}

void exit_graphics()
{	
	struct termios terminalSetting;
	ioctl(STDIN_FILENO, TCGETS, &terminalSetting);
	terminalSetting.c_lflag |= ICANON;	
	terminalSetting.c_lflag |= ECHO;
	ioctl(STDIN_FILENO, TCSETS, &terminalSetting);
	munmap(frameBuffer, lineLength * virtualResolutionY); 
	close(fileDescriptor);
	clear_screen();
}

void clear_screen()
{	
	int clearScreen;
	clearScreen = write(1, "\033[2J", 7);
}

char getkey()
{	
	char key;
	fd_set fileDescVar;
	FD_ZERO(&fileDescVar);
	FD_SET(STDIN_FILENO, &fileDescVar);

	struct timeval timeout;
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	
	int result;
	result = select( STDIN_FILENO+1, &fileDescVar, NULL, NULL, &timeout );	
	if( result > 0 )
	{
		read(0, &key, sizeof(key));
	}
	return key;	
}

void sleep_ms(long ms)
{
    struct timespec timeSetting;
	timeSetting.tv_sec = 0;
	timeSetting.tv_nsec = ms * 1000000;
	nanosleep(&timeSetting, NULL);
}

void draw_pixel(int x, int y, color_t color)
{
	if(x < 0 || x >= virtualResolutionX)
	{
		return;
	}
	else
	{
		if(y < 0 || y >= virtualResolutionY)
		{
			return;
		}	
	}
 	unsigned long x1 = x;
	unsigned long y1 = y * (lineLength / 2);
	unsigned short *pixel_ptr = (frameBuffer + x1 + y1);
	*pixel_ptr = color;
}

void draw_rect(int x1, int y1, int width, int height, color_t color)
{
	int lowerLeftX = x1;
	int lowerLeftY = y1;

	int upperRightX = x1 + width;
	int upperRightY = y1 + height;
	
	int upperLeftX = x1;
	int upperLeftY = y1 + height;
	
	int lowerRightX = x1 + width;
	int lowerRightY = y1;

	draw_line(lowerLeftX, lowerLeftY, lowerRightX, lowerRightY, color);
	draw_line(lowerLeftX, lowerLeftY, upperLeftX, upperLeftY, color);
	draw_line(upperLeftX, upperLeftY, upperRightX, upperRightY, color);
	draw_line(lowerRightX, lowerRightY, upperRightX, upperRightY, color);
}

void fill_rect(int x1, int y1, int width, int height, color_t color)
{
	int x;
	int y;	
	for(x = x1; x <= x1 + width; x++)
	{
		for(y = y1; y <= y1 + height; y++)
		{
			draw_pixel(x, y, color);
		}
	}
}

//Midpoint Circle Algorithm similar to wikipedia implementation
void fill_circle(int x, int y, int radius, color_t color)
{
	int x1 = radius;
	int y1 = 0;
	int decisionOver2 = 1 - x1;

	while( y1 <= x1)
	{

		// setPixel(x0 + x, y0 + y), setPixel(x0 - x, y0 + y)
		// is equivalent of drawLine(x0 - x, y0 + y, x0 + x, y0 + y)
		draw_line(x - x1, y + y1, x + x1, y + y1, color);
		draw_line(x - x1, y - y1, x + x1, y - y1, color);
		draw_line(x - y1, y + x1, x + y1, y + x1, color);
		draw_line(x - y1, y - x1, x + y1, y - x1, color);
		y1++;
		if(decisionOver2 <= 0)
		{
			decisionOver2 += 2 * y1 + 1;
		}
		else
		{
			x1--;
			decisionOver2 += 2 * (y1 - x1) + 1;
		}
	}
}

void draw_text(int x, int y, const char *text, color_t color)
{	
	const char lineEscape = '\0';
	unsigned char ch;
	int i = 0;
	while(*(text + i) != lineEscape)
	{	
		ch = *(text + i);
		x = x + 8;
		draw_char(x, y, iso_font + ch * 16, color);
		i++;
	}
}

void draw_char(int x, int y, unsigned char *ch, color_t color)
{	
	int i;
	int j;
	unsigned char c;
	for(i = 0;i < 16; i++)
	{	
		y++;
		for(j = 0, c = *(ch + i); j < 8; j++)
		{
			c = c >> 1;
			if(c & 0x01)
			{
				draw_pixel(x + j, y, color);
			}
		}
	}
}

void draw_line(int x1, int y1, int x2, int y2, color_t color)
{
	int dx = x2 - x1;
	int dy = y2 - y1;
	if( dx < 0 ) { dx = -dx; }
	if( dy < 0 ) { dy = -dy; }
	int sx = x1 < x2 ? 1 : -1;
	int sy = y1 < y2 ? 1 : -1;
	int err = ( dx > dy ? dx : -dy) / 2;
	int e2;

	for(;;) {

		draw_pixel(x1, y1, color);
		if(x1 == x2 && y1 == y2) break;
		e2 = err;
		if (e2 >-dx) { err -= dy; x1 += sx; }
    	if (e2 < dy) { err += dx; y1 += sy; }
	}
}
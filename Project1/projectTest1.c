/*
	Author: Danchen Huang
	Date Created: 09/21/2015
	This file is a driver file for library.c
*/
typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void fill_rect(int x1, int y1, int width, int height, color_t c);
void fill_circle(int x, int y, int radius, color_t color);
void draw_text(int x, int y, const char *text, color_t color);

int main(int argc, char** argv)
{
	init_graphics();

	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;

	const char *comment = "Those are shapes required for this project";
	const char *text = "ABCD";
	int x0 = 50;
	int y0 = 20;

	int x1 = 100;
	int y1 = 120;

	int x2 = 200;
	int y2 = 100;

	int x3 = 300;
	int y3 = 100;

	//comment for the program
	draw_text(x0, y0, comment, -80);

	//draw a circle
	fill_circle(x1, y1, 20, -80);
		
	//draw a rectangle
	draw_rect(x2, y2, 35, 35, -80);
		
	//fill a rectangle
	fill_rect(x3, y3, 35, 35, -80);

	
	do
	{
		key = getkey();
		if(key == 'w') y -= 55;
		else if(key == 's') y += 55;
		else if(key == 'a') x -= 55;
		else if(key == 'd') x += 55;
		
		//draw text
		draw_text(x, y, text, -80);

		sleep_ms(20);
	} while(key != 'q');

	exit_graphics();

	return 0;
}
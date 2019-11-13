#include <stdint.h>
#include <avr/io.h> 
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "usb_serial.h"
#include <macros.h>
#include <lcd_model.h>
#include <graphics.h>

#define FREQ (8000000)
#define PRESCALE (64.0)
#define STATUS_BAR_HEIGHT (9)
#define LCD_X  84
#define LCD_Y  48
#define image_width 8
#define image_height 7
#define trap_width 6
#define trap_height 5


// define pi 
#ifndef PI
#define PI 3.14159265358979323846264338327950288
#endif

// Each arg corresponds to the state of a character or game mode
int score = 0; 
int level; 
int health = 6;
int fireworks_current = 0;
int mousetraps_current = 0;
bool super_jerry_mode = false;
char *jerrymode = "False";
bool pause = true;
bool not_paused_status = false;
char *is_game_paused = "False";

// Haven't implemented super jerry mode, but this function is for information output on Putty when user clicks 'i'
void super_jerry(void){
	if (super_jerry_mode == true){
		jerrymode = "True";

	}
}

// To store counters for each button or joystick
volatile uint8_t joystick_up_counter = 0; 
volatile uint8_t joystick_down_counter = 0; 
volatile uint8_t joystick_left_counter = 0; 
volatile uint8_t joystick_right_counter = 0; 
volatile uint8_t joystick_button_counter = 0; 
volatile uint8_t left_button_counter = 0; 
volatile uint8_t right_button_counter = 0; 

// Will store the current states of the switches.
volatile uint8_t joystick_up_switch_closed = 0;
volatile uint8_t joystick_down_switch_closed = 0;
volatile uint8_t joystick_left_switch_closed = 0;
volatile uint8_t joystick_right_switch_closed = 0;
volatile uint8_t joystick_button_switch_closed = 0;
volatile uint8_t left_button_switch_closed = 0;
volatile uint8_t right_button_switch_closed = 0;

// Bring back functions to avoid undeclared function errors
void move_tom(void);
void setup_jerry(void);
void setup_cheese(void);
void setup_door(void);
void setup_trap(void);
void setup_usb_serial( void );
void level_one(void);
void level_two(void);
void usb_serial_send(char * message);

void game_over(void){
	char *gameover = "GAME OVER!";
	clear_screen();
	draw_string(10, 20, gameover, FG_COLOUR);
	show_screen();
}

// Start screen for when game first loads up
void start_screen(void){
	char *student_name = "Ignacio Rodino";
	char *student_id = "n10446052";
	char *game_name1 = "Tom & Jerry";
	char *game_name2 = "The Game!";
	clear_screen();
	draw_string(0, 0, student_name, FG_COLOUR);
	draw_string(0, 8, student_id, BG_COLOUR);
	draw_string(0, 24, game_name1, FG_COLOUR);
	draw_string(0, 32, game_name2, FG_COLOUR);
	show_screen();

	//block here until button is pressed, for level 1
	while(!BIT_IS_SET(PINF, 5));

}

void setup( void ) {
	set_clock_speed(CPU_8MHz);
	clear_screen();
	// Enable input 
	DDRD &= ~(1<<1); //up joystick
	DDRB &= ~(1<<1); //left joystick
	DDRB &= ~(1<<7); //down joystick
	DDRD &= ~(1<<0); //right joystick
	DDRB &= ~(1<<0); //SW1 joystick button
	DDRF &= ~(1<<6); //SW2 left button
	DDRF &= ~(1<<5); //SW3 right button

	// Enable centre LED for output
	SET_BIT(DDRD, 6);
	SET_BIT(DDRB, 3);
	SET_BIT(DDRB, 2);
	SET_BIT(DDRF, 5);

	// Initialise the LCD display using the default contrast setting.
	lcd_init(LCD_DEFAULT_CONTRAST);

	//  Initialise Timer 3 in normal mode so that it overflows 
	//	with a period of approximately 0.5 seconds.
	TCCR3A = 0;
	TCCR3B = 3;
	//Enable timer overflow for Timer 3.
	TIMSK3 = 1;
	// Turn on interrupts.
	sei();
	setup_usb_serial(); // Set up function for use of usb serial with Putty
	show_screen();

	start_screen(); // Waits for button press
	srand(TCNT3); // Creates random seed
	level_one(); // Start levelone after button press
	level_two();

	// Setup characters
	move_tom();
	setup_jerry();
	setup_cheese();
	setup_door();
	setup_trap();

}

// Buffer for storing and printing int on function
char buffer[20];
void draw_int(uint8_t x, uint8_t y, int value, colour_t colour) {
	snprintf(buffer, sizeof(buffer), "%d", value);
	draw_string(x, y, buffer, colour);
}

// Set up debouncing for joystick up
void joystick_up_debounce(void){

	//	Left-shift counter one place;
	joystick_up_counter = (joystick_up_counter << 1);

	uint8_t mask = 0b01111111;

	joystick_up_counter = joystick_up_counter & mask;

	if (PIND & (1 << 1)){

	//  Bitwise AND with a mask in which the 7 bits on the right
	//	are 1 and the others are 0.
	joystick_up_counter = joystick_up_counter | (PIND & (1 << 1));
	}

	//	If counter is equal to the bit mask, then the switch has been 
	//	observed 7 times in a row to be closed. Assign the value 1 to 
	//	switch_closed, indicating that the switch should now be considered to be officially "closed".
	if (joystick_up_counter == mask){
		joystick_up_switch_closed = 1;
	}

	// If counter is equal to 0, then the switch has been observed 
	// to be open at least 7 in a row, indicating that the switch should now be considered to be officially "closed".
	else if (joystick_up_counter == 0){
		joystick_up_switch_closed = 0;
	}
}

// Set up debouncing for joystick down
void joystick_down_debounce(void){

	//	Left-shift counter one place;
	joystick_down_counter = (joystick_down_counter << 1);

	uint8_t mask = 0b01111111;

	joystick_down_counter = joystick_down_counter & mask;

	if (PINB & (1 << 7)){

	//  Bitwise AND with a mask in which the 7 bits on the right
	//	are 1 and the others are 0.
	joystick_down_counter = joystick_down_counter | (PINB & (1 << 7));
	}

	//	If counter is equal to the bit mask, then the switch has been 
	//	observed 7 times in a row to be closed. Assign the value 1 to 
	//	switch_closed, indicating that the switch should now be considered to be officially "closed".
	if (joystick_down_counter == mask){
		joystick_down_switch_closed = 1;
	}

	// If counter is equal to 0, then the switch has been observed 
	// to be open at least 7 in a row, indicating that the switch should now be considered to be officially "closed".
	else if (joystick_down_counter == 0){
		joystick_down_switch_closed = 0;
	}
}

// Set up debouncing for joystick left
void joystick_left_debounce(void){

	//	Left-shift counter one place;
	joystick_left_counter = (joystick_left_counter << 1);

	uint8_t mask = 0b01111111;

	joystick_left_counter = joystick_left_counter & mask;

	if (PINB & (1 << 1)){

	//  Bitwise AND with a mask in which the 7 bits on the right
	//	are 1 and the others are 0.
	joystick_left_counter = joystick_left_counter | (PINB & (1 << 1));
	}

	//	If counter is equal to the bit mask, then the switch has been 
	//	observed 7 times in a row to be closed. Assign the value 1 to 
	//	switch_closed, indicating that the switch should now be considered to be officially "closed".
	if (joystick_left_counter == mask){
		joystick_left_switch_closed = 1;
	}

	// If counter is equal to 0, then the switch has been observed 
	// to be open at least 7 in a row, indicating that the switch should now be considered to be officially "closed".
	else if (joystick_left_counter == 0){
		joystick_left_switch_closed = 0;
	}
}

// Set up debouncing for joystick right
void joystick_right_debounce(void){

	//	Left-shift counter one place;
	joystick_right_counter = (joystick_right_counter << 1);

	uint8_t mask = 0b01111111;

	joystick_right_counter = joystick_right_counter & mask;

	if (PIND & (1 << 0)){

	//  Bitwise AND with a mask in which the 7 bits on the right
	//	are 1 and the others are 0.
	joystick_right_counter = joystick_right_counter | (PIND & (1 << 0));
	}

	//	If counter is equal to the bit mask, then the switch has been 
	//	observed 7 times in a row to be closed. Assign the value 1 to 
	//	switch_closed, indicating that the switch should now be considered to be officially "closed".
	if (joystick_right_counter == mask){
		joystick_right_switch_closed = 1;
	}

	// If counter is equal to 0, then the switch has been observed 
	// to be open at least 7 in a row, indicating that the switch should now be considered to be officially "closed".
	else if (joystick_right_counter == 0){
		joystick_right_switch_closed = 0;
	}
}

// Set up debouncing for joystick button
void joystick_button_debounce(void){

	//	Left-shift counter one place;
	joystick_button_counter = (joystick_button_counter << 1);

	uint8_t mask = 0b01111111;

	joystick_button_counter = joystick_button_counter & mask;

	if (PINB & (1 << 0)){

	//  Bitwise AND with a mask in which the 7 bits on the right
	//	are 1 and the others are 0.
	joystick_button_counter = joystick_button_counter | (PINB & (1 << 0));
	}

	//	If counter is equal to the bit mask, then the switch has been 
	//	observed 7 times in a row to be closed. Assign the value 1 to 
	//	switch_closed, indicating that the switch should now be considered to be officially "closed".
	if (joystick_button_counter == mask){
		joystick_button_switch_closed = 1;
	}

	// If counter is equal to 0, then the switch has been observed 
	// to be open at least 7 in a row, indicating that the switch should now be considered to be officially "closed".
	else if (joystick_button_counter == 0){
		joystick_button_switch_closed = 0;
	}
}

// Set up debouncing for SW2
void left_button_debounce(void){

	//	Left-shift counter one place;
	left_button_counter = (left_button_counter << 1);

	uint8_t mask = 0b01111111;

	left_button_counter = left_button_counter & mask;

	if (PINF & (1 << 6)){

	//  Bitwise AND with a mask in which the 7 bits on the right
	//	are 1 and the others are 0.
	left_button_counter = left_button_counter | (PINF & (1 << 6));
	}

	//	If counter is equal to the bit mask, then the switch has been 
	//	observed 7 times in a row to be closed. Assign the value 1 to 
	//	switch_closed, indicating that the switch should now be considered to be officially "closed".
	if (left_button_counter == mask){
		left_button_switch_closed = 1;
	}

	// If counter is equal to 0, then the switch has been observed 
	// to be open at least 7 in a row, indicating that the switch should now be considered to be officially "closed".
	else if (left_button_counter == 0){
		left_button_switch_closed = 0;
	}
}

// Set up debouncing for SW3
void right_button_debounce(void){

	//	Left-shift counter one place;
	right_button_counter = (right_button_counter << 1);

	uint8_t mask = 0b01111111;

	right_button_counter = right_button_counter & mask;

	if (PINF & (1 << 5)){

	//  Bitwise AND with a mask in which the 7 bits on the right
	//	are 1 and the others are 0.
	right_button_counter = right_button_counter | (PINF & (1 << 5));
	}

	//	If counter is equal to the bit mask, then the switch has been 
	//	observed 7 times in a row to be closed. Assign the value 1 to 
	//	switch_closed, indicating that the switch should now be considered to be officially "closed".
	if (right_button_counter == mask){
		right_button_switch_closed = 1;
	}

	// If counter is equal to 0, then the switch has been observed 
	// to be open at least 7 in a row, indicating that the switch should now be considered to be officially "closed".
	else if (right_button_counter == 0){
		right_button_switch_closed = 0;
	}
}

// Used for timers and pause function
volatile uint32_t overflow_count = 0;
volatile uint32_t cycle_count = 0;
ISR(TIMER3_OVF_vect) {
	cycle_count++;
	joystick_up_debounce();
	joystick_down_debounce();
	joystick_left_debounce();
	joystick_right_debounce();
	joystick_button_debounce();
	left_button_debounce();
	right_button_debounce();

// If the game isn't paused, keep counting the timer
	if (pause == false){
		overflow_count++;
	}
}

// Function to create a timer
double get_elapsed_time(){
	double time = (cycle_count * 65536.0 + TCNT3) * PRESCALE / FREQ;
	return time;
}

// Pause game function, for use when right button is pressed
void pause_game(void){

	if ((BIT_IS_SET(PINF, 5)) && pause == false){
		pause = true;
		not_paused_status = false;
	}

	else if ((BIT_IS_SET(PINF, 5)) && pause == true){
		pause = false;
		not_paused_status = true;
		is_game_paused = "True";
	}
}

// To split timer into a clock with minutes and seconds
int secs = 0;
int min = 0;

void clock(void){
	int time = get_elapsed_time();
	secs = time % 60;
	min = time / 60;
}

// Status bar for level 1
void status_bar(void){
	if (not_paused_status == true){
		clock();
	}
	char *status = "L:  H:  S:   :";
	level = 1;

	// Draw info
	draw_line(0, STATUS_BAR_HEIGHT - 1, LCD_X - 1, STATUS_BAR_HEIGHT - 1, FG_COLOUR);
	draw_string(0, 0, status, FG_COLOUR);
	draw_int(10, 0, level, FG_COLOUR);
	draw_int(30, 0, health, FG_COLOUR);
	draw_int(50, 0, score, FG_COLOUR);
	draw_int(60, 0, min, FG_COLOUR);
	draw_int(69, 0, secs, FG_COLOUR);
}

// Status bar for level 2
void status_bar_level2(void){
	if (not_paused_status == true){
		clock();
	}
	char *status = "L:  H:  S:   :";
	level = 2;

	// Draw info
	draw_line(0, STATUS_BAR_HEIGHT - 1, LCD_X - 1, STATUS_BAR_HEIGHT - 1, FG_COLOUR);
	draw_string(0, 0, status, FG_COLOUR);
	draw_int(10, 0, level, FG_COLOUR);
	draw_int(30, 0, health, FG_COLOUR);
	draw_int(50, 0, score, FG_COLOUR);
	draw_int(60, 0, min, FG_COLOUR);
	draw_int(69, 0, secs, FG_COLOUR);
}

// For drawing walls on level 1
void initial_walls(void){
	draw_line(18, 15, 13, 25, FG_COLOUR);
	draw_line(25, 35, 25, 45, FG_COLOUR);
	draw_line(45, 10, 60, 10, FG_COLOUR);
	draw_line(58, 25, 72, 30, FG_COLOUR);
}

// For drawing walls on level 2
void level2_walls(void){
	draw_line(33, 21, 42, 25, FG_COLOUR);
	draw_line(10, 15, 15, 25, FG_COLOUR);
	draw_line(51, 15, 62, 40, FG_COLOUR);
	draw_line(25, 35, 25, 45, FG_COLOUR);	
	draw_line(58, 25, 72, 30, FG_COLOUR);


}

// Bitmap image for jerry character
uint8_t jerry_bitmap[7] = {
	0b11111000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b10100000,
	0b10100000,
	0b01000000,
};

// To store jerry coordinates
int jerry_x, jerry_y;

// To store door coordinates
int door_x, door_y;

// Set up Jerry's initial position
void setup_jerry(void){
	jerry_x = 0;
	jerry_y = STATUS_BAR_HEIGHT + 1; 
    
}

// Function to draw jerry using bitmap
void draw_jerry(void) {
	for (int x = 0; x < image_height; x++){
		for (int y = 0; y < image_width; y++){
			if(jerry_bitmap[x] & (0b10000000 >> y)){
				draw_pixel(jerry_x + y, jerry_y + x, FG_COLOUR);
			}
		}
	}
}

// Arrays hold position of every pixel for walls in level 1

int wall_x[80] = {45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 18, 18, 17, 17, 16, 16, 15, 15, 14, 14, 13};
int wall_y[80] = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 25, 25, 26, 26, 26, 27, 27, 28, 28, 28, 29, 29, 29, 30, 30, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};
int arraysize = sizeof(wall_x)/sizeof(wall_x[0]);


// To move jerry and create boundaries on screen
void move_jerry(void){

	if (BIT_IS_SET(PINB, 1) && (jerry_x > 0))//move left
	{
		jerry_x -= 1;
	}

	else if (BIT_IS_SET(PIND, 0) && (jerry_x < 79))//move right
	{
		jerry_x += 1;
	}
	else if (PINB & (1 << 7) && (jerry_y < 41))//move down
	{
		jerry_y += 1;
	}
	else if (BIT_IS_SET(PIND, 1) && (jerry_y > STATUS_BAR_HEIGHT))//move up
	{
		jerry_y -= 1;
	}

	// For collission of jerry with the walls on level 1
	for (int x = 0; x <= arraysize ; x++){

		if (wall_x[x] <= jerry_x && wall_x[x] >= jerry_x){
				if (wall_y[x] <= jerry_y && wall_y[x] >= jerry_y){
					jerry_x += 1;
				}
		}

		else if (wall_x[x] == jerry_x){
			if (wall_y[x] <= jerry_y && wall_y[x] >= jerry_y){
				jerry_y -= 1;
			}
		}
        
		else if (wall_x[x] <= jerry_x && wall_x[x] >= jerry_x){
			if (wall_y[x] == jerry_y){
				jerry_x -= 1;
			}
		}
		else if (wall_x[x] <= jerry_x && wall_x[x] >= jerry_x){
			if (wall_y[x] <= jerry_y && wall_y[x] >= jerry_y){
				jerry_y += 1;
			}
		}
	}
	}


// Bitmap image for cheese
uint8_t cheese_bitmap[7] = {
	0b01111000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b01111000,
};

// Array to position cheese
int cheesex[5];
int cheesey[5];

// Setup intial positions of cheese
void setup_cheese(void){
	for (int i = 0; i < 5; i++){

// Store cheese off screen
	cheesex[i] = -100;
	cheesey[i] = -100;
	}
    
}

// Used for cheese state
int if_cheese = 2;
int cheese_on_screen = 0;
int cheese_consumed = 0;
int max_cheese_on_screen = 5;

// Draw cheese using bitmap image
void draw_cheese(void) {
	for (int x = 0; x < image_height; x++){
		for (int y = 0; y < image_width; y++){
			if(cheese_bitmap[x] & (0b10000000 >> y)){
				for (int i = 0; i < max_cheese_on_screen; i++){
				draw_pixel(cheesex[i] + y, cheesey[i] + x, FG_COLOUR);
			}
			}
		}
	}
}



// To draw cheese every 2 seconds
void update_cheese(void){
	if (pause == false){
		if (secs >= if_cheese){
			for (int i = 0; i < max_cheese_on_screen; i++){
				if (cheesex[i] == -100 || cheesey[i] == -100){
					cheesex[i] = rand() % LCD_X - 5;
					cheesey[i] = STATUS_BAR_HEIGHT + rand() % LCD_Y - 5;
					
					// Drop cheese every 2 seconds and increment cheese on screen for putty info
					if_cheese = secs + 2;
					cheese_on_screen++;
					break;
				}
			}
		}
	}
}

// Cheese collission with jerry
void cheese_eaten(void){
	for (int i = 0; i < max_cheese_on_screen; i++){
			if (((cheesex[i] >= (jerry_x -5)) && (cheesex[i] <= (jerry_x + 5)))){
				if(((cheesey[i] >= (jerry_y -5)) && (cheesey[i] <= (jerry_y + 5)))){
			
				// Update score, update cheese consumed for putty info, and move cheese back to offscreen
				score++;
				cheese_consumed++;
				cheese_on_screen--;
				cheesex[i] = -100;
				cheesey[i] = -100;   
		} 
	}
}}
 
// Bitmap image for door
uint8_t door_bitmap[7] = {
	0b11111000,
	0b10001000,
	0b10001000,
	0b10011000,
	0b10001000,
	0b10001000,
	0b11111000,
};

// Setup where the draw will be drawn
void setup_door(void){

		door_x = 20;
		door_y = 20;
	
    
}

// Draw door using bitmap image
void draw_door(void) {
    
	for (int x = 0; x < image_height; x++){
		for (int y = 0; y < image_width; y++){
			if(cheese_bitmap[x] & (0b10000000 >> y)){
				draw_pixel(door_x + y, door_y + x, FG_COLOUR);
			}
		}
	}
}

// Bitmap image for toms character
uint8_t tom_bitmap[7] = {
	0b11111000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
};

// To store toms position and direction
int tom_x, tom_y;


// Draw tom using bitmap image
void draw_tom(void) {
	for (int x = 0; x < image_height; x++){
		for (int y = 0; y < image_width; y++){
			if(tom_bitmap[x] & (0b10000000 >> y)){
				draw_pixel(tom_x + y, tom_y + x, FG_COLOUR);
			}
		}
	}
}

// Boolean to see if tom hit something, doubles for toms direction and steps
bool tom_collided = false;
double tom_direction_x, tom_direction_y;
double step;

// Toms initial coordinates
void setup_tom(void){
	tom_x = (LCD_X - 5);
	tom_y = (LCD_Y - 9);
}

// Setup toms movement and initial position
void move_tom(void) {

	// Setup toms direction using pi and randmax, so that he always goes somewhere random
	double tom_direction = rand() * PI * 2 / RAND_MAX;

	// To calculate random number for steps
	step = rand() % 20;
	while (step < 10){
		step = rand() % 20;
	}
    
	// Times by .3 for pixel steps
	const double updated_step = step * .3;

	// Set toms direction
	tom_direction_x = updated_step * cos(tom_direction);
	tom_direction_y = updated_step * sin(tom_direction);

	// For collission of tom with the walls on level 1
	for (int x = 0; x <= arraysize ; x++){

		if (wall_x[x] <= tom_x && wall_x[x] >= tom_x){
				if (wall_y[x] <= tom_y && wall_y[x] >= tom_y){
					tom_x += 1;
				}
		}

		else if (wall_x[x] == tom_x){
			if (wall_y[x] <= tom_y && wall_y[x] >= tom_y){
				tom_y -= 1;
			}
		}
        
		else if (wall_x[x] <= tom_x && wall_x[x] >= tom_x){
			if (wall_y[x] == tom_y){
				tom_x -= 1;
			}
		}
		else if (wall_x[x] <= tom_x && wall_x[x] >= tom_x){
			if (wall_y[x] <= tom_y && wall_y[x] >= tom_y){
				tom_y += 1;
			}
		}
	}
}


// Function to move tom and keep him within screen boundaries
void update_tom() {

	// Booleon to check if tom has hit something
	tom_collided = false;

	// Create new positions for tom
	double updated_x = round(tom_x + tom_direction_x);
	double updated_y = round(tom_y + tom_direction_y);

	// If tom hits a wall go the other way
	if (updated_x <= 0 || updated_x >= LCD_X - 5 ) {
	// Swap direction on x axis
		tom_collided = true;
		tom_direction_x = -tom_direction_x;
	}

	// If tom hits a wall go the other way
	if (updated_y <= (STATUS_BAR_HEIGHT + 1) || updated_y >= LCD_Y - 5) {
	// Swap direction on y axis
		tom_collided = true;
		tom_direction_y = -tom_direction_y;
	}

	if ((tom_collided == false) && (pause == false)) {
	// Go normal direction if it hasn't hit anything
		tom_x += tom_direction_x;
		tom_y += tom_direction_y;
	}	
}

// Trap arrays for coordinates and positions of traps
int trap_x[5];
int trap_y[5];

// Timer for traps so they drop every 3 seconds
int trap_timer = 3;

// Bitmap image for traps
uint8_t trap_bitmap[7] = {
    0b10001000,
    0b01010000,
    0b00100000,
    0b01010000,
    0b10001000,
	0b00000000,
	0b00000000
};

void setup_trap(){
    // Setup traps to start off the screen, just like cheese
    for(int i = 0; i < 5; i++){
        trap_x[i] = -100;
        trap_y[i] = -100;
   }
}

// Draw trap function using bitmap image
void draw_trap() {
    for(int x = 0; x < trap_height; x++){
        for( int y = 0; y < trap_width; y++){
            if(trap_bitmap[x] & (0b10000000 >> y)){
                for(int i = 0; i < 5; i++){
                    draw_pixel(trap_x[i] + y, trap_y[i] + x, FG_COLOUR );
                }
            }
        }
    }    
}

// Update trap if it collides with jerry, also update health, timer, and if traps are stepped on, move off screen.
void update_trap() { 
        for(int i = 0; i < 5; i++){
            if (((trap_x[i] >= (jerry_x -5)) && (trap_x[i] <= (jerry_x + 5)))){
                if(((trap_y[i] >= (jerry_y -5)) && (trap_y[i] <= (jerry_y + 5)))){
                
				// Update jerry's health when he collides with a trap, move traps off screen, and increment timer by 3 seconds
                trap_x[i] = -100;
                trap_y[i] = -100;
				health -= 1;
				mousetraps_current -= 1;
                trap_timer = secs + 3;
                
            }
        }
    }
} 

void drop_trap(void){
	// Make tom drop traps every 3 secs from his position
    if(secs >= trap_timer){
        for(int i=0; i<5; i++){
            if(trap_x[i] == -100 || trap_y[i] == -100){ 
				
				// Drop traps from toms position and icrement timer
                trap_x[i] = tom_x;
                trap_y[i] = tom_y;
				mousetraps_current += 1;
                trap_timer = secs + 3;
                break;
            }
        }   
    }
}

// Function to return jerry and tom to initial positions and deduct health or add score
void jerry_lose_life(){

		if (((tom_x >= (jerry_x - 4)) && (tom_x <= (jerry_x + 4)))){
			if(((tom_y >= (jerry_y -5)) && (tom_y <= (jerry_y + 5)))){
				health -= 1;
				setup_tom();
				setup_jerry();

			}
		}
}

// Put all draw functions in one
void draw_all(void){
	draw_jerry();
	draw_tom();
	draw_trap();
	draw_cheese();
}

// Setup for level 2 screen
void level_two(void){
	clear_screen();
	level = 2;
	clear_screen();
	status_bar_level2();
	level2_walls();
	draw_all();
	move_jerry();
	update_tom();
	update_cheese();
	cheese_eaten();
	jerry_lose_life();
	show_screen();
}

// Setup for level 1 screen
bool levelpress = false;
void level_one(void){
	clear_screen();
	jerry_lose_life();
	initial_walls();
	move_jerry();
	cheese_eaten();
	status_bar();
	show_screen();
	levelpress = false;
}


// Process for game
void process(void) {

	if(levelpress == false){
		level_one();
		pause_game();
		draw_all();
		move_jerry();
		update_tom();
		update_cheese();
		update_trap();
		drop_trap();
		}
	if (score == 5){
		setup_door();
		draw_door(); // Doesn't work
		}

		show_screen();

		// For going to the next level
		if(BIT_IS_SET(PINF, 6)){
			levelpress = true;
		}

		// Go to level two if SW2 is pressed
		if(levelpress == true){
			level_two();
			pause_game();
			draw_all();
			move_jerry();
			update_tom();
			update_cheese();
			update_trap();
			drop_trap();
		}
}



// Buffer to store print lines to Puttom_y
char buffer2[400];

// For usb communication with Puttom_y and Teensy
void serial_comms(void) {
int16_t char_code = usb_serial_getchar();

	if(char_code == 'a' && (jerry_x > 0)){// move left
		//register and pin
		usb_serial_send( "Received 'a'\r\n" );
		jerry_x -= 1;
	}

	else if (char_code == 'a' && (jerry_x <= 0)){ // Out of bounds error
		usb_serial_send( "Received 'a', but you have reached the border's limit\r\n" );
	}

	else if(char_code == 'd' && (jerry_x < 79)){ // move right
	//register and pin
		usb_serial_send( "Received 'd'\r\n" );
		jerry_x += 1;
	}

	else if(char_code == 'd' && (jerry_x >= 79)){// Out of bounds error
		usb_serial_send( "Received 'd', but you have reached the border's limit\r\n" );
	}

	else if(char_code == 'w' && (jerry_y > STATUS_BAR_HEIGHT)){// move up
	//register and pin
		usb_serial_send( "Received 'w'\r\n" );
		jerry_y -= 1;
	}

	else if(char_code == 'w' && (jerry_y <= STATUS_BAR_HEIGHT)){// Out of bounds error
		usb_serial_send( "Received 'w', but you have reached the border's limit\r\n" );
	}

	else if(char_code == 's' && (jerry_y < 41)){ // move down
	//register and pin
		usb_serial_send( "Received 's'\r\n" );
		jerry_y += 1;
	}
	else if(char_code == 's' && (jerry_y >= 41)){// Out of bounds error
		usb_serial_send( "Received 's', but you have reached the border's limit\r\n" );
	}

	else if(char_code == 'l'){ // change back to level 1
	//register and pin
		levelpress = true;
		usb_serial_send( "Received 'l'\r\n" );
		if (levelpress == true){
			levelpress = false;
		}
	}

	else if(char_code == 'p'){ // pause
	//register and pin
		pause = true;
		pause_game();
		usb_serial_send( "Received 'p'\r\n" );
	}

	else if(char_code == 'i'){//info for game
	//register and pin
		super_jerry();
    
		if (not_paused_status == true){
			is_game_paused = "False";
		}
		else{
			is_game_paused = "True";
		}
		//Print string with buffered args
		snprintf( buffer2, sizeof(buffer2), "\r\nTime = 0%d:%d\r\nCurrent level number = %d\r\nJerry's lives left = %d\r\nScore = %d\r\nNumber of fireworks on screen = %d\r\nNumber of mousetraps on screen = %d\r\nNumber of cheese on screen = %d\r\nNumber of cheese consumed in current room = %d\r\nSuper Jerry Mode = %s\r\nGame Paused = %s\r\n", min, secs, level, health, score, fireworks_current, mousetraps_current, cheese_on_screen, cheese_consumed, jerrymode, is_game_paused);
		usb_serial_send (buffer2);
	}

	else if (char_code >= 0) {
		// For when a key or command is not found
		snprintf( buffer2, sizeof(buffer2), "Command not found '%c'\r\n", char_code );
		usb_serial_send( buffer2 );
		}
}

//	USB serial set up.
void usb_serial_send(char * message) {
	usb_serial_write((uint8_t *) message, strlen(message));
}

void setup_usb_serial(void) {
// Set up LCD and display message
	lcd_init(LCD_DEFAULT_CONTRAST);

	show_screen();
	usb_init();

	while ( !usb_configured() ) {
	// Block until USB is read.
	}
}

// Main function to run game
int main(void) {
	setup();

	// Run loop unless game over
	for ( ;; ) {

		serial_comms();
		process();
		_delay_ms(100);

		if (health == -1){
			game_over();
		break;
		}
	}
							            
	return 0;
}
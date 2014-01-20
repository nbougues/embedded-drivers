/******************************************************************************
 * LC7981/HD61830 Graphics LCD Driver
 *
 * Configured for the Samsung LJ41-00192B 160x80 monochrome graphics lcd.
 *
 * Written by Vanya A. Sergeev - <vsergeev@gmail.com>                         
 *
 * February 16, 2007                                                       
 *
 * Modified 01/2014 by Nicolas Bougues <nicolas@bougues.net>
 *
***************************************************************************/

/* lc7981.c: Source code for the LC7981/HD61830 graphics lcd driver.
 * The hardware port defines can be found in lc7981.h. */

#include "lc7981.h"
#ifndef LC7981_DRIVER_ONLY
#include "graphics.h"
#include "draw_penguin.c"
#include "uparrow.xbm"
#endif

/** 
 * Strobes the Enable control line to trigger the lcd to process the
 * transmitted instruction.
 */
void lcd_strobe_enable(void) {
	lcd_enable_high();
	__asm("nop;"); __asm("nop;"); __asm("nop;");
	lcd_enable_low();
	__asm("nop;"); __asm("nop;"); __asm("nop;");
}

/**
 * Older implementation : just wait
 *
 * Waits for the busy flag to clear, which should take
 * around the maximum time for an instruction to complete.
 * Note, LCD operation is kind of sensitive to this configuration. If the delay
 * is too fast, the LCD will miss some pixels when it is really put through
 * a stress test. This dela time seems to work great.
 */
/*void lcd_wait_busy(void) {
	_delay_us(4);
}*/

/**
 * For board layout reasons, I had to revert all the data lines on the
 * PCB (D0 -> D7, D1 -> D6... D7 -> D0), so I "correct" it in software
 */
#ifdef LC7981_REVERSED_DATA_PORT
unsigned char reverse(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}
#else
#define reverse(a) a
#endif

/**
 * Nicer implementation
 *
 * Wait for the busy flag to clear
 */
void lcd_wait_busy(void) {
	unsigned char data;
	/*Set RW and RS high */
	lcd_rw_high();
	lcd_rs_high();
	__asm("nop;"); __asm("nop;"); __asm("nop;");

	/* Wait until busy flag on last bit of the data port clears. */

	/* Switch the data port to input */
	LCD_DATA_DDR = 0;
	lcd_enable_high();
	do {
		data = LCD_DATA_PIN;
	} while (data & reverse(0x80));
	lcd_enable_low();

	/* Switch the data port to output */
	LCD_DATA_DDR = 0xFF;
}

/**
 * Writes a raw instruction to the LCD. 
 * @param command The 4-bit instruction code.
 * @param data The 8-bit paramater/data to the specified instruction.
 */
void lcd_write_command(unsigned char command, unsigned char data) {
	/* Wait for the busy flag to clear */
	lcd_wait_busy();
	
	/* Set RW low, RS high to write the instruction command */
	lcd_rw_low();
	lcd_rs_high();
	/* Instruction commands are a maximum of 4 bits long, so 
	 * just mask off the rest. */
	LCD_DATA_PORT = reverse((command&0x0F));
	__asm("nop;"); __asm("nop;"); __asm("nop;");
	lcd_strobe_enable();
	__asm("nop;"); __asm("nop;"); __asm("nop;");

	/* Set RW low, RW low to write the instruction data */
	lcd_rw_low();
	lcd_rs_low();
	LCD_DATA_PORT = reverse(data);
	__asm("nop;"); __asm("nop;"); __asm("nop;");
	lcd_strobe_enable();
}

/**
 * Reads a byte from the LCD
 * @param data A pointer to a char receiving the read byte
 */
void lcd_read_data(unsigned char *data) {
	/* Wait for the busy flag to clear */
	lcd_wait_busy();
	
	/* Set RW low, RS high to write the instruction command */
	lcd_rw_low();
	lcd_rs_high();
	/* Instruction commands are a maximum of 4 bits long, so 
	 * just mask off the rest. */
	LCD_DATA_PORT = reverse(LCD_CMD_READ_DATA);
	__asm("nop;"); __asm("nop;"); __asm("nop;");
	lcd_strobe_enable();
	__asm("nop;"); __asm("nop;"); __asm("nop;");

	/* Switch the data port to input */
	LCD_DATA_DDR = 0;

	/* Set RW low, RS low to read the data */
	lcd_rw_high();
	lcd_rs_low();
	lcd_enable_high();
	__asm("nop;"); __asm("nop;"); __asm("nop;");

	/* Read the data */
	*data = reverse(LCD_DATA_PIN);
	lcd_enable_low();

	/* Switch the data port back to output */
	LCD_DATA_DDR = 0xFF;
}

/**
 * Initializes the LCD in graphics mode.
 * Uses a character pitch of 8 (8 bits are plotted whenever a byte is drawn)
 */ 
void lcd_graphics_init(void) {
	unsigned char commandData;

	/* Set the data direction registers apprioriately */
	LCD_DATA_DDR = 0xFF;
	LCD_CTRL_DDR |= (1<<LCD_CTRL_RS)|(1<<LCD_CTRL_RW)|(1<<LCD_CTRL_E);

	/* Assert all control lines to low */
	lcd_rw_low();
	lcd_rs_low();
	lcd_enable_low();

	/* Send mode configuration command with
	 * Toggle Display On, Master, Mode Graphics bits set */
	commandData = LCD_MODE_ON_OFF | LCD_MODE_MASTER_SLAVE | LCD_MODE_MODE;
	lcd_write_command(LCD_CMD_MODE, commandData);

	/* Send the set character pitch command with horizontal
 	 * character pitch of 8 (so 8 pixels are painted when we draw) */	
	commandData = LCD_CHAR_PITCH_HP_8;
	lcd_write_command(LCD_CMD_CHAR_PITCH, commandData);
	
	/* Send the number of characters command with the total
	 * number of graphics bytes that can be painted horizontally 
	 * (width/8) */
	commandData = (LCD_WIDTH/8)-1;
	lcd_write_command(LCD_CMD_NUM_CHARS, commandData);

	/* Set the time division */
	commandData = 128-1;
	lcd_write_command(LCD_CMD_TIME_DIVISION, commandData);
	
	/* Set the display low/high start address to 0x00 (left corner) */
	commandData = 0x00;
	lcd_write_command(LCD_CMD_DISPLAY_START_LA, commandData);
	lcd_write_command(LCD_CMD_DISPLAY_START_HA, commandData);

	/* Reset the cursor to home 0x00 (left corner) */
	commandData = 0x00;
	lcd_write_command(LCD_CMD_CURSOR_LA, commandData);
	lcd_write_command(LCD_CMD_CURSOR_HA, commandData);
}

/**
 * Moves the LCD cursor to the specified coordinates. 
 * @param x The new x coordinante of the cursor.
 * @param y The new y coordinante of the cursor.
 */
void lcd_graphics_move(unsigned short x, unsigned short y) {
	unsigned short pos;

	/* Calculate the raw address in terms of bytes on the screen */
	pos = ((y*LCD_WIDTH)+x)/8;

	/* Move the cursor to the new address */
	lcd_write_command(LCD_CMD_CURSOR_LA, pos&0xFF);
	lcd_write_command(LCD_CMD_CURSOR_HA, pos>>8);
}

/**
 * Draws a byte to the LCD at the current LCD's cursor location.
 * @param data The byte to draw. The pixels are drawn MSB to LSB.
 */
void lcd_graphics_draw_byte(unsigned char data) {
	lcd_write_command(LCD_CMD_WRITE_DATA, data);
}

/**
 * Plots a byte at the specified coordinates. 
 * @param x The x coordinante of the byte to be drawn.
 * @param y The y coordinante of the byte to be drawn.
 * @param data The byte to draw. The pixels are drawn MSB to LSB.
 */
void lcd_graphics_plot_byte(unsigned short x, unsigned short y, unsigned char data) {
	lcd_graphics_move(x, y);
	lcd_graphics_draw_byte(data);
}

/**
 * Plots a pixel at the specified coordinates.
 * @param x The x coordinante of the pixel.
 * @param y The y coordinante of the pixel.
 * @param state PIXEL_ON to set the pixel, otherwise pixel will be cleared.
 */
void lcd_graphics_plot_pixel(unsigned short x, unsigned short y, unsigned char state) {
	unsigned char pos;
	
	lcd_graphics_move(x, y);
	/* Since lcd_graphics_move() moves the cursor to a particular
	 * byte, not bit, we need the relative distance to the specified 
	 * bit we are going to set/clear. */
	pos = x%8;

	if (state == PIXEL_ON) 
		lcd_write_command(LCD_CMD_SET_BIT, pos);
	else 
		lcd_write_command(LCD_CMD_CLEAR_BIT, pos);
}

/**
 * Clears the LCD screen 
 */
void lcd_graphics_clear(void) {
	unsigned short i;
	/* Move cursor to home (top left corner) */
	lcd_graphics_move(0, 0);
	/* Draw empty bytes to ocucpy the entire screen */
	for (i = 0; i < ((LCD_WIDTH*LCD_HEIGHT)/8); i++) 
		lcd_graphics_draw_byte(0x00);
}

/**
 * Scrolls the LCD upwards
 * @param lines The number of lines to scroll upwards
 */
void lcd_scroll_screen (unsigned char lines)
{
	unsigned char i,j;
	unsigned char data[LCD_WIDTH/8];

	for (i = lines; i < LCD_HEIGHT - lines; i++) {

		/* Move to start of line */
		lcd_graphics_move (0, i);

		lcd_read_data(data);

		/* Read one line */
		for (j=0; j < LCD_WIDTH/8; j++) {
			lcd_read_data(&data[j]);
		}

		/* Move to start of line n lines up */
		lcd_graphics_move (0, i-lines);

		/* Write one line */
		for (j=0; j < LCD_WIDTH/8; j++) {
			lcd_write_command(LCD_CMD_WRITE_DATA, data[j]);
		}

	}

	/* Clear the bottom of the screen */
	for (i = LCD_HEIGHT - lines; i < LCD_HEIGHT; i++) {
		lcd_graphics_move (0, i);

		for (j=0; j < LCD_WIDTH/8; j++) {
			lcd_write_command(LCD_CMD_WRITE_DATA, 0);
		}
	}
}

#ifndef LC7981_DRIVER_ONLY
int main(void) {

	lcd_graphics_init();
	lcd_graphics_clear();
	g_draw_rectangle(6, 5, 100, 30);
	g_draw_rectangle(8, 7, 100, 30);
	g_draw_rectangle(10, 9, 100, 30);
	g_draw_rectangle(12, 11, 100, 30);
	g_draw_string(17, 15, "Graphics Demo!\nHello World!");
	draw_penguin();
	lcd_graphics_draw_xbm(140, 10, uparrow_width, uparrow_height, uparrow_bits);
	g_draw_string(22, 44, "!\"#$%&'=\n()*+,-./\n:;<>?@\[\n]^_`|{}");
	while (1) ;
	return 0;

}
#endif

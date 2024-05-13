#include "ssd1306.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "font8x8_basic.h"

#define tag "SSD1306"

#define I2C_NUM I2C_NUM_0
#define PACK8 __attribute__((aligned( __alignof__( uint8_t ) ), packed ))

typedef union out_column_t 
{
	uint32_t u32;
	uint8_t  u8[4];
} PACK8 out_column_t;

void i2c_master_init()
{
	//setup the i2c configuration using
	//i2c_config_t
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = 21,
		.scl_io_num = 22,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 100000
	};
	//error checking
	ESP_ERROR_CHECK(i2c_param_config(I2C_NUM, &i2c_config));
	ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM, I2C_MODE_MASTER, 0, 0, 0));
}


void ssd1306_init() 
{
	esp_err_t espRc;

	//*********************** I2C INITIALIZATION ***********************
	//the function create an i2c command handle using 
	//i2c_command_link_create();
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	//start an i2c transaction with
	//i2c_master_start
	i2c_master_start(cmd);

	//oled i2c address is writeen to the bus with the write bit
	//(OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);


	//*********************** SPECIFIC COMMAND SENT ***********************
	//set charge pump to 0x14
	i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP, true);
	i2c_master_write_byte(cmd, 0x14, true);

	// reverse left-right mapping
	i2c_master_write_byte(cmd, OLED_CMD_SET_SEGMENT_REMAP, true); 
	// reverse up-bottom mapping
	i2c_master_write_byte(cmd, OLED_CMD_SET_COM_SCAN_MODE, true); 

	//set the display to normal mode
	i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_NORMAL, true);
	//turn off display
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_OFF, true);
	//turn on display
	i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_ON, true);

	//stop transaction with
	//i2c_master_stop(cmd);
	i2c_master_stop(cmd);


	//*********************** ERROR HANDLING ***********************
	//the result of i2c command execution is checked using
	//i2c_master_cmd_begin();
	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	//if successful (ESP_OK), a log message indiacates successful configuration
	if (espRc == ESP_OK) 
	{
		ESP_LOGI(tag, "OLED configured successfully");
	} 
	//otherwise, and error log message is printed
	else 
	{
		ESP_LOGE(tag, "OLED configuration failed. code: 0x%.2X", espRc);
	}
	i2c_cmd_link_delete(cmd);
}

void task_ssd1306_display_text(const void *arg_text) 
{
	//*********************** INITIALIZATION ***********************
	//the function receives a pointer to the text to be displayed (arg_text)
	char *text = (char*)arg_text;
	//calculate the length of the text using strlen(text);
	uint8_t text_len = strlen(text);

	//*********************** PAGE AND COLUMN SETUP ***********************
	//create a command cmd
	i2c_cmd_handle_t cmd;

	//current page is 0
	uint8_t cur_page = 0;

	//create an i2c command handle using 
	//i2c_cmd_link_create()
	cmd = i2c_cmd_link_create();

	//start an i2c transaction and send start bit with 
	//i2c_master_start()
	i2c_master_start(cmd);

	//oled i2c address is written to the bus with i2c_master_write_byte()
	//address  frame (cmd,address,ack)
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);	

	//control byte for command stream
	//read/write bit (cmd,data,n,ack)
	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);	

	//the column and line are reset to 0
	i2c_master_write_byte(cmd, 0x00, true); 
	i2c_master_write_byte(cmd, 0x10, true); 

	//current page is set to 0
	i2c_master_write_byte(cmd, 0xB0 | cur_page, true); // reset page

	//stop and i2c transaction with sending stop bit
	i2c_master_stop(cmd);

	//send all the commands in the specified command handle (cmd_handle) to i2c bus
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);	//(i2c_port,cmd,wait)

	//delete command
	i2c_cmd_link_delete(cmd);	//(cmd)

	for (uint8_t i = 0; i < text_len; i++) 
	{
		//meet \n, enter to new line by increase current page and reset column and line
		if (text[i] == '\n') 
		{
			cmd = i2c_cmd_link_create();
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

			i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
			i2c_master_write_byte(cmd, 0x00, true); // reset column
			i2c_master_write_byte(cmd, 0x10, true);
			i2c_master_write_byte(cmd, 0xB0 | ++cur_page, true); // increment page

			i2c_master_stop(cmd);
			i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
			i2c_cmd_link_delete(cmd);
		} 
		//otherwise, send the character data to the display using
		//oled_control_byte_data_stream
		else 
		{
			cmd = i2c_cmd_link_create();
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

			i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
			i2c_master_write(cmd, font8x8_basic_tr[(uint8_t)text[i]], 8, true);

			i2c_master_stop(cmd);
			i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
			i2c_cmd_link_delete(cmd);
		}
	}

	//delete task
	vTaskDelete(NULL);
}

void task_ssd1306_display_clear() 
{
	//*********************** INITIALIZATION ***********************
	//create command using i2c_cmd_handle_t
	i2c_cmd_handle_t cmd;

	//initialize and array called clear with 128 zeros to represent the cleaned display content
	uint8_t clear[128];

	for (uint8_t i = 0; i < 128; i++) 
	{
		clear[i] = 0;
	}

	//*********************** PAGE-BY-PAGE CLEARING ***********************
	//the function iterates through each of the 8 display pages
	//for each page
	for (uint8_t i = 0; i < 8; i++) 
	{
		//create i2c command link using i2c_cmd_link_create()
		cmd = i2c_cmd_link_create();

		//send start bit using i2c_master_start()
		i2c_master_start(cmd);

		//oled i2c address is written to the bus with the write bit
		i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

		//the control byte for a single command is sent
		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_SINGLE, true);
		//the page address 0 to 7 is set using 0xB0|i
		i2c_master_write_byte(cmd, 0xB0 | i, true);

		//the data stream control byte is used
		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
		//clear[i]=0 is sent to the display
		i2c_master_write(cmd, clear, 128, true);

		//stop command by sending stop bit
		i2c_master_stop(cmd);
		//send all the commands in the specified command handle (cmd_handle) to i2c bus
		i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);

		//delete command
		i2c_cmd_link_delete(cmd);
	}

	vTaskDelete(NULL);
}

//dev: 		a pointer to the screen structure
//page: 	the page (vertical position) where the image will be displayed
//seg: 		the segment (horizontal position) where the image will start
//images: 	a pointer to the image data (pixel values)
//width: 	the width of the image in the pixel
//calculates the lower and higher column start addresses based on the seg value
//responsible for displaying an image on ssd1306
void ssd1306_display_image(Screen_t * dev, int page, int seg, uint8_t * images, int width) 
{
	i2c_cmd_handle_t cmd;

	uint8_t columLow = seg & 0x0F;
	uint8_t columHigh = (seg >> 4) & 0x0F;

	//*********************** PAGE AND COLUMN SETUP ***********************
	//create i2c command link using i2c_cmd_link_create()
	cmd = i2c_cmd_link_create();

	//start bit
	i2c_master_start(cmd);
	//send oled i2c address and write bit
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	//send data 
	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
	// Set Lower Column Start Address for Page Addressing Mode
	i2c_master_write_byte(cmd, (0x00 + columLow), true);
	// Set Higher Column Start Address for Page Addressing Mode
	i2c_master_write_byte(cmd, (0x10 + columHigh), true);
	// Set Page Start Address for Page Addressing Mode
	i2c_master_write_byte(cmd, 0xB0 | page, true);

	//stop bit
	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
	i2c_master_write(cmd, images, width, true);

	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);
}

//src: source byte from which a specific bit will be copied
//srcBits: the position 0 to 7 of the bit to be copied from the source
//dst: the destination byte where the copied bit will be placed
//dstBits: the position 0 to 7 where the copied bit will be set or cleared in the destination
//used to manipultate individual bits within two bytes
uint8_t ssd1306_copy_bit(uint8_t src, int srcBits, uint8_t dst, int dstBits)
{
	//calculate smask and dmask for the specified bit positions
	uint8_t smask = 0x01 << srcBits;
	uint8_t dmask = 0x01 << dstBits;

	//extracts the value of the source bit using bitwise and 
	uint8_t _src = src & smask;

	//if the source bit is set (non-zero), the corresponding bit in the destination is set using bitwise or (dst|mask)
	//if the source bit is clear (zero), the corresponding bit in the destination is cleared using bitwise and (dst&~dmask)
	uint8_t _dst;
	if (_src != 0) {
		_dst = dst | dmask; // set bit
	} else {
		_dst = dst & ~(dmask); // clear bit
	}

	//the modified destination byte _dst is returned
	return _dst;
}

//screen: a pointer to the screen 
//image: a pointer to bitmap image data (pixel values)
//used to displayed bitmap image on ssd1306
void ssd1306_bitmap_picture(Screen_t * screen, uint8_t * image)
{
	//initialize variables for page - segment - destination bit position
	uint8_t wk0, wk1, wk2;
	uint8_t page = 0;
	uint8_t seg = 0;
	uint8_t dstBits = 0;
	int offset = 0;

	//iterates through each row (height) of the display (64 rows)
	//for each row:
	//	iterate through 16 bytes of the image data (each by corresponds to 8 pixels)
	//	for each bytes:
	//		iterates through each bit (source bit) within the byte (from 7 to 0)
	//		copy the source bit from the image data to the corresponding position in the display buffer
	//		the destination bit position (dstBits) is incremented
	//		if all 8bit are processed, it moves to the nex page (if needed) and reses the destination bit position
	for(int _height = 0; _height < 64; _height++) {
		for (int index = 0; index < 16; index++) {
			for (int srcBits = 7; srcBits >= 0; srcBits--) {
				wk0 = screen->_page[page]._segs[seg];

				wk1 = image[index+offset];

				wk2 = ssd1306_copy_bit(wk1, srcBits, wk0, dstBits);

				screen->_page[page]._segs[seg] = wk2;
				
				seg++;
			}
		}
		//delay 1ms
		vTaskDelay(1);
		offset = offset + 16;
		dstBits++;
		seg = 0;
		if (dstBits == 8) {
			page++;
			dstBits=0;
		}
	}
}

//used to display a picture on ssd1306
//screen: a pointer to the screen structure
void ssd1306_display_picture(Screen_t * screen)
{
	//iterate through each of the 8 display pages
	//for each page:
	//	call ssd1306_display_image() function to display the image data from the corresponding page segment
	//	the image width is set to 128
	for (int page = 0; page < 8; page++) {
			ssd1306_display_image(screen, page, 0, screen->_page[page]._segs, 128);
	}
}
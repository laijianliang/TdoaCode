#include <iocc2530.h>
#include "lcd_serial.h"
#include "delay.h"
#include "OSAL.h"
#include "hal_sleep.h"
#include "version.h"
#include "MobilePhone_MenuLib.h"
#if (defined WATCHDOG) &&(WATCHDOG==TRUE)
#include "WatchDogUtil.h"
#endif

static  const   uint8 bat_mode_0[BATTERY_GRAPH_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0x40, 0x01, 0x40, 0x01, 0xC0, 0x01, 0xC0, 0x01, 0x40, 0x01, 0x40, 0x01, 0x7F, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static  const   uint8 bat_mode_1[BATTERY_GRAPH_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0x40, 0x01, 0x40, 0x09, 0xC0, 0x09, 0xC0, 0x09, 0x40, 0x09, 0x40, 0x01, 0x7F, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static  const   uint8 bat_mode_2[BATTERY_GRAPH_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0x40, 0x01, 0x40, 0x25, 0xC0, 0x25, 0xC0, 0x25, 0x40, 0x25, 0x40, 0x01, 0x7F, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static  const   uint8 bat_mode_3[BATTERY_GRAPH_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0x40, 0x01, 0x41, 0x25, 0xC1, 0x25, 0xC1, 0x25, 0x41, 0x25, 0x40, 0x01, 0x7F, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static  const   uint8 bat_mode_4[BATTERY_GRAPH_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0x40, 0x01, 0x49, 0x25, 0xC9, 0x25, 0xC9, 0x25, 0x49, 0x25, 0x40, 0x01, 0x7F, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static  const   uint8 SMS_ICON_data[BATTERY_GRAPH_LEN]   = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFC, 0x60, 0x0C, 0x50, 0x14, 0x48, 0x24, 0x44, 0x44, 0x42, 0x84, 0x41, 0x04, 0x7F, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static  __code const uint8  char_0[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0xE0,0x1F,0xF0,
	0x3E,0xF8,0x3C,0x78,0x78,0x3C,0x78,0x3C,0x78,0x3C,0x70,0x1C,0x70,0x1C,0x70,0x1C,
	0x70,0x1C,0x70,0x1C,0x78,0x3C,0x78,0x3C,0x78,0x3C,0x3C,0x78,0x3E,0xF8,0x1F,0xF0,
	0x0F,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static  __code const uint8  char_1[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x80,0x03,0x80,
	0x0F,0x80,0x1F,0x80,0x03,0x80,0x03,0x80,0x03,0x80,0x03,0x80,0x03,0x80,0x03,0x80,
	0x03,0x80,0x03,0x80,0x03,0x80,0x03,0x80,0x03,0x80,0x03,0x80,0x03,0x80,0x03,0x80,
	0x03,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static  __code const uint8  char_2[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0xC0,0x3F,0xE0,
	0x3D,0xF0,0x78,0xF0,0x78,0x70,0x70,0x70,0x70,0xF0,0x00,0xF0,0x00,0xF0,0x01,0xE0,
	0x03,0xC0,0x07,0x80,0x0F,0x00,0x1E,0x00,0x3C,0x00,0x78,0x00,0x78,0x00,0x7F,0xF0,
	0x7F,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static  __code const uint8  char_3[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0xE0,0x3F,0xF0,
	0x3C,0xF8,0x78,0x78,0x78,0x38,0x70,0x38,0x00,0x78,0x01,0xF8,0x03,0xE0,0x03,0xF0,
	0x01,0xF8,0x00,0x78,0x00,0x38,0x70,0x38,0x78,0x38,0x78,0x78,0x7C,0xF8,0x3F,0xF0,
	0x1F,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static  __code const uint8  char_4[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x01,0xE0,
	0x03,0xE0,0x03,0xE0,0x07,0xE0,0x07,0xE0,0x0E,0xE0,0x1E,0xE0,0x1C,0xE0,0x38,0xE0,
	0x78,0xE0,0x70,0xE0,0x7F,0xFC,0x7F,0xFC,0x00,0xE0,0x00,0xE0,0x00,0xE0,0x00,0xE0,
	0x00,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static  __code const uint8  char_5[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0xF8,0x3F,0xF8,
	0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x3F,0xE0,0x3F,0xF0,0x3C,0xF8,0x38,0x78,
	0x00,0x78,0x00,0x38,0x00,0x38,0x70,0x38,0x78,0x78,0x78,0x78,0x3C,0xF0,0x3F,0xE0,
	0x1F,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static  __code const uint8  char_6[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0xE0,0x0F,0xF0,
	0x1E,0x78,0x1C,0x78,0x3C,0x38,0x3C,0x00,0x3F,0xE0,0x3F,0xF0,0x3E,0xF8,0x3C,0x78,
	0x3C,0x78,0x38,0x38,0x38,0x38,0x38,0x38,0x3C,0x78,0x3C,0x78,0x1E,0xF8,0x0F,0xF0,
	0x07,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static  __code const uint8  char_7[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0xF8,0x3F,0xF8,
	0x00,0x78,0x00,0x78,0x00,0x78,0x00,0x70,0x00,0xF0,0x00,0xF0,0x00,0xF0,0x01,0xE0,
	0x01,0xE0,0x01,0xE0,0x03,0xC0,0x03,0xC0,0x03,0xC0,0x03,0x80,0x07,0x80,0x07,0x80,
	0x07,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static  __code const uint8  char_8[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0xC0,0x1F,0xE0,
	0x3C,0xF0,0x3C,0xF0,0x38,0x70,0x38,0x70,0x3C,0xF0,0x3C,0xF0,0x1F,0xE0,0x1F,0xE0,
	0x3C,0xF0,0x78,0x78,0x70,0x38,0x70,0x38,0x78,0x78,0x78,0x78,0x7C,0xF8,0x3F,0xF0,
	0x1F,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static  __code const uint8  char_9[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x80,0x3F,0xC0,
	0x7D,0xE0,0x78,0xF0,0x78,0xF0,0x70,0x70,0x70,0x70,0x70,0x70,0x78,0xF0,0x7D,0xF0,
	0x3F,0xF0,0x1F,0x70,0x00,0x70,0x00,0xF0,0x70,0xF0,0x78,0xE0,0x7D,0xE0,0x3F,0xC0,
	0x1F,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static __code const uint8  char_percent[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x10,0x7E,0x30,
	0x7E,0x20,0x66,0x60,0x66,0x60,0x66,0xC0,0x66,0xC0,0x7F,0x80,0x7F,0x80,0x3D,0x78,
	0x03,0xFC,0x03,0xFC,0x06,0xCC,0x06,0xCC,0x0C,0xCC,0x0C,0xCC,0x08,0xFC,0x18,0xFC,
	0x10,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static  __code const uint8  char_dot[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x00,0x38,0x00,
	0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static  __code const uint8  char_minus[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0xF8,0x7F,0xF8,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static  __code const uint8  char_celsius[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x00,0x00,
0x0F,0x03,0xF0,0x00,0x19,0x8F,0xFC,0x80,0x19,0x9E,0x07,0x80,0x19,0xB8,0x03,0x80,
0x0F,0x38,0x01,0x80,0x00,0x70,0x01,0x80,0x00,0x70,0x00,0x80,0x00,0x60,0x00,0x80,
0x00,0xE0,0x00,0x00,0x00,0xE0,0x00,0x00,0x00,0xE0,0x00,0x00,0x00,0xE0,0x00,0x00,
0x00,0xE0,0x00,0x00,0x00,0x60,0x00,0x00,0x00,0x70,0x00,0x00,0x00,0x70,0x00,0x80,
0x00,0x30,0x01,0x80,0x00,0x38,0x03,0x00,0x00,0x1C,0x07,0x00,0x00,0x0F,0xFC,0x00,
0x00,0x07,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00
};
enum
{
    SIGNAL_GRAPHIC = 1,
    BATTERY_GRAPHIC
};
/* handle bat or sms icon draw*/
static  uint8 const  * graphicdraw_p;

/* Process side bar */
#define  BAR_START   LCD_LINE_HIGH
#define  BAR_LEN      3*LCD_LINE_HIGH
#define  BAR_BLACK   0xFC
#define  BAR_WHITE   0x00
#define  MAX_BARINV_LEN  BAR_LEN

static  uint8 barinv_beg;
static  uint8 barinv_len;

/*Lcd Backlight */
static uint8    backlight_ctl = BACKLIGHT_CTL_10S;
/***************************
// Local subroutine define
****************************/
static void LcdTestFont();

// Set cursor position
static void LcdSetCurXY(uint8 x, uint8 y);
// Get cursor position
static UINT8_XY LcdGetCurXY(void);
// Print Font
static void LcdPrintFont(uint8 *pdata, uint8 len);
// write lcd reg
static void LcdWriteReg(uint8 addr, uint8 data);
// read lcd reg
static uint8 LcdReadReg(uint8 addr);
// write lcd display memory
static void LcdWriteMem(uint8 data);
// read lcd display memory
static uint8 LcdReadMem(void);

static uint8 LCDConvertChar(uint8 aChar);

static void LcdTestFont(void);

static void LCD_BatSmsIconDraw(uint16 len, uint8, uint8);
/****************************************************************
*** lcd clear
*****************************************************************/
void LCD_Clear(uint8 x_start, uint8 y_start, uint8 x_end, uint8 y_end)
{
    uint8 i = 0;

    LcdWriteReg(MWMR, 0x52);

    LcdSetCurXY(x_start, y_start * LCD_LINE_HIGH);

    if(y_end == y_start)
    {
        for(i = 0; i < x_end - x_start; i++)
        {
            LcdWriteMem(0x00);
        }
    }
    if((y_end - y_start) == 1)
    {
        for(i = 0; i < ((LCD_LINE_WIDTH + 2) - x_start + x_end); i++)
        {
            LcdWriteMem(0x00);
        }
    }
    if((y_end - y_start) == 2)
    {
        for(i = 0; i < (2 * (LCD_LINE_WIDTH + 2) - x_start + x_end); i++)
        {
            LcdWriteMem(0x00);
        }
    }
    if((y_end - y_start) == 3)
    {
        for(i = 0; i < (3 * (LCD_LINE_WIDTH + 2) - x_start + x_end); i++)
        {
            LcdWriteMem(0x00);
        }
    }
}

/****************************************************************
*** clear lcd first line
*****************************************************************/
void LCD_Sig_Bat_Clear(uint8 index)
{
    if(index == SIGNAL_GRAPHIC)
    {
        LcdWriteReg(MWMR, 0x52);
        LcdSetCurXY(1, 0);
        LcdWriteMem(0x00);
    }
    else if(index == BATTERY_GRAPHIC)
    {
        LcdWriteReg(MWMR, 0x52);
        LcdSetCurXY(14, 0);
        LcdWriteMem(0x00);
        LcdWriteMem(0x00);
    }
}


void LCD_ShowCursor(uint8 x, uint8 y)
{
    uint8 data;

    LcdSetCurXY(x, y * LCD_LINE_HIGH);
    data = LcdReadReg(CURCR);
    data &=0x02;		
    data |= 1 << 0; // cursor display on
    data |= 1 << 2; // cursor blink
    LcdWriteReg(CURCR, data);

}
void LCD_CloseCursor(void)
{
    uint8 data;
    data = LcdReadReg(CURCR);
    data &= ~1 << 0; // cursor display off
    data &= ~1 << 2; // close cursor blink
    LcdWriteReg(CURCR, data);
}

/****************************************************************
*** clear one line of lcd
*****************************************************************/
void LCD_Line_Clear(uint8 line_id)
{
    LCD_Clear(0, line_id, LCD_LINE_WIDTH, line_id);
    LCD_CloseCursor();
}
void LCD_ListLine_Clear(uint8 line)
{
    LCD_Clear(0, line, LCD_LINE_WIDTH - 1, line);
}
/****************************************************************
*** clear lcd display
*****************************************************************/
void LcdClearDisplay(void)
{
#if 0
    int i = 0, p = 0;
    LcdWriteReg(BLTR, 0x00);
    LcdWriteReg(MWMR, 0x00);
    LcdWriteReg(XCUR, 0x00);
    LcdWriteReg(YCUR, 0x00);
    for(i = 0; i < 128; i++)
    {
        for(p = 0; p < 8; p++)
        {
            LcdWriteMem(0x00);
        }
    }
#else
    uint8 i = 0;
    LcdWriteReg(BLTR, 0x00);
    LcdWriteReg(MWMR, 0x52);
    LcdWriteReg(XCUR, 0x00);
    LcdWriteReg(YCUR, 0x00);
    for(i = 0; i < 72; i++)
    {
        LcdWriteMem(0x00);
    }
    LCD_CloseCursor();
#endif

}
/****************************************************************
*** Write Lcd register
*****************************************************************/

void LcdWriteReg(uint8 addr, uint8 data)
{
    int8 i = 0;

    LCD_CS = 0;

    DelayUs(20);

    LCD_SCK = 0;
    LCD_SDA = 0;    // RW = 0, write LCD
    DelayUs(2);
    LCD_SCK = 1;
    DelayUs(2);

    LCD_SCK = 0;
    LCD_SDA = 0;    // RS = 0;
    DelayUs(2);
    LCD_SCK = 1;
    DelayUs(2);

    for(i = 7; i >= 0; i--)
    {
        LCD_SCK = 0;
        if(addr & 0x80)
        {
            LCD_SDA = 1;
        }
        else
        {
            LCD_SDA = 0;
        }
        addr = addr << 1;
        DelayUs(8);
        LCD_SCK = 1;
        DelayUs(2);

    }

    for(i = 7; i >= 0; i--)
    {
        LCD_SCK = 0;
        if(data & 0x80)
        {
            LCD_SDA = 1;
        }
        else
        {
            LCD_SDA = 0;
        }
        data = data << 1;
        DelayUs(8);
        LCD_SCK = 1;
        DelayUs(2);
    }
    LCD_SCK = 1;
    LCD_SDA = 1;
    LCD_CS = 1;
}

/****************************************************************
*** Read lcd register
*****************************************************************/
uint8 LcdReadReg(uint8 addr)
{
    int8 i = 0;
    uint8 data = 0;

    LCD_CS = 0;
    DelayUs(20);

    LCD_SCK = 0;
    LCD_SDA = 1;    // RW = 1, read LCD
    DelayUs(1);
    LCD_SCK = 1;
    DelayUs(1);

    LCD_SCK = 0;
    LCD_SDA = 0;    // RS = 0;
    DelayUs(1);
    LCD_SCK = 1;
    DelayUs(1);

    for(i = 7; i >= 0; i--)
    {
        LCD_SCK = 0;
        LCD_SDA = (addr >> i) & 1;  // send addr
        DelayUs(1);
        LCD_SCK = 1;
        DelayUs(1);
    }

    P1DIR &= ~0x40; // set SDA(P1.6) to input

    for(i = 7; i >= 0; i--)
    {
        LCD_SCK = 0;
        DelayUs(1);
        data |= LCD_SDA << i;       // read data
        LCD_SCK = 1;
        DelayUs(1);
    }

    LCD_CS = 1;

    P1DIR |= 0x40;  // set SDA(P1.6) back to output
    return data;
}
/****************************************************************
*** Write lcd memory
*****************************************************************/

void LcdWriteMem(uint8 data)
{
    int8 i = 0;

    LCD_CS = 0;
    DelayUs(20);

    LCD_SCK = 0;
    LCD_SDA = 0;    // RW = 0, write LCD
    DelayUs(8);
    LCD_SCK = 1;
    DelayUs(8);

    LCD_SCK = 0;
    LCD_SDA = 1;    // RS = 1
    DelayUs(8);
    LCD_SCK = 1;
    DelayUs(8);

    for(i = 7; i >= 0; i--)
    {
        LCD_SCK = 0;
        if(data & 0x80)
        {
            LCD_SDA = 1;
        }
        else
        {
            LCD_SDA = 0;
        }
        data = data << 1;
        DelayUs(16);
        LCD_SCK = 1;
        DelayUs(8);
    }

    LCD_SCK = 1;
    LCD_SDA = 1;
    LCD_CS = 1;

//    DelayMs(1);
}
/****************************************************************
*** Read lcd memory
*****************************************************************/
uint8 LcdReadMem(void)
{
    int8 i = 0;
    uint8 data = 0;

    LCD_CS = 0;

    DelayUs(20);

    LCD_SCK = 0;
    LCD_SDA = 1;    // RW = 1, read LCD
    DelayUs(1);
    LCD_SCK = 1;
    DelayUs(1);


    LCD_SCK = 0;
    LCD_SDA = 1;    // RS = 1
    DelayUs(1);
    LCD_SCK = 1;
    DelayUs(1);

    P1DIR &= ~0x40; // set SDA(P1.6) to input

    for(i = 7; i >= 0; i--)
    {
        LCD_SCK = 0;
        data |= LCD_SDA << i;
        DelayUs(1);
        LCD_SCK = 1;
        DelayUs(1);
    }

    LCD_CS = 1;

    P1DIR |= 0x40;  // set SDA(P1.6) back to output
    return data;
}

/*******************************************************************************
//       Initial LCD
*******************************************************************************/
void InitialLcd(void)
{
    P1SEL &= ~(BV(6) | BV(7));          //  P1.6, P1.7 to general io
    P1DIR |= (BV(6) | BV(7));           //  P1.6, P1.7 to output

    P2SEL &= ~(BV(0));          //  p2_0 to general io
    P2DIR |= (BV(0));           //  p2_0 output

    LCD_CS = 1;
    LCD_SCK = 1;
    LCD_SDA = 1;

//    P1_0 = 1;
    DelayMs(5);
//    P1_0 = 0;
    DelayMs(30);
//    P1_0 = 1;
    DelayMs(150);
    
#if (defined WATCHDOG) &&(WATCHDOG==TRUE)
    FeedWatchDog();
#endif

    LcdWriteReg(0x00, 0x00);    // ���������趨������
    DelayMs(5);
    LcdWriteReg(0x01, 0x00);    // ����ʾ,�رռ���
    DelayMs(5);
    LcdWriteReg(0x02, 0x7B);    // 128*64���ļ���
    LcdWriteReg(0x03, 0x00);    // �ڴ�����ģʽ��������������ʾȫ����ģʽ�����Ӵ֣�������
    LcdWriteReg(0x04, 0x00);        // �����ƻ�����������ʾ��������
    LcdWriteReg(0x05, 0x00);    // ���X λ�û�����
    LcdWriteReg(0x06, 0x00);    // ���Y λ�û�����
    LcdWriteReg(0x07, 0x00);    // ����ɨ����ƻ�����
    LcdWriteReg(SWSXR, 0x00);   // X �������ʼ�㻺����
    LcdWriteReg(SWSYR, 0x00);   // Y �������ʼ�㻺����
    LcdWriteReg(SWRXR, 0x00);   // X �������Χ������
    LcdWriteReg(SWRYR, 0x00);   // Y �������Χ������
    LcdWriteReg(0x0C, 0x00);    // ����λ����������
    LcdWriteReg(0x0D, 0x00);    // �Զ��������ƻ�����
    LcdWriteReg(0x0E, 0x00);    // �������ƻ�����
    LcdWriteReg(0x0F, 0x00);    // �ж�״̬������

    displayParam_t  displayParam;     
    Read_display_param_form_flash(&displayParam);
    LcdWriteReg(0x10, displayParam.paramTen);
    DelayMs(5);
    LcdWriteReg(0x12, displayParam.paramTwelve);    
    LcdWriteReg(BLTR, 0x00);
    DelayMs(5);
    
    LcdWriteReg(0x14, 0x00);    // IO �˿ڷ����趨������
    LcdWriteReg(0x15, 0x00);    // IO �˿����ݻ�����
    LcdWriteReg(0x16, 0x80);    // �����ƻ�����
    LcdWriteReg(0x17, 0x00);    // ����ѡ�񻺴���
    LcdWriteReg(0x18, 0x00);    // �������ݻ�����
    LcdWriteReg(0x11, 0xf8);    //ȫ���ڲ��ĵ�ѹ����

#if (defined WATCHDOG) &&(WATCHDOG==TRUE)
    FeedWatchDog();
#endif
    LcdClearDisplay();
    DelayMs(300);
    LcdWriteReg(0x01, 0x02);    //

}

/****************************************************************
*** Set cursor position
*****************************************************************/
void LcdSetCurXY(uint8 x, uint8 y)
{
    LcdWriteReg(XCUR, x);
    DelayMs(1);
    LcdWriteReg(YCUR, y);
    DelayMs(1);
}
/****************************************************************
*** Print font to lcd
*****************************************************************/
void LcdPrintFont(uint8 *pdata, uint8 len)
{
    uint8 i = 0;

    LcdWriteReg(PWRR, 0x02);
    LcdWriteReg(MWMR, 0x47);
    LcdWriteReg(SWRYR, 0x00);

    LcdSetCurXY(0, 0);

    for(i = 0; i < len; i++)
    {
        LcdWriteMem(pdata[i]);
        DelayMs(1);
    }

}
/****************************************************************
*** Font testing
*****************************************************************/
void LcdTestFont()
{
    unsigned int ii;
    unsigned char a[16] = {"���տƼ����޹�˾"};
//    unsigned char b[16]={"    �����ֿ�    "};
    LcdWriteReg(PWRR, 0x02);
    LcdWriteReg(MWMR, 0x43);    // 0x47);
    LcdWriteReg(SWRYR, 0x00);
    LcdWriteReg(XCUR, 0x00);
    LcdWriteReg(YCUR, 0x00);

    LcdSetCurXY(0, 0);
    for(ii = 0; ii < 16; ii++)
    {
        LcdWriteMem(a[ii]);
//      KeyDelay(1000);
        DelayMs(1);
    }

    LcdSetCurXY(0, 16);
    for(ii = 0; ii < 16; ii++)
    {
        LcdWriteMem(a[ii]);
//      KeyDelay(1000);
        DelayMs(1);
    }

    LcdSetCurXY(0, 32);
    for(ii = 0; ii < 16; ii++)
    {
        LcdWriteMem(a[ii]);
//      KeyDelay(1000);
        DelayMs(1);
    }

    LcdSetCurXY(0, 48);
    for(ii = 0; ii < 16; ii++)
    {
        LcdWriteMem(a[ii]);
//      KeyDelay(1000);
        DelayMs(1);
    }

    LcdWriteReg(MWMR, 0x00);
}

uint8 LCDConvertChar(uint8 aChar)
{
    uint8 lChar = 0;

    return lChar;
}
#if 0
void LCDWriteString(char* pStr, uint8 x, uint8 y)
{
    //uint8 strlen = osal_strlen(pStr);
    //UINT8_XY xy = LcdGetCurXY();

}

void LCDWriteChar(uint8 aChar)
{
    UINT8_XY xy;
    if(aChar <= '9' && aChar >= '0')
    {
        LcdWriteReg(MWMR, 0x52);
        LcdWriteMem('0');
        LcdWriteMem(aChar);
    }
    xy = LcdGetCurXY();
    if(xy.y >= LCD_MAX_Y)
    {
        if(xy.x < LCD_MAX_X - 1)
            LcdSetCurXY(xy.x + 1, 0);
        else
            LcdSetCurXY(0, 0);
    }
}
#endif

void LCD_BigAscii_Print(uint8 value, uint8 x, uint8 y)
{
    LcdWriteReg(MWMR, 0x52);
    LcdSetCurXY(x, y * LCD_LINE_HIGH);
    LcdWriteMem(value);
    DelayMs(1);
}

void LCD_Memory_Print(uint8* mem, uint8 len, uint8 x, uint8 y)
{
    uint8 i = 0;

    if(mem == NULL)
        return;

    LcdWriteReg(PWRR, 0x02);
    LcdWriteReg(MWMR, 0x43);
    LcdWriteReg(SWRYR, 0x00);
    LcdWriteReg(BLTR, 0x00);

    if((LCD_LINE_WIDTH * 4 - x - LCD_LINE_WIDTH * y) < len)
        len = LCD_LINE_WIDTH * 4 - x - LCD_LINE_WIDTH * y;

    LcdSetCurXY(x, y * LCD_LINE_HIGH);

    if(len <= LCD_LINE_WIDTH)
    {
        for(i = 0; i < len; i++)
        {
            LcdWriteMem(mem[i]);
            DelayMs(1);
        }
    }
    else if(len <= 2 * LCD_LINE_WIDTH)
    {
        for(i = 0; i < LCD_LINE_WIDTH - x; i++)
        {
            LcdWriteMem(mem[i]);
            DelayMs(1);
        }
        LcdSetCurXY(0, (y + 1)*LCD_LINE_HIGH);
        for(; i < len; i++)
        {
            LcdWriteMem(mem[i]);
            DelayMs(1);
        }
    }
    else if(len <= 3 * LCD_LINE_WIDTH)
    {
        for(i = 0; i < LCD_LINE_WIDTH - x; i++)
        {
            LcdWriteMem(mem[i]);
            DelayMs(1);
        }
        LcdSetCurXY(0, (y + 1)*LCD_LINE_HIGH);
        for(; i < len; i++)
        {
            LcdWriteMem(mem[i]);
            DelayMs(1);
        }
        LcdSetCurXY(0, (y + 2)*LCD_LINE_HIGH);
        for(; i < len; i++)
        {
            LcdWriteMem(mem[i]);
            DelayMs(1);
        }
    }
    else if(len <= 4 * LCD_LINE_WIDTH)
    {
        for(i = 0; i < LCD_LINE_WIDTH - x; i++)
        {
            LcdWriteMem(mem[i]);
            DelayMs(1);
        }
        LcdSetCurXY(0, (y + 1)*LCD_LINE_HIGH);
        for(; i < len; i++)
        {
            LcdWriteMem(mem[i]);
            DelayMs(1);
        }
        LcdSetCurXY(0, (y + 2)*LCD_LINE_HIGH);
        for(; i < len; i++)
        {
            LcdWriteMem(mem[i]);
            DelayMs(1);
        }
        LcdSetCurXY(0, (y + 3)*LCD_LINE_HIGH);
        for(; i < len; i++)
        {
            LcdWriteMem(mem[i]);
            DelayMs(1);
        }
    }

}

void LCD_Str_Print(uint8 *pdata, uint8 x, uint8 y, bool direction)
{
    uint8 len;

    if(pdata == NULL)
        return;

    len = osal_strlen((char*)pdata);

    if(!direction)
    {
        if(len <= LCD_LINE_WIDTH)
            x = LCD_LINE_WIDTH - len;
        else if(len <= 2 * LCD_LINE_WIDTH)
        {
            x = 2 * LCD_LINE_WIDTH - len;
            y = y - 1;
        }
        else if(len <= 3 * LCD_LINE_WIDTH)
        {
            x = 3 * LCD_LINE_WIDTH - len;
            y = y - 2;
        }
        else if(len <= 4 * LCD_LINE_WIDTH)
        {
            x = 4 * LCD_LINE_WIDTH - len;
            y = y - 3;
        }
    }

    LCD_Memory_Print(pdata, len, x, y);
}

uint8 ascii_scan(uint8*p , uint8 len)
{
    uint8 i, j;

    j = 0;
    for(i = 0; i < len; i++)
    {
        if(p[i] >= 0x80)
            j++;
    }
    return j;
}

/*********************************************************************
* @fn      LCD_SMS_Print
*
* @brief   It is called when there are chinese or other double byte charaters in the mem
*
* @param
*
* @return
*/
uint8 LCD_SMS_Print(uint8* mem, uint8 len, uint8 x, uint8 y)
{
    uint8 i, j, k;
    uint8 temp, finish_len, y_next_line;

    k = 0;
    y_next_line = y;
    temp = ((LCD_LINE_WIDTH - x) > len) ? len : (LCD_LINE_WIDTH - x);

    LcdWriteReg(PWRR, 0x02);
    LcdWriteReg(MWMR, 0x43);
    LcdWriteReg(SWRYR, 0x00);
    LcdWriteReg(BLTR, 0x00);

    LcdSetCurXY(x, y * LCD_LINE_HIGH);
    do
    {
        //j = 0;
        /*
        for(i=k;i<(k+temp);i++)
        {
                    if(mem[i]>=0x80)
                j++;
        }*/
        j = ascii_scan(&mem[k], temp);
        if(j % 2 != 0)
        {
            for(i = k; i < (k + temp - 1); i++)
            {
                LcdWriteMem(mem[i]);
                DelayMs(1);
            }
            LcdWriteMem(0);
            DelayMs(1);
            finish_len = temp - 1;
        }
        else
        {
            for(i = k; i < (k + temp); i++)
            {
                LcdWriteMem(mem[i]);
                DelayMs(1);
            }
            finish_len = temp;
        }
        k += finish_len;

        if(++y_next_line > 3)
            break;
        len = len - finish_len;
        if(len > LCD_LINE_WIDTH)
            temp = LCD_LINE_WIDTH;
        else
            temp = len;
        LcdSetCurXY(0, y_next_line * LCD_LINE_HIGH);
    }
    while(len > 0);

    return k;
}

void LCD_Str_Print_Pixel(uint8 *pdata, uint8 x, uint8 y)
{
    LcdWriteReg(PWRR, 0x02);
    LcdWriteReg(MWMR, 0x43);
    LcdWriteReg(SWRYR, 0x00);
    LcdWriteReg(BLTR, 0x00);
    uint8 len = osal_strlen((char*)pdata);

    if(x + len > LCD_LINE_WIDTH)
        return;

    LcdSetCurXY(x, y);
    for(uint8 i = 0; i < len; i++)
    {
        LcdWriteMem(pdata[i]);
        DelayMs(1);
    }
}
uint8 LCD_ID_Show(uint8 id, uint8 x, uint8 y)
{
    uint8 temp = id + '0';

    if(id <= 9)
    {
        LCD_BigAscii_Print(temp, x, y);
        return 1;
    }
    else if(id <= 99)
    {
        temp = id / 10 + '0';
        LCD_BigAscii_Print(temp, x, y);
        temp = id % 10 + '0';
        LCD_BigAscii_Print(temp, x + 1, y);
        return 2;
    }
    else
    {
        temp = id / 100 + '0';
        LCD_BigAscii_Print(temp, x, y);
        temp = (id % 100) / 10 + '0';
        LCD_BigAscii_Print(temp, x + 1, y);
        temp = (id % 100) % 10 + '0';
        LCD_BigAscii_Print(temp, x + 2, y);
        return 3;
    }
}

void LCD_ListLine_Inv(uint8 line)
{
    LCD_Char_Inv(0, line, LCD_LINE_WIDTH - 1);
}
void LCD_Char_Inv(uint8 x, uint8 y, uint8 len)
{
    LcdWriteReg(BLTR, 0x00);
    LcdWriteReg(SWSXR, x);
    LcdWriteReg(SWSYR, y * LCD_LINE_HIGH);
    LcdWriteReg(SWRXR, len - 1);
    LcdWriteReg(SWRYR, 0x80 | LCD_LINE_HIGH);
    LcdWriteReg(BLTR, 0x10);
}
void LCD_Clear_Inv(void)
{
    LcdWriteReg(BLTR, 0x00);
}

uint8 LCD_Signal_Show(uint8 index)
{
    if(index > MAX_SIG)
        return INVALID_INDEX;

    LCD_BigAscii_Print(SIGNAL_ASCII, 0, 0);
    if(index > 0)
        LCD_BigAscii_Print(SIGNAL_ASCII + index, 1, 0);

    return LCD_SUCCESS;
}

/* Note: buffer is in graphicdraw_p, do not pass by input */
void LCD_BatSmsIconDraw(uint16 len, uint8 x, uint8 y)
{
    LcdWriteReg(MWMR, 0x0);
    LcdSetCurXY(x, y);
    uint8 tmp;
    
    for(uint8 i = 0; i < BATTERY_GRAPH_LEN; i++)
    {
        if(i % 2 == 0)
            LcdSetCurXY(x, y + i / 2);
        tmp = *(graphicdraw_p+i);
        LcdWriteMem(tmp);
        DelayMs(1);
    }
}
void LCD_SMS_ICON_Show(uint8 x, uint8 line_id)
{
    uint8 y;

    y = line_id * LCD_LINE_WIDTH;
    graphicdraw_p = SMS_ICON_data;
    LCD_BatSmsIconDraw(BATTERY_GRAPH_LEN, x, y);
    LCD_CloseCursor();
}
uint8 LCD_Battery_Show(uint8 index)
{
    if(index > MAX_BAT)
        return INVALID_INDEX;

    switch(index)
    {
    case NO_BAT:
        graphicdraw_p = bat_mode_0;
        break;
    case QUA_BAT:
        graphicdraw_p = bat_mode_1;
        break;
    case HALF_BAT:
        graphicdraw_p = bat_mode_2;
        break;
    case BIG_BAT:
        graphicdraw_p = bat_mode_3;
        break;
    case FULL_BAT:
        graphicdraw_p = bat_mode_4;
        break;
    }

    LCD_BatSmsIconDraw(BATTERY_GRAPH_LEN, 14, 0);
    return LCD_SUCCESS;
}


void LCD_ProgBar_open(void)
{
    LcdWriteReg(MWMR, 0x0);
    for(uint8 i = 0; i < BAR_LEN; i++)
    {
        LcdSetCurXY(15, BAR_START + i);
        LcdWriteMem(BAR_BLACK);
    }
    barinv_beg = 0;
    barinv_len = 0;
}

void LCD_ProgBar_update(uint8 sel_item, uint8 total_item)
{
    LcdWriteReg(MWMR, 0x52);

    if(sel_item < 9)
    {
        LCD_Clear(LCD_LINE_WIDTH - 2, 0, LCD_LINE_WIDTH, 0);
        LcdSetCurXY(LCD_LINE_WIDTH - 1, 0);
        LcdWriteMem(sel_item + '1');
    }
    else if(sel_item < 99)
    {
        uint8 temp;

        LCD_Clear(LCD_LINE_WIDTH - 3, 0, LCD_LINE_WIDTH, 0);
        LcdSetCurXY(LCD_LINE_WIDTH - 2, 0);
        temp = (sel_item + 1) / 10 + '0';
        LcdWriteMem(temp);
        temp = (sel_item + 1) % 10 + '0';
        LcdWriteMem(temp);
    }
    else
    {
        uint8 temp;

        LCD_Clear(LCD_LINE_WIDTH - 3, 0, LCD_LINE_WIDTH, 0);
        LcdSetCurXY(LCD_LINE_WIDTH - 3, 0);
        temp = (sel_item + 1) / 100 + '0';
        LcdWriteMem(temp);
        temp = ((sel_item + 1) % 100) / 10 + '0';
        LcdWriteMem(temp);
        temp = ((sel_item + 1) % 100) % 10 + '0';
        LcdWriteMem(temp);
    }


    //LcdWriteMem('1'+sel_item);

    LcdWriteReg(MWMR, 0x0);

    for(uint8 i = 0; i < barinv_len; i++)
    {
        LcdSetCurXY(15, BAR_START + barinv_beg + i);
        LcdWriteMem(BAR_BLACK);
    }

    barinv_len = MAX_BARINV_LEN / (total_item + 1);
    if(barinv_len < 1)
    {
        barinv_len = 1;
    }
    barinv_beg = (BAR_LEN - barinv_len) * sel_item / (total_item - 1);

    for(uint8 i = 0; i < barinv_len; i++)
    {
        LcdSetCurXY(15, BAR_START + barinv_beg + i);
        LcdWriteMem(BAR_WHITE);
    }
    return;
}

void LCDSetBackLightCtl(uint8 ctl)
{
    if(ctl <= BACKLIGHT_CTL_30S)//BACKLIGHT_CTL_ALWAYSON)
    {
        backlight_ctl = ctl;
    }
}

uint8 LCDGetBackLightCtl(void)
{
    return backlight_ctl;
}

void LCDDisplayOff()
{
    LcdWriteReg(PWRR, 0x00);
    return;
}
void LCDIntoSleep()
{
     /*turn off driver voltage */
    LcdWriteReg(DRCRB, 0x00);
    LcdWriteReg(CSTR, 0x80);
    LcdWriteReg(DRCRA, 0x00);    
     
    /* turn off  display */    
    LcdWriteReg(PWRR, 0x00);

    DelayMs(10);//must
    
    /* go to sleep */    
    LcdWriteReg(PWRR, 0x01);
    LCD_CS = 1;
    LCD_SCK = 0;
    P1DIR &= ~(0x01 << 6);  // LCD_SDA  p1_6 input
    return;
}
void LCDWakeUp()
{
    LcdWriteReg(PWRR, 0x02);
    return;
}

void do_Sleep(uint32 timeout_ms)
{
     halSleep_immediately( timeout_ms);    
}

void LCD_GraphicMode_Write( uint8 const *p, uint8 len,uint8 x, uint8 y,uint8 width)
{
       uint8 __code const * _p=(uint8 __code const *)p;
	LcdWriteReg(MWMR,0x0);	
	LcdSetCurXY(x, y);	
        uint8 w = width/8;
	for(uint8 i=0; i<len; i++)
	{
		if(i%w == 0)
			LcdSetCurXY(x,y+i/w);
		LcdWriteMem(_p[i]);
	  }
}
void LCD_BigCharPrint(char data, uint8 x, uint8 y)
{
	uint8 const *p = NULL;
	uint8 len;
        uint8 width = 16;
	switch(data)
	{
	case '0':
		{
			p = (uint8 const *)(char_0);
			len = sizeof(char_0);
			break;
		}
	case '1':
		{
			p = (uint8 const *)(char_1);
			len = sizeof(char_1);
			break;
		}
	case '2':
		{
			p = (uint8 const *)(char_2);
			len = sizeof(char_2);
			break;
		}
	case '3':
		{
			p = (uint8 const *)(char_3);
			len = sizeof(char_3);
			break;
		}
	case '4':
		{
			p = (uint8 const *)(char_4);
			len = sizeof(char_4);
			break;
		}
	case '5':
		{
			p = (uint8 const *)(char_5);
			len = sizeof(char_5);
			break;
		}
	case '6':
		{
			p = (uint8 const *)(char_6);
			len = sizeof(char_6);
			break;
		}
	case '7':
		{
			p = (uint8 const *)(char_7);
			len = sizeof(char_7);
			break;
		}
	case '8':
		{
			p = (uint8 const *)(char_8);
			len = sizeof(char_8);
			break;
		}
	case '9':
		{
			p = (uint8 const *)(char_9);
			len = sizeof(char_9);
			break;
		}
	case '%':
		{
			p = (uint8 const *)(char_percent);
			len = sizeof(char_percent);
			break;
		}
	case '.':
		{
			p = (uint8 const *)(char_dot);
			len = sizeof(char_dot);
			break;
		}
	case '-':
		{
			p = (uint8 const *)(char_minus);
			len = sizeof(char_minus);
			break;
		}
	case 'C':
		{
			p = (uint8 const *)(char_celsius);
			len = sizeof(char_celsius);
                     width = 32; //
			break;
		}
	case ' ':
		{
			p = NULL;
			len = 0;
		}
	}
	LCD_GraphicMode_Write((uint8 const *)p,len,x,y,width);
}

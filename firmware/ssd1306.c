#include "ssd1306.h"

#include "ch32fun.h"
#include <string.h>

// SSD1306 I2C address
#define SSD1306_I2C_ADDR 0x3c
// I2C Bus clock rate - must be lower the Logic clock rate
#define SSD1306_I2C_CLKRATE 1000000
// I2C Logic clock rate - must be higher than Bus clock rate
#define SSD1306_I2C_PRERATE 2000000
// I2C Timeout count
#define TIMEOUT_MAX 100000

// event codes we use
#define SSD1306_I2C_EVENT_MASTER_MODE_SELECT                                   \
    ((uint32_t) 0x00030001) /* BUSY, MSL and SB flag */
#define SSD1306_I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED                     \
    ((uint32_t) 0x00070082) /* BUSY, MSL, ADDR, TXE and TRA flags */
#define SSD1306_I2C_EVENT_MASTER_BYTE_TRANSMITTED                              \
    ((uint32_t) 0x00070084) /* TRA, BUSY, MSL, TXE and BTF flags */

#define SSD1306_SET_MEM_MODE     0x20
#define SSD1306_SET_COL_ADDR     0x21
#define SSD1306_SET_PAGE_ADDR    0x22
#define SSD1306_SET_HORIZ_SCROLL 0x26
#define SSD1306_SET_SCROLL       0x2E

#define SSD1306_SET_DISP_START_LINE 0x40

#define SSD1306_SET_CONTRAST    0x81
#define SSD1306_SET_CHARGE_PUMP 0x8D

#define SSD1306_SET_SEG_REMAP        0xA0
#define SSD1306_SET_ENTIRE_ON        0xA4
#define SSD1306_SET_ALL_ON           0xA5
#define SSD1306_SET_NORM_DISP        0xA6
#define SSD1306_SET_INV_DISP         0xA7
#define SSD1306_SET_MUX_RATIO        0xA8
#define SSD1306_SET_DISP             0xAE
#define SSD1306_SET_COM_OUT_DIR      0xC0
#define SSD1306_SET_COM_OUT_DIR_FLIP 0xC0

#define SSD1306_SET_DISP_OFFSET  0xD3
#define SSD1306_SET_DISP_CLK_DIV 0xD5
#define SSD1306_SET_PRECHARGE    0xD9
#define SSD1306_SET_COM_PIN_CFG  0xDA
#define SSD1306_SET_VCOM_DESEL   0xDB

#define SSD1306_PAGE_HEIGHT 8
#define SSD1306_NUM_PAGES   (32 / SSD1306_PAGE_HEIGHT)

// Code borrowed from ch32fun/extralibs/ssd1306_i2c.h
static uint8_t ssd1306_i2c_init(void) {
    // Enable GPIOC and I2C
    RCC->APB1PCENR |= RCC_APB1Periph_I2C1;

    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
    // PC1 is SDA, 10MHz Output, alt func, open-drain
    GPIOC->CFGLR &= ~(0xf << (4 * 1));
    GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF) << (4 * 1);

    // PC2 is SCL, 10MHz Output, alt func, open-drain
    GPIOC->CFGLR &= ~(0xf << (4 * 2));
    GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF) << (4 * 2);

    // load I2C regs
    uint16_t tempreg;

    // Reset I2C1 to init all regs
    RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;
    RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;

    // set freq
    tempreg = I2C1->CTLR2;
    tempreg &= ~I2C_CTLR2_FREQ;
    tempreg |=
        (FUNCONF_SYSTEM_CORE_CLOCK / SSD1306_I2C_PRERATE) & I2C_CTLR2_FREQ;
    I2C1->CTLR2 = tempreg;

    // Set clock config
    tempreg = 0;
    // 36% duty cycle
    tempreg = (FUNCONF_SYSTEM_CORE_CLOCK / (25 * SSD1306_I2C_CLKRATE)) &
              I2C_CKCFGR_CCR;
    tempreg |= I2C_CKCFGR_DUTY;

    tempreg |= I2C_CKCFGR_FS;
    I2C1->CKCFGR = tempreg;

    // Enable I2C
    I2C1->CTLR1 |= I2C_CTLR1_PE;

    // set ACK mode
    I2C1->CTLR1 |= I2C_CTLR1_ACK;

    return 0;
}

// ch32fun/extralibs/ssd1306_i2c.h
static uint8_t ssd1306_i2c_chk_evt(uint32_t event_mask) {
    /* read order matters here! STAR1 before STAR2!! */
    uint32_t status = I2C1->STAR1 | (I2C1->STAR2 << 16);
    return (status & event_mask) == event_mask;
}

// ch32fun/extralibs/ssd1306_i2c.h
static uint8_t ssd1306_i2c_send(uint8_t addr, const uint8_t* data, int sz) {
    int32_t timeout;

    // wait for not busy
    timeout = TIMEOUT_MAX;
    while ((I2C1->STAR2 & I2C_STAR2_BUSY) && (timeout--))
        ;
    if (timeout == -1)
        return 1;

    // Set START condition
    I2C1->CTLR1 |= I2C_CTLR1_START;

    // wait for master mode select
    timeout = TIMEOUT_MAX;
    while ((!ssd1306_i2c_chk_evt(SSD1306_I2C_EVENT_MASTER_MODE_SELECT)) &&
           (timeout--))
        ;
    if (timeout == -1)
        return 1;

    // send 7-bit address + write flag
    I2C1->DATAR = addr << 1;

    // wait for transmit condition
    timeout = TIMEOUT_MAX;
    while ((!ssd1306_i2c_chk_evt(
               SSD1306_I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) &&
           (timeout--))
        ;
    if (timeout == -1)
        return 1;

    // send data one byte at a time
    while (sz--) {
        // wait for TX Empty
        timeout = TIMEOUT_MAX;
        while (!(I2C1->STAR1 & I2C_STAR1_TXE) && (timeout--))
            ;
        if (timeout == -1)
            return 1;

        // send command
        I2C1->DATAR = *data++;
    }

    // wait for tx complete
    timeout = TIMEOUT_MAX;
    while ((!ssd1306_i2c_chk_evt(SSD1306_I2C_EVENT_MASTER_BYTE_TRANSMITTED)) &&
           (timeout--))
        ;
    if (timeout == -1)
        return 1;

    // set STOP condition
    I2C1->CTLR1 |= I2C_CTLR1_STOP;

    // we're happy
    return 0;
}

static void SSD1306_Command(uint8_t cmd) {
    // I2C write process expects a control byte followed by data
    // this "data" can be a command or data to follow up a command
    // Co = 1, D/C = 0 => the driver expects a command
    uint8_t buf[2] = {0x80, cmd};
    ssd1306_i2c_send(SSD1306_I2C_ADDR, buf, sizeof(buf));
}

static void SSD1306_Commands(const uint8_t* buf, int num) {
    for (int i = 0; i < num; i++)
        SSD1306_Command(buf[i]);
}

void SSD1306_Init(void) {
    ssd1306_i2c_init();
    // initialize OLED
    // OLED initialization commands for 128x32
    const uint8_t cmds[] = {
        SSD1306_SET_DISP,   // set display off
        /* memory mapping */
        SSD1306_SET_MEM_MODE,   // set memory address mode 0 = horizontal, 1 =
                                // vertical, 2 = page
        0x00,                   // horizontal addressing mode
        /* resolution and layout */
        SSD1306_SET_DISP_START_LINE,   // set display start line to 0
        SSD1306_SET_SEG_REMAP |
            0x01,   // set segment re-map, column address 127 is mapped to SEG0
        SSD1306_SET_MUX_RATIO,   // set multiplex ratio
        31,                      // Display height - 1
        SSD1306_SET_COM_OUT_DIR |
            0x08,                  // set COM (common) output scan direction.
                                   // Scan from bottom up, COM[N-1] to COM0
        SSD1306_SET_DISP_OFFSET,   // set display offset
        0x00,                      // no offset
        SSD1306_SET_COM_PIN_CFG,   // set COM (common) pins hardware
                                   // configuration. Board specific magic
                                   // number. 0x02 Works for 128x32, 0x12
                                   // Possibly works for 128x64. Other options
                                   // 0x22, 0x32
        0x02,
        /* timing and driving scheme */
        SSD1306_SET_DISP_CLK_DIV,   // set display clock divide ratio
        0x80,                       // div ratio of 1, standard freq
        SSD1306_SET_PRECHARGE,      // set pre-charge period
        0xF1,                       // Vcc internally generated on our board
        SSD1306_SET_VCOM_DESEL,     // set VCOMH deselect level
        0x30,                       // 0.83xVcc
        /* display */
        SSD1306_SET_CONTRAST,   // set contrast control
        0xFF,
        SSD1306_SET_ENTIRE_ON,   // set entire display on to follow RAM content
        SSD1306_SET_NORM_DISP,   // set normal (not inverted) display
        SSD1306_SET_CHARGE_PUMP,   // set charge pump
        0x14,                      // Vcc internally generated on our board
        SSD1306_SET_SCROLL |
            0x00,   // deactivate horizontal scrolling if set. This is necessary
                    // as memory writes will corrupt if scrolling was enabled
        SSD1306_SET_DISP | 0x01,   // turn display on
    };
    SSD1306_Commands(cmds, sizeof(cmds));
}

void SSD1306_Display(const uint8_t* buffer) {
    // in horizontal addressing mode, the column address pointer auto-increments
    // and then wraps around to the next page, so we can send the entire frame
    // buffer in one gooooooo!

    // copy our frame buffer into a new buffer because we need to add the
    // control byte to the beginning

    uint8_t temp_buf[SSD1306_BUFFER_SIZE + 1];
    temp_buf[0] = 0x40;
    memcpy(temp_buf + 1, buffer, SSD1306_BUFFER_SIZE);
    ssd1306_i2c_send(SSD1306_I2C_ADDR, temp_buf, SSD1306_BUFFER_SIZE + 1);
}

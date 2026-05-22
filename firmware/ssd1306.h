#ifndef ssd1306_h
#define ssd1306_h

#include <inttypes.h>

#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      32
#define SSD1306_BUFFER_SIZE ((SSD1306_WIDTH * SSD1306_HEIGHT) / 8)

/**
 * @brief Initialize the SSD1306 display controller.
 *
 * Configures the display hardware and prepares it for use.
 */
void SSD1306_Init();

/**
 * @brief Send a full framebuffer to the SSD1306 display.
 *
 * Updates the display contents using the provided buffer.
 *
 * @param buffer Pointer to the framebuffer data to display.
 *
 * @note The buffer must be 512 bytes in size.
 */
void SSD1306_Display(const uint8_t* buffer);

#endif

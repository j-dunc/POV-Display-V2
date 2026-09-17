// animation_store: flash used to store animations

#include "animation_store.h"

#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

// Reserve space for firmware
#define ANIMATION_FLASH_OFFSET (1u * 1024 * 1024)
#define ANIMATION_REGION_BYTES (PICO_FLASH_SIZE_BYTES - ANIMATION_FLASH_OFFSET)

#define FRAME_BYTES (NUM_COLUMNS * LEDS_PER_WING * CHANNELS)

// flash_range_program only takes whole 256-byte pages, so each frame is padded up
// to a page boundary and stored at a fixed stride
#define FRAME_STRIDE (((FRAME_BYTES + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE) // round up to multiple of 256

// Erase in chunks so interrupts are only held off for one chunk at a time
#define ERASE_CHUNK_BYTES (64u * 1024)

// two phases so nothing reads a half written animation
// Set by animation_store_end_write - 0 until this boot has actually written something
static uint32_t stored_frame_count = 0;
// frame_count passed to begin_write, held until end_write commits it
static uint32_t pending_frame_count = 0;

uint32_t animation_store_frame_count(void) {
    return stored_frame_count;
}

void animation_store_read_column(uint32_t frame_index, int col, uint8_t dest[LEDS_PER_WING][CHANNELS]) {
    uint32_t column_bytes = LEDS_PER_WING * CHANNELS;
    // Calculate flash offset for column - initial offset -> start of frame -> column within it
    uint32_t column_offset = ANIMATION_FLASH_OFFSET + frame_index * FRAME_STRIDE + (uint32_t)col * column_bytes;
    // XIP_BASE is the address where flash is mapped in memory - add columnn offset in flash to get pointer to column
    // Copy raw bytes from column pointer to destination pointer
    memcpy(dest, (const void *)(XIP_BASE + column_offset), column_bytes);
}

// Frame data is streamed through this one page at a time, so writing an animation
// needs 256 bytes of RAM rather than a whole 42KB frame
static uint8_t page_buffer[FLASH_PAGE_SIZE];
static uint32_t page_fill; // how many bytes of page buffer are used
static uint32_t page_offset; // flash offset the next full page lands at

// write page into flash and flush it
// pad with zeros if not full
static void page_flush(void) {
    if (page_fill == 0) return;

    memset(page_buffer + page_fill, 0, FLASH_PAGE_SIZE - page_fill); // pad out a part-filled page
    uint32_t ints = save_and_disable_interrupts(); // interrupts disabled while writing
    flash_range_program(page_offset, page_buffer, FLASH_PAGE_SIZE); // write page to flash
    restore_interrupts(ints);

    page_offset += FLASH_PAGE_SIZE;
    page_fill = 0;
}

// Copies as much of src as fits in remaining space in page_buffer
static void page_append(const uint8_t *src, uint32_t len) {
    while (len > 0) {
        uint32_t space = FLASH_PAGE_SIZE - page_fill;
        uint32_t take = (len < space) ? len : space;

        memcpy(page_buffer + page_fill, src, take);
        page_fill += take;
        src += take;
        len -= take;

        if (page_fill == FLASH_PAGE_SIZE) page_flush();
    }
}

// Erases flash ready for write
bool animation_store_begin_write(uint32_t frame_count) {
    stored_frame_count = 0; // nothing valid to read while a write is in progress

    // Return false if no frames or too many frames for space remaining
    if (frame_count == 0) return false;
    uint32_t needed = frame_count * FRAME_STRIDE;
    if (needed > ANIMATION_REGION_BYTES) return false;

    // Erase a chunk at a time, so interrupts aren't disabled the entire erase
    uint32_t erase_bytes = ((needed + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE; // round up to multiple of flash sector size
    for (uint32_t done = 0; done < erase_bytes; done += ERASE_CHUNK_BYTES) {
        uint32_t chunk = erase_bytes - done;
        if (chunk > ERASE_CHUNK_BYTES) chunk = ERASE_CHUNK_BYTES;

        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(ANIMATION_FLASH_OFFSET + done, chunk);
        restore_interrupts(ints);
    }

    page_offset = ANIMATION_FLASH_OFFSET;
    page_fill = 0;
    pending_frame_count = frame_count;
    return true;
}


void animation_store_write_column(int col, const uint8_t rgb[LEDS_PER_WING][CHANNELS]) {
    page_append((const uint8_t *)rgb, LEDS_PER_WING * CHANNELS);
    if (col == NUM_COLUMNS - 1) page_flush(); // flushes last column even if page note full - ensures frame meets stride
}

void animation_store_end_write(void) {
    stored_frame_count = pending_frame_count;
}

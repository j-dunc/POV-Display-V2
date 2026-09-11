#include "led_shift.h"
#include "led_shift.pio.h"
#include "pins.h"

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"


// System clock is 150 MHz, 150/3 = 50, 2 SM cycles per output bit gives effective 25 MHz clock rate
#define SM_CLKDIV 3.0f

static PIO lhs_pio = pio0;
static uint lhs_sm;
static int lhs_dma;

static PIO rhs_pio = pio0;
static uint rhs_sm;
static int rhs_dma;

static void init_lhs_sm(void) {
    // Load program into PIO memory and reserve SM
    uint offset = pio_add_program(lhs_pio, &led_shift_lhs_program);
    lhs_sm = pio_claim_unused_sm(lhs_pio, true);

    // Configure clock pin
    pio_gpio_init(lhs_pio, PIN_LHS_CLK);
    pio_sm_set_consecutive_pindirs(lhs_pio, lhs_sm, PIN_LHS_CLK, 1, true);

    // Set GPIO 0-4,6-8,10 as SM outputs, do not claim GPIO 5,9 so that SM out does not affect them
    pio_sm_set_consecutive_pindirs(lhs_pio, lhs_sm, 0, 5, true);
    pio_sm_set_consecutive_pindirs(lhs_pio, lhs_sm, 6, 3, true);
    pio_sm_set_consecutive_pindirs(lhs_pio, lhs_sm, 10, 1, true);
    for (int i = 0; i < 9; i++) pio_gpio_init(lhs_pio, LHS_DATA_PINS[i]);

    // Configure SM
    pio_sm_config c = led_shift_lhs_program_get_default_config(offset);
    sm_config_set_out_pins(&c, 0, 11); // GPIO 0-10 are out
    sm_config_set_sideset_pins(&c, PIN_LHS_CLK);
    sm_config_set_out_shift(&c, /*shift_right=*/true, /*autopull=*/true, /*pull_threshold=*/11); // Auto grab next word once 11 bits are shifted out
    sm_config_set_clkdiv(&c, SM_CLKDIV);
    pio_sm_init(lhs_pio, lhs_sm, offset, &c);
    pio_sm_set_enabled(lhs_pio, lhs_sm, true);
}

static void init_rhs_sm(void) {
    // Load program into PIO memory and reserve SM
    uint offset = pio_add_program(rhs_pio, &led_shift_rhs_program);
    rhs_sm = pio_claim_unused_sm(rhs_pio, true);

    // Configure clock pin
    pio_gpio_init(rhs_pio, PIN_RHS_CLK);
    pio_sm_set_consecutive_pindirs(rhs_pio, rhs_sm, PIN_RHS_CLK, 1, true);

    // Set GPIO 21-29 as SM outputs, note these are contiguous
    pio_sm_set_consecutive_pindirs(rhs_pio, rhs_sm, 21, 9, true);
    for (int i = 0; i < 9; i++) pio_gpio_init(rhs_pio, RHS_DATA_PINS[i]);

    // Configure SM
    pio_sm_config c = led_shift_rhs_program_get_default_config(offset);
    sm_config_set_out_pins(&c, 21, 9); // GPIO 21-29 are out
    sm_config_set_sideset_pins(&c, PIN_RHS_CLK);
    sm_config_set_out_shift(&c, /*shift_right=*/true, /*autopull=*/true, /*pull_threshold=*/9); // Auto grab next word once 9 bits are shifted out
    sm_config_set_clkdiv(&c, SM_CLKDIV);
    pio_sm_init(rhs_pio, rhs_sm, offset, &c);
    pio_sm_set_enabled(rhs_pio, rhs_sm, true);
}

static int init_dma_channel(PIO pio, uint sm) {
    int chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16); // Move 16-bit words
    channel_config_set_read_increment(&c, true); // Walk forward through array
    channel_config_set_write_increment(&c, false); // Fixed destination (PIO FIFO)
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true)); // Only transfer when space in PIO FIFO
    dma_channel_set_config(chan, &c, false);
    dma_channel_set_write_addr(chan, &pio->txf[sm], false); // Set destination as FIFO for SM used
    return chan;
}

void led_shift_init(void) {
    init_lhs_sm();
    init_rhs_sm();
    lhs_dma = init_dma_channel(lhs_pio, lhs_sm);
    rhs_dma = init_dma_channel(rhs_pio, rhs_sm);
}

void led_shift_start(const uint16_t lhs_words[16], const uint16_t rhs_words[16]) {
    // Set DMA source as words array with length of 16 - send both LHS and RHS in parallel.
    // Returns immediately - the DMA/PIO shift itself runs in the background from here
    dma_channel_transfer_from_buffer_now(lhs_dma, lhs_words, 16);
    dma_channel_transfer_from_buffer_now(rhs_dma, rhs_words, 16);
}

void led_shift_finish(void) {
    // Busy-wait until DMA finished
    dma_channel_wait_for_finish_blocking(lhs_dma);
    dma_channel_wait_for_finish_blocking(rhs_dma);

    // Busy-wait until FIFO is empty
    while (!pio_sm_is_tx_fifo_empty(lhs_pio, lhs_sm) ||
           !pio_sm_is_tx_fifo_empty(rhs_pio, rhs_sm)) {
        tight_loop_contents();
    }
    __asm__ volatile ("nop\n nop\n nop\n nop\n nop\n nop\n"); // ~40ns delay to ensure all bits have been shifted
}

void led_shift_latch(void) {
    gpio_put(PIN_LE, 1);
    __asm__ volatile ("nop\n nop\n nop\n nop\n nop\n nop\n"); // ~40ns delay to ensure proper latching
    gpio_put(PIN_LE, 0);
}

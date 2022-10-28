#include "hardware/flash.h"

/* rationale: half engine, half games? */
/* NOTE: this has to be a multiple of 4096 (FLASH_SECTOR_SIZE) */
#define FLASH_TARGET_OFFSET (512 * 1024)

const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

uint16_t SPRIG_MAGIC[FLASH_PAGE_SIZE/2] = { 1337, 42, 69, 420, 420, 1337 };

static const char *save_read(void) {
  if (memcmp(&SPRIG_MAGIC, flash_target_contents, sizeof(SPRIG_MAGIC)) != 0) {
    puts("no magic :(");
    return NULL;
  }

  /* add a page to get what's after the magic */
  const char *save = flash_target_contents + FLASH_PAGE_SIZE;
  return save;
}


typedef enum {
  UplProg_Header,
  UplProg_Body,
} UplProg;
static struct {
  UplProg prog;
  uint32_t len, len_i;
  char buf[256];
  int page;
} upl_state = {0};

static void upl_flush_buf(void) {
  uint32_t interrupts = save_and_disable_interrupts();
  flash_range_program(FLASH_TARGET_OFFSET + (upl_state.page++) * 256,
                      (void *)upl_state.buf,
                      256);
  restore_interrupts(interrupts);
  memset(upl_state.buf, 0, sizeof(upl_state.buf));
  printf("wrote page (%d/%d)\n",
         upl_state.page,
         (upl_state.len/(FLASH_PAGE_SIZE + 1)));
}

static void upl_stdin_read(void) {
  while (1) {
    int c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT) return;

    switch (upl_state.prog) {
      case UplProg_Header: {
        ((char *)(&upl_state.len))[upl_state.len_i++] = c;
        if (upl_state.len_i >= sizeof(uint32_t)) {
          printf("ok reading %d chars\n", upl_state.len);
          upl_state.prog = UplProg_Body;
          upl_state.len_i = 0;
          upl_state.page = 1; /* skip first, that's for magic */

          int char_len = upl_state.len;
          /* one to round up, one for magic */
          int page_len   = (char_len/FLASH_PAGE_SIZE   + 2) * FLASH_PAGE_SIZE  ;
          int sector_len = (page_len/FLASH_SECTOR_SIZE + 1) * FLASH_SECTOR_SIZE;
          
          uint32_t interrupts = save_and_disable_interrupts();
          flash_range_erase(FLASH_TARGET_OFFSET, sector_len);
          restore_interrupts(interrupts);
        }
      } break;
      case UplProg_Body: {
        // printf("upl char (%d/%d)\n", upl_state.len_i, upl_state.len);
        upl_state.buf[upl_state.len_i++ % 256] = c;
        if (upl_state.len_i % 256 == 0) {
          upl_flush_buf();
        }

        if (upl_state.len_i == upl_state.len) {
          upl_flush_buf();

          uint32_t interrupts = save_and_disable_interrupts();
          flash_range_program(FLASH_TARGET_OFFSET, (void *)SPRIG_MAGIC, FLASH_PAGE_SIZE);
          restore_interrupts(interrupts);
          
          printf("read in %d chars\n", upl_state.len);
          memset(&upl_state, 0, sizeof(upl_state));
        }
      } break;
    }
  }
}

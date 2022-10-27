typedef enum {
  UplProg_Header,
  UplProg_Body,
  UplProg_Done,
} UplProg;
static struct {
  UplProg prog;
  uint32_t len, len_i;
  char *str;
} upl_state = {0};

static void upl_stdin_read(void) {
  if (upl_state.prog == UplProg_Done) return;

  while (1) {
    int c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT) return;

    switch (upl_state.prog) {
      case UplProg_Header: {
        ((char *)(&upl_state.len))[upl_state.len_i++] = c;
        if (upl_state.len_i >= sizeof(uint32_t)) {
          printf("ok reading %d chars\n", upl_state.len);
          upl_state.prog = UplProg_Body;
          upl_state.str = calloc(upl_state.len+1, 1); /* null term */
          upl_state.len_i = 0;
        }
      } break;
      case UplProg_Body: {
        upl_state.str[upl_state.len_i++] = c;
        if (upl_state.len_i == upl_state.len)
          upl_state.prog = UplProg_Done;
      } break;
      case UplProg_Done: {} break;
    }
  }
}

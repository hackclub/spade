typedef struct {
  void (*song_free)(void *);
  int (*song_chars)(void *p, char *buf, int buf_len);
} PianoOpts;

void piano_init(PianoOpts);
void piano_try_push_samples(void);

int  piano_queue_song(void *, double times);
int  piano_unqueue_song(void *p);
int  piano_is_song_queued(void *p);

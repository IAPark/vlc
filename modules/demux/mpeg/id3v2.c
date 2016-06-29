#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_codec.h>
#include <vlc_input.h>

void foo(demux_t* p_demux) {
      printf("made it to OpenAudio\n");
      const uint8_t *test;
      stream_Peek( p_demux->s, &test, 3);
      printf("%c%c%c\n",test[0], test[1], test[2]);
}

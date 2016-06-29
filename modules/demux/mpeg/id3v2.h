#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_codec.h>
#include <vlc_input.h>

typedef struct {
  unsigned char major;
  unsigned char minor;
} id3v2_version;

typedef struct {
  char unsynchronisation;
  char extended_header;
  char experimental;
} id3v2_flags;

typedef struct {
  id3v2_version version;
  id3v2_flags flags;
  int size;
  char valid;
} id3v2_header;

typedef struct {
  char id[5];
  int size;
  long start;

} id3v2_frame_header;

typedef struct {
  char id[20];
  long start_time;
  long end_time;
  long start_offset;
  long end_offset;
} chapter_frame;

typedef struct {
  char *title;
  long start_time;
} chapter;

typedef struct {
  chapter *chapters;
  int size;
} chapters;


id3v2_header parse_header(stream_t *stream) {
  stream_Seek(stream, 0);

  id3v2_header header;
  char identifier[4];
  identifier[3] = 0;

  stream_Read(stream, identifier, 3);

  if (memcmp(identifier,"ID3", 3) != 0) {
    header.valid = 0; //false
    return header;
  }
  unsigned char version[2];
  stream_Read(stream, version, 2);
  header.version.major = version[0];
  header.version.minor = version[1];


  unsigned char flags;
  stream_Read(stream, &flags, 1);


  unsigned char size[4];
  stream_Read(stream, size, 4);

  header.size = size[0]*128*128*128 + size[1]*128*128 + size[2]*128 + size[3];
  return header;
}


id3v2_frame_header parse_frame_header(stream_t* stream) {
  id3v2_frame_header header;
  header.start = stream_Tell(stream);

  char identifier[5];
  identifier[4] = 0;

  stream_Read(stream, identifier, 4);
  strcpy(header.id, identifier);

  char size[4];
  stream_Read(stream, size, 4);
  header.size = size[3] + size[2]*128 + size[1]*128*128 + size[0]*128*128;

  unsigned char flags[2];
  stream_Read(stream, flags, 2);
  return header;
}

void skip_frame(stream_t* stream, id3v2_frame_header frame) {
  stream_Seek(stream, frame.size + frame.start + 10);
}

chapter_frame parse_chapter(FILE* stream) {
  chapter_frame frame;
  for(int i=0; i<20; ++i) {
    stream_Read(stream, (frame.id + i), 1);

    if (frame.id[i] == 0) {
      break;
    }
  }
  unsigned char time[4];
  stream_Read(stream, time, 4);
  frame.start_time = time[3] + time[2]*128 + time[1]*128*128 + time[0]*128*128;

  stream_Read(stream, time, 4);
  frame.end_time = time[3] + time[2]*128 + time[1]*128*128 + time[0]*128*128;


  stream_Read(stream, time, 4);
  frame.start_offset = time[3] + time[2]*128 + time[1]*128*128 + time[0]*128*128;

  stream_Read(stream, time, 4);
  frame.end_offset = time[3] + time[2]*128 + time[1]*128*128 + time[0]*128*128;
  return frame;
}

void swap(char *a, char *b) {
  char temp = *a;
  *a = *b;
  *b = temp;
}

chapter get_chapter(FILE* stream, id3v2_frame_header frame) {
  chapter chap;
  chapter_frame chap_frame = parse_chapter(stream);
  chap.start_time = chap_frame.start_time;
  if (stream_Tell(stream) <= frame.start + frame.size + 10) {
    id3v2_frame_header frame = parse_frame_header(stream);
    char *title = malloc(frame.size);
    char type;
    stream_Read(stream, &type, 1);
    for(int i=0; i<frame.size -1; i++) {
      stream_Read(stream, title + i, 1);
      if (title[i] == 0) {
        break;
      }
    }
    title[frame.size - 1] = 0;
    chap.title = title;
  } else {
    chap.title = 0;
  }
  return chap;
}


chapters get_chapters(FILE* stream) {
  chapters chaps;
  chaps.size = 0;
  id3v2_header header = parse_header(stream);
  while (stream_Tell(stream) < header.size) {
    id3v2_frame_header frame_header = parse_frame_header(stream);
    skip_frame(stream, frame_header);
    if (memcmp(frame_header.id, "CHAP", 4) == 0) {
      chaps.size += 1;
    }
  }
  stream_Seek(stream, 0);
  header = parse_header(stream);
  chaps.chapters = malloc (sizeof(chapter)*chaps.size);

  int i = 0;
  while (stream_Tell(stream) < header.size) {
    id3v2_frame_header frame_header = parse_frame_header(stream);
    if (memcmp(frame_header.id, "CHAP", 4) == 0) {
      chaps.chapters[i] = get_chapter(stream, frame_header);
      i++;
    }
    skip_frame(stream, frame_header);
  }
  return chaps;
}

input_title_t* get_title(demux_t* p_demux) {
  input_title_t *title = vlc_input_title_New();
  uint64_t start = stream_Tell(p_demux->s);
  chapters chaps = get_chapters(p_demux->s);
  stream_Seek(p_demux->s, 0);

  for(int i = 0; i < chaps.size; ++i) {
    printf("%s::%i\n", chaps.chapters[i].title, chaps.chapters[i].start_time);

    seekpoint_t *p_seekpoint = vlc_seekpoint_New();
    p_seekpoint->i_time_offset = chaps.chapters[i].start_time;
    p_seekpoint->psz_name = chaps.chapters[i].title; // releasing responsibality for closing
    TAB_APPEND( title->i_seekpoint, title->seekpoint, p_seekpoint );
  }
  stream_Seek(p_demux->s, start);
  p_demux->info.i_update |= INPUT_UPDATE_TITLE_LIST;
  return title;
}

void foo(demux_t* p_demux) {
      uint64_t start = stream_Tell(p_demux->s);
      chapters chaps = get_chapters(p_demux->s);
      stream_Seek(p_demux->s, 0);

      for(int i = 0; i < chaps.size; ++i) {
        printf("%s::%i\n", chaps.chapters[i].title, chaps.chapters[i].start_time);
      }
      stream_Seek(p_demux->s, start);
}

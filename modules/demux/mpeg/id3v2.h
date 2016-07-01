#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_codec.h>
#include <vlc_input.h>
#include <vlc_charset.h>

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
  unsigned long size;
  unsigned long start;

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
  id3v2_header header;
  char identifier[4];
  identifier[3] = 0;

  stream_Read(stream, identifier, 3);

  if (memcmp(identifier,"ID3", 3) != 0) {
    header.valid = 0; //false
    printf("invalid header\n");
    return header;
  }
  unsigned char version[2];
  stream_Read(stream, version, 2);
  header.version.major = version[0];
  header.version.minor = version[1];


  unsigned char flags;
  stream_Read(stream, &flags, 1);
  printf("flags %i\n", flags);


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

  unsigned char size[4];
  if(stream_Read(stream, size, 4) != 4) {
    printf("something went wrong\n");
  }
  bool standard_size = true;
  for (int i=0; i<4; ++i) {
    if (size[i] > 127) {
      standard_size = false;
      break;
    }
  }

  header.size = 0;
  if (standard_size) {
    header.size = size[0]*128*128*128 + size[1]*128*128 + size[2]*128 + size[3];;
  } else {
    ((unsigned char*)&header.size)[3] = size[0];
    ((unsigned char*)&header.size)[2] = size[1];
    ((unsigned char*)&header.size)[1] = size[2];
    ((unsigned char*)&header.size)[0] = size[3];
  }


  unsigned char flags[2];
  if(stream_Read(stream, flags, 2) != 2) {
    printf("something went wrong\n");
  }
  return header;
}

int skip_frame(stream_t* stream, id3v2_frame_header frame) {
  return stream_Seek(stream, frame.size + frame.start + 10);
}

chapter_frame parse_chapter(stream_t* stream) {
  chapter_frame frame;
  for(int i=0; i<20; ++i) {
    stream_Read(stream, (frame.id + i), 1);

    if (frame.id[i] == 0) {
      break;
    }
  }
  unsigned char time[4];
  stream_Read(stream, time, 4);
  frame.start_time =  time[3] + time[2]*128*2 + time[1]*128*128*4 + time[0]*128*128*8;

  stream_Read(stream, time, 4);
  frame.end_time =  time[3] + time[2]*128*2 + time[1]*128*128*4 + time[0]*128*128*8;


  stream_Read(stream, time, 4);
  frame.start_offset =  time[3] + time[2]*128*2 + time[1]*128*128*4 + time[0]*128*128*8;

  stream_Read(stream, time, 4);
  frame.end_offset =  time[3] + time[2]*128*2 + time[1]*128*128*4 + time[0]*128*128*8;
  return frame;
}

void swap(char *a, char *b) {
  char temp = *a;
  *a = *b;
  *b = temp;
}

chapter get_chapter(stream_t* stream, id3v2_frame_header frame) {
  chapter chap;
  chapter_frame chap_frame = parse_chapter(stream);
  chap.start_time = chap_frame.start_time;
  if (stream_Tell(stream) <= frame.start + frame.size + 10) {
    id3v2_frame_header frame = parse_frame_header(stream);
    char type;
    stream_Read(stream, &type, 1);
    unsigned char *title = malloc(frame.size);
    //title[frame.size-1] = 0;
    if(stream_Read(stream, title, frame.size) < frame.size) {
      printf("something went wrong" );
    };
    if (type == 1) {
      vlc_iconv_t*  cd = vlc_iconv_open("utf-8", "UCS-2");

      printf("\n");
      size_t srclen = frame.size;
      size_t dstlen = frame.size;
      chap.title = malloc(dstlen);


      char * pIn = title;
      char * pOut = ( char*)chap.title;
      vlc_iconv(cd, &pIn, &srclen, &pOut, &dstlen);
      chap.title[frame.size-dstlen] = 0;
      vlc_iconv_close(cd);
      utf8_fprintf(stdout,"%s with remaining %lu and non converted %lu\n", chap.title, dstlen, dstlen);

    } else {
      chap.title = title;
    }
  } else {
    chap.title = 0;
  }
  return chap;
}


chapters get_chapters(stream_t* stream) {
  chapters chaps;
  chaps.size = 0;
  id3v2_header header = parse_header(stream);
  while (stream_Tell(stream) < header.size) {
    id3v2_frame_header frame_header = parse_frame_header(stream);
    if(skip_frame(stream, frame_header) < 0) {
      printf("couldn't skip size was %lu and trying to go to %lu\n", header.size, frame_header.size + frame_header.start + 10);
      return chaps;
    }
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

  stream_Seek(p_demux->s, 0);

  chapters chaps = get_chapters(p_demux->s);

  for(int i = 0; i < chaps.size; ++i) {
    seekpoint_t *p_seekpoint = vlc_seekpoint_New();
    p_seekpoint->i_time_offset = chaps.chapters[i].start_time * 1000;
    p_seekpoint->psz_name = chaps.chapters[i].title; // releasing responsibality for closing
    TAB_APPEND( title->i_seekpoint, title->seekpoint, p_seekpoint );
  }
  stream_Seek(p_demux->s, start);
  p_demux->info.i_update |= INPUT_UPDATE_TITLE_LIST;
  return title;
}

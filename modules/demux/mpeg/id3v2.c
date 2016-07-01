/*****************************************************************************
 * id3v2.h: ID3v2 Chapter Frame parser
 *****************************************************************************
 * Copyright (C) 2004-2015 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "id3v2.h"

input_title_t* get_title(demux_t* p_demux) {
  input_title_t *title = vlc_input_title_New();
  uint64_t start = stream_Tell(p_demux->s);

  if (stream_Seek(p_demux->s, 0) < 0) {
    printf("Coudln't seek to the start of the file\n");
    return NULL;
  }

  chapters_t chaps = get_chapters(p_demux->s);

  for (int i = 0; i < chaps.size; ++i) {
    seekpoint_t *p_seekpoint = vlc_seekpoint_New();
    p_seekpoint->i_time_offset = chaps.chapters[i].start_time * 1000;
    p_seekpoint->psz_name = chaps.chapters[i].title; // releasing responsibality for closing
    TAB_APPEND( title->i_seekpoint, title->seekpoint, p_seekpoint );
  }
  p_demux->info.i_update |= INPUT_UPDATE_TITLE_LIST;

  if (stream_Seek(p_demux->s, start) < 0) {
    printf("Couldn't seek back to the starting location\n");
  }

  return title;
}

chapters_t get_chapters(stream_t* stream) {
  printf("getting titles\n");
  chapters_t chaps;

  chaps.size = 0;

  id3v2_header_t header = parse_header(stream);
  while (stream_Tell(stream) < header.size) {
    id3v2_frame_header_t frame_header = parse_frame_header(stream);
    if(skip_frame(stream, frame_header) < 0) {
      printf("couldn't skip size was %u and trying to go to %u\n", header.size, frame_header.size + frame_header.start + 10);
      return chaps;
    }
    if (memcmp(frame_header.id, "CHAP", 4) == 0) {
      chaps.size += 1;
    }
  }
  if(stream_Seek(stream, 0) < 0) {
    printf("couldn't seek to the start of the file\n");
  }
  header = parse_header(stream);
  chaps.chapters = malloc (sizeof(chapter_t)*chaps.size);

  int i = 0;
  while (stream_Tell(stream) < header.size) {
    id3v2_frame_header_t frame_header = parse_frame_header(stream);
    if (memcmp(frame_header.id, "CHAP", 4) == 0) {
      chaps.chapters[i] = get_chapter(stream, frame_header);
      i++;
    }
    skip_frame(stream, frame_header);
  }
  return chaps;
}


id3v2_header_t parse_header(stream_t *stream) {
  id3v2_header_t header;
  char identifier[4];
  identifier[3] = 0;

  if (stream_Read(stream, identifier, 3) < 3) {
    printf("couldn't read anything from the stream\n");
    header.valid = false;
    return header;
  }

  if (memcmp(identifier,"ID3", 3) != 0) {
    header.valid = false;
    printf("invalid header\n");
    return header;
  }
  unsigned char version[2];

  if (stream_Read(stream, version, 2) < 2) {
    printf("coudln't read version info");
    header.valid = false;
    return header;
  }

  header.version.major = version[0];
  header.version.minor = version[1];


  unsigned char flags;
  if (stream_Read(stream, &flags, 1) < 1) {
    printf("coudln't read flags");
    header.valid = false;
    return header;
  }


  unsigned char size[4];
  if (stream_Read(stream, size, 4) < 4) {
    printf("coudln't read size");
    header.valid = false;
    return header;
  }

  header.size = size[0]*128*128*128 + size[1]*128*128 + size[2]*128 + size[3];
  header.valid = true;
  return header;
}


id3v2_frame_header_t parse_frame_header(stream_t* stream) {
  id3v2_frame_header_t header;
  header.start = stream_Tell(stream);

  char identifier[5];
  identifier[4] = 0;

  if (stream_Read(stream, identifier, 4) < 4) {
    printf("failed to read frame id\n");
    header.size = 0;
    header.id[0] = 0;
    return header;
  }
  strcpy(header.id, identifier);

  unsigned char size[4];
  if (stream_Read(stream, size, 4) != 4) {
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

int skip_frame(stream_t* stream, id3v2_frame_header_t frame) {
  return stream_Seek(stream, frame.size + frame.start + 10);
}

id3v2_chapter_frame_t parse_chapter(stream_t* stream) {
  id3v2_chapter_frame_t frame;
  for(int i=0; i<20; ++i) {
    if (stream_Read(stream, (frame.id + i), 1) < 1) {
      printf("coudln't read char\n");
      frame.id[i] = 0;
      break;
    }

    if (frame.id[i] == 0) {
      break;
    }
  }
  unsigned char time[4];
  if (stream_Read(stream, time, 4) < 4) {
    printf("couldn't read time\n");
  }
  frame.start_time =  time[3] + time[2]*128*2 + time[1]*128*128*4 + time[0]*128*128*8;

  if (stream_Read(stream, time, 4) < 4) {
    printf("couldn't read time\n");
  }
  frame.end_time =  time[3] + time[2]*128*2 + time[1]*128*128*4 + time[0]*128*128*8;


  if (stream_Read(stream, time, 4) < 4) {
    printf("couldn't read time\n");
  }
  frame.start_offset =  time[3] + time[2]*128*2 + time[1]*128*128*4 + time[0]*128*128*8;

  if (stream_Read(stream, time, 4) < 4) {
    printf("couldn't read time\n");
  }
  frame.end_offset =  time[3] + time[2]*128*2 + time[1]*128*128*4 + time[0]*128*128*8;
  return frame;
}

chapter_t get_chapter(stream_t* stream, id3v2_frame_header_t frame) {
  chapter_t chap;
  id3v2_chapter_frame_t chap_frame = parse_chapter(stream);
  chap.start_time = chap_frame.start_time;
  if (stream_Tell(stream) <= frame.start + frame.size + 10) {
    id3v2_frame_header_t frame = parse_frame_header(stream);

    char type = -1;

    if (stream_Read(stream, &type, 1) < 1) {
      printf("couldn't read type");
    }

    char *title = malloc(frame.size);
    if (stream_Read(stream, title, frame.size) < frame.size) {
      printf("something went wrong" );
    };
    if (type == 1) {
      vlc_iconv_t*  cd = vlc_iconv_open("utf-8", "UCS-2");

      size_t srclen = frame.size;
      size_t dstlen = frame.size;
      chap.title = malloc(dstlen);
      const char * pIn = (char*) title;
      char * pOut = (char*) chap.title;
      if (vlc_iconv(cd, &pIn, &srclen, &pOut, &dstlen) == 0) {
        printf("coudln't convert\n");
      }
      chap.title[frame.size-dstlen] = 0;
      vlc_iconv_close(cd);
      free(title);
    } else if(type==0) {
      chap.title = title;
    } else {
      printf("don't know what to do with that encoding\n");
      chap.title = malloc(1);
      chap.title[0] = 0;
    }
  } else {
    chap.title = malloc(1);
    chap.title[0] = 0;
  }
  return chap;
}

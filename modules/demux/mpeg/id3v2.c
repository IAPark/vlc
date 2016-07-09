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

static int skip_frame(stream_t* p_stream, id3v2_frame_header_t frame) {
  return stream_Seek(p_stream, frame.i_size + frame.i_start + 10);
}

static id3v2_header_t parse_header(stream_t *p_stream) {
  id3v2_header_t header;
  char p_identifier[3];
  // read the identifier for the tag
  if (stream_Read(p_stream, p_identifier, 3) < 3) {
    fprintf(stderr, "couldn't read anything from the stream\n");
    header.b_valid = false;
    return header;
  }
  // confirm that it's an id3 tag
  if (memcmp(p_identifier,"ID3", 3) != 0) {
    fprintf(stderr, "invalid header\n");
    header.b_valid = false;
    return header;
  }

  // read the version
  unsigned char p_version[2];
  if (stream_Read(p_stream, p_version, 2) < 2) {
    fprintf(stderr, "coudln't read version info");
    header.b_valid = false;
    return header;
  }

  header.version.i_major = p_version[0];
  header.version.i_minor = p_version[1];

  unsigned char i_flags;
  if (stream_Read(p_stream, &i_flags, 1) < 1) {
    fprintf(stderr, "coudln't read flags");
    header.b_valid = false;
    return header;
  }
  header.flags.b_unsynchronisation = (0b10000000 & i_flags) != 0;
  header.flags.b_extended_header = (0b01000000 & i_flags) != 0;
  header.flags.b_experimental = (0b00100000 & i_flags) != 0;

  if (header.flags.b_unsynchronisation) {
    fprintf(stderr, "Can't handle unsynchronisation");
    header.b_valid = false;
    return header;
  }

  unsigned char p_size[4];
  if (stream_Read(p_stream, p_size, 4) < 4) {
    fprintf(stderr, "coudln't read size");
    header.b_valid = false;
    return header;
  }

  header.i_size = p_size[0]*128*128*128 + p_size[1]*128*128 + p_size[2]*128 + p_size[3];
  header.b_valid = true;

  if (header.flags.b_extended_header) {
    if (stream_Seek(p_stream, stream_Tell(p_stream) + 10) < 10) {
      fprintf(stderr, "Couldn't skip exteneded header\n");
    }
  }

  return header;
}

static void read_int32(uint32_t* p_out, unsigned char *p_in) {
  #ifdef WORDS_BIGENDIAN
    for (int i=0; i<4; ++i) {
      ((unsigned char*)p_out)[i] = p_in[i];
    }
  #else
    for (int i=0; i<4; ++i) {
      ((unsigned char*)p_out)[3-i] = p_in[i];
    }
  #endif
}

static id3v2_frame_header_t parse_frame_header(stream_t* p_stream, id3v2_header_t id3v2_header) {
  id3v2_frame_header_t header;
  // consider making start the start of the body of the frame
  header.i_start = stream_Tell(p_stream);


  if (stream_Read(p_stream, header.p_id, 4) < 4) {
    fprintf(stderr, "failed to read frame id\n");
    header.i_size = 0;
    header.p_id[0] = 0;
    return header;
  }

  unsigned char p_size[4];
  if (stream_Read(p_stream, p_size, 4) != 4) {
    fprintf(stderr, "something went wrong\n");
  }
  if (id3v2_header.version.i_major == 4) {
    header.i_size = p_size[0]*128*128*128 + p_size[1]*128*128 + p_size[2]*128 + p_size[3];
  } else {
    read_int32(&header.i_size, p_size);
  }


  unsigned char p_flags[2];
  if(stream_Read(p_stream, p_flags, 2) != 2) {
    fprintf(stderr, "something went wrong\n");
  }
  return header;
}

static id3v2_frame_handler_t* get_handler(id3v2_frame_handlers_t handlers, char* p_id) {
  for (uint32_t i=0; i<handlers.i_handlers; i++) {
    if (memcmp(p_id, handlers.p_handlers[i].p_frame_id, 4) == 0) {
      return &handlers.p_handlers[i];
    }
  }
  return NULL;
}

void id3v2_parse(stream_t* p_stream, id3v2_frame_handlers_t handlers) {
  id3v2_header_t header = parse_header(p_stream);
  if (!header.b_valid) {
    fprintf(stderr, "couldn't read i3v2 header\n");
    return;
  }
  id3v2_frame_header_t frame_header;
  while(stream_Tell(p_stream) < header.i_size) {
    frame_header = parse_frame_header(p_stream, header);
    if (frame_header.p_id[0] == 0) {
      return;
    }
    id3v2_frame_handler_t *handler = get_handler(handlers, frame_header.p_id);

    if (handler != 0) {
      handler->p_handler(p_stream, header, frame_header, handler->p_data);
    }
    skip_frame(p_stream, frame_header);
  }

}

typedef struct {
  char *psz_title;
  uint32_t i_start_time;
} chapter_t;

typedef struct {
  chapter_t *p_chapters;
  uint32_t i_size;
  uint32_t i_capacity;
} chapters_t;

static int to_utf8(char **p_in_buf, char **p_out_buf, size_t i_length, uint8_t i_type) {
  if (i_type == 0||i_type == 1||i_type == 2) {
    size_t i_in_len = i_length;
    size_t i_out_len = i_length*4;

    *p_out_buf = malloc(i_out_len*4);
    char *p_out = *p_out_buf;
    const char *p_in = *p_in_buf;

    vlc_iconv_t* handle;
    if (i_type == 0) {
      handle = vlc_iconv_open("utf-8", "ISO-8859-1");
    } else if (i_type == 1) {
      handle = vlc_iconv_open("utf-8", "UCS-2");
    } else if (i_type == 2) {
      handle = vlc_iconv_open("utf-8", "UTF-16BE");
    }
    if (vlc_iconv(handle, &p_in, &i_in_len, &p_out, &i_out_len) == (size_t) -1) {
      fprintf(stderr, "couldnt convert\n");
    }

    (*p_out_buf)[i_length*4 - i_out_len] = 0;
    vlc_iconv_close(handle);
  } else if (i_type == 3) {
    *p_out_buf = malloc(strlen(*p_in_buf));
    memcpy(*p_out_buf, *p_in_buf, strlen(*p_in_buf));
  } else {
    *p_out_buf = malloc(1);
    *p_out_buf[0] = 0;
  }
  return 0;
}

static int parse_chapter(stream_t* p_stream,
                  id3v2_header_t header,
                  id3v2_frame_header_t frame_header,
                  void* p_data) {
  chapters_t *chapters = (chapters_t*)p_data;

  // resize chapeters todo: consider making proper vector instead
  if (chapters->i_size >= chapters->i_capacity) {
    chapters->i_capacity = (chapters->i_capacity*2 + 1);
    chapters->p_chapters = realloc(chapters->p_chapters, chapters->i_capacity* sizeof(chapters_t));
  }
  // get the next chapter
  chapter_t *chapter = &chapters->p_chapters[chapters->i_size++];
  chapter->psz_title = 0;
  // skip id: we don't need that thing
  char i_out;
  do {
    if (stream_Read(p_stream, &i_out, 1) < 1) {
      fprintf(stderr, "coudln't read char\n");
      break;
    }
  } while(i_out != 0);

  // Read the start time, there's also and end time and byte offsets, but we won't read it
  unsigned char p_time[4];
  if (stream_Read(p_stream, p_time, 4) < 4) {
    fprintf(stderr, "couldn't read time\n");
  }
  read_int32(&(chapter->i_start_time), p_time);

  // check if there's space for a title, if this frame doesn't have an embedded frame skip setting the title
  if (stream_Tell(p_stream) + 12 + 10 - (frame_header.i_start + 10) >= frame_header.i_size) {
    fprintf(stderr, "no space for title\n");
    return 0;
  }

  if (stream_Seek(p_stream, stream_Tell(p_stream) + 12) < 0) {
    fprintf(stderr, "couldn't read from stream\n");
    return 0;
  }

  id3v2_frame_header_t title_header = parse_frame_header(p_stream, header);

  uint8_t text_type;
  if (stream_Read(p_stream, &text_type, 1) < 1) {
    fprintf(stderr, "coudln't read char\n");
    return 0;
  }

  char* psz_title = malloc(title_header.i_size + 1);
  if (stream_Read(p_stream, psz_title, title_header.i_size-1) < title_header.i_size - 1) {
    fprintf(stderr, "coudln't read char\n");
    return 0;
  }

  size_t i_title_length = title_header.i_size - 1;
  to_utf8(&psz_title, &chapter->psz_title, i_title_length, text_type);
  free(psz_title);
  return 0;
}


// todo: think about changing this to take a stream directly
input_title_t* get_title(demux_t* p_demux) {
  // get current stream location so we can return to it when parsing finishes
  uint64_t i_start = stream_Tell(p_demux->s);

  input_title_t* p_title = vlc_input_title_New();

  // the id3v2 tag should be at the start of file
  if (stream_Seek(p_demux->s, 0) < 0) {
    fprintf(stderr, "Coudln't seek to the start of the file\n");
    return NULL;
  }

  id3v2_frame_handler_t handler;
  memcpy(handler.p_frame_id, "CHAP", 4);
  handler.p_handler = parse_chapter;
  chapters_t chapters;
  chapters.i_size = 0;
  chapters.i_capacity = 0;
  chapters.p_chapters = NULL;

  handler.p_data = &chapters;
  id3v2_frame_handlers_t handlers;
  handlers.i_handlers = 1;
  handlers.p_handlers = &handler;

  id3v2_parse(p_demux->s, handlers);

  for (uint32_t i = 0; i < chapters.i_size; ++i) {
    seekpoint_t *p_seekpoint = vlc_seekpoint_New();

    // time is stored by the seek points as microseconds, here it's in milliseconds
    p_seekpoint->i_time_offset = chapters.p_chapters[i].i_start_time * 1000;
    // releasing responsibality for freeing
    p_seekpoint->psz_name = chapters.p_chapters[i].psz_title;

    TAB_APPEND( p_title->i_seekpoint, p_title->seekpoint, p_seekpoint );
  }
  free(chapters.p_chapters);
  p_demux->info.i_update |= INPUT_UPDATE_TITLE_LIST;

  // return to where we started
  if (stream_Seek(p_demux->s, i_start) < 0) {
    fprintf(stderr, "Couldn't seek back to the starting location\n");
  }

  return p_title;
}

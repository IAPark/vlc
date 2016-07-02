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

// todo: think about changing this to take a stream direcctly
input_title_t* get_title(demux_t* p_demux) {
  // get current stream location so we can return to it when parsing finishes
  uint64_t i_start = stream_Tell(p_demux->s);

  input_title_t* p_title = vlc_input_title_New();

  // the id3v2 tag should be at the start of file
  if (stream_Seek(p_demux->s, 0) < 0) {
    printf("Coudln't seek to the start of the file\n");
    return NULL;
  }

  chapters_t chapters = get_chapters(p_demux->s);

  for (int i = 0; i < chapters.i_size; ++i) {
    seekpoint_t *p_seekpoint = vlc_seekpoint_New();

    // time is stored by the seek points as microseconds, here it's in milliseconds
    p_seekpoint->i_time_offset = chapters.p_chapters[i].i_start_time * 1000;
    // releasing responsibality for freeing
    p_seekpoint->psz_name = chapters.p_chapters[i].psz_title;

    TAB_APPEND( p_title->i_seekpoint, p_title->seekpoint, p_seekpoint );
  }
  p_demux->info.i_update |= INPUT_UPDATE_TITLE_LIST;

  // return to where we started
  if (stream_Seek(p_demux->s, i_start) < 0) {
    printf("Couldn't seek back to the starting location\n");
  }

  return p_title;
}


chapters_t get_chapters(stream_t* p_stream) {
  chapters_t chapters; // the result to send back
  chapters.i_size = 0;

  // get the header ID3V2
  id3v2_header_t header = parse_header(p_stream);
  if (!header.b_valid) {
    printf("header is invalid");
    return chapters;
  }

  // counting all the chapters, todo perform parse in one go
  while (stream_Tell(p_stream) < header.i_size) {
    id3v2_frame_header_t frame_header = parse_frame_header(p_stream);
    if(skip_frame(p_stream, frame_header) < 0) {
      printf("couldn't skip size was %u and trying to go to %u\n", header.i_size,
       frame_header.i_size + frame_header.i_start + 10);
      return chapters;
    }
    if (memcmp(frame_header.p_id, "CHAP", 4) == 0) {
      chapters.i_size += 1;
    }
  }

  if(stream_Seek(p_stream, 10) < 0) {
    printf("couldn't seek to the start of the file\n");
  }

  chapters.p_chapters = malloc (sizeof(chapter_t)*chapters.i_size);

  int i = 0;
  while (stream_Tell(p_stream) < header.i_size) {
    id3v2_frame_header_t frame_header = parse_frame_header(p_stream);
    if (memcmp(frame_header.p_id, "CHAP", 4) == 0) {
      chapters.p_chapters[i] = get_chapter(p_stream, frame_header);
      i++;
    }
    skip_frame(p_stream, frame_header);
  }
  return chapters;
}


id3v2_header_t parse_header(stream_t *p_stream) {
  id3v2_header_t header;
  char p_identifier[3];

  if (stream_Read(p_stream, p_identifier, 3) < 3) {
    printf("couldn't read anything from the stream\n");
    header.b_valid = false;
    return header;
  }

  if (memcmp(p_identifier,"ID3", 3) != 0) {
    printf("invalid header\n");
    header.b_valid = false;
    return header;
  }
  unsigned char p_version[2];

  if (stream_Read(p_stream, p_version, 2) < 2) {
    printf("coudln't read version info");
    header.b_valid = false;
    return header;
  }

  header.version.i_major = p_version[0];
  header.version.i_minor = p_version[1];

  // todo parse chapter information
  unsigned char i_flags;
  if (stream_Read(p_stream, &i_flags, 1) < 1) {
    printf("coudln't read flags");
    header.b_valid = false;
    return header;
  }


  unsigned char p_size[4];
  if (stream_Read(p_stream, p_size, 4) < 4) {
    printf("coudln't read size");
    header.b_valid = false;
    return header;
  }

  header.i_size = p_size[0]*128*128*128 + p_size[1]*128*128 + p_size[2]*128 + p_size[3];
  header.b_valid = true;
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

id3v2_frame_header_t parse_frame_header(stream_t* p_stream) {
  id3v2_frame_header_t header;
  // consider making start the start of the body of the frame
  header.i_start = stream_Tell(p_stream);


  if (stream_Read(p_stream, header.p_id, 4) < 4) {
    printf("failed to read frame id\n");
    header.i_size = 0;
    header.p_id[0] = 0;
    return header;
  }

  unsigned char p_size[4];
  if (stream_Read(p_stream, p_size, 4) != 4) {
    printf("something went wrong\n");
  }
  bool p_standard_size = true;
  for (int i=0; i<4; ++i) {
    if (p_size[i] > 127) {
      p_standard_size = false;
      break;
    }
  }

  header.i_size = 0;
  if (p_standard_size) {
    header.i_size = p_size[0]*128*128*128 + p_size[1]*128*128 + p_size[2]*128 + p_size[3];
  } else {
    read_int32(&header.i_size, p_size);
  }


  unsigned char p_flags[2];
  if(stream_Read(p_stream, p_flags, 2) != 2) {
    printf("something went wrong\n");
  }
  return header;
}


int skip_frame(stream_t* p_stream, id3v2_frame_header_t frame) {
  return stream_Seek(p_stream, frame.i_size + frame.i_start + 10);
}

id3v2_chapter_frame_t parse_chapter(stream_t* p_stream) {
  id3v2_chapter_frame_t frame;
  for(int i=0; i<20; ++i) {
    if (stream_Read(p_stream, (frame.psz_id + i), 1) < 1) {
      printf("coudln't read char\n");
      frame.psz_id[i] = 0;
      break;
    }

    if (frame.psz_id[i] == 0) {
      break;
    }
  }

  unsigned char p_time[4];
  if (stream_Read(p_stream, p_time, 4) < 4) {
    printf("couldn't read time\n");
  }
  read_int32(&(frame.i_start_time) , p_time);

  if (stream_Read(p_stream, p_time, 4) < 4) {
    printf("couldn't read time\n");
  }
  read_int32(&(frame.i_end_time) , p_time);

  if (stream_Read(p_stream, p_time, 4) < 4) {
    printf("couldn't read time\n");
  }
  read_int32(&(frame.i_start_offset) , p_time);

  if (stream_Read(p_stream, p_time, 4) < 4) {
    printf("couldn't read time\n");
  }
  read_int32(&(frame.i_end_offset) , p_time);

  return frame;
}


chapter_t get_chapter(stream_t* p_stream, id3v2_frame_header_t frame) {
  chapter_t chapter;
  id3v2_chapter_frame_t chapter_frame = parse_chapter(p_stream);
  chapter.i_start_time = chapter_frame.i_start_time;
  if (stream_Tell(p_stream) <= frame.i_start + frame.i_size + 10) {
    id3v2_frame_header_t frame = parse_frame_header(p_stream);

    char i_type = -1;
    if (stream_Read(p_stream, &i_type, 1) < 1) {
      printf("couldn't read type");
    }

    char *psz_title = malloc(frame.i_size);
    if (stream_Read(p_stream, psz_title, frame.i_size) < frame.i_size) {
      printf("something went wrong" );
    };
    if (i_type == 1) {
      size_t i_in_len = frame.i_size;
      size_t i_out_len = frame.i_size;
      chapter.psz_title = malloc(i_out_len);

      const char* p_in = (char*) psz_title;
      char* p_out = (char*) chapter.psz_title;

      vlc_iconv_t*  handle = vlc_iconv_open("utf-8", "UCS-2");
      if (vlc_iconv(handle, &p_in, &i_in_len, &p_out, &i_out_len) == 0) {
        printf("coudln't convert\n");
      }
      vlc_iconv_close(handle);

      chapter.psz_title[frame.i_size-i_out_len] = 0;
      free(psz_title);
    } else if (i_type==0) {
      chapter.psz_title = psz_title;
    } else {
      printf("don't know what to do with that encoding\n");
      chapter.psz_title = malloc(1);
      chapter.psz_title[0] = 0;
    }
  } else {
    chapter.psz_title = malloc(1);
    chapter.psz_title[0] = 0;
  }
  return chapter;
}

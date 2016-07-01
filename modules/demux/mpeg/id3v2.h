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

#ifndef id3v2_h
#define id3v2_h

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

/*
High level function to parse chapter information from id3v2 tag and create title info

*/
input_title_t* get_title(demux_t* p_demux);

// returns the id3v2 header found
id3v2_header parse_header(stream_t *stream);

id3v2_frame_header parse_frame_header(stream_t* stream);

int skip_frame(stream_t* stream, id3v2_frame_header frame);

chapter_frame parse_chapter(stream_t* stream);

chapter get_chapter(stream_t* stream, id3v2_frame_header frame);


chapters get_chapters(stream_t* stream);
#endif

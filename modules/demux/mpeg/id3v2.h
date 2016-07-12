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
  unsigned char i_major;
  unsigned char i_minor;
} id3v2_version_t;

typedef struct {
  bool b_unsynchronisation;
  bool b_extended_header;
  bool b_experimental;
} id3v2_flags_t;

typedef struct {
  id3v2_version_t version;
  id3v2_flags_t flags;
  uint32_t i_size;
  bool b_valid;
} id3v2_header_t;

typedef struct {
  char p_id[4];
  uint32_t i_size;
  uint32_t i_start;
  bool b_unsynced;

} id3v2_frame_header_t;


typedef int (*frame_callback)(stream_t*, id3v2_header_t, id3v2_frame_header_t, void*);

typedef struct {
  char p_frame_id[4]; // the frame id this function should be called to handle
  frame_callback p_handler;
  void* p_data;
} id3v2_frame_handler_t;

typedef struct {
  id3v2_frame_handler_t *p_handlers; // handlers, should be ordered by
  uint32_t i_handlers;
} id3v2_frame_handlers_t;

/**
 * High level function to parse chapter information from id3v2 tag and create title info
 */
input_title_t* get_title(demux_t* p_demux);

void id3v2_parse(stream_t* p_stream, id3v2_frame_handlers_t handlers);

#endif

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
} id3v2_version_t;

typedef struct {
  char unsynchronisation;
  char extended_header;
  char experimental;
} id3v2_flags_t;

typedef struct {
  id3v2_version_t version;
  id3v2_flags_t flags;
  uint32_t size;
  char valid;
} id3v2_header_t;

typedef struct {
  char id[5];
  uint32_t size;
  uint32_t start;

} id3v2_frame_header_t;

typedef struct {
  char id[20];
  long start_time;
  long end_time;
  long start_offset;
  long end_offset;
} id3v2_chapter_frame_t;

typedef struct {
  char *title;
  long start_time;
} chapter_t;

typedef struct {
  chapter_t *chapters;
  int size;
} chapters_t;

/**
 * High level function to parse chapter information from id3v2 tag and create title info
 */
input_title_t* get_title(demux_t* p_demux);

/**
 * Parses overall header for ID3v2
 *
 * Currently exists as a minimum inplimentation. No effort is made to
 * parse flag information or respond to it according to spec
 *
 * @param stream a stream pointing to the start of the ID3V2 header
 */
id3v2_header_t parse_header(stream_t *stream);

/**
 * Parses header for ID3V2 frame
 *
 * Makes some attemt to detirmine if size is incoded to avoid false syncsignals
 * as the standard requires, but may fail to properly decode if standard
 * is not followed
 *
 * @param stream a stream pointing to the start of the frame header
 */
id3v2_frame_header_t parse_frame_header(stream_t* stream);

/**
 * Calls stream_Seek to bring it past the end of the frame
 */
int skip_frame(stream_t* stream, id3v2_frame_header_t frame);

/**
 * Parses a CHAP frame
 *
 * No attempt is made to read any text information frames that may or
 * may not be embedded in this tag
 *
 * @param stream a stream pointing just after the hader for the CHAP frame
 */
id3v2_chapter_frame_t parse_chapter(stream_t* stream);


/**
 * Parses a chapter frame into a high level structure for use
 *
 * It is assumed a TIT2 frame or similar is embedded containing the title
 * of the chapter as is recomended by the spec
 *
 * @param stream a stream pointing just after the hader for the CHAP frame
 */
chapter_t get_chapter(stream_t* stream, id3v2_frame_header_t frame);

/**
 * Get all chapters in ID3V2 tag
 *
 * Chapters will be parsed and saved in the order they are found in
 */
chapters_t get_chapters(stream_t* stream);
#endif

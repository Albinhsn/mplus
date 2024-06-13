#ifndef FONT_H
#define FONT_H

#include "common.h"
#include "files.h"
#include <assert.h>
#include <byteswap.h>
#include <cstring>

static inline u64 swap_u64(u64 input)
{
  return __bswap_64(input);
}

static inline u32 swap_u32(u32 input)
{
  return __bswap_32(input);
}

static inline u16 swap_u16(u16 input)
{
  return __bswap_16(input);
}

struct TableRecord
{
public:
  static TableRecord read(Buffer* buffer);
  bool               match_tag(const char t[4])
  {
    assert(strlen(t) == 4 && "need tag to be length 4?");

    return strncmp(tag, t, 4) == 0;
  }
  char tag[4];
  u32  checksum;
  u32  offset;
  u32  length;
};

struct TableDirectory
{
public:
  static TableDirectory read(Buffer* buffer);
  u32                   sfnt_version;
  u16                   num_tables;
  u16                   search_ranges;
  u16                   entry_selector;
  u16                   range_shirt;
};

enum TableType
{
  TABLE_OS2,
  TABLE_GLYF,
  TABLE_MAXP,
  TABLE_HEAD,
  TABLE_LOCA,
  DUMMY_TABLE
};

struct CompoundGlyph
{
  u16 flags;
  u16 glyph_index;
  u16 argument1;
  u16 argument2;
};

struct SimpleGlyph
{
  u16* end_pts_of_contours;
  u16  n;
  u16  instruction_length;
  u8*  instructions;
  u8*  flags;
  i16* x_coordinates;
  i16* y_coordinates;
};

struct TableMaxp
{
  u32 version;
  u16 num_glyphs;
  u16 max_points;
  u16 max_contours;
  u16 max_component_points;
  u16 max_component_contours;
  u16 max_zones;
  u16 max_twilight_points;
  u16 max_storage;
  u16 max_function_defs;
  u16 max_instruction_defs;
  u16 max_stack_elements;
  u16 max_size_of_instructions;
  u16 max_component_elements;
  u16 max_component_depth;
};

struct TableGlyf
{
  i16 number_of_contours;
  i16 min_x;
  i16 min_y;
  i16 max_x;
  i16 max_y;
};

struct TableOSSlashTwo
{
  u16  version;
  i16  x_avg_char_width;
  u16  us_weight_class;
  u16  us_width_class;
  i16  fs_type;
  i16  y_subscript_x_size;
  i16  y_subscript_y_size;
  i16  y_subscript_x_offset;
  i16  y_subscript_y_offset;
  i16  y_strikeout_size;
  i16  y_strikeout_position;

  i16  s_family_class;
  char panose[10];
  u32  ul_unicode_range[4];
  i8   ach_vend_id[4];
  u16  fs_selection;
  u16  fs_first_char_index;
  u16  fs_last_char_index;
};

struct TableHead
{
public:
  char* buffer;
  u32   version()
  {
    u32 out = *(u32*)buffer;
    return swap_u32(out);
  } // 0

  u32 font_revision()
  {
    const int offset = 4;
    u32       out    = *(u32*)(buffer + offset);
    return swap_u32(out);
  } // 4
  u32 checksum_adjustment()
  {
    const int offset = 8;
    u32       out    = *(u32*)(buffer + offset);
    return swap_u32(out);
  } // 8
  u32 magic_number()
  {
    const int offset = 12;
    u32       out    = *(u32*)(buffer + offset);
    return swap_u32(out);
  } // 12
  u16 flags()
  {
    const int offset = 16;
    u16       out    = *(u16*)(buffer + offset);
    return swap_u16(out);
  } // 16
  u16 units_per_em()
  {
    const int offset = 18;
    u16       out    = *(u16*)(buffer + offset);
    return swap_u16(out);
  } // 18
  i64 created()
  {
    const int offset = 20;
    i64       out    = *(i64*)(buffer + offset);
    return swap_u64(out);
  } // 20
  // i64 modifier;            // 28
  // i16 min_x;               // 36
  // i16 min_y;               // 38
  // i16 max_x;               // 40
  // i16 max_y;               // 42
  // u16 mac_style;           // 44
  // u16 lowest_rec_ppem;     // 46
  // i16 font_direction_hint; // 48
  i16 index_to_loc_format(){
    const int offset = 50;
    i16 out = *(i16*)(buffer + offset);
    return swap_u16(out);
  }
  i16 glyph_data_format;
};

struct Table
{
  TableType type;
  union
  {
    TableOSSlashTwo* os_slash_two;
    TableGlyf*       glyf;
    TableMaxp*       maxp;
    TableHead*       head;
    TableRecord*     loca;
  };
  u32 offset;
};
enum GlyphType
{

};
struct Glyph
{
  bool is_simple;
  union
  {
    SimpleGlyph   simple;
    CompoundGlyph compound;
  };
};

struct Font
{
  Glyph * glyphs;
  u32 glyph_count;
};

void sta_font_parse_ttf(Font* font, const char* filename);

#endif

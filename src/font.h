#ifndef FONT_H
#define FONT_H

#include "common.h"
#include "files.h"
#include <assert.h>
#include <byteswap.h>
#include <cstring>
#include <iterator>

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
  void swap_endianess()
  {
    this->length   = swap_u32(this->length);
    this->offset   = swap_u32(this->offset);
    this->checksum = swap_u32(this->checksum);
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
  void                  swap_endianess()
  {
    this->num_tables     = swap_u16(this->num_tables);
    this->range_shirt    = swap_u16(this->range_shirt);
    this->search_ranges  = swap_u16(this->search_ranges);
    this->entry_selector = swap_u16(this->entry_selector);
    this->sfnt_version   = swap_u16(this->sfnt_version);
  }
  u32 sfnt_version;
  u16 num_tables;
  u16 search_ranges;
  u16 entry_selector;
  u16 range_shirt;
};

enum TableType
{
  TABLE_OS2,
  TABLE_GLYF,
  TABLE_MAXP,
  TABLE_HEAD,
  TABLE_LOCA,
  TABLE_CMAP,
  TABLE_HHEA,
  TABLE_HMTX,
  DUMMY_TABLE_TYPE
};

struct LongHorMetric
{
  u16 advance_width;
  i16 left_side_bearing;
};

struct SimpleGlyph
{
  u16* end_pts_of_contours;
  u16  n;
  i16* x_coordinates;
  i16* y_coordinates;
};

struct Glyph;

struct CompoundGlyph
{
  Glyph* glyphs;
  u32    glyph_count;
  u32    glyph_capacity;
};

struct Glyph
{
public:
  union
  {
    SimpleGlyph   simple;
    CompoundGlyph compound;
  };
  u16  advance_width;
  bool s; // simple
};

struct TableMaxp
{
public:
  void swap_endianess()
  {
    this->num_glyphs               = swap_u16(this->num_glyphs);
    this->version                  = swap_u32(this->version);
    this->max_points               = swap_u16(this->max_points);
    this->max_contours             = swap_u16(this->max_contours);
    this->max_component_points     = swap_u16(this->max_component_points);
    this->max_component_contours   = swap_u16(this->max_component_contours);
    this->max_zones                = swap_u16(this->max_zones);
    this->max_twilight_points      = swap_u16(this->max_twilight_points);
    this->max_storage              = swap_u16(this->max_storage);
    this->max_function_defs        = swap_u16(this->max_function_defs);
    this->max_instruction_defs     = swap_u16(this->max_instruction_defs);
    this->max_stack_elements       = swap_u16(this->max_stack_elements);
    this->max_size_of_instructions = swap_u16(this->max_size_of_instructions);
    this->max_component_elements   = swap_u16(this->max_component_elements);
    this->max_component_depth      = swap_u16(this->max_component_depth);
  }
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
public:
  void swap_endianess()
  {
    this->number_of_contours = swap_u16(this->number_of_contours);
    this->min_x              = swap_u16(this->min_x);
    this->max_x              = swap_u16(this->max_x);
    this->min_y              = swap_u16(this->min_y);
    this->max_y              = swap_u16(this->max_y);
  }
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
  u16 units_per_em;
  i16 index_to_loc_format;
};

struct CmapSubtable
{
  u16 platform_id;
  u16 platform_specific_id;
  u32 offset;
};

struct TableCmap
{
  u16 version;
  u16 number_of_subtables;
};

struct TableHhea
{
  u32 version;
  i16 ascent;
  i16 descent;
  i16 line_gap;
  u16 advance_width_max;
  i16 min_left_side_bearing;
  i16 min_right_side_bearing;
  i16 x_max_extent;
  i16 caret_slope_rise;
  i16 caret_slope_run;
  i16 caret_offset;
  i16 reserved[4];
  i16 metric_data_format;
  u16 num_of_long_hor_metrics;
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
    TableCmap*       cmap;
    TableRecord*     loca;
    TableHhea*       hhea;
  };
  u32 offset;
};

struct Font
{
public:
  void   parse_ttf(const char* filename);
  Glyph  get_glyph(u32 code);
  u32*   char_codes;
  u32*   glyph_indices;
  Glyph* glyphs;
  f32    scale;
  u32    glyph_count;
};

#endif

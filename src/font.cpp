#include "font.h"
#include "common.h"
#include "files.h"
#include "platform.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#define BIT_IS_SET(flag, c) ((((flag) >> c) & 1) == 1)
#define REPEAT(flag)        (BIT_IS_SET(flag, 3))

static void parse_glyph(char* buf, Glyph* glyph, u32 offset, u32* glyph_locations, u32 idx);
static f32  transform_from_fixed_point(u16 x)
{
  return ((i16)(x)) / (f64)(1 << 14);
}

static inline u16 read_u16(Buffer* buffer)
{
  u16 out = swap_u16(*(u16*)buffer->current_address());
  buffer->advance(sizeof(u16));
  return out;
}

static inline u32 read_u32(Buffer* buffer)
{
  u32 out = swap_u32(*(u32*)buffer->current_address());
  buffer->advance(sizeof(u32));
  return out;
}

static void read_coordinates(i16* coordinates, u8* flags, u32 count, Buffer* buffer, bool reading_x)
{
  u8  offset_size_flag_bit    = reading_x ? 1 : 2;
  u8  offset_sign_or_skip_bit = reading_x ? 4 : 5;

  i16 curr                    = 0;
  for (u32 i = 0; i < count; i++)
  {

    u8   flag        = flags[i];
    bool sign_or_bit = BIT_IS_SET(flag, offset_sign_or_skip_bit);

    if (BIT_IS_SET(flag, offset_size_flag_bit))
    {
      u8 offset = buffer->advance();
      curr += (sign_or_bit ? offset : -offset);
    }
    else if (!sign_or_bit)
    {
      curr += (i16)swap_u16(*(u16*)buffer->current_address());
      buffer->advance(sizeof(u16));
    }

    coordinates[i] = curr;
  }
}
static bool glyphs_intersect(i16 v0[][2], u16 c0, i16 v1[][2], u16 c1)
{
  return false;
}

static bool should_be_compound(Glyph* glyph)
{
  assert(!glyph->s && "Already compound?");

  if (glyph->simple.n > 1)
  {
    for (u32 i = 0; i < glyph->simple.n - 1; i++)
    {
      u16 c0 = i == 0 ? glyph->simple.end_pts_of_contours[i] : glyph->simple.end_pts_of_contours[i - 1] - glyph->simple.end_pts_of_contours[i];
      i16 v0[c0][2];
      // hoist vertices
      bool found = false;
      for (u32 j = i + 1; j < glyph->simple.n; j++)
      {
        u16 c1 = glyph->simple.end_pts_of_contours[j - 1] - glyph->simple.end_pts_of_contours[j];
        i16 v1[c1][2];
        if (glyphs_intersect(v0, c0, v1, c1))
        {
          found = true;
          break;
        }
      }
      if (!found)
      {
        return true;
      }
    }
  }
  return false;
}

static void read_simple_glyph(Glyph* glyph, Buffer* buffer, int n)
{

  SimpleGlyph* simple = &glyph->simple;
  simple->n           = n;

  if (n > 0)
  {

    simple->end_pts_of_contours = (u16*)sta_allocate_struct(u16, n);
    for (u16 i = 0; i < simple->n; i++)
    {
      simple->end_pts_of_contours[i] = read_u16(buffer);
    }

    // skip instructions
    buffer->advance(read_u16(buffer));

    u16 number_of_points = simple->end_pts_of_contours[simple->n - 1] + 1;
    u8* flags            = (u8*)sta_allocate_struct(u8, number_of_points);
    for (u32 i = 0; i < number_of_points; i++)
    {
      u8 flag  = buffer->advance();
      flags[i] = flag;

      if (REPEAT(flag))
      {
        u32 repeat_for = buffer->advance();
        for (u32 repeat = 0; repeat < repeat_for; repeat++)
        {
          flags[++i] = flag;
        }
        assert(i <= number_of_points && "Repeated past the end?");
      }
    }
    simple->x_coordinates = (i16*)sta_allocate_struct(i16, number_of_points * 2);
    simple->y_coordinates = &simple->x_coordinates[number_of_points];

    read_coordinates(simple->x_coordinates, flags, number_of_points, buffer, true);
    read_coordinates(simple->y_coordinates, flags, number_of_points, buffer, false);

    if (should_be_compound(glyph))
    {
    }
    sta_deallocate(flags, number_of_points);
  }
}

static void get_all_glyph_locations(Table* tables, char* buf, u32** glyph_locations, u32& number_of_glyphs)
{
  number_of_glyphs            = tables[TABLE_MAXP].maxp->num_glyphs;
  u32    bytes_per_entry      = tables[TABLE_HEAD].head->index_to_loc_format == 0 ? 2 : 4;

  u32    location_table_start = tables[TABLE_LOCA].offset;
  u32    glyph_table_start    = tables[TABLE_GLYF].offset;

  u32*   locations            = (u32*)sta_allocate_struct(u32, number_of_glyphs);

  Buffer buffer(buf, 0);

  for (u32 glyph_index = 0; glyph_index < number_of_glyphs; glyph_index++)
  {
    buffer.index = location_table_start + glyph_index * bytes_per_entry;

    u32 glyph_data_offset;
    if (bytes_per_entry == 2)
    {
      glyph_data_offset = read_u16(&buffer) * 2;
    }
    else
    {
      glyph_data_offset = read_u32(&buffer);
    }

    locations[glyph_index] = glyph_table_start + glyph_data_offset;
  }

  *glyph_locations = locations;
}

Glyph Font::get_glyph(u32 code)
{
  for (u32 i = 0; i < this->glyph_count; i++)
  {
    if (this->char_codes[i] == code)
    {
      return this->glyphs[this->glyph_indices[i]];
    }
  }
  return this->glyphs[0];
}

static void resize_compound_glyph(Glyph* glyph, i32 offset_x, i32 offset_y, f32 a, f32 b, f32 c, f32 d)
{

  if (glyph->s)
  {
    SimpleGlyph* current_glyph = &glyph->simple;
    for (u32 i = 0; i <= current_glyph->end_pts_of_contours[current_glyph->n - 1]; i++)
    {
      i16 x                           = current_glyph->x_coordinates[i];
      i16 y                           = current_glyph->y_coordinates[i];

      current_glyph->x_coordinates[i] = x * a + c * y + offset_x;
      current_glyph->y_coordinates[i] = x * b + d * y + offset_y;
    }
  }
  else
  {
    CompoundGlyph* comp = &glyph->compound;
    for (u32 i = 0; i < comp->glyph_count; i++)
    {
      resize_compound_glyph(&comp->glyphs[i], offset_x, offset_y, a, b, c, d);
    }
  }
}

static void read_compound_glyph(u32* glyph_locations, char* original_buf, Glyph* glyph, Buffer* buffer)
{
  CompoundGlyph* comp_glyph      = &glyph->compound;

  bool           has_more_glyphs = true;
  comp_glyph->glyph_count        = 0;
  comp_glyph->glyph_capacity     = 2;
  comp_glyph->glyphs             = (Glyph*)sta_allocate_struct(Glyph, comp_glyph->glyph_capacity);
  while (has_more_glyphs)
  {
    RESIZE_ARRAY(comp_glyph->glyphs, Glyph, comp_glyph->glyph_count, comp_glyph->glyph_capacity);

    u16 flags                 = read_u16(buffer);
    u16 glyph_index           = read_u16(buffer);

    has_more_glyphs           = BIT_IS_SET(flags, 5);

    bool has_scale            = BIT_IS_SET(flags, 3);
    bool xy_scale             = BIT_IS_SET(flags, 6);
    bool two_x_two            = BIT_IS_SET(flags, 7);
    bool arg_1and_2_are_words = BIT_IS_SET(flags, 0);
    bool args_are_xy_values   = BIT_IS_SET(flags, 1);

    // words or bytes
    i16 offset_x, offset_y;
    if (arg_1and_2_are_words && args_are_xy_values)
    {
      offset_x = *(i16*)buffer->current_address();
      offset_x = swap_u16(offset_x);
      buffer->advance(sizeof(i16));
      offset_y = *(i16*)buffer->current_address();
      offset_y = swap_u16(offset_y);
      buffer->advance(sizeof(i16));
    }
    else if (!arg_1and_2_are_words && args_are_xy_values)
    {
      offset_x = *(i8*)buffer->current_address();
      buffer->advance(sizeof(u8));
      offset_y = *(i8*)buffer->current_address();
      buffer->advance(sizeof(u8));
    }
    else
    {
      assert(0 && "Don't know what to do here, should be points not xy");
    }

    // should be fixed binary point numbers with one bit to the left of the binary point and 14 to the right
    f32 a = 1.0f, b = 0, c = 0, d = 1.0;

    if (has_scale)
    {
      a = transform_from_fixed_point(read_u16(buffer));
      d = a;
    }
    else if (xy_scale)
    {
      a = transform_from_fixed_point(read_u16(buffer));
      d = transform_from_fixed_point(read_u16(buffer));
    }
    else if (two_x_two)
    {
      a = transform_from_fixed_point(read_u16(buffer));
      b = transform_from_fixed_point(read_u16(buffer));
      c = transform_from_fixed_point(read_u16(buffer));
      d = transform_from_fixed_point(read_u16(buffer));
    }

    Glyph* current_glyph = &comp_glyph->glyphs[comp_glyph->glyph_count++];

    parse_glyph(original_buf, current_glyph, glyph_locations[glyph_index], glyph_locations, 0);
    resize_compound_glyph(current_glyph, offset_x, offset_y, a, b, c, d);
  }
}

static void parse_glyph(char* buf, Glyph* glyph, u32 offset, u32* glyph_locations, u32 idx)
{
  const int TABLE_GLYF_SIZE = 10;

  Buffer    buffer(buf + offset);
  i16       number_of_contours = read_u16(&buffer);
  buffer.advance(TABLE_GLYF_SIZE - sizeof(i16));

  if (number_of_contours >= 0)
  {
    read_simple_glyph(glyph, &buffer, number_of_contours);
    glyph->s = true;
  }
  else
  {
    read_compound_glyph(glyph_locations, buf, glyph, &buffer);
    glyph->s = false;
  }
}

static void parse_all_glyphs(char* buf, Glyph** glyphs, u32* glyph_locations, u32 number_of_glyphs)
{
  Glyph* g = (Glyph*)sta_allocate_struct(Glyph, number_of_glyphs);

  for (u32 i = 0; i < number_of_glyphs; i++)
  {
    Glyph* glyph  = &g[i];
    u32    offset = glyph_locations[i];
    parse_glyph(buf, glyph, offset, glyph_locations, i);
  }

  *glyphs = g;
}

void parse_mapping(char* buf, Font* font, Table* cmap_table)
{
  Buffer buffer(buf + cmap_table->offset);
  cmap_table->cmap                      = (TableCmap*)buffer.current_address();
  cmap_table->cmap->version             = swap_u16(cmap_table->cmap->version);
  cmap_table->cmap->number_of_subtables = swap_u16(cmap_table->cmap->number_of_subtables);
  buffer.advance(sizeof(TableCmap));

  u32 subtables            = cmap_table->cmap->number_of_subtables;
  i8  unicode_version      = -1;
  u32 mapping_table_offset = 0;
  for (u32 i = 0; i < subtables; i++)
  {
    CmapSubtable subtable = *(CmapSubtable*)buffer.current_address();
    buffer.advance(sizeof(CmapSubtable));
    subtable.offset               = swap_u32(subtable.offset);
    subtable.platform_specific_id = swap_u16(subtable.platform_specific_id);
    subtable.platform_id          = swap_u16(subtable.platform_id);
    // only parsing unicode
    if (subtable.platform_id == 0)
    {
      if (subtable.platform_specific_id == 0 || subtable.platform_specific_id == 1 || subtable.platform_specific_id == 3 || subtable.platform_specific_id == 4)
      {
        unicode_version      = MAX(unicode_version, subtable.platform_specific_id);
        mapping_table_offset = subtable.offset;
      }
    }
  }

  assert(mapping_table_offset != 0 && "Couldn't find a table to parse with this font");

  buffer.buffer += mapping_table_offset;
  buffer.index = 0;

  u16 format   = read_u16(&buffer);

  assert((format == 4 || format == 12) && "Can't parse this font format");
  font->char_codes    = (u32*)sta_allocate_struct(u32, font->glyph_count);
  font->glyph_indices = (u32*)sta_allocate_struct(u32, font->glyph_count);

  if (format == 4)
  {
    buffer.advance(sizeof(u16) * 2);
    u16 seg_count = read_u16(&buffer) / 2;
    buffer.advance(sizeof(u16) * 3);
    u16 end_codes[seg_count];
    for (u32 i = 0; i < seg_count; i++)
    {
      end_codes[i] = read_u16(&buffer);
    }

    (void)read_u16(&buffer);
    u16 start_codes[seg_count];
    for (u32 i = 0; i < seg_count; i++)
    {
      start_codes[i] = read_u16(&buffer);
    }
    u16 id_delta[seg_count];
    for (u32 i = 0; i < seg_count; i++)
    {
      id_delta[i] = read_u16(&buffer);
    }
    u16 id_offset[seg_count];
    u16 id_range[seg_count];
    for (u32 i = 0; i < seg_count; i++)
    {
      id_offset[i] = read_u16(&buffer);
    }
    for (u32 i = 0; i < seg_count; i++)
    {
      id_range[i] = read_u16(&buffer);
    }

    u32 read_indices = 0;
    for (u32 i = 0; i < seg_count; i++)
    {
      u16 start_code = start_codes[i];
      u16 end_code   = end_codes[i];

      if (start_code == UINT16_MAX)
      {
        break;
      }

      for (; start_code <= end_code; start_code++)
      {
        u32 glyph_index;
        if (id_offset[i] == 0)
        {
          glyph_index = (start_code + id_delta[i]) % (UINT16_MAX + 1);
        }
        else
        {
          u32 current_idx                = buffer.index;
          i32 range_offset_location      = id_range[i] + id_offset[i];
          i32 glyph_index_array_location = 2 * (start_code - start_codes[i]) + range_offset_location;

          buffer.index                   = glyph_index_array_location;
          glyph_index                    = read_u16(&buffer);

          if (glyph_index != 0)
          {
            glyph_index = (glyph_index + id_delta[i]) % (UINT16_MAX + 1);
          }

          buffer.index = current_idx;
        }

        font->glyph_indices[read_indices] = glyph_index;
        font->char_codes[read_indices++]  = start_code;
      }
    }
  }
  // format == 12
  else
  {
    buffer.advance(sizeof(u16) + sizeof(u32) * 2);

    u32 n_groups     = read_u32(&buffer);

    u32 read_indices = 0;
    for (u32 i = 0; i < n_groups; i++)
    {
      u32 start_char_code  = read_u32(&buffer);
      u32 end_char_code    = read_u32(&buffer);
      u32 start_glyph_code = read_u32(&buffer);
      u32 number_of_chars  = end_char_code - start_char_code + 1;
      for (u32 j = 0; j < number_of_chars; j++)
      {
        font->char_codes[read_indices]    = start_char_code + j;
        font->glyph_indices[read_indices] = start_glyph_code + j;
        read_indices++;
        assert(read_indices <= font->glyph_count && "Read more then count?");
      }
    }
  }
}

static void read_table_hhea(Table* table, u32 offset, char* buf)
{
  Buffer buffer(buf + offset);
  table->hhea                  = (TableHhea*)sta_allocate_struct(TableHhea, 1);
  TableHhea* hhea              = table->hhea;
  hhea->version                = read_u32(&buffer);
  hhea->ascent                 = read_u16(&buffer);
  hhea->descent                = read_u16(&buffer);
  hhea->line_gap               = read_u16(&buffer);
  hhea->advance_width_max      = read_u16(&buffer);
  hhea->min_left_side_bearing  = read_u16(&buffer);
  hhea->min_right_side_bearing = read_u16(&buffer);
  hhea->x_max_extent           = read_u16(&buffer);
  hhea->caret_slope_rise       = read_u16(&buffer);
  hhea->caret_slope_run        = read_u16(&buffer);
  hhea->caret_offset           = read_u16(&buffer);
  buffer.advance(sizeof(u16) * 4);
  hhea->metric_data_format      = read_u16(&buffer);
  hhea->num_of_long_hor_metrics = read_u16(&buffer);
}

static void parse_advance_widths(Font* font, Table* hhea, Table* hmtx, char* buf)
{
  u32           number_of_metrics = hhea->hhea->num_of_long_hor_metrics;
  Buffer        buffer(buf + hmtx->offset);
  LongHorMetric metrics[font->glyph_count];
  for (u32 i = 0; i < number_of_metrics; i++)
  {
    metrics[i].advance_width      = read_u16(&buffer);
    metrics[i].left_side_bearing  = read_u16(&buffer);
    font->glyphs[i].advance_width = metrics[i].advance_width;
  }
}

TableDirectory TableDirectory::read(Buffer* buffer)
{
  TableDirectory directory = *(TableDirectory*)buffer->buffer;
  directory.swap_endianess();
  buffer->index += sizeof(TableDirectory);
  return directory;
}

void Font::parse_ttf(const char* filename)
{

  Table  tables[DUMMY_TABLE_TYPE];
  Buffer buffer = {};
  bool   result = sta_read_file(&buffer, filename);
  if (!result)
  {
    printf("Couldn find file %s\n", filename);
    return;
  }
  TableDirectory directory = TableDirectory::read(&buffer);

  TableRecord*   records   = (TableRecord*)sta_allocate(sizeof(TableRecord) * directory.num_tables);

  for (u16 i = 0; i < directory.num_tables; i++, buffer.advance(sizeof(TableRecord)))
  {
    TableRecord record = *(TableRecord*)buffer.current_address();
    record.swap_endianess();
    records[i] = record;

    if (record.match_tag("glyf"))
    {
      Table* table  = &tables[TABLE_GLYF];
      table->type   = TABLE_GLYF;
      table->glyf   = (TableGlyf*)sta_allocate_struct(TableGlyf, 1);
      *table->glyf  = *(TableGlyf*)&buffer.buffer[record.offset];
      table->offset = record.offset;
      table->glyf->swap_endianess();
    }
    else if (record.match_tag("head"))
    {
      Table* table                     = &tables[TABLE_HEAD];
      table->type                      = TABLE_HEAD;
      table->head                      = (TableHead*)sta_allocate_struct(TableHead, 1);
      table->head->units_per_em        = swap_u16(*(u16*)&buffer.buffer[record.offset + 18]);
      table->head->index_to_loc_format = swap_u16(*(u16*)&buffer.buffer[record.offset + 50]);
      this->scale                      = 1.0f / (f32)table->head->units_per_em;
    }
    else if (record.match_tag("maxp"))
    {
      Table* table = &tables[TABLE_MAXP];
      table->type  = TABLE_MAXP;
      table->maxp  = (TableMaxp*)&buffer.buffer[record.offset];
      table->maxp->swap_endianess();
    }
    else if (record.match_tag("loca"))
    {
      Table* table  = &tables[TABLE_LOCA];
      table->type   = TABLE_LOCA;
      table->loca   = &records[i];
      table->offset = record.offset;
    }
    else if (record.match_tag("cmap"))
    {
      Table* table  = &tables[TABLE_CMAP];
      table->type   = TABLE_CMAP;
      table->offset = record.offset;
    }
    else if (record.match_tag("hhea"))
    {
      Table* table  = &tables[TABLE_HHEA];
      table->type   = TABLE_HHEA;
      table->offset = record.offset;
      read_table_hhea(table, record.offset, buffer.buffer);
    }
    else if (record.match_tag("hmtx"))
    {
      Table* table  = &tables[TABLE_HMTX];
      table->type   = TABLE_HMTX;
      table->offset = record.offset;
    }
  }

  u32* glyph_locations;
  get_all_glyph_locations(tables, buffer.buffer, &glyph_locations, this->glyph_count);
  parse_all_glyphs(buffer.buffer, &this->glyphs, glyph_locations, this->glyph_count);
  parse_advance_widths(this, &tables[TABLE_HHEA], &tables[TABLE_HMTX], buffer.buffer);
  parse_mapping(buffer.buffer, this, &tables[TABLE_CMAP]);
}

#undef REPEAT
#undef BIT_IS_SET

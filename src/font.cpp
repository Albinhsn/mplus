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

static inline u16 read_u16(Buffer* buffer)
{
  u16 out = swap_u16(*(u16*)buffer->current_address());
  buffer->advance(sizeof(u16));
  return out;
}
static inline u32 read_u32(Buffer* buffer)
{
  u32 out = swap_u16(*(u32*)buffer->current_address());
  buffer->advance(sizeof(u32));
  return out;
}

void swap_record(TableRecord* record)
{
  record->length   = swap_u32(record->length);
  record->offset   = swap_u32(record->offset);
  record->checksum = swap_u32(record->checksum);
}
void swap_directory(TableDirectory* directory)
{
  directory->num_tables     = swap_u16(directory->num_tables);
  directory->range_shirt    = swap_u16(directory->range_shirt);
  directory->search_ranges  = swap_u16(directory->search_ranges);
  directory->entry_selector = swap_u16(directory->entry_selector);
  directory->sfnt_version   = swap_u16(directory->sfnt_version);
}

void swap_maxp_data(TableMaxp* data)
{
  data->num_glyphs               = swap_u16(data->num_glyphs);
  data->version                  = swap_u32(data->version);
  data->max_points               = swap_u16(data->max_points);
  data->max_contours             = swap_u16(data->max_contours);
  data->max_component_points     = swap_u16(data->max_component_points);
  data->max_component_contours   = swap_u16(data->max_component_contours);
  data->max_zones                = swap_u16(data->max_zones);
  data->max_twilight_points      = swap_u16(data->max_twilight_points);
  data->max_storage              = swap_u16(data->max_storage);
  data->max_function_defs        = swap_u16(data->max_function_defs);
  data->max_instruction_defs     = swap_u16(data->max_instruction_defs);
  data->max_stack_elements       = swap_u16(data->max_stack_elements);
  data->max_size_of_instructions = swap_u16(data->max_size_of_instructions);
  data->max_component_elements   = swap_u16(data->max_component_elements);
  data->max_component_depth      = swap_u16(data->max_component_depth);
}

void swap_glyph_data(TableGlyf* data)
{
  data->number_of_contours = swap_u16(data->number_of_contours);
  data->min_x              = swap_u16(data->min_x);
  data->max_x              = swap_u16(data->max_x);
  data->min_y              = swap_u16(data->min_y);
  data->max_y              = swap_u16(data->max_y);
}

void read_coordinates(i16* coordinates, u8* flags, u32 count, Buffer* buffer, bool reading_x, i16& min, i16& max)
{
  min = INT16_MAX;
  max = -INT16_MAX;
  // check if 1 or 2 byte long
  u8 offset_size_flag_bit = reading_x ? 1 : 2;

  // if offset_size_flag_bit is set
  //    this bit describes the sign of the value, with a value of 1 equalling positive and a zero value negative
  // if the offset_size_flag_bit bit is not set, and this bit is set,
  //    then the current coordinate is the same as the previous one

  // if the offset_size_flag_bit bit is not set, and this bit is not set
  //    the current coordinate is a signed 16 bit delta vector. In this case, the delta vector is the change in x/y
  u8  offset_sign_or_skip_bit = reading_x ? 4 : 5;

  i16 curr                    = 0;
  for (u32 i = 0; i < count; i++)
  {

    u8   flag        = flags[i];
    bool on_curve    = BIT_IS_SET(flag, 0);
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
    min            = MIN(curr, min);
    max            = MAX(curr, max);

    coordinates[i] = curr;
  }
}

void read_simple_glyph(Glyph* simple, Buffer* buffer, int n)
{

  if (n > 0)
  {
    simple->n                   = n;
    simple->end_pts_of_contours = (u16*)sta_allocate_struct(u16, n);
    for (u16 i = 0; i < simple->n; i++, buffer->advance(sizeof(u16)))
    {
      simple->end_pts_of_contours[i] = swap_u16(*(u16*)buffer->current_address());
    }
  }

  simple->instruction_length = swap_u16(*(u16*)buffer->current_address());
  buffer->advance(sizeof(u16));

  simple->instructions = (u8*)sta_allocate_struct(u8, simple->instruction_length);
  strncpy((char*)simple->instructions, buffer->current_address(), simple->instruction_length);
  buffer->advance(simple->instruction_length);

  u16 number_of_points = n > 0 ? simple->end_pts_of_contours[simple->n - 1] + 1 : 0;

  simple->flags        = (u8*)sta_allocate_struct(u8, number_of_points);

  for (u32 i = 0; i < number_of_points; i++)
  {
    u8 flag          = buffer->advance();
    simple->flags[i] = flag;

    if (REPEAT(flag))
    {
      u32 repeat_for = buffer->advance();
      for (u32 repeat = 0; repeat < repeat_for; repeat++)
      {
        simple->flags[++i] = flag;
      }
      assert(i <= number_of_points && "Repeated past the end?");
    }
  }

  if (number_of_points > 0)
  {
    simple->x_coordinates = (i16*)sta_allocate_struct(i16, number_of_points);
    simple->y_coordinates = (i16*)sta_allocate_struct(i16, number_of_points);
    read_coordinates(simple->x_coordinates, simple->flags, number_of_points, buffer, true, simple->min_x, simple->max_x);
    read_coordinates(simple->y_coordinates, simple->flags, number_of_points, buffer, false, simple->min_y, simple->max_y);
  }
}

static void get_all_glyph_locations(Table* tables, char* buf, u32** glyph_locations, u32& number_of_glyphs)
{
  number_of_glyphs            = tables[TABLE_MAXP].maxp->num_glyphs;
  u32    bytes_per_entry      = tables[TABLE_HEAD].head->index_to_loc_format() == 0 ? 2 : 4;

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
      glyph_data_offset = swap_u16(*(u16*)buffer.current_address()) * 2;
      buffer.advance(sizeof(u16));
    }
    else
    {
      glyph_data_offset = swap_u32(*(u32*)buffer.current_address());
      buffer.advance(sizeof(u32));
    }

    locations[glyph_index] = glyph_table_start + glyph_data_offset;
  }

  *glyph_locations = locations;
}

f32 transform_from_fixed_point(u16 x)
{
  return ((i16)(x)) / (f64)(1 << 14);
}

void        parse_glyph(char* buf, Glyph* glyph, u32 offset, u32* glyph_locations, u32 this_glyph);

static void read_compound_glyph(u32* glyph_locations, char* original_buf, Glyph* comp_glyph, Buffer* buffer, u32 this_glyph)
{
  // should have header?

  bool   has_more_glyphs = true;
  u32    total_points    = 0;
  u32    glyph_count = 0, glyph_capacity = 2;
  Glyph* glyphs = (Glyph*)sta_allocate_struct(Glyph, glyph_capacity);
  while (has_more_glyphs)
  {
    RESIZE_ARRAY(glyphs, Glyph, glyph_count, glyph_capacity);

    u16 flags = *(u16*)buffer->current_address();
    flags     = swap_u16(flags);
    buffer->advance(sizeof(u16));

    u16 glyph_index = *(u16*)buffer->current_address();
    glyph_index     = swap_u16(glyph_index);
    buffer->advance(sizeof(u16));

    has_more_glyphs           = BIT_IS_SET(flags, 5);

    bool has_scale            = BIT_IS_SET(flags, 3);
    bool xy_scale             = BIT_IS_SET(flags, 6);
    bool two_x_two            = BIT_IS_SET(flags, 7);
    bool instructions         = BIT_IS_SET(flags, 8);
    bool metrics              = BIT_IS_SET(flags, 9);
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
      a = transform_from_fixed_point(swap_u16(*(u16*)buffer->current_address()));
      buffer->advance(sizeof(u16));
      d = a;
    }
    else if (xy_scale)
    {
      a = transform_from_fixed_point(swap_u16(*(u16*)buffer->current_address()));
      buffer->advance(sizeof(u16));
      d = transform_from_fixed_point(swap_u16(*(u16*)buffer->current_address()));
      buffer->advance(sizeof(u16));
    }
    else if (two_x_two)
    {
      a = transform_from_fixed_point(swap_u16(*(u16*)buffer->current_address()));
      buffer->advance(sizeof(u16));
      b = transform_from_fixed_point(swap_u16(*(u16*)buffer->current_address()));
      buffer->advance(sizeof(u16));
      c = transform_from_fixed_point(swap_u16(*(u16*)buffer->current_address()));
      buffer->advance(sizeof(u16));
      d = transform_from_fixed_point(swap_u16(*(u16*)buffer->current_address()));
      buffer->advance(sizeof(u16));
    }

    Glyph* current_glyph = &glyphs[glyph_count++];

    // parse the glyph from glyphindex
    parse_glyph(original_buf, current_glyph, glyph_locations[glyph_index], glyph_locations, glyph_index);
    comp_glyph->min_x = MIN(comp_glyph->min_x, current_glyph->min_x);
    comp_glyph->min_y = MIN(comp_glyph->min_y, current_glyph->min_y);
    comp_glyph->max_x = MAX(comp_glyph->max_x, current_glyph->max_x);
    comp_glyph->max_y = MAX(comp_glyph->max_y, current_glyph->max_y);

    for (u32 i = 0; i <= current_glyph->end_pts_of_contours[current_glyph->n - 1]; i++)
    {
      i16 x                           = current_glyph->x_coordinates[i];
      i16 y                           = current_glyph->y_coordinates[i];

      current_glyph->x_coordinates[i] = x * a + c * y + offset_x;
      current_glyph->y_coordinates[i] = x * b + d * y + offset_y;
    }
    comp_glyph->n += current_glyph->n;
    total_points += current_glyph->end_pts_of_contours[current_glyph->n - 1];
    // current_glyph->debug();
    // exit(1);
  }

  comp_glyph->end_pts_of_contours = (u16*)sta_allocate_struct(u16, comp_glyph->n);
  comp_glyph->x_coordinates       = (i16*)sta_allocate_struct(i16, total_points);
  comp_glyph->y_coordinates       = (i16*)sta_allocate_struct(i16, total_points);
  u32 n = 0, coords = 0;
  for (u32 i = 0; i < glyph_count; i++)
  {
    Glyph glyph = glyphs[i];
    for (u32 j = 0; j <= glyph.end_pts_of_contours[glyph.n - 1]; j++)
    {
      comp_glyph->x_coordinates[coords + j] = glyph.x_coordinates[j];
      comp_glyph->y_coordinates[coords + j] = glyph.y_coordinates[j];
    }
    for (u32 j = 0; j < glyph.n; j++)
    {
      comp_glyph->end_pts_of_contours[n + j] = glyph.end_pts_of_contours[j] + coords;
    }

    n += glyph.n;
    coords += glyph.end_pts_of_contours[glyph.n - 1] + 1;
    // add x and y from prev count
  }
  sta_deallocate(glyphs, sizeof(Glyph) * glyph_capacity);
}

void parse_glyph(char* buf, Glyph* glyph, u32 offset, u32* glyph_locations, u32 this_glyph)
{
  TableGlyf table = *(TableGlyf*)&buf[offset];
  swap_glyph_data(&table);
  Buffer buffer(buf + offset);
  buffer.index = 0;
  buffer.advance(sizeof(TableGlyf));

  i16 number_of_contours = table.number_of_contours;
  glyph->max_x           = 0;
  glyph->min_x           = 0;
  glyph->max_y           = 0;
  glyph->min_y           = 0;
  // need to parse table  as well
  if (number_of_contours >= 0)
  {
    read_simple_glyph(glyph, &buffer, number_of_contours);
  }
  else
  {
    read_compound_glyph(glyph_locations, buf, glyph, &buffer, this_glyph);
  }
}

static void parse_all_glyphs(char* buf, Glyph** glyphs, u32* glyph_locations, u32 number_of_glyphs)
{
  Glyph* g = (Glyph*)sta_allocate_struct(Glyph, number_of_glyphs);
  memset(g, 0, sizeof(Glyph));

  for (u32 i = 0; i < number_of_glyphs; i++)
  {
    Glyph* glyph  = &g[i];
    u32    offset = glyph_locations[i];
    parse_glyph(buf, glyph, offset, glyph_locations, i);
  }

  *glyphs = g;
}

void parse_mapping(char* buf, Mapping* mapping, Table* cmap_table)
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
    printf("platform: %d specific: %d offset: %d\n", subtable.platform_id, subtable.platform_specific_id, subtable.offset);
    switch (subtable.platform_id)
    {
    // unicode
    case 0:
    {
      if (subtable.platform_specific_id == 0 || subtable.platform_specific_id == 1 || subtable.platform_specific_id == 3 || subtable.platform_specific_id == 4)
      {
        unicode_version      = MAX(unicode_version, subtable.platform_specific_id);
        mapping_table_offset = subtable.offset;
      }
      break;
    }
    default:
    {
      // jk this is valid?
      assert(0 && "Bro why are you using this encoding xD");
    }
    }
  }

  if (mapping_table_offset == 0)
  {
    assert(0 && "Couldn't find a table to parse with this font");
  }

  buffer.buffer += mapping_table_offset;
  buffer.index = 0;

  u16 format   = *(u16*)buffer.current_address();
  format       = swap_u16(format);
  buffer.advance(sizeof(u16));

  assert((format == 4 || format == 12) && "Can't parse this font format");

  if (format == 4)
  {
    u16 length = read_u16(&buffer);
    ;
    u16 language       = read_u16(&buffer);
    u16 seg_count_x2   = read_u16(&buffer);
    u16 seg_count      = seg_count_x2 / 2;
    u16 search_range   = read_u16(&buffer);
    u16 entry_selector = read_u16(&buffer);
    u16 range_shift    = read_u16(&buffer);
    u16 end_code[seg_count];
    for (u32 i = 0; i < seg_count; i++)
    {
      end_code[i] = read_u16(&buffer);
    }
    u16 reserved_pad = read_u16(&buffer);
    u16 start_code[seg_count];
    for (u32 i = 0; i < seg_count; i++)
    {
      start_code[i] = read_u16(&buffer);
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

    // each segment is described by
    //  startcode
    //  endcode 
    //  id delta
    //  id range offset
    //
    //
    //  if the corresponding start_code is <= character code
    //  use the corresponding id_delta nad id_range_offset to map the character code to the glyph index
    for (u32 i = 0; i < seg_count; i++)
    {
    }
  }
  // format == 12
  else
  {
  }
}

void sta_font_parse_ttf(Font* font, const char* filename)
{

  const int number_of_tables = DUMMY_TABLE;
  Table     tables[number_of_tables];
  Buffer    buffer = {};
  bool      result = sta_read_file(&buffer, filename);
  if (!result)
  {
    printf("Couldn find file %s\n", filename);
    return;
  }

  TableDirectory directory = *(TableDirectory*)buffer.buffer;
  swap_directory(&directory);
  buffer.index         = sizeof(TableDirectory);

  TableRecord* records = (TableRecord*)sta_allocate(sizeof(TableRecord) * directory.num_tables);

  for (u16 i = 0; i < directory.num_tables; i++, buffer.advance(sizeof(TableRecord)))
  {
    TableRecord record = *(TableRecord*)buffer.current_address();
    swap_record(&record);
    records[i] = record;

    if (record.match_tag("glyf"))
    {
      Table* table  = &tables[TABLE_GLYF];
      table->type   = TABLE_GLYF;
      table->glyf   = (TableGlyf*)sta_allocate_struct(TableGlyf, 1);
      *table->glyf  = *(TableGlyf*)&buffer.buffer[record.offset];
      table->offset = record.offset;
      swap_glyph_data(table->glyf);
    }
    else if (record.match_tag("head"))
    {
      Table* table        = &tables[TABLE_HEAD];
      table->type         = TABLE_HEAD;
      table->head         = (TableHead*)sta_allocate_struct(TableHead, 1);
      table->head->buffer = &buffer.buffer[record.offset];
    }
    else if (record.match_tag("maxp"))
    {
      Table* table = &tables[TABLE_MAXP];
      table->type  = TABLE_MAXP;
      table->maxp  = (TableMaxp*)&buffer.buffer[record.offset];
      swap_maxp_data(table->maxp);
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
  }

  u32* glyph_locations;
  get_all_glyph_locations(tables, buffer.buffer, &glyph_locations, font->glyph_count);
  parse_all_glyphs(buffer.buffer, &font->glyphs, glyph_locations, font->glyph_count);
  parse_mapping(buffer.buffer, &font->mapping, &tables[TABLE_CMAP]);
}
#undef REPEAT
#undef BIT_IS_SET

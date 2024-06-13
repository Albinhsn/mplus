#include "font.h"
#include "common.h"
#include "files.h"
#include "platform.h"
#include <cassert>
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

void read_coordinates(i16* coordinates, u8* flags, u32 count, Buffer* buffer, bool reading_x)
{
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
      i16 offset = buffer->advance();
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

void read_simple_glyph(SimpleGlyph* simple, Buffer* buffer, int n)
{
  simple->n                   = n;
  simple->end_pts_of_contours = (u16*)sta_allocate_struct(u16, simple->n);
  printf("Found %d contours\n", simple->n);
  for (u16 i = 0; i < simple->n; i++, buffer->advance(sizeof(u16)))
  {
    simple->end_pts_of_contours[i] = swap_u16(*(u16*)buffer->current_address());
    printf("End pt %d: %d\n", i, simple->end_pts_of_contours[i]);
  }

  simple->instruction_length = swap_u16(*(u16*)buffer->current_address());
  buffer->advance(sizeof(u16));

  simple->instructions = (u8*)sta_allocate_struct(u8, simple->instruction_length);
  strncpy((char*)simple->instructions, buffer->current_address(), simple->instruction_length);
  buffer->advance(simple->instruction_length);

  u16 number_of_points = simple->end_pts_of_contours[simple->n - 1] + 1;

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

  simple->x_coordinates = (i16*)sta_allocate_struct(i16, number_of_points);
  simple->y_coordinates = (i16*)sta_allocate_struct(i16, number_of_points);
  read_coordinates(simple->x_coordinates, simple->flags, number_of_points, buffer, true);
  read_coordinates(simple->y_coordinates, simple->flags, number_of_points, buffer, false);

  for (u32 i = 0; i < number_of_points; i++)
  {
    printf("Glyph %d: %d %d\n", i, simple->x_coordinates[i], simple->y_coordinates[i]);
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

static void parse_all_glyphs(char* buf, Glyph** glyphs, u32* glyph_locations, u32 number_of_glyphs)
{
  Glyph* g = (Glyph*)sta_allocate_struct(Glyph, number_of_glyphs);

  for (u32 i = 0; i < number_of_glyphs; i++)
  {
    Glyph*     glyph  = &g[i];

    u32        offset = glyph_locations[i];

    TableGlyf* table  = (TableGlyf*)&buf[offset];

    // check simple
    //
    i16 number_of_contours = table->number_of_contours;
    glyph->is_simple       = number_of_contours >= 0;
    if (glyph->is_simple)
    {
      Buffer buffer(buf + offset, 0);
      printf("Parsing from %d\n", offset);
      read_simple_glyph(&glyph->simple, &buffer, number_of_contours);
    }
    else
    {
      printf("COMPOUND %d\n", i);
    }
  }

  *glyphs = g;
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
  printf("%s: %d\n", filename, directory.num_tables);

  for (u16 i = 0; i < directory.num_tables; i++, buffer.advance(sizeof(TableRecord)))
  {
    TableRecord record = *(TableRecord*)buffer.current_address();
    swap_record(&record);
    records[i] = record;

    if (record.match_tag("glyf"))
    {
      Table* table  = &tables[TABLE_GLYF];
      table->type   = TABLE_GLYF;
      table->glyf   = (TableGlyf*)&buffer.buffer[record.offset];
      table->offset = record.offset;
      swap_glyph_data(table->glyf);
      printf("%d %d %d %d %d\n", table->glyf->number_of_contours, table->glyf->min_x, table->glyf->max_x, table->glyf->min_y, table->glyf->max_y);
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

    printf("Tag: %.*s Location: %d\n", 4, record.tag, record.offset);
  }

  u32* glyph_locations;
  get_all_glyph_locations(tables, buffer.buffer, &glyph_locations, font->glyph_count);
  parse_all_glyphs(buffer.buffer, &font->glyphs, glyph_locations, font->glyph_count);
}
#undef REPEAT
#undef BIT_IS_SET

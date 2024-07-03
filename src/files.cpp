#include "files.h"

static inline Vector2 cast_vec3_to_vec2(Vector3 v)
{
  return (Vector2){v.x, v.y};
}

static inline Vector3 cast_vec4_to_vec3(Vector4 v)
{
  return (Vector3){v.x, v.y, v.z};
}

bool Buffer::match(char c)
{
  return this->current_char() == c;
}
int Buffer::parse_int()
{
  if (this->is_out_of_bounds())
  {
    assert(0 && "Out of bounds!");
  }
  bool sign = false;
  if (this->current_char() == '-')
  {
    this->advance();
    sign = true;
  }
  u64 value = 0;
  while (isdigit(this->current_char()))
  {
    value *= 10;
    value += this->current_char() - '0';
    this->advance();
  }
  return sign ? -value : value;
}

int parse_int_from_string(const char* s)
{
  Buffer buffer = {};
  buffer.index  = 0;
  buffer.len    = strlen(s);
  buffer.buffer = (char*)s;
  return parse_int_from_string(&buffer);
}
int parse_int_from_string(Buffer* buffer)
{
  if (buffer->is_out_of_bounds())
  {
    assert(0 && "Out of bounds!");
  }
  bool sign = false;
  if (CURRENT_CHAR(buffer) == '-')
  {
    ADVANCE(buffer);
    sign = true;
  }
  u64 value = 0;
  while (isdigit(CURRENT_CHAR(buffer)))
  {
    value *= 10;
    value += CURRENT_CHAR(buffer) - '0';
    ADVANCE(buffer);
  }
  return sign ? -value : value;
}
void Buffer::parse_float_array(f32* array, u64 count)
{
  for (unsigned int i = 0; i < count; i++)
  {
    while (!(isdigit(this->current_char()) || this->current_char() == '-' || this->current_char() == 'e'))
    {
      this->advance();
    }
    array[i] = this->parse_float();
  }
}
void Buffer::parse_int_array(i32* array, u64 count)
{
  for (u32 i = 0; i < count; i++)
  {
    while (!(isdigit(this->current_char()) || this->current_char() == '-'))
    {
      this->advance();
    }
    array[i] = this->parse_int();
  }
}

void Buffer::parse_string_array(char** array, u64 count)
{
  u32 prev = 0;
  for (u32 i = 0; i < count; i++)
  {
    while (!this->match(' ') && !this->match('<') && !this->match('\n'))
    {
      this->advance();
    }
    array[i] = (char*)sta_allocate(this->index - prev + 1);
    strncpy(array[i], &this->buffer[prev], this->index - prev);
    array[i][this->index - prev] = '\0';

    this->advance();
    prev = this->index;
  }
}
void Buffer::parse_vector2_array(Vector2* array, u64 count)
{
  for (unsigned int i = 0; i < count; i++)
  {
    Vector2* position = &array[i];
    this->skip_whitespace();
    position->x = this->parse_float();
    this->skip_whitespace();
    position->y = this->parse_float();
  }
}
void Buffer::parse_vector3_array(Vector3* array, u64 count)
{
  for (unsigned int i = 0; i < count; i++)
  {
    Vector3* position = &array[i];
    this->skip_whitespace();
    position->x = this->parse_float();
    this->skip_whitespace();
    position->y = this->parse_float();
    this->skip_whitespace();
    position->z = this->parse_float();
  }
}

float Buffer::parse_float()
{
  bool sign = false;
  if (this->current_char() == '-')
  {
    this->advance();
    sign = true;
  }
  float value = 0;
  while (isdigit(this->current_char()))
  {
    value *= 10;
    value += this->current_char() - '0';
    this->advance();
  }

  float decimal       = 0.0f;
  int   decimal_count = 0;
  if (this->current_char() == '.')
  {
    this->advance();
    while (isdigit(this->current_char()))
    {
      decimal_count++;
      decimal += (this->current_char() - '0') / pow(10, decimal_count);
      this->advance();
    }
  }

  value += decimal;
  if (this->current_char() == 'e' || this->current_char() == 'E')
  {
    this->advance();
    bool sign_exp = false;
    if (this->current_char() == '-')
    {
      this->advance();
      sign_exp = true;
    }
    u32 exp = (this->current_char() - '0');
    this->advance();
    while (isdigit(this->current_char()))
    {
      exp *= 10;
      exp += this->current_char() - '0';
      this->advance();
    }

    if (sign_exp)
    {
      value /= pow(10, exp);
    }
    else
    {
      value *= pow(10, exp);
    }
  }

  return sign ? -value : value;
}
float parse_float_from_string(Buffer* buffer)
{
  bool sign = false;
  if (CURRENT_CHAR(buffer) == '-')
  {
    ADVANCE(buffer);
    sign = true;
  }
  float value = 0;
  while (isdigit(CURRENT_CHAR(buffer)))
  {
    value *= 10;
    value += CURRENT_CHAR(buffer) - '0';
    ADVANCE(buffer);
  }

  float decimal       = 0.0f;
  int   decimal_count = 0;
  if (CURRENT_CHAR(buffer) == '.')
  {
    ADVANCE(buffer);
    while (isdigit(CURRENT_CHAR(buffer)))
    {
      decimal_count++;
      decimal += (CURRENT_CHAR(buffer) - '0') / pow(10, decimal_count);
      ADVANCE(buffer);
    }
  }

  value += decimal;
  if (CURRENT_CHAR(buffer) == 'e' || CURRENT_CHAR(buffer) == 'E')
  {
    ADVANCE(buffer);
    bool sign_exp = false;
    if (CURRENT_CHAR(buffer) == '-')
    {
      ADVANCE(buffer);
      sign_exp = true;
    }
    u32 exp = (CURRENT_CHAR(buffer) - '0');
    ADVANCE(buffer);
    while (isdigit(CURRENT_CHAR(buffer)))
    {
      exp *= 10;
      exp += CURRENT_CHAR(buffer) - '0';
      ADVANCE(buffer);
    }

    if (sign_exp)
    {
      value /= pow(10, exp);
    }
    else
    {
      value *= pow(10, exp);
    }
  }

  return sign ? -value : value;
}

bool match(Buffer* buffer, char target)
{
  return CURRENT_CHAR(buffer) == target;
}
bool consume(Buffer* buffer, char target)
{
  if (!match(buffer, target))
  {
    return false;
  }
  ADVANCE(buffer);
  return true;
}
void Buffer::skip_whitespace()
{
  while (!this->is_out_of_bounds() && (this->match(' ') || this->match('\n') || this->match('\t')))
  {
    this->advance();
  }
}

void skip_whitespace(Buffer* buffer)
{
  SKIP(buffer, !buffer->is_out_of_bounds() && (match(buffer, ' ') || match(buffer, '\n') || match(buffer, '\t')));
}

void sta_targa_save_to_file(TargaImage* image, const char* filename)
{
  printf("Saving at '%s'\n", filename);
  FILE*         filePtr;
  unsigned long count;

  for (u64 idx = 0; idx < image->height * image->width * 4; idx += 4)
  {
    unsigned char tmp    = image->data[idx];
    image->data[idx]     = image->data[idx + 2];
    image->data[idx + 2] = tmp;
  }

  struct TargaHeader h;
  memset(&h.header[0], 0, 18);
  h.width          = image->width;
  h.height         = image->height;
  h.imageType      = 2;
  h.imagePixelSize = 32;

  filePtr          = fopen(filename, "wb");
  count            = fwrite(&h, sizeof(struct TargaHeader), 1, filePtr);
  if (count != 1)
  {
    printf("ERROR: Failed to write into header\n");
    return;
  }

  u32 imageSize = 4 * image->width * image->height;

  // Read in the targa image data.
  count = fwrite(image->data, 1, imageSize, filePtr);
  if (count != imageSize)
  {
    printf("ERROR: count after write doesn't equal imageSize %ld %d\n", count, imageSize);
    return;
  }

  if (fclose(filePtr) != 0)
  {
    printf("Failed to close file\n");
    return;
  }
}

bool sta_write_ppm(Buffer fileName, u8* data, u64 width, u64 height)
{
  char buffer[256] = {};
  strncpy(&buffer[0], &fileName.buffer[0], fileName.len);

  FILE* filePtr = fopen(buffer, "w");
  if (filePtr == 0)
  {
    printf("Failed to open %.*s\n", (i32)fileName.len, fileName.buffer);
    return false;
  }

  int result = fprintf(filePtr, "P3\n%ld %ld\n255\n", width, height);
  if (result < 0)
  {
    return false;
  }

  u64 imageSize = height * width * 4;
  u8* imageData = data;

  for (u64 i = 0; i < imageSize; i += 4)
  {
    result = fprintf(filePtr, "%d %d %d\n", imageData[i + 0], imageData[i + 1], imageData[i + 2]);
    if (result < 0)
    {
      return false;
    }
  }

  (void)fclose(filePtr);

  return true;
}

bool sta_read_file(Buffer* buffer, const char* fileName)
{
  FILE* filePtr;
  long  fileSize, count;
  int   error;

  filePtr = fopen(fileName, "r");
  if (!filePtr)
  {
    return false;
  }

  fileSize                 = fseek(filePtr, 0, SEEK_END);
  fileSize                 = ftell(filePtr);

  buffer->index            = 0;
  buffer->len              = fileSize;
  buffer->buffer           = (char*)sta_allocate(fileSize + 1);
  buffer->buffer[fileSize] = '\0';
  fseek(filePtr, 0, SEEK_SET);
  count = fread(buffer->buffer, 1, fileSize, filePtr);
  if (count != fileSize)
  {
    sta_deallocate(buffer->buffer, buffer->len);
    return false;
  }

  error = fclose(filePtr);
  if (error != 0)
  {
    sta_deallocate((void*)buffer->buffer, fileSize + 1);
    return false;
  }

  return true;
}
bool sta_read_file(Arena* arena, Buffer* string, const char* fileName)
{
  FILE* filePtr;
  long  fileSize, count;
  int   error;

  filePtr = fopen(fileName, "r");
  if (!filePtr)
  {
    return false;
  }

  fileSize                 = fseek(filePtr, 0, SEEK_END);
  fileSize                 = ftell(filePtr);

  string->len              = fileSize;
  string->buffer           = sta_arena_push_array(arena, char, fileSize + 1);
  string->buffer[fileSize] = '\0';
  fseek(filePtr, 0, SEEK_SET);
  count = fread(string->buffer, 1, fileSize, filePtr);
  if (count != fileSize)
  {
    free(string->buffer);
    return false;
  }

  error = fclose(filePtr);
  if (error != 0)
  {
    sta_arena_pop(arena, fileSize + 1);
    return false;
  }

  return true;
}
static inline void read_safe(void* target, u64 size, FILE* ptr)
{
  u64 read = fread(target, 1, size, ptr);
  if (read != size)
  {
    printf("Didn't read correctly? :) %ld %ld\n", read, size);
    exit(1);
  }
}

bool sta_targa_read_from_file_rgba(TargaImage* image, const char* filename)
{

  TargaHeader   targa_file_header;
  FILE*         file_ptr;
  unsigned long image_size;
  const int     RGB          = 24;
  const int     RGBA         = 32;
  const int     UNCOMPRESSED = 2;
  const int     RLE          = 10;

  file_ptr                   = fopen(filename, "rb");
  if (file_ptr == NULL)
  {
    printf("ERROR: file doesn't exist %s\n", filename);
    return false;
  }

  read_safe((void*)&targa_file_header, sizeof(TargaHeader), file_ptr);
  image->width  = targa_file_header.width;
  image->height = targa_file_header.height;
  image->bpp    = targa_file_header.imagePixelSize;

  if (targa_file_header.imageType == UNCOMPRESSED)
  {
    image_size  = image->width * image->height * 4;
    image->data = (unsigned char*)sta_allocate(image_size);
    if (image->bpp == 24)
    {
      long size     = image->width * image->height * 3;
      u8*  rgb_data = (u8*)sta_allocate(size);
      read_safe((void*)rgb_data, size, file_ptr);
      for (int i = 0; i < size / 3; i++)
      {
        image->data[i * 4 + 0] = rgb_data[i * 3 + 0];
        image->data[i * 4 + 1] = rgb_data[i * 3 + 1];
        image->data[i * 4 + 2] = rgb_data[i * 3 + 2];
        image->data[i * 4 + 3] = 255;
      }
    }
    else
    {
      read_safe((void*)image->data, image_size, file_ptr);
    }
  }
  else if (targa_file_header.imageType == RLE)
  {
    image_size  = image->width * image->height * 4;
    u64 bpp     = 4;
    image->data = (unsigned char*)sta_allocate(image_size);

    if (targa_file_header.imagePixelSize == RGB)
    {
      u64 imageIndex = 0;
      u8  byte;
      while (imageIndex < image_size)
      {
        read_safe((void*)&byte, sizeof(u8), file_ptr);
        u8* curr = &image->data[imageIndex];
        if (byte >= 128)
        {
          u8 repeated = byte - 127;
          u8 color[3];
          read_safe((void*)&color, ArrayCount(color), file_ptr);

          for (i32 j = 0; j < repeated; j++)
          {
            curr[j * bpp + 0] = color[0];
            curr[j * bpp + 1] = color[1];
            curr[j * bpp + 2] = color[2];
            curr[j * bpp + 3] = 255;
          }
          imageIndex += repeated * 4;
        }
        else
        {
          u8 repeated = byte + 1;
          for (i32 j = 0; j < repeated; j++)
          {
            u8 color[3];
            read_safe((void*)&color, ArrayCount(color), file_ptr);

            curr[j * bpp + 0] = color[0];
            curr[j * bpp + 1] = color[1];
            curr[j * bpp + 2] = color[2];
            curr[j * bpp + 3] = 255;
          }
          imageIndex += repeated * 4;
        }
      }
    }
    else
    {
      u64 imageIndex = 0;
      u8  byte;
      while (imageIndex < image_size)
      {
        read_safe((void*)&byte, sizeof(u8), file_ptr);
        u8* curr = &image->data[imageIndex];
        if (byte >= 128)
        {
          u8 repeated = byte - 127;

          u8 color[4];
          read_safe((void*)&color, ArrayCount(color), file_ptr);

          for (i32 j = 0; j < repeated; j++)
          {
            curr[j * bpp + 0] = color[0];
            curr[j * bpp + 1] = color[1];
            curr[j * bpp + 2] = color[2];
            curr[j * bpp + 3] = color[3];
          }
          imageIndex += repeated * bpp;
        }
        else
        {
          u8 repeated = byte + 1;

          for (i32 j = 0; j < repeated; j++)
          {
            u8 color[4];
            read_safe((void*)&color, ArrayCount(color), file_ptr);

            curr[j * bpp + 0] = color[0];
            curr[j * bpp + 1] = color[1];
            curr[j * bpp + 2] = color[2];
            curr[j * bpp + 3] = color[3];
          }
          imageIndex += repeated * bpp;
        }
      }
    }
  }
  else
  {
    // ToDo fix this xD
    assert(0 && "Can't parse this targa type");
  }

  // reminder that tga stores it as GBRA not RGBA
  for (u64 idx = 0; idx < image->height * image->width * 4; idx += 4)
  {
    unsigned char tmp    = image->data[idx];
    image->data[idx]     = image->data[idx + 2];
    image->data[idx + 2] = tmp;
  }
  image->bpp = RGBA;

  if (fclose(file_ptr) != 0)
  {
    printf("Failed to close file\n");
    return false;
  }

  return true;
}

bool sta_targa_read_from_file(Arena* arena, TargaImage* image, const char* filename)
{

  TargaHeader   targaFileHeader;

  FILE*         filePtr;
  unsigned long count, imageSize;

  filePtr = fopen(filename, "rb");
  if (filePtr == NULL)
  {
    printf("ERROR: file doesn't exist %s\n", filename);
    return false;
  }

  count = fread(&targaFileHeader, sizeof(TargaHeader), 1, filePtr);
  if (count != 1)
  {
    printf("ERROR: Failed to read into header\n");
    return false;
  }

  image->width  = targaFileHeader.width;
  image->height = targaFileHeader.height;
  image->bpp    = targaFileHeader.imagePixelSize;

  if (image->bpp == 32)
  {
    imageSize = image->width * image->height * 4;
  }
  else if (image->bpp == 24)
  {
    imageSize = image->width * image->height * 3;
  }
  else
  {
    assert(false);
  }

  image->data = sta_arena_push_array(arena, unsigned char, imageSize);

  count       = fread(image->data, 1, imageSize, filePtr);
  if (count != imageSize)
  {
    printf("ERROR: count read doesn't equal imageSize\n");
    return false;
  }

  if (fclose(filePtr) != 0)
  {
    printf("Failed to close file\n");
    return false;
  }

  for (u64 idx = 0; idx < image->height * image->width * 4; idx += 4)
  {
    unsigned char tmp    = image->data[idx];
    image->data[idx]     = image->data[idx + 2];
    image->data[idx + 2] = tmp;
  }

  return true;
}

static inline bool char_is_empty(char c)
{
  return !isdigit(c) && !isalpha(c);
}

static inline bool parse_end_of_csv_field(Buffer* s, u64* curr)
{
  bool quoted = false;
  while (*curr < s->len && ((s->buffer[*curr] != ',' && s->buffer[*curr] != '\n') && !quoted))
  {
    if (s->buffer[*curr] == '\"')
    {
      quoted = !quoted;
    }
    (*curr)++;
  }
  return s->buffer[*curr] == '\n' && !quoted;
}

static void free_csv(CSV* csv)
{

  for (u64 i = 0; i < csv->recordCount; i++)
  {
    CSVRecord* record = &csv->records[i];
    free(record->data);
  }
  free(csv->records);
}

bool sta_read_csv_from_string(CSV* csv, Buffer csvData)
{
  csv->recordCap           = 1;
  csv->recordCount         = 1;
  csv->records             = (CSVRecord*)sta_allocate(sizeof(CSVRecord));
  CSVRecord* currentRecord = &csv->records[0];
  currentRecord->dataCount = 0;
  currentRecord->dataCap   = 1;
  currentRecord->data      = (Buffer*)sta_allocate(sizeof(Buffer));
  u64 columnsPerRecord     = -1;
  u64 fileDataIndex        = 0;

  while (fileDataIndex < csvData.len)
  {
    u64  previousDataIndex = fileDataIndex;
    bool foundEnd          = parse_end_of_csv_field(&csvData, &fileDataIndex);
    RESIZE_ARRAY(currentRecord->data, Buffer, currentRecord->dataCount, currentRecord->dataCap);
    currentRecord->data[currentRecord->dataCount].buffer = &csvData.buffer[previousDataIndex];
    currentRecord->data[currentRecord->dataCount].len    = fileDataIndex - previousDataIndex;
    currentRecord->dataCount++;
    if (foundEnd)
    {
      if ((i32)columnsPerRecord == -1)
      {
        columnsPerRecord = currentRecord->dataCount;
      }
      else if (columnsPerRecord != currentRecord->dataCount)
      {
        free_csv(csv);
        return false;
      }
      RESIZE_ARRAY(csv->records, CSVRecord, csv->recordCount, csv->recordCap);
      currentRecord            = &csv->records[csv->recordCount];
      currentRecord->dataCount = 0;
      currentRecord->dataCap   = columnsPerRecord;
      currentRecord->data      = (Buffer*)malloc(sizeof(Buffer) * columnsPerRecord);
      csv->recordCount++;
    }
    fileDataIndex++;
  }

  return true;
}

bool sta_read_csv_from_file(Arena* arena, CSV* csv, Buffer fileLocation)
{
  Buffer fileData;

  bool   result = sta_read_file(arena, &fileData, fileLocation.buffer);
  if (!result)
  {
    return false;
  }

  return sta_read_csv_from_string(csv, fileData);
}
void sta_debug_csv(CSV* csv)
{
  for (u64 i = 0; i < csv->recordCount; i++)
  {
    CSVRecord currentRecord = csv->records[i];
    for (u64 j = 0; j < currentRecord.dataCount; j++)
    {
      printf("%.*s", (i32)currentRecord.data[j].len, currentRecord.data[j].buffer);
      printf("%c", j == currentRecord.dataCount - 1 ? '\n' : ',');
    }
  }
}

bool sta_write_csv_to_file(CSV* csv, Buffer fileLocation)
{
  Buffer tempStr;
  tempStr.buffer = (char*)malloc(sizeof(char) * (fileLocation.len + 1));
  tempStr.len    = fileLocation.len + 1;
  strncpy(tempStr.buffer, fileLocation.buffer, fileLocation.len);
  tempStr.buffer[tempStr.len - 1] = '\0';
  FILE* filePtr                   = fopen(tempStr.buffer, "w");
  if (!filePtr)
  {
    return false;
  }
  for (u64 i = 0; i < csv->recordCount; i++)
  {
    CSVRecord currentRecord = csv->records[i];
    for (u64 j = 0; j < currentRecord.dataCount; j++)
    {
      i32 res = fprintf(filePtr, "%.*s", (i32)currentRecord.data[j].len, currentRecord.data[j].buffer);
      if (res < 0)
      {
        fclose(filePtr);
        return false;
      }
      res = fprintf(filePtr, "%c", j == currentRecord.dataCount - 1 ? '\n' : ',');
      if (res < 0)
      {
        fclose(filePtr);
        return false;
      }
    }
  }

  return fclose(filePtr) == 0;
}

JsonValue* JsonObject::lookup_value(const char* key)
{
  u32 key_length = strlen(key);
  for (u32 i = 0; i < this->size; i++)
  {
    if (key_length == strlen(this->keys[i]) && strncmp(key, this->keys[i], key_length) == 0)
    {
      return &this->values[i];
    }
  }
  return 0;
}
static void serialize_json_value(JsonValue* value, FILE* filePtr);
static void serialize_json_object(JsonObject* object, FILE* filePtr);

static void serialize_json_array(JsonArray* arr, FILE* filePtr)
{
  fwrite("[", 1, 1, filePtr);
  for (u32 i = 0; i < arr->arraySize; i++)
  {
    serialize_json_value(&arr->values[i], filePtr);
    if (i != arr->arraySize - 1)
    {
      fwrite(",", 1, 1, filePtr);
    }
  }
  fwrite("]", 1, 1, filePtr);
}

static void serialize_json_object(JsonObject* object, FILE* filePtr)
{
  fwrite("{", 1, 1, filePtr);
  for (u32 i = 0; i < object->size; i++)
  {
    fprintf(filePtr, "\"%s\":", object->keys[i]);
    serialize_json_value(&object->values[i], filePtr);
    if (i != object->size - 1)
    {
      fwrite(",", 1, 1, filePtr);
    }
  }
  fwrite("}", 1, 1, filePtr);
}

static void serialize_json_value(JsonValue* value, FILE* filePtr)
{
  switch (value->type)
  {
  case JSON_OBJECT:
  {
    serialize_json_object(value->obj, filePtr);
    break;
  }
  case JSON_BOOL:
  {
    if (value->b)
    {
      fwrite("true", 1, 4, filePtr);
    }
    else
    {
      fwrite("false", 1, 5, filePtr);
    }
    break;
  }
  case JSON_NULL:
  {
    fwrite("null", 1, 4, filePtr);
    break;
  }
  case JSON_NUMBER:
  {
    fprintf(filePtr, "%lf", value->number);
    break;
  }
  case JSON_ARRAY:
  {
    serialize_json_array(value->arr, filePtr);
    break;
  }
  case JSON_STRING:
  {
    fprintf(filePtr, "\"%s\"", value->string);
    break;
  }
  default:
  {
    break;
  }
  }
}

bool sta_json_serialize_to_file(Json* json, const char* filename)
{
  FILE* filePtr;

  filePtr = fopen(filename, "w");
  if (!filePtr)
  {
    printf("Failed to open '%s'\n", filename);
    return false;
  }
  switch (json->headType)
  {
  case JSON_OBJECT:
  {
    serialize_json_object(&json->obj, filePtr);
    break;
  }
  case JSON_ARRAY:
  {
    serialize_json_array(&json->array, filePtr);
    break;
  }
  case JSON_VALUE:
  {
    serialize_json_value(&json->value, filePtr);
    break;
  }
  default:
  {
    printf("HOW IS THIS THE HEAD TYPE? %d\n", json->headType);
    break;
  }
  }
  fclose(filePtr);
  return true;
}
void sta_json_debug_object(JsonObject* object);
void sta_json_debug_array(JsonArray* arr);

void sta_json_debug_value(JsonValue* value)
{
  switch (value->type)
  {
  case JSON_OBJECT:
  {
    sta_json_debug_object(value->obj);
    break;
  }
  case JSON_BOOL:
  {
    if (value->b)
    {
      printf("true");
    }
    else
    {
      printf("false");
    }
    break;
  }
  case JSON_NULL:
  {
    printf("null");
    break;
  }
  case JSON_NUMBER:
  {
    printf("%lf", value->number);
    break;
  }
  case JSON_ARRAY:
  {
    sta_json_debug_array(value->arr);
    break;
  }
  case JSON_STRING:
  {
    printf("\"%s\"", value->string);
    break;
  }
  default:
  {
    break;
  }
  }
}

void sta_json_debug_object(JsonObject* object)
{
  printf("{\n");
  for (u32 i = 0; i < object->size; i++)
  {
    printf("\"%s\":", object->keys[i]);
    sta_json_debug_value(&object->values[i]);
    if (i != object->size - 1)
    {
      printf(",\n");
    }
  }
  printf("\n}");
}

void sta_json_debug_array(JsonArray* arr)
{
  printf("[");
  for (u32 i = 0; i < arr->arraySize; i++)
  {
    sta_json_debug_value(&arr->values[i]);
    if (i != arr->arraySize - 1)
    {
      printf(", ");
    }
  }
  printf("]");
}

static inline void json_resize_object(JsonObject* obj)
{
  if (obj->cap == 0)
  {
    obj->cap    = 4;
    obj->values = (JsonValue*)malloc(sizeof(JsonValue) * obj->cap);
    obj->keys   = (char**)malloc(sizeof(char*) * obj->cap);
  }
  else if (obj->size >= obj->cap)
  {
    obj->cap *= 2;
    obj->values = (JsonValue*)realloc(obj->values, obj->cap * sizeof(JsonValue));
    obj->keys   = (char**)realloc(obj->keys, obj->cap * sizeof(char*));
  }
}

static bool json_parse_string(char** key, Buffer* buffer)
{
  ADVANCE(buffer);
  u64 start = buffer->index;
  SKIP(buffer, !buffer->match('"'));
  u64 len     = (buffer->index - start);

  *key        = (char*)malloc(sizeof(char) * (1 + len));
  (*key)[len] = '\0';
  strncpy(*key, &buffer->buffer[start], len);
  ADVANCE(buffer);
  return true;
}

static bool json_parse_value(JsonValue* value, Buffer* buffer);
static bool json_parse_array(JsonArray* arr, Buffer* buffer);

static bool json_parse_key_value_pair(JsonObject* obj, Buffer* buffer)
{
  json_resize_object(obj);

  json_parse_string(&obj->keys[obj->size], buffer);
  skip_whitespace(buffer);

  if (!consume(buffer, ':'))
  {
    return false;
  }

  bool res = json_parse_value(&obj->values[obj->size], buffer);
  if (!res)
  {
    return false;
  }
  obj->size++;

  skip_whitespace(buffer);
  return true;
}

static bool json_parse_object(JsonObject* obj, Buffer* buffer)
{
  buffer->advance();
  skip_whitespace(buffer);
  while (!buffer->match('}'))
  {
    bool res = json_parse_key_value_pair(obj, buffer);
    if (!res)
    {
      return false;
    }

    skip_whitespace(buffer);
    if (match(buffer, ','))
    {
      // It's illegal to have a ',' and then end it
      buffer->advance();
    }
    skip_whitespace(buffer);
  }
  buffer->advance();
  return true;
}

static inline void json_resize_array(JsonArray* arr)
{
  if (arr->arraySize == 0)
  {
    arr->arrayCap = 4;
    arr->values   = (JsonValue*)malloc(sizeof(JsonValue) * arr->arrayCap);
  }
  else if (arr->arraySize >= arr->arrayCap)
  {
    arr->arrayCap *= 2;
    arr->values = (JsonValue*)realloc(arr->values, arr->arrayCap * sizeof(JsonValue));
  }
}
static bool json_parse_array(JsonArray* arr, Buffer* buffer)
{
  ADVANCE(buffer);
  skip_whitespace(buffer);
  bool res;
  while (!buffer->match(']'))
  {
    json_resize_array(arr);
    res = json_parse_value(&arr->values[arr->arraySize], buffer);
    if (!res)
    {
      return false;
    }
    arr->arraySize++;
    skip_whitespace(buffer);
    (void)consume(buffer, ',');
  }

  ADVANCE(buffer);

  return true;
}
static bool json_parse_number(f32* number, Buffer* buffer)
{
  *number = parse_float_from_string(buffer);
  return true;
}

static bool json_parse_value(JsonValue* value, Buffer* buffer)
{
  skip_whitespace(buffer);
  if (isdigit(buffer->current_char()) || buffer->match('-'))
  {
    value->type = JSON_NUMBER;
    return json_parse_number(&value->number, buffer);
  }
  switch (buffer->current_char())
  {
  case '\"':
  {
    value->type = JSON_STRING;
    return json_parse_string(&value->string, buffer);
  }
  case '{':
  {
    value->type      = JSON_OBJECT;
    value->obj       = (JsonObject*)malloc(sizeof(JsonObject));
    value->obj->cap  = 0;
    value->obj->size = 0;
    return json_parse_object(value->obj, buffer);
  }
  case '[':
  {
    value->type           = JSON_ARRAY;
    value->arr            = (JsonArray*)malloc(sizeof(JsonArray));
    value->arr->arrayCap  = 0;
    value->arr->arraySize = 0;
    return json_parse_array(value->arr, buffer);
  }
  case 't':
  {
    if (strncmp(&buffer->buffer[buffer->index + 1], "rue", 3) == 0)
    {
      value->type = JSON_BOOL;
      value->b    = true;
      buffer->index += 4;
      return true;
    }
    printf("Got 't' but wasn't true?\n");
    return false;
  }
  case 'f':
  {
    if (strncmp(&buffer->buffer[buffer->index + 1], "alse", 4))
    {
      value->type = JSON_BOOL;
      value->b    = false;
      buffer->index += 5;
      return true;
    }
    printf("Got 'f' but wasn't false?\n");
    return false;
  }
  case 'n':
  {
    if (strncmp(&NEXT_CHAR(buffer), "ull", 3))
    {
      value->type = JSON_NULL;
      buffer->index += 4;
      return true;
    }
    printf("Got 'n' but wasn't null?\n");
    return false;
  }
  default:
  {
    printf("Unknown value token '%c' %d\n", buffer->current_char(), buffer->current_char());
    return false;
  }
  }
}

bool sta_json_deserialize_from_string(Buffer* buffer, Json* json)
{
  bool res;
  buffer->skip_whitespace();
  switch (buffer->current_char())
  {
  case '{':
  {
    json->headType = JSON_OBJECT;
    json->obj.cap  = 0;
    json->obj.size = 0;
    res            = json_parse_object(&json->obj, buffer);
    break;
  }
  case '[':
  {
    json->headType        = JSON_ARRAY;
    json->array.arrayCap  = 0;
    json->array.arraySize = 0;
    res                   = json_parse_array(&json->array, buffer);
    break;
  }
  default:
  {
    json->headType = JSON_VALUE;
    res            = json_parse_value(&json->value, buffer);
    break;
  }
  }

  if (!res)
  {
    return false;
  }
  if (buffer->index != buffer->len)
  {
    // printf("Didn't reach eof after parsing first value? %ld %ld %.*s\n", buffer->index, buffer->len, (i32)(buffer->len - buffer->index), buffer->current_address());
    // return true;
  }
  return true;
}

bool sta_json_deserialize_from_file(Arena* arena, Json* json, const char* filename)
{
  Buffer fileContent;
  bool   result;

  result = sta_read_file(arena, &fileContent, filename);
  if (!result)
  {
    return false;
  }
  return sta_json_deserialize_from_string(&fileContent, json);
}

void sta_json_debug(Json* json)
{
  switch (json->headType)
  {
  case JSON_ARRAY:
  {
    sta_json_debug_array(&json->array);
    break;
  }
  case JSON_VALUE:
  {
    sta_json_debug_value(&json->value);
    break;
  }
  case JSON_OBJECT:
  {
    sta_json_debug_object(&json->obj);
    break;
  }
  default:
  {
    assert(0 && "Invalid json head?");
  }
  }
}

static void skip_until_digit(Buffer* buffer)
{
  SKIP(buffer, buffer->index < buffer->len && !(isdigit(buffer->current_char()) || (buffer->match('-'))));
}

static void parse_wavefront_texture(WavefrontObject* obj, Buffer* buffer)
{

  obj->texture_coordinate_count++;
  RESIZE_ARRAY(obj->texture_coordinates, Vector3, obj->texture_coordinate_count, obj->texture_coordinate_capacity);

  for (i32 i = 0; i < 2; i++)
  {
    skip_until_digit(buffer);
    obj->texture_coordinates[obj->texture_coordinate_count - 1].v[i] = parse_float_from_string(buffer);
  }

  // Parse optional w
  skip_until_digit(buffer);
  if (buffer->index < buffer->len)
  {
    obj->texture_coordinates[obj->texture_coordinate_count - 1].z = parse_float_from_string(buffer);
  }
  else
  {
    obj->texture_coordinates[obj->texture_coordinate_count - 1].z = 0;
  }
}
static void parseWavefrontNormal(WavefrontObject* obj, Buffer* buffer)
{

  obj->normal_count++;
  RESIZE_ARRAY(obj->normals, Vector3, obj->normal_count, obj->normal_capacity);

  // Parse x,y,z
  for (i32 i = 0; i < 3; i++)
  {
    skip_until_digit(buffer);
    obj->normals[obj->normal_count - 1].v[i] = parse_float_from_string(buffer);
  }
}
static void parseWavefrontVertex(WavefrontObject* obj, Buffer* buffer)
{
  obj->vertex_count++;
  RESIZE_ARRAY(obj->vertices, Vector4, obj->vertex_count, obj->vertex_capacity);

  // Parse x,y,z
  for (i32 i = 0; i < 3; i++)
  {
    skip_until_digit(buffer);
    obj->vertices[obj->vertex_count - 1].v[i] = parse_float_from_string(buffer);
  }

  // Parse optional w
  skip_until_digit(buffer);
  if (buffer->index < buffer->len)
  {
    obj->vertices[obj->vertex_count - 1].w = parse_float_from_string(buffer);
  }
}
static void parseWavefrontFace(WavefrontObject* obj, Buffer* buffer)
{
  obj->face_count++;
  RESIZE_ARRAY(obj->faces, WavefrontFace, obj->face_count, obj->face_capacity);

  WavefrontFace* face      = &obj->faces[obj->face_count - 1];
  u32            vertexCap = 8;
  face->count              = 0;
  face->vertices           = (WavefrontVertexData*)sta_allocate(sizeof(WavefrontVertexData) * vertexCap);
  ADVANCE(buffer);
  skip_until_digit(buffer);
  while (buffer->index < buffer->len)
  {
    face->count++;
    RESIZE_ARRAY(face->vertices, WavefrontVertexData, face->count, vertexCap);

    face->vertices[face->count - 1].vertex_idx = parse_int_from_string(buffer);

    skip_until_digit(buffer);
    face->vertices[face->count - 1].texture_idx = parse_int_from_string(buffer);

    skip_until_digit(buffer);
    face->vertices[face->count - 1].normal_idx = parse_int_from_string(buffer);
    skip_until_digit(buffer);
  }
}
static inline void parseWavefrontLine(WavefrontObject* obj, Buffer* buffer)
{
  if (CURRENT_CHAR(buffer) == 'v' && NEXT_CHAR(buffer) == 't')
  {
    parse_wavefront_texture(obj, buffer);
  }
  else if (CURRENT_CHAR(buffer) == 'v' && NEXT_CHAR(buffer) == 'n')
  {
    parseWavefrontNormal(obj, buffer);
  }
  else if (CURRENT_CHAR(buffer) == 'v')
  {
    parseWavefrontVertex(obj, buffer);
  }
  else if (CURRENT_CHAR(buffer) == 'f')
  {
    parseWavefrontFace(obj, buffer);
  }
}

bool sta_parse_wavefront_object_from_file(ModelData* model, const char* filename)
{
  WavefrontObject obj             = {};
  u64             init_cap        = 8;
  obj.vertex_count                = 0;
  obj.vertices                    = (Vector4*)sta_allocate(sizeof(struct Vector4) * init_cap);
  obj.vertex_capacity             = init_cap;

  obj.normal_count                = 0;
  obj.normals                     = (Vector3*)sta_allocate(sizeof(struct Vector3) * init_cap);
  obj.normal_capacity             = init_cap;

  obj.texture_coordinate_count    = 0;
  obj.texture_coordinates         = (Vector3*)sta_allocate(sizeof(Vector3) * init_cap);
  obj.texture_coordinate_capacity = init_cap;

  obj.face_count                  = 0;
  obj.face_capacity               = init_cap;
  obj.faces                       = (WavefrontFace*)sta_allocate(sizeof(WavefrontFace) * init_cap);

  FILE* filePtr;
  char* line = NULL;
  u64   len  = 0;
  i64   read;

  filePtr = fopen(filename, "r");
  if (filePtr == NULL)
  {
    return false;
  }

  Buffer buffer = {};
  while ((read = getline(&line, &len, filePtr)) != -1)
  {
    buffer.len           = strlen(line);
    line[buffer.len - 1] = '\0';
    buffer.buffer        = line;
    buffer.index         = 0;
    parseWavefrontLine(&obj, &buffer);
  }

  model->vertex_count = obj.face_count * obj.faces[0].count;
  model->indices      = (u32*)sta_allocate(sizeof(u32) * model->vertex_count);
  model->vertices     = (VertexData*)sta_allocate(sizeof(VertexData) * model->vertex_count);

  float low = FLT_MAX, high = -FLT_MAX;
  for (u64 i = 0; i < obj.face_count; i++)
  {
    WavefrontFace* face = &obj.faces[i];
    for (u64 j = 0; j < face->count; j++)
    {
      u64 index                = i * face->count + j;
      model->indices[index]    = index;
      WavefrontVertexData data = face->vertices[j];

      Vector4             v    = obj.vertices[data.vertex_idx - 1];
      if (low > v.x)
      {
        low = v.x;
      }
      if (low > v.y)
      {
        low = v.y;
      }
      if (low > v.z)
      {
        low = v.z;
      }
      if (high < v.x)
      {
        high = v.x;
      }
      if (high < v.y)
      {
        high = v.y;
      }
      if (high < v.z)
      {
        high = v.z;
      }

      model->vertices[index].vertex = cast_vec4_to_vec3(obj.vertices[data.vertex_idx - 1]);
      model->vertices[index].uv     = cast_vec3_to_vec2(obj.texture_coordinates[data.texture_idx - 1]);
      model->vertices[index].uv.y   = -model->vertices[index].uv.y;

      model->vertices[index].normal = obj.normals[data.normal_idx - 1];
    }
    // printf("\n");
    sta_deallocate(obj.faces[i].vertices, sizeof(WavefrontVertexData) * obj.faces[i].count);
  }

  sta_deallocate(obj.texture_coordinates, sizeof(Vector2) * obj.texture_coordinate_capacity);
  sta_deallocate(obj.vertices, sizeof(Vector4) * obj.vertex_capacity);
  sta_deallocate(obj.faces, sizeof(WavefrontFace) * obj.face_capacity);
  sta_deallocate(obj.normals, sizeof(Vector3) * obj.normal_capacity);

  float diff = high - low;
  for (unsigned int i = 0; i < model->vertex_count; i++)
  {
    VertexData* vertex = &model->vertices[i];
    Vector3*    v      = &vertex->vertex;
    if (low < 0)
    {
      v->x = ((v->x - low) / diff) * 2.0f - 1.0f;
      v->y = ((v->y - low) / diff) * 2.0f - 1.0f;
      v->z = ((v->z - low) / diff) * 2.0f - 1.0f;
    }
    else
    {
      v->x = ((v->x + low) / diff) * 2.0f - 1.0f;
      v->y = ((v->y + low) / diff) * 2.0f - 1.0f;
      v->z = ((v->z + low) / diff) * 2.0f - 1.0f;
    }
  }

  return true;
}

bool convert_obj_to_model(const char* input_filename, const char* output_filename)
{
  ModelData model = {};
  sta_parse_wavefront_object_from_file(&model, input_filename);
  FILE* file_ptr = fopen(output_filename, "w");
  if (file_ptr == 0)
  {
    return false;
  }

  fprintf(file_ptr, "%ld\n", model.vertex_count);
  for (u64 i = 0; i < model.vertex_count; i++)
  {
    VertexData vertex_data = model.vertices[i];
    Vector3    vertex      = vertex_data.vertex;
    fprintf(file_ptr, "%lf %lf %lf ", vertex.x, vertex.y, vertex.z);

    Vector2 uv = vertex_data.uv;
    fprintf(file_ptr, "%lf %lf ", uv.x, uv.y);

    Vector3 normal = vertex_data.normal;
    fprintf(file_ptr, "%lf %lf %lf", normal.x, normal.y, normal.z);
    if (i < model.vertex_count - 1)
    {
      fprintf(file_ptr, "\n");
    }
  }
  fclose(file_ptr);

  return true;
}

void free_xml_node(XML_Node* node)
{
  if (node->type == XML_PARENT)
  {
    XML_Node* child = node->child;
    while (child != 0)
    {
      XML_Node* next = child->next;
      free_xml_node(child);
      child = next;
    }
  }

  sta_deallocate(node->header.tags, sizeof(XML_Tag) * node->header.tag_capacity);
  sta_deallocate(node, sizeof(XML_Node));
}

bool remove_xml_key(XML_Node* xml, const char* node_name, u64 node_name_length)
{

  XML_Node* prev = xml->child;
  XML_Node* curr = xml->child;
  while (curr != 0)
  {
    if (curr->header.name_length == node_name_length && strncmp(curr->header.name, node_name, node_name_length) == 0)
    {
      if (curr == xml->child)
      {
        xml->child = curr->next;
        if (xml->child == 0)
        {
          xml->type = XML_CLOSED;
        }
      }
      else
      {

        prev->next = curr->next;
      }
      free_xml_node(curr);
      return true;
    }
    prev = curr;
    curr = curr->next;
  }
  return false;
}

XML_Node* XML_Node::find_key_with_tag(char* key_name, u32 key_name_length, char* tag_name, u32 tag_name_length, char* tag_value, u32 tag_value_length)
{
  XML_Node* curr = this->child;
  while (curr != 0)
  {
    if (curr->header.name_length == key_name_length && strncmp(curr->header.name, key_name, key_name_length) == 0)
    {
      XML_Tag* tag = curr->find_tag(tag_name);
      if (tag->value_length == tag_value_length && strncmp(tag->value, tag_value, tag->value_length) == 0)
      {
        return curr;
      }
    }
    curr = curr->next;
  }
  return 0;
}
XML_Node* XML_Node::find_key_with_tag(const char* key_name, const char* tag_name, String tag_value)
{
  u64       node_name_length = strlen(key_name);
  XML_Node* curr             = this->child;
  while (curr != 0)
  {
    if (curr->header.name_length == node_name_length && strncmp(curr->header.name, key_name, node_name_length) == 0)
    {
      XML_Tag* tag = curr->find_tag(tag_name);
      if (tag->value_length == tag_value.length && strncmp(tag->value, tag_value.buffer, tag->value_length) == 0)
      {
        return curr;
      }
    }
    curr = curr->next;
  }
  assert(0 && "Couldn't find key with tag");
}
XML_Node* XML_Node::find_key_with_tag(const char* key_name, const char* tag_name, const char* tag_value)
{
  u64       node_name_length = strlen(key_name);
  XML_Node* curr             = this->child;
  while (curr != 0)
  {
    if (curr->header.name_length == node_name_length && strncmp(curr->header.name, key_name, node_name_length) == 0)
    {
      XML_Tag* tag = curr->find_tag(tag_name);
      if (tag->value_length == strlen(tag_value) && strncmp(tag->value, tag_value, tag->value_length) == 0)
      {
        return curr;
      }
    }
    curr = curr->next;
  }
  return 0;
}

XML_Node* XML_Node::find_key(char* key_name, u32 key_name_length)
{
  XML_Node* curr = this->child;
  while (curr != 0)
  {
    if (curr->header.name_length == key_name_length && strncmp(curr->header.name, key_name, key_name_length) == 0)
    {
      return curr;
    }
    curr = curr->next;
  }
  assert(0 && "Couldn't find" && key_name);
}
XML_Node* XML_Node::find_key(const char* key_name)
{
  return this->find_key((char*)key_name, strlen(key_name));
}
XML_Tag* XML_Node::find_tag(char* tag_name, u32 tag_name_length)
{
  XML_Tag* tags = this->header.tags;
  for (unsigned int i = 0; i < this->header.tag_count; i++)
  {
    XML_Tag* tag = &tags[i];
    if (tag->name_length == tag_name_length && strncmp(tag_name, tag->name, tag_name_length) == 0)
    {
      return tag;
    }
  }
  assert(0 && "Couldn't find" && tag_name);
}

XML_Tag* XML_Node::find_tag(const char* tag_name)
{
  return this->find_tag((char*)tag_name, strlen(tag_name));
}

u32 sta_xml_get_child_count_by_name(XML_Node* node, const char* name)
{
  u32       count       = 0;
  XML_Node* child       = node->child;
  u32       name_length = strlen(name);
  while (child)
  {
    if (child->header.name_length == name_length && strncmp(name, child->header.name, name_length) == 0)
    {
      count++;
    }
    child = child->next;
  }

  return count;
}

static void write_xml_node(XML_Node* xml, FILE* ptr, int tabs)
{

  if (xml == 0)
  {
    return;
  }
  XML_Header* header = &xml->header;
  char        t[tabs];
  memset(t, '\t', tabs);
  fprintf(ptr, "%.*s<%.*s", tabs, t, (i32)header->name_length, header->name);
  for (unsigned int i = 0; i < header->tag_count; i++)
  {
    XML_Tag* tag = &header->tags[i];
    fprintf(ptr, " %.*s=\"%.*s\"", (i32)tag->name_length, tag->name, (i32)tag->value_length, tag->value);
  }
  if (xml->type == XML_CLOSED)
  {
    fprintf(ptr, "/>\n");
  }
  else if (xml->type == XML_PARENT)
  {
    fprintf(ptr, ">\n");

    XML_Node* child = xml->child;
    write_xml_node(child, ptr, tabs + 1);

    char t[tabs];
    memset(t, '\t', tabs);
    fprintf(ptr, "%.*s</%.*s>\n", tabs, t, (i32)header->name_length, header->name);
  }
  else if (xml->content.length != 0)
  {
    fprintf(ptr, ">%.*s</%.*s>\n", (i32)xml->content.length, xml->content.buffer, (i32)header->name_length, header->name);
  }
  else
  {
    char t[tabs];
    memset(t, '\t', tabs);
    fprintf(ptr, ">%.*s</%.*s>\n", tabs, t, (i32)header->name_length, header->name);
  }
  write_xml_node(xml->next, ptr, tabs);
}
static void write_xml(XML* xml, FILE* ptr)
{
  fprintf(ptr, "<?");
  for (unsigned int i = 0; i < xml->version_and_encoding_length; i++)
  {

    XML_Tag* tag = &xml->version_and_encoding[i];
    fprintf(ptr, " %.*s=\"%.*s\"", (i32)tag->name_length, tag->name, (i32)tag->value_length, tag->value);
  }
  fprintf(ptr, "?>\n");
  write_xml_node(&xml->head, ptr, 0);
}

void write_xml_to_file(XML* xml, const char* filename)
{
  FILE* file_ptr = fopen(filename, "w");
  write_xml(xml, file_ptr);
}

static bool compare_string(XML_Header* xml, Buffer* buffer, u64 index)
{
  u64 len = buffer->index - index;
  if (len != xml->name_length)
  {
    printf("Not the same length!\n");
    return false;
  }
  return strncmp(xml->name, &buffer->buffer[index], len) == 0;
}

enum XML_Header_Result
{
  XML_HEADER_OKAY,
  XML_HEADER_ERROR,
  XML_HEADER_CLOSED
};

bool parse_tag(Buffer* buffer, XML_Tag* current_tag)
{

  // parse name
  u64 index         = buffer->index;
  current_tag->name = &buffer->buffer[index];

  // very robust yes
  SKIP(buffer, !match(buffer, '='));
  current_tag->name_length = buffer->index - index;

  buffer->advance();
  if (!match(buffer, '\"'))
  {
    printf("Expected '\"' for tag content?\n");
    return false;
  }
  buffer->advance();
  current_tag->value = &buffer->buffer[buffer->index];
  index              = buffer->index;

  SKIP(buffer, buffer->current_char() != '\"');

  current_tag->value_length = buffer->index - index;
  buffer->advance();
  return true;
}
XML_Header_Result parse_header(XML_Node* xml, Buffer* buffer)
{
  const int initial_tag_capacity = 8;
  // <NAME [TAG_NAME="TAG_CONTENT"]>
  XML_Header* header   = &xml->header;
  header->name         = &buffer->buffer[buffer->index];
  header->tag_capacity = initial_tag_capacity;
  header->tags         = (XML_Tag*)sta_allocate(sizeof(XML_Tag) * initial_tag_capacity);
  u64 index            = buffer->index;
  SKIP(buffer, buffer->current_char() != ' ' && buffer->current_char() != '>' && buffer->current_char() != '/');
  header->name_length = buffer->index - index;

  // parse tags
  for (;;)
  {
    skip_whitespace(buffer);
    if (isalpha(buffer->current_char()))
    {
      RESIZE_ARRAY(header->tags, XML_Tag, header->tag_count, header->tag_capacity);
      if (!parse_tag(buffer, &header->tags[header->tag_count++]))
      {
        return XML_HEADER_ERROR;
      }
    }
    else if (match(buffer, '/') && NEXT_CHAR(buffer) == '>')
    {
      buffer->advance();
      buffer->advance();
      return XML_HEADER_CLOSED;
    }
    else if (match(buffer, '>'))
    {
      buffer->advance();
      return XML_HEADER_OKAY;
    }
    else
    {
      printf("Idk should close or alpha? %c\n", buffer->current_char());
      return XML_HEADER_ERROR;
    }
  }
}

bool close_xml(XML_Node* xml, Buffer* buffer)
{

  buffer->advance();
  u64 index = buffer->index;
  SKIP(buffer, buffer->current_char() != '>');

  if (!compare_string(&xml->header, buffer, index))
  {
    printf("Closed the wrong thing?\n");
    return false;
  }
  buffer->advance();
  return true;
}

bool parse_content(XML_Node* xml, Buffer* buffer, u64 index)
{
  xml->type           = XML_CONTENT;
  xml->content.buffer = &buffer->buffer[buffer->index];

  SKIP(buffer, buffer->current_char() != '<');

  xml->content.length = buffer->index - index;
  buffer->advance();
  if (!match(buffer, '/'))
  {
    printf("Should be closing?\n");
    return false;
  }
  return close_xml(xml, buffer);
}

bool sta_xml_parse_from_buffer(XML_Node* xml, Buffer* buffer)
{

  skip_whitespace(buffer);
  if (!match(buffer, '<'))
  {
    printf("Unknown char? %c %ld\n", buffer->buffer[buffer->index], buffer->index);
    return false;
  }
  buffer->advance();
  XML_Header_Result result = parse_header(xml, buffer);
  if (result == XML_HEADER_ERROR)
  {
    return false;
  }

  if (result == XML_HEADER_CLOSED)
  {
    xml->type = XML_CLOSED;
    return true;
  }

  skip_whitespace(buffer);
  u64 index = buffer->index;
  if (match(buffer, '<'))
  {
    // check empty
    if (NEXT_CHAR(buffer) == '/')
    {
      // advance past '<'
      buffer->advance();
      return close_xml(xml, buffer);
    }
    xml->type          = XML_PARENT;
    xml->child         = (XML_Node*)sta_allocate_struct(XML_Node, 1);
    xml->child->parent = xml;
    if (!sta_xml_parse_from_buffer(xml->child, buffer))
    {
      return false;
    }

    XML_Node* next = xml->child;
    for (;;)
    {
      skip_whitespace(buffer);
      if (!match(buffer, '<'))
      {
        return true;
      }
      if (NEXT_CHAR(buffer) == '/')
      {
        buffer->advance();
        return close_xml(xml, buffer);
      }
      next->next         = (XML_Node*)sta_allocate_struct(XML_Node, 1);
      next->next->parent = xml;
      next               = next->next;
      if (!sta_xml_parse_from_buffer(next, buffer))
      {
        return false;
      }
    }
  }
  return parse_content(xml, buffer, index);
}
bool sta_xml_parse_version_and_encoding(XML* xml, Buffer* buffer)
{
  skip_whitespace(buffer);
  // parse header?
  if (!match(buffer, '<'))
  {
    printf("Unknown char? %c %ld\n", buffer->buffer[buffer->index], buffer->index);
    return false;
  }
  buffer->advance();
  if (!match(buffer, '?'))
  {
    printf("Tried to parse xml version thingy but didn't find it?\n");
    return false;
  }
  buffer->advance();
  if (match(buffer, '?') && NEXT_CHAR(buffer) == '>')
  {
    buffer->advance();
    buffer->advance();
    return true;
  }

  const int initial_tag_capacity     = 2;
  xml->version_and_encoding_capacity = initial_tag_capacity;
  xml->version_and_encoding          = (XML_Tag*)sta_allocate_struct(XML_Tag, initial_tag_capacity);
  do
  {
    skip_whitespace(buffer);
    RESIZE_ARRAY(xml->version_and_encoding, XML_Tag, xml->version_and_encoding_length, xml->version_and_encoding_capacity);
    if (!parse_tag(buffer, &xml->version_and_encoding[xml->version_and_encoding_length++]))
    {
      return false;
    }
  } while (!match(buffer, '?'));
  buffer->advance();
  if (!match(buffer, '>'))
  {
    return false;
  }
  buffer->advance();

  return true;
}

void split_buffer_by_newline(StringArray* array, Buffer* buffer)
{
  array->count    = 0;
  array->capacity = 2;
  array->strings  = sta_allocate_struct(char*, array->capacity);
  while (!buffer->is_out_of_bounds())
  {

    u64 prev = buffer->index;
    SKIP(buffer, buffer->current_char() != '\n' || buffer->current_char() == '\0');

    if (buffer->current_char() == '\0')
    {
      break;
    }

    RESIZE_ARRAY(array->strings, char*, array->count, array->capacity);
    array->strings[array->count] = (char*)sta_allocate_struct(char, buffer->index - prev + 1);
    strncpy(array->strings[array->count], &buffer->buffer[prev], buffer->index - prev);
    array->strings[array->count][buffer->index - prev] = '\0';
    array->count++;
    buffer->advance();
  }
}

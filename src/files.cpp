#include "files.h"
#include "common.h"
#include "platform.h"
#include <assert.h>
#include <cfloat>
#include <cmath>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

int parse_int_from_string(Buffer* buffer)
{
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

void skip_whitespace(Buffer* buffer)
{
  SKIP(buffer, match(buffer, ' ') || match(buffer, '\n') || match(buffer, '\t'));
}


void sta_save_targa(TargaImage* image, const char* filename)
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

  buffer->len              = fileSize;
  buffer->buffer           = (char*)allocate(fileSize + 1);
  buffer->buffer[fileSize] = '\0';
  fseek(filePtr, 0, SEEK_SET);
  count = fread(buffer->buffer, 1, fileSize, filePtr);
  if (count != fileSize)
  {
    free(buffer->buffer);
    return false;
  }

  error = fclose(filePtr);
  if (error != 0)
  {
    deallocate((void*)buffer->buffer, fileSize + 1);
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
void read_safe(void* target, u64 size, FILE* ptr, const char* caller)
{
  u64 read = fread(target, 1, size, ptr);
  if (read != size)
  {
    printf("Didn't read correctly? :) %ld %ld %s\n", read, size, caller);
    exit(1);
  }
}

void read_safe(void* target, u64 size, FILE* ptr)
{
  u64 read = fread(target, 1, size, ptr);
  if (read != size)
  {
    printf("Didn't read correctly? :) %ld %ld\n", read, size);
    exit(1);
  }
}

bool sta_read_targa_from_file(TargaImage* image, const char* filename)
{

  TargaHeader   targaFileHeader;

  FILE*         filePtr;
  unsigned long imageSize;

  filePtr = fopen(filename, "rb");
  if (filePtr == NULL)
  {
    printf("ERROR: file doesn't exist %s\n", filename);
    return false;
  }

  read_safe((void*)&targaFileHeader, sizeof(TargaHeader), filePtr);

  image->width  = targaFileHeader.width;
  image->height = targaFileHeader.height;
  image->bpp    = targaFileHeader.imagePixelSize;

  if (targaFileHeader.imageType == 2)
  {
    imageSize   = image->width * image->height * 4;
    image->data = (unsigned char*)allocate(imageSize);
    if (image->bpp == 24)
    {
      long size     = image->width * image->height * 3;
      u8*  rgb_data = (u8*)allocate(size);
      read_safe((void*)rgb_data, size, filePtr);
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
      read_safe((void*)image->data, imageSize, filePtr);
    }
  }
  else if (targaFileHeader.imageType == 10)
  {
    imageSize   = image->width * image->height * 4;
    u64 bpp     = 4;
    image->data = (unsigned char*)allocate(imageSize);

    if (targaFileHeader.imagePixelSize == 24)
    {
      u64 imageIndex = 0;
      u8  byte;
      while (imageIndex < imageSize)
      {
        read_safe((void*)&byte, sizeof(u8), filePtr);
        u8* curr = &image->data[imageIndex];
        if (byte >= 128)
        {
          u8 repeated = byte - 127;
          u8 color[3];
          read_safe((void*)&color, ArrayCount(color), filePtr);

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
            read_safe((void*)&color, ArrayCount(color), filePtr);

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
      while (imageIndex < imageSize)
      {
        read_safe((void*)&byte, sizeof(u8), filePtr);
        u8* curr = &image->data[imageIndex];
        if (byte >= 128)
        {
          u8 repeated = byte - 127;

          u8 color[4];
          read_safe((void*)&color, ArrayCount(color), filePtr);

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
            read_safe((void*)&color, ArrayCount(color), filePtr);

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
    assert(0 && "Can't parse this targa type");
  }

  for (u64 idx = 0; idx < image->height * image->width * 4; idx += 4)
  {
    unsigned char tmp    = image->data[idx];
    image->data[idx]     = image->data[idx + 2];
    image->data[idx + 2] = tmp;
  }
  image->bpp = 32;

  if (fclose(filePtr) != 0)
  {
    printf("Failed to close file\n");
    return false;
  }

  return true;
}

bool sta_read_targa_from_file(Arena* arena, TargaImage* image, const char* filename)
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
  csv->records             = (CSVRecord*)allocate(sizeof(CSVRecord));
  CSVRecord* currentRecord = &csv->records[0];
  currentRecord->dataCount = 0;
  currentRecord->dataCap   = 1;
  currentRecord->data      = (Buffer*)allocate(sizeof(Buffer));
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

bool sta_serialize_json_to_file(Json* json, const char* filename)
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
static void debugJsonObject(JsonObject* object);
static void debugJsonArray(JsonArray* arr);

static void debugJsonValue(JsonValue* value)
{
  switch (value->type)
  {
  case JSON_OBJECT:
  {
    debugJsonObject(value->obj);
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
    debugJsonArray(value->arr);
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

static void debugJsonObject(JsonObject* object)
{
  printf("{\n");
  for (u32 i = 0; i < object->size; i++)
  {
    printf("\"%s\":", object->keys[i]);
    debugJsonValue(&object->values[i]);
    if (i != object->size - 1)
    {
      printf(",\n");
    }
  }
  printf("\n}n");
}

static void debugJsonArray(JsonArray* arr)
{
  printf("[");
  for (u32 i = 0; i < arr->arraySize; i++)
  {
    debugJsonValue(&arr->values[i]);
    if (i != arr->arraySize - 1)
    {
      printf(", ");
    }
  }
  printf("]");
}

static inline void resizeObject(JsonObject* obj)
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
  SKIP(buffer, CURRENT_CHAR(buffer) != '"');
  u64 len     = (buffer->index - start);

  *key        = (char*)malloc(sizeof(char) * (1 + len));
  (*key)[len] = '\0';
  strncpy(*key, &buffer->buffer[start], len);
  ADVANCE(buffer);
  return true;
}

static bool parseJsonValue(JsonValue* value, Buffer* buffer);
static bool parseJsonArray(JsonArray* arr, Buffer* buffer);

static bool parseKeyValuePair(JsonObject* obj, Buffer* buffer)
{
  resizeObject(obj);

  json_parse_string(&obj->keys[obj->size], buffer);
  skip_whitespace(buffer);

  if (!consume(buffer, ':'))
  {
    return false;
  }

  bool res = parseJsonValue(&obj->values[obj->size], buffer);
  if (!res)
  {
    return false;
  }
  obj->size++;

  skip_whitespace(buffer);
  return true;
}

static bool parseJsonObject(JsonObject* obj, Buffer* buffer)
{
  ADVANCE(buffer);
  skip_whitespace(buffer);
  // end or string
  while (CURRENT_CHAR(buffer) != '}')
  {
    bool res = parseKeyValuePair(obj, buffer);
    if (!res)
    {
      return false;
    }

    skip_whitespace(buffer);
    if (match(buffer, ','))
    {
      // It's illegal to have a ',' and then end it
      ADVANCE(buffer);
    }
    skip_whitespace(buffer);
  }
  ADVANCE(buffer);
  return true;
}

static inline void resizeArray(JsonArray* arr)
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
static bool parseJsonArray(JsonArray* arr, Buffer* buffer)
{
  ADVANCE(buffer);
  skip_whitespace(buffer);
  bool res;
  while (CURRENT_CHAR(buffer) != ']')
  {
    resizeArray(arr);
    res = parseJsonValue(&arr->values[arr->arraySize], buffer);
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
static bool parseNumber(f32* number, Buffer* buffer)
{
  *number = parse_float_from_string(buffer);
  return true;
}

static bool parseJsonValue(JsonValue* value, Buffer* buffer)
{
  skip_whitespace(buffer);
  if (isdigit(CURRENT_CHAR(buffer)) || CURRENT_CHAR(buffer) == '-')
  {
    value->type = JSON_NUMBER;
    return parseNumber(&value->number, buffer);
  }
  switch (CURRENT_CHAR(buffer))
  {
  case '\"':
  {
    value->type = JSON_STRING;
    return json_parse_string(&value->string, buffer);
  }
  case '{':
  {
    value->type      = JSON_OBJECT;
    value->obj->cap  = 0;
    value->obj->size = 0;
    return parseJsonObject(value->obj, buffer);
  }
  case '[':
  {
    value->type           = JSON_ARRAY;
    value->arr            = (JsonArray*)malloc(sizeof(JsonArray));
    value->arr->arrayCap  = 0;
    value->arr->arraySize = 0;
    return parseJsonArray(value->arr, buffer);
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
    printf("Unknown value token '%c'\n", CURRENT_CHAR(buffer));
    return false;
  }
  }
}

bool sta_deserialize_json_from_file(Arena* arena, Json* json, const char* filename)
{
  Buffer fileContent;
  bool   result;

  result = sta_read_file(arena, &fileContent, filename);
  if (!result)
  {
    return false;
  }
  u64  curr = 0;
  bool res;
  bool first = false;
  while (!first)
  {
    switch (fileContent.buffer[curr])
    {
    case '{':
    {
      json->headType = JSON_OBJECT;
      json->obj.cap  = 0;
      json->obj.size = 0;
      res            = parseJsonObject(&json->obj, &fileContent);
      first          = true;
      break;
    }
    case '[':
    {
      json->headType        = JSON_ARRAY;
      json->array.arrayCap  = 0;
      json->array.arraySize = 0;
      res                   = parseJsonArray(&json->array, &fileContent);
      first                 = true;
      break;
    }
    case ' ':
    {
    }
    case '\n':
    {
    }
    case '\t':
    {
      curr++;
      break;
    }
    default:
    {
      printf("Default: %c\n", fileContent.buffer[curr]);
      json->headType = JSON_VALUE;
      res            = parseJsonValue(&json->value, &fileContent);
      first          = true;
      break;
    }
    }
  }
  if (!res)
  {
    return false;
  }
  if (curr != fileContent.len)
  {
    printf("Didn't reach eof after parsing first value? %ld %ld\n", curr, fileContent.len);
    return false;
  }
  return true;
}

static void skip_digit(Buffer* buffer)
{
  SKIP(buffer, buffer->index < buffer->len && !(isdigit(CURRENT_CHAR(buffer)) || (CURRENT_CHAR(buffer) == '-')));
}

static void parse_wavefront_texture(WavefrontObject* obj, const char* line)
{
  Buffer buffer = {};
  buffer.len    = strlen(line);
  buffer.buffer = (char*)line;

  skip_digit(&buffer);

  obj->texture_coordinate_count++;
  RESIZE_ARRAY(obj->texture_coordinates, Vector3, obj->texture_coordinate_count, obj->texture_coordinate_capacity);

  for (i32 i = 0; i < 2; i++)
  {
    obj->texture_coordinates[obj->texture_coordinate_count - 1].v[i] = parse_float_from_string(&buffer);
    skip_digit(&buffer);
  }

  // Parse optional w
  if (buffer.index < strlen(line))
  {
    obj->texture_coordinates[obj->texture_coordinate_count - 1].z = parse_float_from_string(&buffer);
  }
  else
  {
    obj->texture_coordinates[obj->texture_coordinate_count - 1].z = 0;
  }
}
static void parseWavefrontNormal(WavefrontObject* obj, const char* line)
{

  Buffer buffer = {};
  buffer.len    = strlen(line);
  buffer.buffer = (char*)line;

  skip_digit(&buffer);
  obj->normal_count++;
  RESIZE_ARRAY(obj->normals, Vector3, obj->normal_count, obj->normal_capacity);

  // Parse x,y,z
  for (i32 i = 0; i < 3; i++)
  {
    obj->normals[obj->normal_count - 1].v[i] = parse_float_from_string(&buffer);
    skip_digit(&buffer);
  }
}
static void parseWavefrontVertex(WavefrontObject* obj, const char* line)
{
  Buffer buffer = {};
  buffer.len    = strlen(line);
  buffer.buffer = (char*)line;
  obj->vertex_count++;
  RESIZE_ARRAY(obj->vertices, Vector4, obj->vertex_count, obj->vertex_capacity);

  // Parse x,y,z
  for (i32 i = 0; i < 3; i++)
  {
    skip_digit(&buffer);
    obj->vertices[obj->vertex_count - 1].v[i] = parse_float_from_string(&buffer);
  }
    skip_digit(&buffer);

  // Parse optional w
  if (buffer.index < strlen(line))
  {
    obj->vertices[obj->vertex_count - 1].w = parse_float_from_string(&buffer);
  }
}
static void parseWavefrontFace(WavefrontObject* obj, const char* line)
{
  Buffer buffer = {};
  buffer.len    = strlen(line);
  buffer.buffer = (char*)line;
  buffer.index  = 0;
  obj->face_count++;
  RESIZE_ARRAY(obj->faces, WavefrontFace, obj->face_count, obj->face_capacity);

  WavefrontFace* face      = &obj->faces[obj->face_count - 1];
  u32            vertexCap = 8;
  face->count              = 0;
  face->vertices           = (WavefrontVertexData*)allocate(sizeof(WavefrontVertexData) * vertexCap);
  ADVANCE(&buffer);
  while (buffer.index < buffer.len)
  {
    face->count++;
    RESIZE_ARRAY(face->vertices, WavefrontVertexData, face->count, vertexCap);

    skip_digit(&buffer);
    face->vertices[face->count - 1].vertex_idx = parse_int_from_string(&buffer);

    skip_digit(&buffer);
    face->vertices[face->count - 1].texture_idx = parse_int_from_string(&buffer);

    skip_digit(&buffer);
    face->vertices[face->count - 1].normal_idx = parse_int_from_string(&buffer);
  }
}
static inline void parseWavefrontLine(WavefrontObject* obj, const char* line)
{
  if (line[0] == 'v' && line[1] == 't')
  {
    parse_wavefront_texture(obj, line);
  }
  else if (line[0] == 'v' && line[1] == 'n')
  {
    parseWavefrontNormal(obj, line);
  }
  else if (line[0] == 'v')
  {
    parseWavefrontVertex(obj, line);
  }
  else if (line[0] == 'f')
  {
    parseWavefrontFace(obj, line);
  }
  else
  {
    printf("Don't know how to parse '%s'\n", line);
  }
}
static inline Vector2 cast_vec3_to_vec2(Vector3 v)
{
  return (Vector2){v.x, v.y};
}

static inline Vector3 cast_vec4_to_vec3(Vector4 v)
{
  return (Vector3){v.x, v.y, v.z};
}

bool sta_parse_wavefront_object_from_file(ModelData* model, const char* filename)
{
  WavefrontObject obj             = {};
  u64             init_cap        = 8;
  obj.vertex_count                = 0;
  obj.vertices                    = (Vector4*)allocate(sizeof(struct Vector4) * init_cap);
  obj.vertex_capacity             = init_cap;

  obj.normal_count                = 0;
  obj.normals                     = (Vector3*)allocate(sizeof(struct Vector3) * init_cap);
  obj.normal_capacity             = init_cap;

  obj.texture_coordinate_count    = 0;
  obj.texture_coordinates         = (Vector3*)allocate(sizeof(Vector3) * init_cap);
  obj.texture_coordinate_capacity = init_cap;

  obj.face_count                  = 0;
  obj.face_capacity               = init_cap;
  obj.faces                       = (WavefrontFace*)allocate(sizeof(WavefrontFace) * init_cap);

  FILE* filePtr;
  char* line = NULL;
  u64   len  = 0;
  i64   read;

  filePtr = fopen(filename, "r");
  if (filePtr == NULL)
  {
    return false;
  }

  while ((read = getline(&line, &len, filePtr)) != -1)
  {
    line[strlen(line) - 1] = '\0';
    parseWavefrontLine(&obj, line);
  }

  model->vertex_count = obj.face_count * obj.faces[0].count;
  model->indices      = (u32*)allocate(sizeof(u32) * model->vertex_count);
  model->vertices     = (VertexData*)allocate(sizeof(VertexData) * model->vertex_count);

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

      model->vertices[index].normal = obj.normals[data.normal_idx - 1];
    }
    deallocate(obj.faces[i].vertices, sizeof(WavefrontVertexData) * obj.faces[i].count);
  }

  deallocate(obj.texture_coordinates, sizeof(Vector2) * obj.texture_coordinate_capacity);
  deallocate(obj.vertices, sizeof(Vector4) * obj.vertex_capacity);
  deallocate(obj.faces, sizeof(WavefrontFace) * obj.face_capacity);
  deallocate(obj.normals, sizeof(Vector3) * obj.normal_capacity);

  printf("Low: %.3f, High: %.3f\n", low, high);

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

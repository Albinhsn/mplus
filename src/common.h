#ifndef COMMON_H
#define COMMON_H

#include "platform.h"
#include <cstring>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define ArrayCount(Array)     (sizeof(Array) / sizeof((Array)[0]))

#define MAX(a, b)             ((a) < (b) ? (b) : (a))
#define MIN(a, b)             ((a) > (b) ? (b) : (a))
#define ABS(x)                ((x) < 0 ? -(x) : x)

#define ANSI_COLOR_RED        "\x1b[31m"
#define ANSI_COLOR_GREEN      "\x1b[32m"
#define ANSI_COLOR_YELLOW     "\x1b[33m"
#define ANSI_COLOR_BLUE       "\x1b[34m"
#define ANSI_COLOR_MAGENTA    "\x1b[35m"
#define ANSI_COLOR_CYAN       "\x1b[36m"
#define ANSI_COLOR_RESET      "\x1b[0m"

#define DEGREES_TO_RADIANS(x) (x * PI / 180.0f)
#define CLAMP(a, min, max)    MAX(MIN(max, a), min)

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef float    f32;
typedef double   f64;

#define PI 3.14159265358979

typedef struct PoolFreeNode PoolFreeNode;
struct PoolFreeNode
{
  PoolFreeNode* next;
};

struct PoolAllocator
{
  u64           memory;
  u64           chunk_size;
  u64           size;
  PoolFreeNode* head;
};
typedef struct PoolAllocator PoolAllocator;

void                         sta_pool_init(PoolAllocator* pool, void* buffer, u64 chunk_size, u64 count);
void*                        sta_pool_alloc(PoolAllocator* pool);
void                         sta_pool_free(PoolAllocator* pool, u64 ptr);
void                         sta_pool_free_all(PoolAllocator* pool);

struct Arena
{
public:
  Arena()
  {
    memory  = 0;
    ptr     = 0;
    maxSize = 0;
  }
  Arena(u64 size)
  {
    memory  = (u64)sta_allocate(size);
    maxSize = size;
    ptr     = 0;
  }
  u64 memory;
  u64 ptr;
  u64 maxSize;
};
typedef struct Arena Arena;
u64                  sta_arena_push(Arena* arena, u64 size, u64 alignment);
void                 sta_arena_pop(Arena* arena, u64 size);
#define DEFAULT_ALIGNMENT                        2 * sizeof(void*)

#define sta_arena_push_array(arena, type, count) (type*)sta_arena_push((arena), sizeof(type) * (count), DEFAULT_ALIGNMENT)
#define sta_arena_push_struct(arena, type)       sta_arena_push_array((arena), type, 1)

#define RESIZE_ARRAY(array, type, count, cap)                                                                                                                                                          \
  if (count >= cap)                                                                                                                                                                                    \
  {                                                                                                                                                                                                    \
    u64 prev_cap = cap;                                                                                                                                                                                \
    cap *= 2;                                                                                                                                                                                          \
    type* arr = (type*)sta_allocate(sizeof(type) * cap);                                                                                                                                               \
    memcpy(arr, array, sizeof(type) * prev_cap);                                                                                                                                                       \
    sta_deallocate(array, sizeof(type) * prev_cap);                                                                                                                                                    \
    array = arr;                                                                                                                                                                                       \
  }

struct Profiler
{
  u64 StartTSC;
  u64 EndTSC;
};
typedef struct Profiler Profiler;

extern Profiler         profiler;
u64                     ReadCPUTimer(void);
u64                     EstimateCPUTimerFreq(void);

void                    initProfiler();
void                    displayProfilingResult();

#define PROFILER 1
#if PROFILER

struct ProfileAnchor
{
  u64         elapsedExclusive;
  u64         elapsedInclusive;
  u64         hitCount;
  u64         processedByteCount;
  char const* label;
};
typedef struct ProfileAnchor ProfileAnchor;

extern ProfileAnchor         globalProfileAnchors[4096];
extern u32                   globalProfilerParentIndex;

struct ProfileBlock
{
  char const* label;
  u64         oldElapsedInclusive;
  u64         startTime;
  u32         parentIndex;
  u32         index;
};
typedef struct ProfileBlock ProfileBlock;
void                        initProfileBlock(ProfileBlock* block, char const* label_, u32 index_, u64 byteCount);
void                        exitProfileBlock(ProfileBlock* block);

#define NameConcat2(A, B) A##B
#define NameConcat(A, B)  NameConcat2(A, B)
#define TimeBandwidth(Name, ByteCount)                                                                                                                                                                 \
  ProfileBlock Name;                                                                                                                                                                                   \
  initProfileBlock(&Name, "Name", __COUNTER__ + 1, ByteCount);
#define ExitBlock(Name)              exitProfileBlock(&Name)
#define TimeBlock(Name)              TimeBandwidth(Name, 0)
#define ProfilerEndOfCompilationUnit static_assert(__COUNTER__ < ArrayCount(GlobalProfilerAnchors), "Number of profile points exceeds size of profiler::Anchors array")
#define TimeFunction                 TimeBlock(__func__)

#else

#define TimeBlock(blockName)
#define TimeFunction
#endif

enum LoggingLevel
{
  LOGGING_LEVEL_INFO,
  LOGGING_LEVEL_WARNING,
  LOGGING_LEVEL_ERROR,
  LOGGING_LEVEL_COUNT
};
typedef enum LoggingLevel LoggingLevel;

struct Logger
{
  FILE* file_ptr;

public:
  Logger()
  {
  }
  bool init_log_to_file(const char* filename);
  bool destroy_log_to_file();
  void log(LoggingLevel level, const char* msg);

private:
  inline void send_log_message(const char* msg, const char* color);
};
typedef struct Logger Logger;

struct String
{
public:
  String()
  {
    buffer = 0;
    length = 0;
  }
  String(char* buffer, u32 length)
  {
    this->buffer = buffer;
    this->length = length;
  }

  bool compare(String s)
  {
    return this->length == s.length && strncmp(this->buffer, s.buffer, this->length) == 0;
  };
  char* buffer;
  u32   length;
};

#endif

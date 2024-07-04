#include "common.h"
#include <cstdarg>
/*
 =========================================
 =========================================
                 ARENA
 =========================================
 =========================================
*/

static int align_offset(u64 offset, u64 alignment)
{
  u64 modulo = offset & (alignment - 1);
  if (modulo != 0)
  {
    offset += alignment - modulo;
  }
  return offset;
}

u64 Arena::push(u64 size)
{
  size += align_offset(size, DEFAULT_ALIGNMENT);
  if (this->ptr + size > this->maxSize)
  {
    return 0;
  }
  u64 out = this->memory + this->ptr;
  memset((void*)out, 0, size);
  this->ptr += size;
  return out;
}

void Arena::pop(u64 size)
{
  this->ptr -= size;
}

u64 sta_arena_push(Arena* arena, u64 size, u64 alignment)
{
  size += align_offset(size, alignment);
  if (arena->ptr + size > arena->maxSize)
  {
    return 0;
  }
  u64 out = arena->memory + arena->ptr;
  memset((void*)out, 0, size);
  arena->ptr += size;
  return out;
}
void sta_arena_pop(Arena* arena, u64 size)
{
  arena->ptr -= size;
}

/*
 =========================================
 =========================================
                POOL
 =========================================
 =========================================
*/
void* sta_pool_alloc(PoolAllocator* pool)
{
  PoolFreeNode* node = pool->head;
  if (node == 0)
  {
    assert(!"Pool allocator has no memory");
    return 0;
  }

  pool->head = node->next;

  return memset(node, 0, pool->chunk_size);
}
void sta_pool_free(PoolAllocator* pool, u64 ptr)
{
  if (ptr == 0)
  {
    return;
  }

  u64 start = pool->memory;
  u64 end   = pool->memory + pool->size;

  if (!(start <= ptr && ptr < end))
  {
    assert(0 && "Memory is out of bounds of the buffer in this pool");
    return;
  }

  PoolFreeNode* node = (PoolFreeNode*)ptr;
  node->next         = pool->head;
  pool->head         = node;
}
void sta_pool_free_all(PoolAllocator* pool)
{
  u64 chunk_count = pool->size / pool->chunk_size;

  for (u64 i = 0; i < chunk_count; i++)
  {
    u64           ptr  = pool->memory + pool->chunk_size * i;
    PoolFreeNode* node = (PoolFreeNode*)ptr;
    node->next         = pool->head;
    pool->head         = node;
  }
}

void sta_pool_init(PoolAllocator* pool, void* buffer, u64 chunk_size, u64 count)
{
  pool->memory     = (u64)buffer;
  pool->chunk_size = align_offset(chunk_size, DEFAULT_ALIGNMENT);
  pool->head       = 0;
  pool->size       = chunk_size * count;

  sta_pool_free_all(pool);
}

/*
 =========================================
 =========================================
                PROFILER
 =========================================
 =========================================
*/

Profiler      profiler;
u32           globalProfilerParentIndex = 0;
ProfileAnchor globalProfileAnchors[4096];

void          initProfileBlock(ProfileBlock* block, char const* label_, u32 index_, u64 byteCount)
{
  block->parentIndex         = globalProfilerParentIndex;

  block->index               = index_;
  block->label               = label_;

  ProfileAnchor* profile     = globalProfileAnchors + block->index;
  block->oldElapsedInclusive = profile->elapsedInclusive;
  profile->processedByteCount += byteCount;

  globalProfilerParentIndex = block->index;
  block->startTime          = ReadCPUTimer();
}
void exitProfileBlock(ProfileBlock* block)
{
  u64 elapsed               = ReadCPUTimer() - block->startTime;
  globalProfilerParentIndex = block->parentIndex;

  ProfileAnchor* parent     = globalProfileAnchors + block->parentIndex;
  ProfileAnchor* profile    = globalProfileAnchors + block->index;

  parent->elapsedExclusive -= elapsed;
  profile->elapsedExclusive += elapsed;
  profile->elapsedInclusive = block->oldElapsedInclusive + elapsed;
  ++profile->hitCount;

  profile->label = block->label;
}

static void PrintTimeElapsed(ProfileAnchor* Anchor, u64 timerFreq, u64 TotalTSCElapsed)
{

  f64 Percent = 100.0 * ((f64)Anchor->elapsedExclusive / (f64)TotalTSCElapsed);
  printf("  %s[%lu]: %lu (%.2f%%", Anchor->label, Anchor->hitCount, Anchor->elapsedExclusive, Percent);
  if (Anchor->elapsedInclusive != Anchor->elapsedExclusive)
  {
    f64 PercentWithChildren = 100.0 * ((f64)Anchor->elapsedInclusive / (f64)TotalTSCElapsed);
    printf(", %.2f%% w/children", PercentWithChildren);
  }
  if (Anchor->processedByteCount)
  {
    f64 mb             = 1024.0f * 1024.0f;
    f64 gb             = mb * 1024.0f;

    f64 seconds        = Anchor->elapsedInclusive / (f64)timerFreq;
    f64 bytesPerSecond = Anchor->processedByteCount / seconds;
    f64 mbProcessed    = Anchor->processedByteCount / mb;
    f64 gbProcessed    = bytesPerSecond / gb;

    printf(" %.3fmb at %.2fgb/s", mbProcessed, gbProcessed);
  }
  printf(")\n");
}
static u64 GetOSTimerFreq(void)
{
  return 1000000;
}

static u64 ReadOSTimer(void)
{
  struct timeval Value;
  gettimeofday(&Value, 0);

  u64 Result = GetOSTimerFreq() * (u64)Value.tv_sec + (u64)Value.tv_usec;
  return Result;
}

u64 ReadCPUTimer(void)
{

  return __rdtsc();
}

#define TIME_TO_WAIT 100

u64 EstimateCPUTimerFreq(void)
{
  u64 OSFreq     = GetOSTimerFreq();

  u64 CPUStart   = ReadCPUTimer();
  u64 OSStart    = ReadOSTimer();
  u64 OSElapsed  = 0;
  u64 OSEnd      = 0;
  u64 OSWaitTime = OSFreq * TIME_TO_WAIT / 1000;
  while (OSElapsed < OSWaitTime)
  {
    OSEnd     = ReadOSTimer();
    OSElapsed = OSEnd - OSStart;
  }

  u64 CPUEnd     = ReadCPUTimer();
  u64 CPUElapsed = CPUEnd - CPUStart;

  return OSFreq * CPUElapsed / OSElapsed;
}
#undef TIME_TO_WAIT

void initProfiler()
{
  profiler.StartTSC = ReadCPUTimer();
}

void displayProfilingResult()
{
  u64 endTime      = ReadCPUTimer();
  u64 totalElapsed = endTime - profiler.StartTSC;
  u64 cpuFreq      = EstimateCPUTimerFreq();

  printf("\nTotal time: %0.4fms (CPU freq %lu)\n", 1000.0 * (f64)totalElapsed / (f64)cpuFreq, cpuFreq);
  for (u32 i = 0; i < ArrayCount(globalProfileAnchors); i++)
  {
    ProfileAnchor* profile = globalProfileAnchors + i;

    if (profile->elapsedInclusive)
    {
      PrintTimeElapsed(profile, cpuFreq, totalElapsed);
    }
  }
}

/*
 =========================================
 =========================================
                LOGGING
 =========================================
 =========================================
*/

inline void Logger::send_log_message(const char* msg, const char* color, va_list args)
{
  char buffer[512];
  vsnprintf(buffer, ArrayCount(buffer), msg, args);
  fprintf(stderr, "%s%s\n", color, buffer);
  if (this->file_ptr)
  {

    fprintf(this->file_ptr, "%s\n", buffer);
  }
}
void Logger::info(const char* msg, ...)
{
  va_list args;
  va_start(args, msg);
  this->log(LOGGING_LEVEL_INFO, msg, args);
  va_end(args);
}
void Logger::warning(const char* msg, ...)
{
  va_list args;
  va_start(args, msg);
  this->log(LOGGING_LEVEL_WARNING, msg, args);
  va_end(args);
}
void Logger::error(const char* msg, ...)
{

  va_list args;
  va_start(args, msg);
  this->log(LOGGING_LEVEL_ERROR, msg, args);
  va_end(args);
}

bool Logger::init_log_to_file(const char* filename)
{

  this->file_ptr = fopen(filename, "a");
  return this->file_ptr != 0;
}
bool Logger::destroy_log_to_file()
{
  return fclose(this->file_ptr);
}
void Logger::log(LoggingLevel level, const char* msg, va_list args)
{
  const char* levels[LOGGING_LEVEL_COUNT] = {
      ANSI_COLOR_GREEN,
      ANSI_COLOR_YELLOW,
      ANSI_COLOR_RED,
  };
  assert(LOGGING_LEVEL_COUNT == 3 && "Need to fill in the color for the logging level!");
  this->send_log_message(msg, levels[level], args);
  fprintf(stderr, ANSI_COLOR_RESET);
}
void Logger::log(LoggingLevel level, const char* msg, ...)
{
  va_list args;
  va_start(args, msg);
  this->log(level, msg, args);
  va_end(args);
}

bool compare_float(f32 a, f32 b)
{
  const float EPSILON = 0.00001f;
  return std::abs(a - b) < EPSILON;
}

u32 sta_hash_string_fnv(String* s)
{
  uint32_t hash = 2166136261u;
  for (u64 i = 0; i < s->length; i++)
  {
    hash ^= (u8)s->buffer[i];
    hash *= 16777619;
  }

  return hash;
}

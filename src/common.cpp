#include "common.h"
#include <cassert>
#include <cstring>
#include <stdio.h>
#include <sys/time.h>
#include <x86intrin.h>

#include <sys/mman.h>
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

u64 sta_arena_push(Arena* arena, u64 size, u64 alignment)
{
  int offset = align_offset(size, alignment);
  if (arena->ptr + offset > arena->maxSize)
  {
    return 0;
  }
  u64 out = arena->memory + arena->ptr;
  arena->ptr += offset;
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
    assert(0 && "Pool allocator has no memory");
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

static inline void sta_writeToLogFile(Logger* logger, const char* msg)
{
  if (logger && logger->filePtr)
  {
    fprintf(logger->filePtr, "%s\n", msg);
  }
}

static inline void sta_sendLogMessage(Logger* logger, const char* msg, const char* color)
{
  fprintf(stderr, "%s%s\n", color, msg);
  sta_writeToLogFile(logger, msg);
}

void sta_log(Logger* logger, LoggingLevel level, const char* msg)
{
  switch (level)
  {
  case LOGGING_LEVEL_INFO:
  {
    sta_sendLogMessage(logger, msg, ANSI_COLOR_GREEN);
    break;
  }
  case LOGGING_LEVEL_WARNING:
  {
    sta_sendLogMessage(logger, msg, ANSI_COLOR_YELLOW);
    break;
  }
  case LOGGING_LEVEL_ERROR:
  {
    sta_sendLogMessage(logger, msg, ANSI_COLOR_RED);
    break;
  }
  }
  printf(ANSI_COLOR_RESET);
}

bool sta_initLogger(Logger* logger, const char* file_name)
{
  logger->filePtr = fopen(file_name, "a");
  return logger->filePtr != 0;
}

bool sta_destroyLogger(Logger* logger)
{
  return fclose(logger->filePtr);
}

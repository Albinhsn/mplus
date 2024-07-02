#include "platform_linux.h"
#include <cstdio>
#include <cstring>
#include <sys/mman.h>

void* linux_reallocate(void* ptr, long prev_size, long new_size)
{
  return mremap(ptr, prev_size, new_size, MREMAP_MAYMOVE, 0);
}
static int align_offset(long long offset, long long alignment)
{
  long long modulo = offset & (alignment - 1);
  if (modulo != 0)
  {
    offset += alignment - modulo;
  }
  return offset;
}

void* linux_allocate(long size)
{
  size += align_offset(64, size);
  void* res = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  memset(res, 0, size);

  return res;
}

bool linux_deallocate(void* ptr, long size)
{
  return munmap(ptr, size);
}

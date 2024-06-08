#include "platform_linux.h"
#include <cstring>
#include <sys/mman.h>

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
  size = align_offset(size, 64);
  void * res = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  memset(res, 0, size);

  return res;
}

bool linux_deallocate(void * ptr, long size){
  return munmap(ptr, size);
}

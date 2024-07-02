#ifndef STA_PLATFORM_H
#define STA_PLATFORM_H

#ifdef __unix__

#include "platform_linux.cpp"
#define sta_allocate(size) linux_allocate(size)
#define sta_allocate_struct(strukt, size) linux_allocate(sizeof(strukt) * size)
#define sta_deallocate(ptr, size) linux_deallocate(ptr, size);
#define sta_reallocate(ptr, size, new_size) linux_reallocate(ptr, size, new_size);
#endif


#endif

#ifndef PLATFORM_LINUX_H
#define PLATFORM_LINUX_H

void * linux_allocate(long size);
bool linux_deallocate(void * ptr, long size);

#endif

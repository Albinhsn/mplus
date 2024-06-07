#ifndef STA_PLATFORM_H
#define STA_PLATFORM_H

#ifdef __unix__

#include "platform_linux.h"
#define allocate(size) linux_allocate(size)
#define deallocate(ptr, size) linux_deallocate(ptr, size);
#endif


#endif

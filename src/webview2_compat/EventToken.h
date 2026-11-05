#pragma once
// On a case-insensitive Windows filesystem, <eventtoken.h> would resolve
// back to this compatibility shim and pragma-once would hide the actual type.
#if defined(__MINGW32__)
#include_next <eventtoken.h>
#else
#include <eventtoken.h>
#endif

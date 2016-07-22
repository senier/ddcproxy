#ifndef DEBUG_H
#define DEBUG_H

#include "chprintf.h"

extern BaseSequentialStream *dbg;

#define DEBUG_DEF BaseSequentialStream *dbg;
#define DEBUG_INIT(stream) { dbg = stream; } while (0);
#define DEBUG(fmt,...) chprintf (dbg, fmt, #__VA_ARGS__);

#endif // DEBUG_H

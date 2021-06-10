/*
**  Copyright 2002-2003 University of Illinois Board of Trustees
**  Copyright 2002-2003 Mark D. Roth
**  All rights reserved.
**
**  internal.h - internal header file for libtar
**
**  Mark D. Roth <roth@uiuc.edu>
**  Campus Information Technologies and Educational Services
**  University of Illinois at Urbana-Champaign
*/

#include "config.h"
#include "compat.h"
#include "libtar.h"
#include "untar.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/xattr.h>
#include <sys/statfs.h>

#include <sys/types.h>

#ifdef TLS
#define TLS_THREAD TLS
#else
#define TLS_THREAD
#endif


#ifndef SDL_FBCON_VIDEO
#define SDL_FBCON_VIDEO

#include "../../SDL_internal.h"
#include "../SDL_sysvideo.h"

#include "SDL_events.h"
#include "SDL_loadso.h"
#include "SDL_stdinc.h"
#include "SDL_syswm.h"
#include "SDL_version.h"
#include "SDL_video.h"
#include "../SDL_blit.h"
#include "SDL_pixels.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/vt.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>

#define FBCON_DRIVER_NAME "fbcon"

typedef struct
{
    int width;
    int height;
    char *mapped_mem;
    Uint32 format;
} FBCon_DisplayData;

typedef struct
{
    char *mmaped_mem;
} FBCon_WindowData;

#endif /* SDL_FBCON_VIDEO */

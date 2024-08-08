#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_FBCON

#include "SDL_fbcon_video.h"

/* Small wrapper for mmap() so we can play nicely with no-mmu hosts
 * (non-mmu hosts disallow the MAP_SHARED flag) */
static void *
do_mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
{
    void *ret = mmap(start, length, prot, flags, fd, offset);
    if (ret == (char *) -1 && (flags & MAP_SHARED)) {
        ret = mmap(start, length, prot,
                   (flags & ~MAP_SHARED) | MAP_PRIVATE, fd, offset);
    }
    return ret;
}

static void
FB_DeleteDevice(_THIS)
{
    if (_this->driverdata != NULL) {
        SDL_free(_this->driverdata);
        _this->driverdata = NULL;
    }
    SDL_free(_this);
}

static void
FB_VideoQuit(_THIS)
{
}

static int FBD;

int
FBCon_VideoInit(_THIS)
{
    FBCon_DisplayData *data = (FBCon_DisplayData *) SDL_calloc(1, sizeof(FBCon_DisplayData));
    if (data == NULL) {
        return SDL_OutOfMemory();
    }

    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        return SDL_SetError("fbcon: unable to open %s", "/dev/fb0");
    }
    FBD = fd;

    struct fb_fix_screeninfo finfo;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        FB_VideoQuit(_this);
        return SDL_SetError("Couldn't get console hardware info");
    }

    struct fb_var_screeninfo vinfo;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        FB_VideoQuit(_this);
        return SDL_SetError("Couldn't get console pixel format");
    }

    printf("fbcon vinfo, xres: %d, yres: %d, bits_per_pixel: %d\n",
           vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    /* Memory map the device, compensating for buggy PPC mmap() */
    int mapped_memlen = vinfo.yres_virtual * finfo.line_length;
    char *mapped_mem = do_mmap(NULL, mapped_memlen,
                               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_mem == (char *) -1) {
        mapped_mem = NULL;
        FB_VideoQuit(_this);
        return SDL_SetError("Unable to memory map the video hardware");
    }

    data->width = vinfo.xres;
    data->height = vinfo.yres;
    data->mapped_mem = mapped_mem;

    SDL_DisplayMode current_mode;
    SDL_zero(current_mode);
    current_mode.w = vinfo.xres;
    current_mode.h = vinfo.yres;
    /* FIXME: Is there a way to tell the actual refresh rate? */
    current_mode.refresh_rate = 60;
    /* 32 bpp for default */
    if (vinfo.bits_per_pixel == 32) {
        current_mode.format = SDL_PIXELFORMAT_ABGR8888;
    } else {
        current_mode.format = SDL_PIXELFORMAT_RGB888;
    }
    data->format = current_mode.format;

    current_mode.driverdata = NULL;

    SDL_VideoDisplay display;
    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = data;

    SDL_AddVideoDisplay(&display, SDL_FALSE);

    _this->checked_texture_framebuffer = SDL_TRUE;

    return 0;
}

void
FBCon_VideoQuit(_THIS)
{
}

void
FBCon_GetDisplayModes(_THIS, SDL_VideoDisplay *display)
{
    /* Only one display mode available, the current one */
    SDL_AddDisplayMode(display, &display->current_mode);
}

int
FBCon_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}

int
FBCon_CreateWindow(_THIS, SDL_Window *window)
{
    FBCon_DisplayData *displaydata = SDL_GetDisplayDriverData(0);

    /* Allocate window internal data */
    FBCon_WindowData *windowdata = (FBCon_WindowData *) SDL_calloc(1, sizeof(FBCon_WindowData));
    if (windowdata == NULL) {
        return SDL_OutOfMemory();
    }

    /* Always fullscreen */
    window->flags |= SDL_WINDOW_FULLSCREEN;
    window->flags |= SDL_WINDOW_SHOWN;
    window->is_hiding = SDL_TRUE;

    window->w = displaydata->width;
    window->h = displaydata->height;
    windowdata->mmaped_mem = displaydata->mapped_mem;

    /* Setup driver data for this window */
    window->driverdata = windowdata;

    SDL_PixelFormat *format = (SDL_PixelFormat *) SDL_calloc(1, sizeof(SDL_PixelFormat));
    format->format = displaydata->format;
    format->BitsPerPixel = 32;
    format->BytesPerPixel = 4;
    format->Ashift = 24;
    format->Amask = 0xffffffff;
    format->Rshift = 16;
    format->Gshift = 8;
    format->Bshift = 0;

    SDL_BlitMap *map = (SDL_BlitMap *) SDL_calloc(1, sizeof(SDL_BlitMap));
    map->info.r = 0xFF;
    map->info.g = 0xFF;
    map->info.b = 0xFF;
    map->info.a = 0xFF;

    SDL_Surface *surface = (SDL_Surface *) SDL_calloc(1, sizeof(SDL_Surface));
    surface->format = format;
    surface->w = displaydata->width;
    surface->h = displaydata->height;
    surface->pixels = displaydata->mapped_mem;
    surface->clip_rect.x = 0;
    surface->clip_rect.y = 0;
    surface->clip_rect.w = displaydata->width;
    surface->clip_rect.h = displaydata->height;
    surface->pitch = displaydata->width * 4;
    surface->map = map;
    window->surface = surface;
    window->surface_valid = SDL_TRUE;

    /* One window, it always has focus */
    // SDL_SetMouseFocus(window);
    // SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    printf("FBCon_CreateWindow done.\n");
    return 0;
}

void
FBCon_DestroyWindow(_THIS, SDL_Window *window)
{
    FBCon_WindowData *data;

    data = window->driverdata;
    if (data) {
        SDL_free(data);
    }
    window->driverdata = NULL;
}

void
FBCon_SetWindowTitle(_THIS, SDL_Window *window)
{
}

void
FBCon_SetWindowPosition(_THIS, SDL_Window *window)
{
}

void
FBCon_SetWindowSize(_THIS, SDL_Window *window)
{
}

void
FBCon_ShowWindow(_THIS, SDL_Window *window)
{
}

void
FBCon_HideWindow(_THIS, SDL_Window *window)
{
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
SDL_bool
FBCon_GetWindowWMInfo(_THIS, SDL_Window *window, struct SDL_SysWMinfo *info)
{
    if (info->version.major <= SDL_MAJOR_VERSION) {
        return SDL_TRUE;
    } else {
        SDL_SetError("application not compiled with SDL %d.%d\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
    }

    /* Failed to get window manager information */
    return SDL_FALSE;
}

void
FBCon_PumpEvents(_THIS)
{
}

int
FBCon_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    return 0;

    FBCon_WindowData *windowdata = (FBCon_WindowData *) window->driverdata;
    // printf("rect, x: %d, y: %d, w: %d, h: %d", rects->x, rects->y, rects->w, rects->h);

    char *buffer = (char *) SDL_malloc(768*4096);
    SDL_memcpy(windowdata->mmaped_mem, buffer, 768*4096);
    SDL_free(buffer);

    // int fd = open("/dev/urandom", O_RDONLY);
    // if (fd == -1) {
    //     return SDL_SetError("open file failed");
    // }
    // read(fd, &buffer, rects->x * rects->y * 32);
    // close(fd);

    // int fd = open("/dev/fb0", O_WRONLY);
    // if (fd < 0) {
    //     return SDL_SetError("fbcon: unable to open %s", "/dev/fb0");
    // }
    // int ret = write(fd, buffer, rects->x * rects->y * 32);
    // if (ret == -1) {
    //     return SDL_SetError("fuck");
    // }
    // close(fd);

    return 0;
}

static SDL_VideoDevice *
FBCon_CreateDevice(void)
{
    SDL_VideoDevice *device;

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    device->driverdata = NULL;

    /* Setup amount of available displays and current display */
    device->num_displays = 0;

    /* Set device free function */
    device->free = FB_DeleteDevice;

    /* Setup all functions which we can handle */
    device->VideoInit = FBCon_VideoInit;
    device->VideoQuit = FBCon_VideoQuit;
    device->GetDisplayModes = FBCon_GetDisplayModes;
    device->SetDisplayMode = FBCon_SetDisplayMode;
    device->CreateSDLWindow = FBCon_CreateWindow;
    device->SetWindowTitle = FBCon_SetWindowTitle;
    device->SetWindowPosition = FBCon_SetWindowPosition;
    device->SetWindowSize = FBCon_SetWindowSize;
    device->ShowWindow = FBCon_ShowWindow;
    device->HideWindow = FBCon_HideWindow;
    device->DestroyWindow = FBCon_DestroyWindow;
    device->GetWindowWMInfo = FBCon_GetWindowWMInfo;
    device->PumpEvents = FBCon_PumpEvents;
    device->UpdateWindowFramebuffer = FBCon_UpdateWindowFramebuffer;

    return device;
}

VideoBootStrap FBCon_bootstrap = {
    FBCON_DRIVER_NAME, "SDL fbcon video driver",
    FBCon_CreateDevice
};

#endif /* SDL_VIDEO_DRIVER_FBCON */

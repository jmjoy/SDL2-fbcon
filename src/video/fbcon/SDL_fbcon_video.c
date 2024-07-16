#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_FBCON

#include "SDL_fbcon_video.h"

/* Small wrapper for mmap() so we can play nicely with no-mmu hosts
 * (non-mmu hosts disallow the MAP_SHARED flag) */
static void *
do_mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
{
    void *ret;
    ret = mmap(start, length, prot, flags, fd, offset);
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

int
FBCon_VideoInit(_THIS)
{
    FBCon_DisplayData *data = (FBCon_DisplayData *) SDL_calloc(1, sizeof(FBCon_DisplayData));
    if (data == NULL) {
        return SDL_OutOfMemory();
    }

    int fd = open("/dev/fb0", O_RDWR, 0);
    if (fd < 0) {
        return SDL_SetError("fbcon: unable to open %s", "/dev/fb0");
    }

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
    int mapped_memlen = finfo.smem_len;
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
        current_mode.format = SDL_PIXELFORMAT_RGBX8888;
    }
    current_mode.driverdata = NULL;

    SDL_VideoDisplay display;
    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = data;
    
    SDL_AddVideoDisplay(&display, SDL_FALSE);

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

    window->w = displaydata->width;
    window->h = displaydata->height;

    /* Setup driver data for this window */
    window->driverdata = windowdata;

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

    return device;
}

VideoBootStrap FBCon_bootstrap = {
    FBCON_DRIVER_NAME, "SDL fbcon video driver",
    FBCon_CreateDevice
};

#endif /* SDL_VIDEO_DRIVER_FBCON */

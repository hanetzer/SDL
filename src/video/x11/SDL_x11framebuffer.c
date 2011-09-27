/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2011 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_config.h"

#if SDL_VIDEO_DRIVER_X11

#include "SDL_x11video.h"
#include "SDL_x11framebuffer.h"


#ifndef NO_SHARED_MEMORY

/* Shared memory error handler routine */
static int shm_error;
static int (*X_handler)(Display *, XErrorEvent *) = NULL;
static int shm_errhandler(Display *d, XErrorEvent *e)
{
        if ( e->error_code == BadAccess ) {
            shm_error = True;
            return(0);
        } else
        return(X_handler(d,e));
}

static SDL_bool have_mitshm(void)
{
    /* Only use shared memory on local X servers */
    if ( (SDL_strncmp(XDisplayName(NULL), ":", 1) == 0) ||
         (SDL_strncmp(XDisplayName(NULL), "unix:", 5) == 0) ) {
        return SDL_X11_HAVE_SHM;
    }
    return SDL_FALSE;
}

#endif /* !NO_SHARED_MEMORY */

int
X11_CreateWindowFramebuffer(_THIS, SDL_Window * window, Uint32 * format,
                            void ** pixels, int *pitch)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;
    XGCValues gcv;
    XVisualInfo vinfo;

    /* Free the old framebuffer surface */
    X11_DestroyWindowFramebuffer(_this, window);

    /* Create the graphics context for drawing */
    gcv.graphics_exposures = False;
    data->gc = XCreateGC(display, data->xwindow, GCGraphicsExposures, &gcv);
    if (!data->gc) {
        SDL_SetError("Couldn't create graphics context");
        return -1;
    }

    /* Find out the pixel format and depth */
    if (X11_GetVisualInfoFromVisual(display, data->visual, &vinfo) < 0) {
        SDL_SetError("Couldn't get window visual information");
        return -1;
    }

    *format = X11_GetPixelFormatFromVisualInfo(display, &vinfo);
    if (*format == SDL_PIXELFORMAT_UNKNOWN) {
        SDL_SetError("Unknown window pixel format");
        return -1;
    }

    /* Calculate pitch */
    *pitch = (((window->w * SDL_BYTESPERPIXEL(*format)) + 3) & ~3);

    /* Create the actual image */
#ifndef NO_SHARED_MEMORY
    if (have_mitshm()) {
        XShmSegmentInfo *shminfo = &data->shminfo;

        shminfo->shmid = shmget(IPC_PRIVATE, window->h*(*pitch), IPC_CREAT | 0777);
        if ( shminfo->shmid >= 0 ) {
            shminfo->shmaddr = (char *)shmat(shminfo->shmid, 0, 0);
            shminfo->readOnly = False;
            if ( shminfo->shmaddr != (char *)-1 ) {
                shm_error = False;
                X_handler = XSetErrorHandler(shm_errhandler);
                XShmAttach(display, shminfo);
                XSync(display, True);
                XSetErrorHandler(X_handler);
                if ( shm_error )
                    shmdt(shminfo->shmaddr);
            } else {
                shm_error = True;
            }
            shmctl(shminfo->shmid, IPC_RMID, NULL);
        } else {
            shm_error = True;
        }
        if (!shm_error) {
            data->ximage = XShmCreateImage(display, data->visual,
                             vinfo.depth, ZPixmap,
                             shminfo->shmaddr, shminfo, 
                             window->w, window->h);
            if (!data->ximage) {
                XShmDetach(display, shminfo);
                XSync(display, False);
                shmdt(shminfo->shmaddr);
            } else {
                /* Done! */
                data->use_mitshm = SDL_TRUE;
                *pixels = shminfo->shmaddr;
                return 0;
            }
        }
    }
#endif /* not NO_SHARED_MEMORY */

    *pixels = SDL_malloc(window->h*(*pitch));
    if (*pixels == NULL) {
        SDL_OutOfMemory();
        return -1;
    }

    data->ximage = XCreateImage(display, data->visual,
                      vinfo.depth, ZPixmap, 0, (char *)(*pixels), 
                      window->w, window->h, 32, 0);
    if (!data->ximage) {
        SDL_free(*pixels);
        SDL_SetError("Couldn't create XImage");
        return -1;
    }
    return 0;
}

int
X11_UpdateWindowFramebuffer(_THIS, SDL_Window * window, SDL_Rect * rects,
                            int numrects)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;
    int i;
    SDL_Rect *rect;

#ifndef NO_SHARED_MEMORY
    if (data->use_mitshm) {
        for (i = 0; i < numrects; ++i) {
            rect = &rects[i];

            if (rect->w == 0 || rect->h == 0) { /* Clipped? */
                continue;
            }
            XShmPutImage(display, data->xwindow, data->gc, data->ximage,
                    rect->x, rect->y,
                    rect->x, rect->y, rect->w, rect->h, False);
        }
    }
    else
#endif /* !NO_SHARED_MEMORY */
    {
        for (i = 0; i < numrects; ++i) {
            rect = &rects[i];

            if (rect->w == 0 || rect->h == 0) { /* Clipped? */
                continue;
            }
            XPutImage(display, data->xwindow, data->gc, data->ximage,
                  rect->x, rect->y,
                  rect->x, rect->y, rect->w, rect->h);
        }
    }
    XSync(display, False);
}

void
X11_DestroyWindowFramebuffer(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    if (data->ximage) {
        XDestroyImage(data->ximage);

#ifndef NO_SHARED_MEMORY
        if (data->use_mitshm) {
            XShmDetach(display, &data->shminfo);
            XSync(display, False);
            shmdt(data->shminfo.shmaddr);
            data->use_mitshm = SDL_FALSE;
        }
#endif /* !NO_SHARED_MEMORY */

        data->ximage = NULL;
    }
    if (data->gc) {
        XFreeGC(display, data->gc);
        data->gc = NULL;
    }
}

#endif /* SDL_VIDEO_DRIVER_X11 */

/* vi: set ts=4 sw=4 expandtab: */

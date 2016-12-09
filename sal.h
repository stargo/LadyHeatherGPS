//****************************************************************************
//*                                                                          *
//*   System Abstraction Layer                                               *
//*                                                                          *
//*   SAL.H: SAL function prototypes                                         *
//*                                                                          *
//*   32-bit protected-mode source compatible with Watcom 10.5/MSC 9.0       *
//*                                                                          *
//*   Version 1.00 of 23-May-96: Initial                                     *
//*           1.10 of  3-Aug-97: General update, numerous features added     *
//*           1.11 of 23-Feb-99: Added SAL_client_resolution()               *
//*           1.12 of  4-Oct-02: Added desktop mode functionality            *
//*                              and region updates                          *
//*                                                                          *
//*   Author: John Miles                                                     *
//*                                                                          *
//****************************************************************************
//*                                                                          *
//*   Copyright (C) 1996 Miles Design, Inc.                                  *
//*                                                                          *
//*   Miles Design, Inc.                                                     *
//*   8301 Elander Drive                                                     *
//*   Austin, TX 78750                                                       *
//*                                                                          *
//*   (512) 345-2642 / FAX (512) 342-9197 / john@miles.io                    *
//*                                                                          *
//****************************************************************************

#ifndef SAL_VERSION

#define SAL_VERSION      "1.14"
#define SAL_VERSION_DATE "18-Nov-05"

#endif

#ifndef SAL_H
#define SAL_H

#if !defined(_WINDOWS_)
#error Windows.h must be included before SAL.h!
#endif

#ifndef IS_WIN32
#ifdef WIN32
  #define IS_WIN32 1
#endif

#ifdef _WIN32
  #define IS_WIN32 1
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

//
// If compiling SAL library, use __declspec(dllexport) for both 
// declarations and definitions
//
// If compiling SAL application, use __declspec(dllimport) for
// declarations -- definitions don't matter
//
// (DOS, other system definitions to be added later)
//

#ifdef IS_WIN32
#undef DXDEC
#undef DXDEF

  #define SALEXPORT WINAPI

  #ifdef BUILD_SAL
    #define DXDEC __declspec(dllexport)
    #define DXDEF __declspec(dllexport)
  #else
    #define DXDEC __declspec(dllimport)
  #endif

#else

  #define DXDEC
  #define DXDEF
  #define SALEXPORT
  #define WINAPI

#endif

//
// Misc. constant definitions  
//

//
// General type definitions for portability
// 

#ifndef US_TYPEDEFS
#define US_TYPEDEFS

  typedef signed   long   BOOL32;
  typedef          char   C8 ;
#ifndef U8
  typedef unsigned char   U8 ;
  typedef unsigned short  U16;
  typedef unsigned long   U32;
  typedef signed   char   S8 ;
  typedef signed   short  S16;
  typedef signed   long   S32;
#endif
#endif

#ifndef YES
#define YES 1
#endif

#ifndef NO
#define NO  0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE  0
#endif

#ifndef SAL_TYPES
#define SAL_TYPES

#define SAL_FULLSCREEN     0               // Set fullscreen DDraw mode
#define SAL_WINDOW         1               // Set DIB windowed mode
#define SAL_TRY_FULLSCREEN 2               // Try fullscreen, fall back to DIB
#define SAL_DESKTOP        3               // Use current desktop mode in fullscreen, ignoring X and Y size parameters

//
// Predefined surface identifiers
// 
// 0 and 1 are reserved values for the front and back buffer surfaces, 
// respectively.  All other values are "black box" pointers to surfaces 
// allocated with SAL_allocate_video_surface().
//

#define SAL_FRONT_SURFACE 0
#define SAL_BACK_SURFACE  1

//
// Preference names and default values
//

#define SAL_ALLOW_FRONT_LOCK         0     // Disregard request for front 
#define DEFAULT_AFL                  NO    // surface lock
                                     
#define SAL_BUFFER_IF_NO_LFB         1     // Use system RAM page buffer 
#define DEFAULT_BINL                 YES   // if no LFB available
                                     
#define SAL_MAX_VIDEO_PAGES          2     // Allocate up to 3 video pages 
#define DEFAULT_MVP                  3     // if possible

#define SAL_ALLOW_WINDOW_RESIZE      3     // Allow window to be resized
#define DEFAULT_AWR                  YES

#define SAL_PREVENT_ALT_MENU_POPUP   4     // Do not allow ALT to pause app
#define DEFAULT_PAMP                 YES

#define SAL_ALWAYS_ON_TOP            5     // App has normal window Z order
#define DEFAULT_SAOT                 NO

#define SAL_MAXIMIZE_TO_FULLSCREEN   6     // Maximize button switches to FS
#define DEFAULT_MTF                  YES   // (or desktop, if TOGGLE_DESKTOP_WINDOW=TRUE)

#define SAL_USE_DDRAW_IN_WINDOW      7     // Use CreateDIBSection() or
#define DEFAULT_UDIW                 NO    // DirectDraw in windowed mode

#define SAL_USE_PAGE_FLIPPING        8     // Use page-flipping for refresh if
#define DEFAULT_UPF                  YES   // possible, else Blt()

#define SAL_USE_DDRAW_CLIPPER        9     // Always create DirectDraw clipper, 
#define DEFAULT_UDDC                 NO    // even in fullscreen mode

#define SAL_TOGGLE_DESKTOP_WINDOW    10    // TRUE if window toggles should go to
#define DEFAULT_TDW                  NO    // desktop rather than fullscreen mode

#define SAL_USE_DDRAW_IN_DESKTOP     11    // Use CreateDIBSection() or 
#define DEFAULT_UDID                 NO    // DirectDraw in desktop mode

#define SAL_SLEEP_WHILE_INACTIVE     12    // Pause in flip() if app loses focus
#define DEFAULT_SWI                  NO

#define SAL_QUIET                    13    // Inhibit printing of internal informational messages
#define DEFAULT_SQMODE               YES

#define SAL_USE_PARAMON              14    // Check for parallel-port access for debug traffic
#define DEFAULT_SUP                  YES

#define N_SAL_PREFS                  15    // # of preference types

//
// Structures
//

typedef struct
{
   U8 r;
   U8 g;
   U8 b;
}
SAL_RGB;

typedef struct
{
   S32 x0;
   S32 y0;
   S32 x1;
   S32 y1;
}
SAL_REGION;

typedef struct
{
   S32 x;
   S32 y;
   S32 w;
   S32 h;
}
SAL_WINAREA;

#ifdef IS_WIN32

typedef struct tagSAL_DDRAWINFO
{
   struct IDirectDraw2        * lpDD;
   struct IDirectDrawSurface3 * lpDDSPrimary;
   struct IDirectDrawSurface3 * lpDDSBack;
   struct IDirectDrawPalette  * lpDDPal;
   struct IDirectDrawClipper  * lpDDClipper;
}
SAL_DDRAWINFO;

#endif

//
// Function pointer types
//

typedef void   (SALEXPORT * SALFOCUSCB)       (S32 status);
typedef void   (SALEXPORT * SALEXITCB)        (void);

#ifdef IS_WIN32

typedef LONG HRESULT; 
typedef HRESULT (SALEXPORT * SALDDSTARTUPCB)  (S32            res_X,
                                               S32            res_Y,
                                               S32            bpp,
                                               LPPALETTEENTRY initial_palette,
                                               S32            window_mode,
                                               SAL_DDRAWINFO *dest);

typedef void    (SALEXPORT * SALDDSHUTDOWNCB) (SAL_DDRAWINFO *dest);

#endif

#endif   // end #ifndef SAL_TYPES

//
// Miscellaneous/support services (not system-specific)
//

DXDEC void __cdecl SAL_debug_printf          (char *fmt, ...);

DXDEC  S32 WINAPI  SAL_get_preference        (U32   number);
                   
DXDEC  S32 WINAPI  SAL_set_preference        (U32   number, 
                                              S32   value);

//
// General services (not system-specific)
//

DXDEC void WINAPI SAL_set_palette_entry      (S32         index,
                                              SAL_RGB    *entry,
                                              S32         wait_flag);
                                                        
DXDEC void WINAPI SAL_get_palette_entry      (S32         index,
                                              SAL_RGB    *entry);
                                                        
DXDEC void WINAPI SAL_set_palette_range      (S32         index,
                                              S32         num_entries,
                                              SAL_RGB    *entry_list,
                                              S32         wait_flag);
                                                        
DXDEC void WINAPI SAL_get_palette_range      (S32         index,
                                              S32         num_entries,
                                              SAL_RGB    *entry_list);
                                                        
DXDEC void WINAPI SAL_get_pixel_format       (S32        *pixel_pitch,
                                              S32        *bytes_per_pixel,
                                              S32        *R_shift,
                                              U32        *R_mask,
                                              S32        *R_width,
                                              S32        *G_shift,
                                              U32        *G_mask,
                                              S32        *G_width,
                                              S32        *B_shift,
                                              U32        *B_mask,
                                              S32        *B_width);
                                                        
DXDEC void WINAPI SAL_client_resolution      (S32        *X,
                                              S32        *Y);
                                                        
DXDEC void WINAPI SAL_display_resolution     (S32        *X,
                                              S32        *Y);
                                                        
DXDEC S32  WINAPI SAL_display_page_count     (void);    
                                                        
DXDEC void WINAPI SAL_flip_surface           (void);    
                                                        
DXDEC void WINAPI SAL_blit_surface				(void);    
                                                        
DXDEC void WINAPI SAL_blit_region 				(SAL_REGION *region); 
                                                        
DXDEC void WINAPI SAL_wipe_surface           (S32         surface,
                                              U32         color);
                                                        
DXDEC void WINAPI SAL_lock_surface           (S32         surface,
                                              U8        **ptr,
                                              S32        *pitch);
                                                       
DXDEC void WINAPI SAL_unlock_surface         (S32         surface,
                                              S32         perform_flip);
                                                       
DXDEC S32  WINAPI SAL_allocate_video_surface (S32         width,
                                              S32         height);
                                                       
DXDEC void WINAPI SAL_release_video_surface  (S32         surface);

DXDEC void WINAPI SAL_wait_for_vblank        (void);

//
// Mouse services
//

DXDEC void WINAPI SAL_show_system_mouse      (void);
DXDEC void WINAPI SAL_hide_system_mouse      (void);

DXDEC void WINAPI SAL_constrain_mouse        (void);
DXDEC void WINAPI SAL_unconstrain_mouse      (void);

//
// System-specific functions for Win32 version
//

#ifdef IS_WIN32
  
  DXDEC S32        WINAPI SAL_startup                 (HINSTANCE instance,
                                                       char     *app_name,
                                                       S32       allow_multiple_instances,
                                                       SALEXITCB exit_handler);

  DXDEC void       WINAPI SAL_shutdown                (void);

  DXDEC void       WINAPI SAL_desktop_mode            (S32 *X_size, 
                                                       S32 *Y_size,         
                                                       S32 *BPP,            
                                                       S32 *refresh_rate);

  DXDEC void       WINAPI SAL_desktop_format          (U32 *R_mask,               
                                                       U32 *G_mask,              
                                                       U32 *B_mask);      

  DXDEC HWND       WINAPI SAL_create_main_window_with_WNDCLASS
                                                      (const WNDCLASSEX *wc);

  DXDEC void       WINAPI SAL_set_application_icon    (C8   *resource);

  DXDEC HWND       WINAPI SAL_create_main_window      (void);
                                                      
  DXDEC S32        WINAPI SAL_set_display_mode        (S32   display_size_X,
                                                       S32   display_size_Y,
                                                       S32   display_bpp,
                                                       S32   initial_window_mode,
                                                       S32   allow_mode_switch);

  DXDEC S32        WINAPI SAL_window_status           (void);
  DXDEC void       WINAPI SAL_window_area             (SAL_WINAREA *area);
  DXDEC void       WINAPI SAL_client_area             (SAL_WINAREA *area);
  DXDEC S32        WINAPI SAL_is_app_active           (void);
  DXDEC SALFOCUSCB WINAPI SAL_register_focus_callback (SALFOCUSCB fn);
  DXDEC WNDPROC    WINAPI SAL_register_WNDPROC        (WNDPROC fn);

  DXDEC void       WINAPI SAL_FlipToGDISurface        (void);

  DXDEC void       WINAPI SAL_get_back_buffer_DC      (HDC *dc);
  DXDEC void       WINAPI SAL_release_back_buffer_DC  (HDC  dc);

  DXDEC void       WINAPI SAL_serve_message_queue     (void);
                                                                           
  DXDEC void      __cdecl SAL_alert_box               (C8 *caption,
                                                       C8 *fmt, ...);

  DXDEC SALDDSTARTUPCB
                   WINAPI SAL_register_DDSTARTUP_callback
                                                      (SALDDSTARTUPCB fn);

  DXDEC SALDDSHUTDOWNCB
                   WINAPI SAL_register_DDSHUTDOWN_callback
                                                      (SALDDSHUTDOWNCB fn);

  DXDEC SAL_DDRAWINFO
                   WINAPI SAL_get_DDRAW_info          (void);
#endif

#ifdef __cplusplus
}
#endif

#endif

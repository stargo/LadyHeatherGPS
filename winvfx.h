//****************************************************************************
//*                                                                          *
//*  WINVFX.H: C type definitions & API function prototypes                  *
//*                                                                          *
//*  Source compatible with 32-bit 80386 C/C++                               *
//*                                                                          *
//*  V1.00 of 21-Jul-96: Initial version, derived from VFX.H 1.16            *
//*                                                                          *
//*  32-bit protected-mode source compatible with Watcom 10.5/MSC 9.0        *
//*                                                                          *
//*  Project: 386FX Sound & Light(TM)                                        *
//*   Author: Ken Arnold, John Miles, John Lemberger                         *
//*                                                                          *
//****************************************************************************
//*                                                                          *
//*  Copyright (C) 1992-1996 Miles Design, Inc.                              *
//*                                                                          *
//*  Miles Design, Inc.                                                      *
//*  8301 Elander Drive                                                      *
//*  Austin, TX 78750                                                        *
//*                                                                          *
//*  jmiles@pop.net                                                          *
//*  (512) 345-2642                                                          *
//*                                                                          *
//****************************************************************************

#ifndef WINVFX_VERSION

#define WINVFX_VERSION      "1.00"
#define WINVFX_VERSION_DATE "21-Jul-96"

#endif

#ifndef WINVFX_H
#define WINVFX_H

#ifdef WIN32
  #define IS_WIN32 1
#endif

#ifdef _WIN32
  #define IS_WIN32 1
#endif

#ifdef WIN32
#include "sal.h"     // Video system abstraction layer
#endif

#ifdef __cplusplus
extern "C" {
#endif

//
// If compiling WINVFX library, use __declspec(dllexport) for both 
// declarations and definitions
//
// If compiling WINVFX application, use __declspec(dllimport) for
// declarations -- definitions don't matter
//

#ifdef IS_WIN32
#undef DXDEC
#undef DXDEF

  #ifdef BUILD_VFX
    #define DXDEC __declspec(dllexport)
    #define DXDEF __declspec(dllexport)
  #else
    #define DXDEC __declspec(dllimport)
  #endif

#else

  #define DXDEC
  #define DXDEF
  #define VFXEXPORT
  #define WINAPI

#endif

//
// General type definitions for portability
// 

#include "typedefs.h"

#ifndef FIX_TYPEDEFS

  typedef long F16;           // 16:16 fixed-point type [-32K,+32K]
  typedef long F30;           // 2:30 fixed-point type [-1.999,+1.999]

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

//
// Preference names and default values
//

#define N_VFX_PREFS                  0     // # of preference types

//
// Misc. definitions
//

#define SHAPE_FILE_VERSION '01.1' // 1.10 backwards for big-endian compare

#define GIF_SCRATCH_SIZE 20526L   // Temp memory req'd for GIF decompression

//
// VFX_map_polygon() flags
//

#define MP_XLAT      0x0001       // Use lookaside table (speed loss = ~9%)
#define MP_XP        0x0002       // Enable transparency (speed loss = ~6%)

//
// VFX_shape_transform() flags
//

#define ST_XLAT      0x0001       // Use shape_lookaside() table
#define ST_REUSE     0x0002       // Use buffer contents from prior call

//
// VFX_line_draw() modes
//  

#define LD_DRAW      0
#define LD_TRANSLATE 1
#define LD_EXECUTE   2

//
// VFX_pane_scroll() modes
//

#define PS_NOWRAP    0
#define PS_WRAP      1

#define NO_COLOR -1

//
// VFX_shape_visible_rectangle() mirror values
//

#define VR_NO_MIRROR 0
#define VR_X_MIRROR  1
#define VR_Y_MIRROR  2
#define VR_XY_MIRROR 3

//
// Transparent color keys for 8-bit and high-color modes
//

#define PAL_TRANSPARENT 255       // Default transparent color for many primitives
#define RGB_TRANSPARENT 0xfffe    // Reserved 16-bit RGB transparency key

//
// PANE_LIST.flags values
//

#define PL_FREE      0           // Free and available for assignment
#define PL_VALID     1           // Assigned; to be refreshed
#define PL_CONTAINED 2           // Contained within another pane; don't refresh

//
// Window flags
//

#define VWF_BUFF_OWNED 0x0001    // Set if VFX owns buffer memory, else clear
#define VWF_FRONT_LOCK 0x0002    // Window buffer = locked front surface
#define VWF_BACK_LOCK  0x0004    // Window buffer = locked back surface

//
// Mode/surface equates (compatible with SAL)
// 

#define VFX_FULLSCREEN_MODE SAL_FULLSCREEN      // Set fullscreen DDraw mode
#define VFX_WINDOW_MODE     SAL_WINDOW          // Set DIB windowed mode
#define VFX_TRY_FULLSCREEN  SAL_TRY_FULLSCREEN  // Try fullscreen, fall back to DIB

#define VFX_FRONT_SURFACE   SAL_FRONT_SURFACE   // VFX_lock_window_surface()
#define VFX_BACK_SURFACE    SAL_BACK_SURFACE

// 
// Table selectors for VFX_default_system_font_color_table()
//

#define VFC_BLACK_ON_WHITE 0
#define VFC_WHITE_ON_BLACK 1
#define VFC_BLACK_ON_XP    2
#define VFC_WHITE_ON_XP    3

//
// VFX data structures
//

#pragma pack(1)                  // Do NOT allow compiler to reorder structs!

typedef struct
{
   S32 X_size;                   // Width of window for which stencil was made
   S32 Y_size;                   // Height of window

   S32 dir[1];                   // Stencil row-offset directory [height],
}                                // followed by stencil packet data...
VFX_STENCIL;

typedef struct _window
{
	void  *buffer;                // Pointer to window buffer

	S32    x_max;                 // Maximum X-coordinate in window [0,x_max]
	S32    y_max;                 // Maximum Y-coordinate in window [0,y_max]
                                 
   S32    pixel_pitch;           // # of bytes between adjacent pixels
   S32    bytes_per_pixel;       // # of bytes to write per pixel
                 
   //
   // RGB shift/mask values for mode active when window was created
   //

   S32    R_left;                // # of bits left to shift component
   S32    R_right;               // # of bits right to shift 8-bit component
   U32    R_mask;                // Component mask
   S32    R_width;               // # of bits in component

   S32    G_left;
   S32    G_right;
   U32    G_mask;
   S32    G_width;

   S32    B_left;
   S32    B_right;
   U32    B_mask;
   S32    B_width;

   S32    flags;
} 
VFX_WINDOW;

typedef struct _pane
{
	VFX_WINDOW *window;
	S32 x0;
	S32 y0;
	S32 x1;
	S32 y1;
} 
PANE;

typedef struct _pane_list
{
   PANE  *array;
   U32   *flags;
   U32   *user;
   S32    list_size;
}
PANE_LIST;

#ifndef VFX_RGB_DEFINED
#define VFX_RGB_DEFINED

typedef struct    // Binary-compatible with SAL_RGB
{
   U8 r;
   U8 g;
   U8 b;
}
VFX_RGB;

#endif

typedef struct
{
   U8      color;
   VFX_RGB rgb;
}
VFX_CRGB;

typedef struct
{
   S32 x;
   S32 y;
}
VFX_POINT;

typedef struct
{
   S32 version;
   S32 char_count;
   S32 char_height;
   S32 font_background;

   //
   // Font data follows header
   //
}
VFX_FONT;

typedef struct
{
   U32 version;
   U32 shape_count;

   //
   // rest of table...
   //
}
VFX_SHAPETABLE;

typedef struct        // Vertex structure used by VFX3D polygon primitives
{
   S32 x;             // Screen X
   S32 y;             // Screen Y

   F16 color;         // Color/addition value used by some primitives

   F16 u;             // Texture source X
   F16 v;             // Texture source Y
   F30 w;             // Homogeneous perspective divisor (unused by VFX3D)
}
SCRNVERTEX;               

typedef struct
{
   S32 x0;
   S32 y0;
   S32 x1;
   S32 y1;
}
VFX_RECT;

#pragma pack()       // Restore default structure-packing convention

#define INT_TO_F16(x)       (((long)(int)(x)) << 16)
#define DOUBLE_TO_F16(x)    ((long) ((x) * 65536.0 + 0.5))
#define F16_TO_DOUBLE(x)    (((double)(x)) / 65536.0)
#define F16_TO_SINGLE(x)    (((float)(x)) / 65536.0)
#define F16_TO_INT(x)       ((int) ((x)<0 ? -(-(x) >> 16) : (x) >> 16))
#define ROUND_F16_TO_INT(x) ((int) ((x)<0 ? -((32768-(x)) >> 16) : ((x)+32768) >> 16))

#define F16_TO_F30(x)       ((x) << 14)
#define F30_TO_F16(x)       ((x) >> 14)
#define F30_TO_DOUBLE(x)    (((double)x) / 1073741824.0)
#define F30_TO_SINGLE(x)    (((float)x) / 1073741824.0)
#define DOUBLE_TO_F30(x)    ((long) (x * 1073741824.0 + 0.5))
#define SINGLE_TO_F30(x)    ((long) (x * 1073741824.0 + 0.5))

#define PIXELS_IN_PANE(pane)    (((pane).x1-(pane).x0+1)*((pane).y1-(pane).y0+1))
#define PIXELS_IN_PANEP(pane)   (((pane)->x1-(pane)->x0+1)*((pane)->y1-(pane)0>y0+1))

//
// Macro to allow direct specification of 8-8-8 RGB triplet instead of
// global palette index
//
// The following functions accept either a global palette index (0-255)
// or RGB_NATIVE() / RGB_TRIPLET() values in high-color mode:
//
//   VFX_line_draw()
//   VFX_ellipse_draw()
//   VFX_ellipse_fill()
//   VFX_pane_wipe()
//   VFX_pane_scroll()
//   VFX_pixel_write()
//   VFX_rectangle_hash()
//   VFX_flat_polygon()
//

#define RGB_NATIVE(r,g,b)  (VFX_triplet_value(r,g,b) | 0x80000000)

#define RGB_TRIPLET(r,g,b) ((((r) >> 3) << 10) | \
                            (((g) >> 3) << 5)  | \
                            (((b) >> 3) << 0)  | \
                            0x40000000)

//
// Device-independent VFX API functions (WINVFXxx.CPP)
//

DXDEC S32  WINAPI VFX_set_display_mode      (S32             display_size_X,
                                             S32             display_size_Y,
                                             S32             display_bpp,
                                             S32             initial_window_mode,
                                             S32             allow_mode_switch);

DXDEC void WINAPI VFX_lock_window_surface   (VFX_WINDOW     *window, 
                                             S32             surface);

DXDEC void WINAPI VFX_unlock_window_surface (VFX_WINDOW     *window, 
                                             S32             perform_flip);

DXDEC void WINAPI VFX_set_palette_entry     (S32             index,
                                             VFX_RGB        *entry,
                                             S32             wait_flag);
                                               
DXDEC void WINAPI VFX_get_palette_entry     (S32             index,
                                             VFX_RGB        *entry);

DXDEC void WINAPI VFX_set_palette_range     (S32             index,
                                             S32             num_entries,
                                             VFX_RGB        *entry_list,
                                             S32             wait_flag);
                                               
DXDEC void WINAPI VFX_get_palette_range     (S32             index,
                                             S32             num_entries,
                                             VFX_RGB        *entry_list);

DXDEC U32  WINAPI VFX_pixel_value           (VFX_RGB        *VFX_RGB);

DXDEC U32  WINAPI VFX_triplet_value         (U32             r, 
                                             U32             g, 
                                             U32             b);

DXDEC VFX_RGB * 
           WINAPI VFX_RGB_value             (U32             native_pixel);

DXDEC VFX_RGB *
           WINAPI VFX_color_to_RGB          (U32             color);

DXDEC U32  WINAPI VFX_stencil_size          (VFX_WINDOW     *source, 
                                             U32             transparent_color);

DXDEC VFX_STENCIL * 
           WINAPI VFX_stencil_construct    (VFX_WINDOW      *source, 
                                            VFX_STENCIL     *dest, 
                                            U32              transparent_color);

DXDEC void WINAPI VFX_stencil_destroy       (VFX_STENCIL    *stencil);

DXDEC VFX_WINDOW * 
           WINAPI VFX_window_construct      (S32             width, 
                                             S32             height);

DXDEC void * 
           WINAPI VFX_assign_window_buffer  (VFX_WINDOW     *window,
                                             void           *buffer,
                                             S32             pitch);

DXDEC void WINAPI VFX_window_destroy        (VFX_WINDOW     *window);

DXDEC PANE * 
           WINAPI VFX_pane_construct        (VFX_WINDOW     *window, 
                                             S32             x0, 
                                             S32             y0, 
                                             S32             x1, 
                                             S32             y1);

DXDEC void WINAPI VFX_pane_destroy          (PANE           *pane);

DXDEC PANE_LIST *     
           WINAPI VFX_pane_list_construct   (S32             n_entries);

DXDEC void WINAPI VFX_pane_list_destroy     (PANE_LIST      *list);

DXDEC void WINAPI VFX_pane_list_clear       (PANE_LIST      *list);

DXDEC S32  WINAPI VFX_pane_list_add         (PANE_LIST      *list, 
                                             PANE           *target);

DXDEC S32  WINAPI VFX_pane_list_add_area    (PANE_LIST      *list, 
                                             VFX_WINDOW     *window, 
                                             S32             x0, 
                                             S32             y0,
                                             S32             x1, 
                                             S32             y1);

DXDEC void WINAPI VFX_pane_list_delete_entry(PANE_LIST      *list, 
                                             S32             entry_num);

DXDEC S32  WINAPI VFX_pane_list_identify_point
                                            (PANE_LIST      *list,
                                             S32             x,
                                             S32             y);

DXDEC PANE *
           WINAPI VFX_pane_list_get_entry   (PANE_LIST      *list,
                                             S32             entry_num);

DXDEC U32  WINAPI VFX_pane_entry_user_value (PANE_LIST      *list,
                                             S32             entry_num);

DXDEC U32  WINAPI VFX_set_pane_entry_user_value
                                            (PANE_LIST      *list,
                                             S32             entry_num,
                                             U32             user);

DXDEC void WINAPI VFX_pane_list_refresh     (PANE_LIST      *list);

//
// Device-independent VFX API functions (WINVFXxx.ASM)
//

DXDEC S32  WINAPI VFX_line_draw             (PANE           *pane, 
                                             S32             x0, 
                                             S32             y0, 
                                             S32             x1, 
                                             S32             y1, 
                                             S32             mode, 
                                             U32             parm);

DXDEC void WINAPI VFX_rectangle_draw        (PANE           *pane, 
                                             S32             x0, 
                                             S32             y0, 
                                             S32             x1, 
                                             S32             y1, 
                                             S32             mode, 
                                             U32             parm);

DXDEC void WINAPI VFX_rectangle_fill        (PANE           *pane, 
                                             S32             x0, 
                                             S32             y0, 
                                             S32             x1, 
                                             S32             y1, 
                                             S32             mode, 
                                             U32             parm);

DXDEC void WINAPI VFX_shape_draw            (PANE           *pane, 
                                             VFX_SHAPETABLE *shape_table,
                                             S32             shape_number, 
                                             S32             hotX, 
                                             S32             hotY);

DXDEC void WINAPI VFX_shape_lookaside       (U8             *table);

DXDEC void WINAPI VFX_shape_translate_draw  (PANE           *pane, 
                                             VFX_SHAPETABLE *shape_table,
                                             S32             shape_number, 
                                             S32             hotX, 
                                             S32             hotY);

DXDEC void WINAPI VFX_shape_transform       (PANE           *pane,
                                             VFX_SHAPETABLE *shape_table, 
                                             S32             shape_number, 
                                             S32             hotX, 
                                             S32             hotY,
                                             void           *buffer, 
                                             S32             rot, 
                                             S32             x_scale, 
                                             S32             y_scale, 
                                             U32             flags);

DXDEC void WINAPI VFX_shape_area_translate  (PANE           *pane,
                                             VFX_SHAPETABLE *shape_table, 
                                             S32             shape_number, 
                                             S32             hotX, 
                                             S32             hotY,
                                             void           *buffer, 
                                             S32             rot, 
                                             S32             x_scale, 
                                             S32             y_scale, 
                                             U32             flags,
                                             void           *lookaside);

DXDEC void WINAPI VFX_shape_remap_colors    (VFX_SHAPETABLE *shape_table,
                                             U32             shape_number);

DXDEC void WINAPI VFX_shape_visible_rectangle
                                            (VFX_SHAPETABLE *shape_table,
                                             S32             shape_number, 
                                             S32             hotX, 
                                             S32             hotY,
                                             S32             mirror, 
                                             VFX_RECT       *rectangle);

DXDEC S32  WINAPI VFX_shape_scan            (PANE           *pane, 
                                             U32             transparent_color,
                                             S32             hotX, 
                                             S32             hotY, 
                                             VFX_SHAPETABLE *shape_table);

DXDEC S32  WINAPI VFX_shape_bounds          (VFX_SHAPETABLE *shape_table, 
                                             S32             shape_num);

DXDEC S32  WINAPI VFX_shape_origin          (VFX_SHAPETABLE *shape_table, 
                                             S32             shape_num);

DXDEC S32  WINAPI VFX_shape_resolution      (VFX_SHAPETABLE *shape_table, 
                                             S32             shape_num);

DXDEC S32  WINAPI VFX_shape_minxy           (VFX_SHAPETABLE *shape_table, 
                                             S32             shape_num);

DXDEC void WINAPI VFX_shape_palette         (VFX_SHAPETABLE *shape_table, 
                                             S32             shape_num,
                                             VFX_RGB        *palette);

DXDEC S32  WINAPI VFX_shape_colors          (VFX_SHAPETABLE *shape_table, 
                                             S32             shape_num,
                                             VFX_CRGB       *colors);

DXDEC S32  WINAPI VFX_shape_set_colors      (VFX_SHAPETABLE *shape_table, 
                                             S32             shape_number,
                                             VFX_CRGB       *colors);

DXDEC S32  WINAPI VFX_shape_count           (VFX_SHAPETABLE *shape_table);

DXDEC S32  WINAPI VFX_shape_list            (VFX_SHAPETABLE *shape_table, 
                                             U32            *index_list);

DXDEC S32  WINAPI VFX_shape_palette_list    (VFX_SHAPETABLE *shape_table, 
                                             U32            *index_list);

DXDEC U32  WINAPI VFX_pixel_write           (PANE           *pane, 
                                             S32             x, 
                                             S32             y, 
                                             U32             color);

DXDEC U32  WINAPI VFX_pixel_read            (PANE           *pane, 
                                             S32             x, 
                                             S32             y);

DXDEC S32  WINAPI VFX_rectangle_hash        (PANE           *pane, 
                                             S32             x0, 
                                             S32             y0,
                                             S32             x1, 
                                             S32             y1, 
                                             U32             color);
                                                             
DXDEC S32  WINAPI VFX_pane_wipe             (PANE           *pane, 
                                             U32             color);

DXDEC S32  WINAPI VFX_pane_copy             (PANE           *source, 
                                             S32             sx, 
                                             S32             sy,
                                             PANE           *target, 
                                             S32             tx, 
                                             S32             ty, 
                                             S32             fill);

DXDEC S32  WINAPI VFX_pane_stretch          (PANE           *source,
                                             PANE           *target);

DXDEC S32  WINAPI VFX_pane_locate           (PANE           *source, //)
                                             PANE           *target,
                                             S32            *x,
                                             S32            *y);

DXDEC S32  WINAPI VFX_pane_scroll           (PANE           *pane, 
                                             S32             dx, 
                                             S32             dy,
                                             S32             mode, 
                                             S32             parm);

DXDEC void WINAPI VFX_ellipse_draw          (PANE           *pane, 
                                             S32             xc, 
                                             S32             yc, 
                                             S32             width, 
                                             S32             height, 
                                             U32             color);

DXDEC void WINAPI VFX_ellipse_fill          (PANE           *pane, 
                                             S32             xc, 
                                             S32             yc, 
                                             S32             width, 
                                             S32             height, 
                                             U32             color);

DXDEC void WINAPI VFX_point_transform       (VFX_POINT      *in, 
                                             VFX_POINT      *out, 
                                             VFX_POINT      *origin,
                                             S32             rot, 
                                             S32             x_scale, 
                                             S32             y_scale);

DXDEC void WINAPI VFX_Cos_Sin               (S32             angle, 
                                             F16            *cos, 
                                             F16            *sin);

DXDEC void WINAPI VFX_fixed_mul             (F16             M1, 
                                             F16             M2,
                                             F16            *result);

DXDEC VFX_FONT *  
           WINAPI VFX_default_system_font   (void);

DXDEC void *      
           WINAPI VFX_default_font_color_table
                                            (S32             table_selector);

DXDEC S32  WINAPI VFX_font_height           (VFX_FONT       *font);

DXDEC S32  WINAPI VFX_character_width       (VFX_FONT       *font, 
                                             S32             character);

DXDEC S32  WINAPI VFX_character_draw        (PANE           *pane, 
                                             S32             x, 
                                             S32             y, 
                                             VFX_FONT       *font,
                                             S32             character, 
                                             void           *color_translate);

DXDEC void WINAPI VFX_string_draw           (PANE           *pane, 
                                             S32             x, 
                                             S32             y, 
                                             VFX_FONT       *font,
                                             char           *string, 
                                             void           *color_translate);

DXDEC S32  WINAPI VFX_ILBM_draw             (PANE           *pane, 
                                             void           *ILBM_buffer);

DXDEC void WINAPI VFX_ILBM_palette          (void           *ILBM_buffer, 
                                             VFX_RGB        *palette);

DXDEC S32  WINAPI VFX_ILBM_resolution       (void           *ILBM_buffer);

DXDEC void WINAPI VFX_PCX_draw              (PANE           *pane, 
                                             S32             PCX_file_size, 
                                             void           *PCX_buffer);

DXDEC void WINAPI VFX_PCX_palette           (void           *PCX_buffer, 
                                             S32             PCX_file_size,
                                             VFX_RGB        *palette);

DXDEC S32  WINAPI VFX_PCX_resolution        (void           *PCX_buffer);

DXDEC S32  WINAPI VFX_GIF_draw              (PANE           *pane, 
                                             void           *GIF_buffer);

DXDEC void WINAPI VFX_GIF_palette           (void           *GIF_buffer, 
                                             VFX_RGB        *palette);

DXDEC S32  WINAPI VFX_GIF_resolution        (void           *GIF_buffer);

DXDEC S32  WINAPI VFX_color_scan            (PANE           *pane, 
                                             U32            *colors);

//
// Legacy VFX 2D polygon functions (originally from VFX3D.ASM)
//
// Only VFX_flat_polygon() and VFX_translate_polygon() are supported
// in high-color modes
//

DXDEC void WINAPI VFX_flat_polygon          (PANE           *pane, 
                                             S32             vcnt, 
                                             SCRNVERTEX     *vlist);

DXDEC void WINAPI VFX_Gouraud_polygon       (PANE           *pane, 
                                             S32             vcnt, 
                                             SCRNVERTEX     *vlist);

DXDEC void WINAPI VFX_dithered_Gouraud_polygon
                                            (PANE           *pane, 
                                             F16             dither_amount, 
                                             S32             vcnt, 
                                             SCRNVERTEX     *vlist);

DXDEC void WINAPI VFX_map_lookaside         (U8             *table);

DXDEC void WINAPI VFX_map_polygon           (PANE           *pane, 
                                             S32             vcnt, 
                                             SCRNVERTEX     *vlist,
                                             VFX_WINDOW     *texture, 
                                             U32             flags);

DXDEC void WINAPI VFX_translate_polygon     (PANE           *pane, 
                                             S32             vcnt, 
                                             SCRNVERTEX     *vlist,
                                             void           *lookaside);

DXDEC void WINAPI VFX_illuminate_polygon    (PANE           *pane, 
                                             F16             dither_amount, 
                                             S32             vcnt, 
                                             SCRNVERTEX     *vlist);

#ifdef __cplusplus
}
#endif

#endif

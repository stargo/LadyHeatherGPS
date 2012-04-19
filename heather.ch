#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>
#include <io.h>
#include <malloc.h>
#include <math.h>

#ifdef __DMC__          // DigitalMars.com C compiler
   #define _QC
   #define halloc(a,b) _halloc(a,b)
   #define hfree(a)    _hfree(a)
   #define huge        __huge
#endif

#ifdef __386__          // WATCOM 386 C compiler
// #define DOS 
// #define DOS_EXTENDER // #define this for DOS4GW DOS extender
   #define DOS_VFX
#endif

#ifdef _QC
  #define DOS           // #define this for DOS/QuickC environment
#else
  #ifdef _MSC_VER
    #define WINDOWS     // #define this for WIN32 environment
  #endif
#endif

#define VERSION "3.10"

#define DEBUG_TEMP_CONTROL
#define DEBUG_OSC_CONTROL
#define DEBUG_PLOTS

#define ADEV_STUFF     // define this to enable ADEV calculation and plotting
#define PRECISE_STUFF  // define this to enable precise lat/lon/alt code
#define DIGITAL_CLOCK  // define this to enable the big digital clock display
#define ANALOG_CLOCK   // define this to enable the analog clock display
#define AZEL_STUFF     // define this to enable azel map drawing
#define SAT_TRAILS     // define this to plot sat trails in the az/el map
#define TEMP_CONTROL   // define this to enable unit temperature control
#define GIF_FILES      // define this to enable GIF screen dumps
#define FFT_STUFF      // define this to enable FFT calculations

#ifndef DOS
   #define OSC_CONTROL    // define this to enable unit oscillator control
   #define GREET_STUFF    // define this to show holiday greetings
#endif

#define HT_SIZE  (1<<13)  // 13 bit GIF hash table size

#ifdef AZEL_STUFF
   #define AZEL_OK 1
#else
   #define AZEL_OK 0
#endif

#define PI          (3.1415926535897932384626433832795)
#define RAD_TO_DEG  (180.0/PI)
#define DEG_TO_RAD  (PI/180.0)
#define SQRT3       (1.732050808)

#define IABS(x)     (((x)<0)?(0-(x)):(x))
#define ABS(x)      (((x)<0.0F)?(0.0F-(x)):(x))
#define DABS(x)     (((x)<0.0)?(0.0-(x)):(x))
#define ROUND(x,to) ((((x)+(to)-1)/(to))*to)
#define DEBUGSTR(x) vidstr(MOUSE_ROW-1,MOUSE_COL, RED, x)

#define DEFAULT_SLEEP 25

#define OFS_SIZE float  // the type of the adev queue entries (float/double)

#ifdef DOS
   #define DOS_BASED
   #define KBHIT kbhit
   #define GETCH getch
   #define refresh_page() {}

   #ifdef DOS_EXTENDER
      #include "conio.h"
      #include "ll_comm.h"
      #define HUGE
      #define halloc(a,b) calloc((a),(b))
      #define hfree(a)    free(a)
      #define int86x int386x
      #define int86  int386 
      #define wd w
      EXTERN COMM PortID;
      #define SERIAL_DATA_AVAILABLE() ioReadStatus(PortID)
      #define DOS4GW_IO
      #define SIN_TABLES     // use sin/cos tables for drawing circles
      #define SIG_LEVELS     // track sig levels vs azimuth
      #define BUFFER_LLA     // save lla x/y plot data in a buffer for screen redraws
   #else
      #define wd x
      #define HUGE huge   // define this for over 720 log entries (array exceeds 64K)
      #define SERIAL_DATA_AVAILABLE() (com_q_in != com_q_out)
      #define EMS_MEMORY  // define this to enable EMS memory support
      #define DOS_IO
   #endif

   #define DOS_VIDEO      // BIOS driven video I/O
   #define DOS_MOUSE      // define this to enable mouse support

   #define COORD int
   #define u08 unsigned char
   #define u16 unsigned int
   #define u32 unsigned long
   #define s16 short
   #define s08 char
   #define C8  char
   #define S32 long

   #define FALSE 0
   #define TRUE  1

   #define DEGREES     'ø'   // special chars
   #define LEFT_ARROW  27
   #define RIGHT_ARROW 26
   #define UP_ARROW    24
   #define DOWN_ARROW  25
   #define CHIME_CHAR  13    // char to signal cuckoo clock on
   #define SONG_CHAR   14    // char to signal singing clock on
   #define ALARM_CHAR  15    // char to signal alarm or egg timer set
   #define DUMP_CHAR   19    // char to signal screen dump timer set

   #define TEXT_X_MARGIN 0
   #define TEXT_Y_MARGIN 0

   #define FIXED_FONT
   #define BEEP() if(beep_on){printf("\007"); fflush(stdout);}

   u08 get_serial_char(void);

   EXTERN union  REGS  reg;
   EXTERN struct SREGS seg;

   #define LONG_LONG long
#endif

#ifdef DOS_VFX
   #include "conio.h"
   #include "vfx.h"
   #include "dll.h"
   #include "ll_comm.h"

   #define DOS_BASED
   #define DOS4GW_IO
   #define KBHIT kbhit
   #define GETCH getch

   #define WIN_VFX
   #define VFX_FONT FONT
   #define VFX_WINDOW WINDOW
   #define DOS_MOUSE
   #define RGB_TRIPLET(a,b,c) 0

   #define HUGE
   #define int86x int386x
   #define int86  int386 
   #define wd w
   EXTERN COMM PortID;
   #define SERIAL_DATA_AVAILABLE() ioReadStatus(PortID)

   #define COORD int
   #define u08 unsigned char
   #define C8  char
   #define U08 unsigned char
   #define u16 unsigned short
   #define U16 unsigned short
   #define u32 unsigned long
   #define U32 unsigned long
   #define s32 long
   #define S32 long
   #define s16 short
   #define s08 char
   #define S08 char

   #define FIXED_FONT

   #define DEGREES     'ø'   // special chars
   #define UP_ARROW    24
   #define DOWN_ARROW  25
   #ifdef VARIABLE_FONT
      #define LEFT_ARROW  '<'
      #define RIGHT_ARROW '>'
      #define ALARM_CHAR  '*'
      #define DUMP_CHAR   19    // char to signal screen dump timer set
      #define CHIME_CHAR  '#'
      #define SONG_CHAR   '#'
   #endif
   #ifdef FIXED_FONT
      #define LEFT_ARROW  27
      #define RIGHT_ARROW 26
      #define CHIME_CHAR  13    // char to signal cuckoo clock on
      #define SONG_CHAR   14    // char to signal singing clock on
      #define ALARM_CHAR  15    // char to signal alarm or egg timer set
      #define DUMP_CHAR   19    // char to signal screen dump timer set
   #endif

   #define TEXT_X_MARGIN (0)
   #define TEXT_Y_MARGIN (0)

   #define BEEP() if(beep_on){printf("\007"); fflush(stdout);}
   unsigned char get_serial_char(void);
   void refresh_page(void);
   void VFX_rectangle_fill(PANE *stage,  int x1,int y1,  int x2,int y2, int mode, int color);

   EXTERN PANE *stage;
   EXTERN u08 VFX_io_done;   
   EXTERN U08 palette[256]; 
   EXTERN union  REGS  reg;
   EXTERN struct SREGS seg;

   #define LONG_LONG long

   #define SIN_TABLES     // use sin/cos tables for drawing circles
   #define SIG_LEVELS     // track sig levels vs azimuth
   #define BUFFER_LLA     // save lla x/y plot data in a buffer for screen redraws
#endif

// #define PLM            // define this for power line monitoring

#ifdef WINDOWS
   #include <assert.h>
   #include <conio.h>
   #define WIN32_LEAN_AND_MEAN
   #ifdef PLM
      #define _WIN32_WINNT 0x0500  // needed for high-res timer
   #endif
   #define WINCON         // printf() goes to TTY console if compiled with JM's modified windows.h, no effect otherwise, please leave in place
   #include <windows.h>
   #include <winbase.h>
   #include <windowsx.h>
   #include <commdlg.h>
   #include <mmsystem.h>
   #include <shellapi.h>
   #include <shlobj.h>

   #include <winsock2.h>      // TCP/IP
   #include <ws2tcpip.h>
   #include <mswsock.h>

   #include "typedefs.h"

   #define KBHIT win_kbhit
   #define GETCH win_getch
   #define SERIAL_DATA_AVAILABLE() (check_incoming_data())
   #define HUGE

   #include "sal.h"
   #include "winvfx.h"

   #include "resource.h"

   #define unlink(s)       _unlink(s)
   #define strupr(s)       _strupr(s)
   #define strlwr(s)       _strlwr(s)
   #define strnicmp(a,b,c) _strnicmp((a),(b),(c))
   #define outp(a,b)       _outp((a),(b))

   #define COORD int
   #define u08 unsigned char
   #define u16 unsigned short
   #define u32 unsigned int
   #define s16 short
   #define s08 int

///#define VARIABLE_FONT
   #define FIXED_FONT

   #define DEGREES     'ø'   // special chars
   #define UP_ARROW    24
   #define DOWN_ARROW  25
   #ifdef VARIABLE_FONT
      #define LEFT_ARROW  '<'
      #define RIGHT_ARROW '>'
      #define ALARM_CHAR  '*'
      #define DUMP_CHAR   19    // char to signal screen dump timer set
      #define CHIME_CHAR  '#'
      #define SONG_CHAR   '#'
   #endif
   #ifdef FIXED_FONT
      #define LEFT_ARROW  27
      #define RIGHT_ARROW 26
      #define CHIME_CHAR  13    // char to signal cuckoo clock on
      #define SONG_CHAR   14    // char to signal singing clock on
      #define ALARM_CHAR  15    // char to signal alarm or egg timer set
      #define DUMP_CHAR   19    // char to signal screen dump timer set
   #endif

   #define TEXT_X_MARGIN (TEXT_WIDTH*2)
   #define TEXT_Y_MARGIN (TEXT_HEIGHT*1)

   EXTERN HINSTANCE hInstance;
   EXTERN HWND      hWnd;
   EXTERN char root[MAX_PATH+1];
   EXTERN int root_len;
   #define BEEP() if(beep_on)MessageBeep(0)

   extern PANE *stage;
   EXTERN u08 VFX_io_done;   
   EXTERN U32 palette[16]; 

   EXTERN u08 downsized;  // flag set if screen has been windowed to show help dialog
   EXTERN u08 sal_ok;     // flag set once hardware has been setup

   unsigned char get_serial_char(void);
   void add_kbd(int key);
   int win_kbhit(void);
   int win_getch(void);
   void refresh_page(void);
   void go_fullscreen(void);
   void go_windowed(void);
   #define LONG_LONG long long

   #define TCP_IP         // enable TCP/IP networking
   #define WIN_VFX        // use VFX for screen I/O

   #define SIN_TABLES     // use sin/cos tables for drawing circles
   #define SIG_LEVELS     // track sig levels vs azimuth
   #define BUFFER_LLA     // save lla x/y plot data in a buffer for screen redraws
#endif

#ifdef TCP_IP
   EXTERN char   IP_addr[512];         // TCP/IP server address specified with /ip= parameter
   const S32 DEFAULT_RECV_BUF = 1024;
   const S32 DEFAULT_PORT_NUM = 45000;
#endif

#ifdef SIN_TABLES   // if memory is available use tables to speed up circle drawing
   #define sin360(x) sin_table[((int)(x))%360]
   #define cos360(x) cos_table[((int)(x))%360]
   EXTERN float sin_table[360+1];
   EXTERN float cos_table[360+1];
#else               // to save memory, DOS does not use tables to draw circles
   #define sin360(x) ((float)sin(((float)(((int)(x))%360))/(float)RAD_TO_DEG))
   #define cos360(x) ((float)cos(((float)(((int)(x))%360))/(float)RAD_TO_DEG))
#endif

#ifdef GREET_STUFF
   EXTERN char *moons[31+1];
#endif

#ifdef SIG_LEVELS
   EXTERN int min_el[360+1];    // minimum sat elevation seen at each azimuth
   EXTERN int max_el[360+1];    // maximum sat elevation seen at each azimuth
   EXTERN float db_az_sum[360+1];
   EXTERN float db_weighted_az_sum[360+1];
   EXTERN float db_az_count[360+1];
   EXTERN float db_el_sum[90+1];
   EXTERN float db_el_count[90+1];
   EXTERN float db_3d_sum[360+1][90+1];
   EXTERN float db_3d_count[360+1][90+1];
   EXTERN float max_sat_db[32+1];
   EXTERN float max_sig_level;
#endif // SIG_LEVELS

#define MAX_LLA_SIZE 320
#ifdef BUFFER_LLA
   EXTERN u08 lla_data[MAX_LLA_SIZE+1][MAX_LLA_SIZE+1];
#endif

// define the second to perform certain tasks at...
// ... so a bunch of stuff does not all happen at the same time
#define MOON_STUFF          48   // update moon info at 48 seconds
#define AZEL_UPDATE_SECOND  37   // update azel plot at 37 seconds
#define TRAIL_UPDATE_SECOND 22   // update azel trails at 22 seconds
#define CUCKOO_SECOND       00   // cuckoo clock at 00 seconds
#define SYNC_SECOND         06   // sync cpu time to gps time at 06 seconds
#define SYNC_MINUTE         05   // ... 05 minutes
#define SYNC_HOUR           04   // ... 04 hours
#define GREET_SECOND        12   // update greetings at 00:00:12
#define GREET_MINUTE        00   
#define GREET_HOUR          00   


/* screen attributes for output messages */
#define BLACK        0
#define DIM_BLUE     1
#define DIM_GREEN    2     
#define DIM_CYAN     3
#define DIM_RED      4
#define DIM_MAGENTA  5
#define BROWN        6
#define DIM_WHITE    7
#define GREY         8
#define BLUE         9
#define GREEN       10
#define CYAN        11 
#define RED         12
#define MAGENTA     13
#define YELLOW      14
#define WHITE       15
#define BMP_YELLOW  16


#define PPS_ADEV_COLOR  BLUE      // graph colors
#define OSC_ADEV_COLOR  BROWN
#define OSC_COLOR       WHITE
#define PPS_COLOR       MAGENTA
#define DAC_COLOR       GREEN
#define TEMP_COLOR      YELLOW
#define XXX_COLOR CYAN            // DEBUG_PLOTS
#define YYY_COLOR BROWN
#define ZZZ_COLOR BLUE

#define COUNT_COLOR     CYAN
#define CONST_COLOR     CYAN
#define SKIP_COLOR      RED       // timestamp error markers
#define HOLDOVER_COLOR  RED       // holdover/temp spike markers
#define MOUSE_COLOR     CYAN      // DOS mouse cursor
#define HELP_COLOR      WHITE     // help text
#define PROMPT_COLOR    CYAN      // string editing
#define STD_TIME_COLOR  BLUE      // time while in standard time
#define DST_TIME_COLOR  CYAN      // time while in daylight savings time
#define MARKER_COLOR    CYAN      // plot marker numbers
#define ALARM_COLOR     RED       // alarm clock color
#define DOP_COLOR       WHITE     // dilution of precision info
#define OSC_PID_COLOR   CYAN      // external oscillator disciplining status
#define TITLE_COLOR     WHITE     // plot title color
#define LEVEL_COLOR     BLUE      // avg signal level vs azimuth

#define HIGHLIGHT_REF   1         // put > < ticks on plot center reference line


// Where to place the various information onto the screen.
// These are in character coordinates.
#define TIME_ROW 0          // time stuff
#define TIME_COL 0

#define VAL_ROW 0           // oscillator values stuff
#define VAL_COL 15

#define VER_ROW 0           // version stuff
#define VER_COL 38

#define ADEV_ROW 0          // adev text tables
#define ADEV_COL 79

#define INFO_ROW 16         // status info
EXTERN int INFO_COL;

#define CRIT_ROW 0          // critical alarms
#define CRIT_COL INFO_COL   // ... show_satinfo() assumes CRIT_stuff is next to SAT_stuff

#define MINOR_ROW 6         // minor alarms
#define MINOR_COL INFO_COL

#define POSN_ROW 6          // lat/lon/alt info
#define POSN_COL 0

#define SURVEY_ROW 6        // self survey info / osc params
#define SURVEY_COL 22

#define DIS_ROW 6           // oscillator disciplining info
#define DIS_COL 40

#define SAT_ROW  10         // sat info (gets shifted down 2 more rows unless Ebolt at 800x600)
#define SAT_COL  0

#ifdef WINDOWS
  #define HELP_COL (PLOT_LEFT/TEXT_WIDTH)
  #define HELP_ROW (PLOT_TEXT_ROW+1) 
#endif

#ifdef DOS_VFX
  #define HELP_COL (PLOT_LEFT/TEXT_WIDTH)
  #define HELP_ROW (PLOT_TEXT_ROW-2) 
#endif

#ifdef DOS
  #define HELP_COL (PLOT_LEFT/TEXT_WIDTH)
  #define HELP_ROW (PLOT_TEXT_ROW-2) 
#endif

#define ALL_ROW (TIME_ROW+7)// for showing all adevs

EXTERN int FILTER_ROW;      // where to put the filter mode info
EXTERN int FILTER_COL;

EXTERN int MOUSE_ROW;       // where to put the data at the mouse cursor
EXTERN int MOUSE_COL;

EXTERN int time_row;        // where the big digital time clock is
EXTERN int time_col; 

EXTERN int all_adev_row;    // screen row to draw the adev tables at in all_adev mode
EXTERN int view_row;        // the text row number just above the view info


//
//  Screen / video mode stuff
//

EXTERN u08 ENDIAN;                 // 0=INTEL byte order,  1=other byte order
EXTERN int vmode;                  // BIOS video mode
EXTERN int text_mode;              // BIOS text mode (2,3, or 7)
EXTERN unsigned GRAPH_MODE;        // current video mode number
EXTERN unsigned user_video_mode;   // user specified video mode number
EXTERN S32 initial_window_mode;    // Windows version initial screen mode
EXTERN u08 need_screen_init;       // flag set if user did a /V command from the keyboard
EXTERN u08 dos_mouse_found;        // flag set if dos mouse seen
EXTERN u08 full_circle;            // full screen is taken over by a special display mode
EXTERN int calendar_entries;       // how many entries are in the greetings structure

#define WIDE_SCREEN (SCREEN_WIDTH >= 1680)  // screen is wide enough for watch and (azel or lla)
#define ADEV_AZEL_THRESH 1280               // screen is wide enough to always show adevs and azel map

EXTERN int SCREEN_WIDTH;     // screen size in pixels
EXTERN int SCREEN_HEIGHT;
EXTERN int TEXT_COLS;        // screen size in text chars
EXTERN int TEXT_ROWS;
EXTERN int TEXT_WIDTH;       // char size in pixels
EXTERN int TEXT_HEIGHT;
EXTERN int custom_width;     // custom screen size
EXTERN int custom_height;
EXTERN u08 big_plot;         // flag set if plot area is large
EXTERN u08 screen_type;      // 's', 'm', 'l' = small, medium, large, etc.
EXTERN int invert_video;     // swap black and white in screen dumps

EXTERN u08 user_font_size;   // set to size of font to use
EXTERN u08 small_font;       // set flag if font is < 8x16
                             // 1=proportional windows font  2=8x8 DOS font

EXTERN u08 no_x_margin;      // used to override windows screen margins
EXTERN u08 no_y_margin;
EXTERN char *print_using;    // used to print columns with VARIABLE_FONT
EXTERN u08 graphics_coords;  // set flag to pass graphics screen coords to vidstr
                             // ... (which normally uses character coords)


EXTERN int VERT_MAJOR;       // the graph axes tick mark spacing (in pixels)
EXTERN int VERT_MINOR;       // VERT_MAJOR/VERT_MINOR should be 5
EXTERN int HORIZ_MAJOR;
EXTERN int HORIZ_MINOR;

#define PLOT_LEFT 0          // left margin of plot area - should be 0 or PLOT_COL
EXTERN int PLOT_ROW;         // where the plotting area is on the screen
EXTERN int PLOT_COL;
EXTERN int PLOT_WIDTH;       // size of the plot window
EXTERN int PLOT_HEIGHT;
EXTERN int PLOT_TEXT_COL;    // PLOT_COL/TEXT_WIDTH
EXTERN int PLOT_TEXT_ROW;    // PLOT_ROW/TEXT_HEIGHT
EXTERN int PLOT_CENTER;      // center line of the plot window
EXTERN int PLOT_SCROLL;      // when graph fills, scroll it left this many pixels

EXTERN int day_plot;         // flag set to scale plot to 12/24 hours
EXTERN int day_size;         // the number of hours long the day_plot is

EXTERN int last_count_y;


EXTERN int COUNT_SCALE;      // used to scale the satellite count plot

EXTERN double osc_integral;  // integral of oscillator error

EXTERN u08 disable_kbd;             // set flag to disable keyboard commands
EXTERN u08 kbd_flag;
EXTERN u08 esc_esc_exit;            // set flag to allow ESC ESC to exit program
EXTERN u08 com_error_exit;          // set flag to abort on com port error
EXTERN u08 com_error;
EXTERN u08 pause_data;              // set flag to pause updating queues
EXTERN u08 user_pause_data;         // flag set if user set something on the command line that needs to pause data input
EXTERN u08 no_eeprom_writes;        // set flag to disable writing EEPROM

EXTERN u08 mouse_disabled;          // if set, do not do mouse stuff
EXTERN u08 mouse_shown;             // if set,  mouse cursor is being shown
EXTERN S32 mouse_x, mouse_y;        // current mouse coordinates
EXTERN u08 mouse_time_valid;        // flag set if mouse points to valid queue entry
EXTERN u08 last_mouse_time_valid;   // used to minimize clearing of mouse data area
EXTERN int this_button;             // current mouse button state
EXTERN int last_button;             // previous mouse button state
EXTERN long plot_q_col0;            // queue entry of leftmost column in the plot
EXTERN long plot_q_last_col;        // queue entry of rightmost column in the plot
EXTERN long last_mouse_q;           // the queue entry the mouse was last over
EXTERN long last_q_place;           // where we were before we moved to a marker

#define MAX_MARKER 10
EXTERN long mark_q_entry[MAX_MARKER]; // the queue entry we list clicked on

// bits in the sat_flags byte
#define TEMP_SPIKE   HOLDOVER
#define CONST_CHANGE 0x80
#define TIME_SKIP    0x40
#define UTC_TIME     0x20
#define HOLDOVER     0x10
#define SAT_FLAGS    (CONST_CHANGE | TIME_SKIP | UTC_TIME | HOLDOVER)
#define SAT_COUNT    0x0F

#define SENSOR_TC    10.0F          // tbolt firmware temperature sensor averaging time (in seconds)
EXTERN  u08 undo_fw_temp_filter;
EXTERN  u08 user_set_temp_filter;

#define OSC   0      // the various data that we can plot
#define PPS   1      // plots 0..3 are the standard plots.  they are always drawn
#define DAC   2
#define TEMP  3
#define XXX   4      // plots 4 and above are option/extra plots
#define YYY   5
#define ZZZ   6
#define FFT   7
#ifdef FFT_STUFF
   #define NUM_PLOTS 8
#else
   #define NUM_PLOTS 7
#endif

#define FIRST_EXTRA_PLOT XXX

struct PLOT_Q {    // the data we can plot
   u08 sat_flags;     // misc status and events
   u08 hh,mm,ss;      // time of this queue entry
   u08 dd,mo,yr;
   float data[NUM_PLOTS];    // the data values we can plot
};

EXTERN struct PLOT_Q HUGE *plot_q;
EXTERN unsigned long HUGE *hash_table;

EXTERN long plot_q_size;     // number of entries in the plot queue
EXTERN long plot_q_in;       // where next data point goes into the plot queue
EXTERN long plot_q_out;      // next data point that comes out of the queue
EXTERN long plot_q_count;    // how many points are in the plot queue
EXTERN u08  plot_q_full;     // flag set if plot queue is full
EXTERN long plot_start;      // queue entry at first point in the plot window
EXTERN u08  user_set_plot_size;

EXTERN u16 ems_ok;           // flag set if DOS version can use EMS memory
EXTERN u32 max_ems_size;
EXTERN int in_service;

EXTERN u08 interval_set;     // flag set if user did /i command to change plot queue interval
EXTERN long queue_interval;  // number of seconds between plot queue entries
                             // ... used to average each plot point over

EXTERN long view_interval;   // this can be used to set the interval that data will be
                             // extracted from the plot queue and displayed (to allow plots
                             // to be displayed at a different time scale that the queue interval)
EXTERN long user_view;       // the view time the user set on the command line
EXTERN u08  view_all_data;   // set flag to view all data in the queue
EXTERN u08 slow_q_skips;     // if set, skip over sub-sampled queue entries using the slow method
EXTERN u08 set_view;
EXTERN u08 continuous_scroll;// if set, redraw plot on every incoming point

EXTERN int plot_column;             // the current pixel column we are plotting
EXTERN int plot_mag;                // magnifies plot this many times
EXTERN int plot_time;               // indicates when it is time to draw a new pixel
EXTERN int view_time;               // same, but used when view_interval is not 1

EXTERN u08 auto_scale;              // if set, enables auto scaling of plots 
EXTERN u08 auto_center;             // if set, enables auto centering of plots
EXTERN u08 peak_scale;              // if set, don't let auto scale factors get smaller
EXTERN u08 off_scale;               // flag set whenever a plot goes off scale

//   values used to center plots around
#define NEED_CENTER 99999.0         // value used to indicate uninitialized

EXTERN u08 plot_adev_data;          // flags to control whether or not to plot the
EXTERN u08 plot_sat_count;          // ... various parameters
EXTERN u08 small_sat_count;         // is set, use compressed sat count graph
EXTERN u08 plot_const_changes;
EXTERN u08 plot_skip_data;          // time sequence and message errors
EXTERN u08 plot_holdover_data;
//EXTERN u08 plot_temp_spikes;  
#define plot_temp_spikes spike_mode   
EXTERN u08 plot_version;            // set flag to show program version on main sreen
EXTERN u08 plot_loc;                // set flag to show lat/lon/alt
EXTERN u08 plot_stat_info;          // set flag to show statistics value of plots
EXTERN u08 plot_digital_clock;      // set flag to show digital clock display
EXTERN u08 plot_watch;              // set flag to show watch face where the azel map goes
EXTERN u08 watch_face;              // which watch face to use
EXTERN u08 plot_azel;               // set flag to enable the az/el map
EXTERN u08 plot_signals;            // set flag to enable the signal level displays
EXTERN u08 plot_el_mask;            // set flag to show elevation mask in the az/el map
EXTERN u08 clock_12;                // set flag to show 12 hour clock
EXTERN u08 map_vectors;             // set flag to draw vectors to sats in azel map
EXTERN u08 no_greetings;            // set flag to disable holiday greetings
EXTERN u08 plot_background;         // if set, highlight WINDOWS plot background
EXTERN u08 erase_every_hour;        // set flag to redraw lla map every hour
EXTERN u08 user_set_bigtime;        // flag set if user specified the big clock option on the command line
EXTERN u08 beep_on;                 // flag set to enable beeper
EXTERN u08 blank_underscore;        // if set, a '_' on output gets blanked
EXTERN u08 show_live_fft;           // if set, display FFT on the incomming data
EXTERN u08 live_fft;                // the plot to show live data on
EXTERN u08 alt_calendar;            // use alternate calendar for date display
#define GREGORIAN  0
#define HEBREW     1
#define INDIAN     2
#define ISLAMIC    3
#define PERSIAN    4
#define AFGHAN     5
#define KURDISH    6
#define MJD        7
#define JULIAN     8
#define ISO        9
#define MAYAN      10     // Mayan long count
#define HAAB       11     // Mayan Haab date
#define TZOLKIN    12     // Mayan Tzolkin date
#define AZTEC      13     // Aztec Tzolkin type date
#define AZTEC_HAAB 14     // Aztec Haab type date
#define DRUID      15     // a pseudo-Druid calendar
#define CHINESE    16     // the chinese calendar  

#define MAYAN_CORR 584283L          // default mayan correlation cnstant
EXTERN long mayan_correlation;
EXTERN long aztec_epoch;            // day offset from default epoch
EXTERN long chinese_epoch;          // year offset from default epoch
EXTERN long druid_epoch;            // day offset from default epoch
EXTERN long calendar_offset;        // day offset from default epoch

EXTERN long filter_count;           // number of entries to average for display filter
EXTERN u08 filter_log;              // if set, write filtered values to the log
                                    // when writing a log from queued data

EXTERN double pps_base_value;       // used to keep values in the queue reasonable for single precision numbers
EXTERN double osc_base_value;
EXTERN u08 subtract_base_value;     // if set base values are subtracted/added
                                    // 1 = base values subtracted/added to queued values
                                    // 2 = base values subtarcted when read in

EXTERN float pdop;                  // dilution of precision values
EXTERN float hdop;
EXTERN float vdop;
EXTERN float tdop;
EXTERN u08 plot_dops;               // set flag to show the dops

// vector character stuff (plot markers and big digital clock display)
EXTERN int VCHAR_SCALE;
#define VCharWidth     (VCHAR_SCALE?(VCHAR_SCALE*8):8)
#define VCharHeight    (VCHAR_SCALE?(VCHAR_SCALE*16):16)
#define VCharThickness (VCHAR_SCALE?VCHAR_SCALE/2:1)

#define VSTRING_LEN 32              // max vector character string length
EXTERN char date_string[VSTRING_LEN];
EXTERN char last_vstring[VSTRING_LEN];
EXTERN int time_color;
EXTERN int last_time_color;

EXTERN long last_stamp;  // time stamp checking stuff
EXTERN u08 time_checked;
EXTERN long idle_sleep;  // Sleep() this long when idle


#ifdef ADEV_STUFF
   #define OSC_ADEV 0    // adev_types (even=OSC,  odd=PPS)
   #define PPS_ADEV 1
   #define OSC_HDEV 2
   #define PPS_HDEV 3
   #define OSC_MDEV 4
   #define PPS_MDEV 5
   #define OSC_TDEV 6
   #define PPS_TDEV 7

   EXTERN int ATYPE;              // the adev type to display

   #define ADEVS 30               // max number of adev bins to process
   EXTERN int max_adev_rows;      // actual number of bins we use (based upon adev_q_size)

   #define ADEV_DISPLAY_RATE  10  // update adev plots at this rate

   struct ADEV_INFO {
      u08   adev_type;            // pps or osc and 
      float adev_taus[ADEVS];
      float adev_bins[ADEVS];
      long  adev_on[ADEVS];
      int   bin_count;
      float adev_min;
      float adev_max;
   };
   EXTERN struct ADEV_INFO pps_bins;
   EXTERN struct ADEV_INFO osc_bins;

   struct BIN {
      S32    m;        // Integer tau factor (where tau = m*tau0)
      S32    n;        // # of phase points already contributing to this bin's value; calc loops run between B->n and n_points
      double sum;      // Running sum of squared variances
      double value;    // Latest final calculation result
      double tau;      // Tau factor times sample period in seconds
      double accum;    // Auxiliary sum for multiloop calculations 
      S32    i;        // Auxiliary index for multiloop calculations
      S32    j;        // Auxiliary index for multiloop calculations  
      S32    init;     // Flag processing needed on initial step
   };

   EXTERN double adev_q_overflow;   // counts how many entries have dropped out of the queue

   EXTERN struct BIN pps_adev_bins[ADEVS+1];  // incremental adev info bins
   EXTERN struct BIN osc_adev_bins[ADEVS+1];
   EXTERN struct BIN pps_hdev_bins[ADEVS+1];
   EXTERN struct BIN osc_hdev_bins[ADEVS+1];
   EXTERN struct BIN pps_mdev_bins[ADEVS+1];
   EXTERN struct BIN osc_mdev_bins[ADEVS+1];

   EXTERN double global_adev_max;   // max and min values found in all the adev bins
   EXTERN double global_adev_min;

   struct ADEV_Q {   // adev queue data values
      OFS_SIZE osc;
      OFS_SIZE pps;
   };

   EXTERN struct ADEV_Q HUGE *adev_q;  // queue of points to calc adevs over
   EXTERN long adev_q_in;
   EXTERN long adev_q_out;
   EXTERN long adev_q_count;

   #define get_adev_q(i)    adev_q[i]
   #define put_adev_q(i, q) adev_q[i] = q

   EXTERN u08 osc_adev_time;       // says when to show new osc adev tables
   EXTERN u08 pps_adev_time;       // says when to show new pps adev tables 

   EXTERN int bin_scale;           // adev bin sequence (default is 1-2-5)
   EXTERN int last_bin_count;      // how many adev bins were filled

   EXTERN int adev_time;           // says when to take an adev data sample
   EXTERN int adev_mouse_time;     // used to keep the DOS mouse lively during long adev calculations

   EXTERN int min_points_per_bin;  // number of points needed before we display the bin
   EXTERN int n_bins;              // max number of adev bins we will calculate

   void update_adev_display(int type);
   void show_adev_info(void);
   void add_adev_point(double osc, double pps);

   void incr_adev(u08 id, struct BIN *bins);
   void incr_hdev(u08 id, struct BIN *bins);
   void incr_mdev(u08 id, struct BIN *bins);

   int fetch_adev_info(u08 dev_id, struct ADEV_INFO *bins);
   void reset_incr_bins(struct BIN *bins);
#endif   // ADEV_STUFF

EXTERN long adev_q_size;         // number of entries in the adev queue
EXTERN u08 user_set_adev_size;

EXTERN float adev_period;        // adev data sample period in seconds
EXTERN u08 keep_adevs_fresh;     // if flag is set,  reset the adev bins
                                 // once the adev queue has overflowed twice

EXTERN u08 mixed_adevs;          // if flag set display normal plots along with all adev types
EXTERN u08 all_adevs;            //    0=normal display mode
                                 //    1=display all types of OSC adevs   
                                 //    2=display all types of PPS adevs



//  keyboard cursor and function key codes
#define F5_CHAR     0x013F
#define HOME_CHAR   0x0147
#define UP_CHAR     0x0148
#define PAGE_UP     0x0149
#define LEFT_CHAR   0x014B
#define RIGHT_CHAR  0x014D
#define END_CHAR    0x014F
#define DOWN_CHAR   0x0150
#define PAGE_DOWN   0x0151
#define INS_CHAR    0x0152
#define DEL_CHAR    0x0153

#define F8_CHAR     0x0142
#define F12_CHAR    0x0186

#define ESC_CHAR    0x1B

EXTERN u08  review_mode;       // flag set if scrolling through old data
EXTERN u08  review_home;       // flag set if we are at beginning of data
EXTERN long review;            // where we are looking in the queue
EXTERN long right_time;        // how long the right mouse button has been held
#define RIGHT_DELAY  10        // how many mouse checks before we start right-click mouse scrolling

EXTERN char read_log[128];     // name of log file to preload data from
EXTERN char log_name[128];     // name of log file to write
EXTERN char *log_mode;         // file write mode / append mode
EXTERN FILE *log_file;         // log file I/O handle
EXTERN u08 log_written;        // flag set if we wrote to the log file
EXTERN u08 log_header;         // write log file timestamp headers
EXTERN u08 log_loaded;         // flag set if a log file has been read
EXTERN u08 log_errors;         // if set, log data errors
EXTERN u08 user_set_log;       // flag set if user set any of the log parametrs on the command line
EXTERN long log_interval;      // seconds between log file entries
EXTERN long log_file_time;     // used to determine when to write a log entry
EXTERN u08 adev_log;           // flag set if file read in was adev intervals
EXTERN u08 dump_type;          // used to control what to write
EXTERN u08 log_stream;         // if set, write the incomming serial data to the log file
EXTERN u08 log_db;             // set flag to log sat signal levels
EXTERN u08 reading_log;        // flag set while reading log file

EXTERN FILE *lla_file;         // lat/lon/alt log file
EXTERN u08 lla_log;            // flag set if file read in was lat/lon/alt values

#define SCRIPT_NEST 4          // how deep we can nest script files
#define SCRIPT_LEN  128        // how long a script name can be
struct SCRIPT_STRUCT {         // keeps track of script info
   char name[SCRIPT_LEN+1];
   FILE *file;
   int line;
   int col;
   int err;
   int fault;
   u08 pause;
};
EXTERN struct SCRIPT_STRUCT scripts[SCRIPT_NEST];

EXTERN char script_name[SCRIPT_LEN+1]; // current script file name
EXTERN int script_nest;         // how many script files are open
EXTERN FILE *script_file;       // script file pointer
EXTERN int script_line;         // the position we are at in the file (for error reporting)
EXTERN int script_col;
EXTERN int script_err;          // flag set if error found in the script
EXTERN int script_fault;        // flag set if scripting needs to be aborted
EXTERN u08 script_pause;        // flag set if script input paused for keyboard input
EXTERN u08 skip_comment;        // set flag to skip to end of line in script file
EXTERN u08 script_exit;         // user hit the key to abort a script


EXTERN u08 first_sample;   // flag cleared after first data point has been received
EXTERN u08 have_time;      // flag set when a valid time has been received
EXTERN int have_year;
EXTERN int have_osc;
EXTERN u08 have_osc_params;
EXTERN int req_num;        // used to sequentially request various minor GPS messages
EXTERN int status_prn;     // used to sequentially request satellite status
EXTERN u08 amu_mode;       // flag set if signal level is in AMU units

EXTERN int sat_count;      // number of sats being used
EXTERN int max_sats;       // max number of sats we can display
EXTERN int temp_sats;      // temporary number of sats we can display
EXTERN u08 ebolt;          // flag set if ThunderBolt-E message seen
EXTERN u08 res_t;          // flag set if Resolution-T message seen
EXTERN u08 nortel;         // flag set to wake up a Nortel NTGxxxx unit
EXTERN u08 res_t_init;     // set flag to do 9800,ODD,1 
EXTERN u08 saw_nortel;     // flag set if nortel format osc params worked
EXTERN u08 last_ebolt;     // used to detect changes in ebolt setting
EXTERN int eofs;           // used to tweak things around on the screen if ebolt seen

EXTERN u32 this_const;     // keep track of constellation changes
EXTERN u32 last_const;
EXTERN u08 new_const;    

EXTERN int debug;
EXTERN int take_a_dump;
EXTERN int murray;         // testing hack for murray's weirdo GPSDO
                           // (does polled requests for timing messages if the
                           //  receiver does not broadcast them every second)

EXTERN int first_key;      // the first character of a keyboard command 
                           // ...used to confirm that user wants to do something 
                           // ...dangerous or prompt for the second char of a 
                           // ...two char keyboard command


#define ETX 0x03          // TSIP message end code
#define DLE 0x10          // TSIP message start code and byte stuffing escape value

// TSIP message handler error codes
#define MSG_ID      0x1000
#define MSG_END     0x0300
#define MSG_TIMEOUT 0xFF00

EXTERN u16 msg_id;         // TSIP message type code
EXTERN u08 subcode;        // TSIP message type subcode

EXTERN int last_was_dle;   // used to debug dump the serial stream to the log file
EXTERN int kol;

EXTERN u08 flag_faults;
EXTERN int msg_fault;     
EXTERN u16 tsip_error;     // flag set if error detected in the message
                           //   0x01 bit: msg start seen in middle of message
                           //   0x02 bit: msg end seen in middle of message
                           //   0x04 bit: msg end not seen where expected
                           //   0x08 bit: msg error in tsip byte
                           //   0x10 bit: msg error in tsip word
                           //   0x20 bit: msg error in tsip dword
                           //   0x40 bit: msg error in tsip float
                           //   0x80 bit: msg error in tsip double


EXTERN u32 packet_count;   // how many TSIP packets we have read
EXTERN u32 bad_packets;    // how many packets had known errors
EXTERN u32 math_errors;    // counts known math errors


struct SAT_INFO {  // the accumulated wisdom of the ages
   u08 health_flag;    // packet 49

   u08 disabled;       // packet 59
   u08 forced_healthy;


   float sample_len;   // packet 5A
   float sig_level;    // (also packet 47, 5C)
   u08 level_msg;      // the message type that set sig_level
   float code_phase;
   float doppler;
   double raw_time;

   float eph_time;     // packet 5B
   u08 eph_health;
   u08 iode;
   float toe;
   u08 fit_flag;          
   float sv_accuracy;

   u08 slot, chan;     // packet 5C
   u08 acq_flag;
   u08 eph_flag;
   float time_of_week;
   float azimuth;
   float elevation;
   u08 el_dir;
   u08 age;
   u08 msec;
   u08 bad_flag;
   u08 collecting;

   int tracking;       // 6D

   float sat_bias;     // 8F.A7
   float time_of_fix;
   u08 last_bias_msg;  // flag set if sat info was from last message
}; 

EXTERN struct SAT_INFO sat[1+32];


EXTERN double pps_offset;         // latest values from the receiver
EXTERN double osc_offset;
EXTERN float dac_voltage;
EXTERN float temperature;
EXTERN float pps_quant;

EXTERN double last_pps_offset;    // previous values from the receiver
EXTERN double last_osc_offset;
EXTERN float last_dac_voltage;
EXTERN float last_temperature;

EXTERN float last_temp_val;       // used in temp control code
EXTERN u08 temp_dir;              // direction we are moving the temperature

EXTERN float stat_count;          // how many data points we have summed 
                                  // up for the statisticsvalues

EXTERN float spike_threshold;     // used to filter out temperature sensor spikes
EXTERN int   spike_delay;         // ... when controlling tbolt temperature
EXTERN u08   spike_mode;          // 0=no filtering,  1=filter temp pid,  2=filter all

EXTERN char DEG_SCALE;            // 'C' or 'F'
EXTERN char deg_string[2];
EXTERN char *alt_scale;           // 'm' or 'f'
EXTERN u08 dms;                   // use deg.min.sec format

EXTERN double ANGLE_SCALE;        // earth size in degrees/ft or meter
EXTERN char *angle_units;         // "ft" or "m "

EXTERN double lat;                // receiver position
EXTERN double lon;
EXTERN double alt;

EXTERN u08 pv_filter;             // dynamics filters
EXTERN u08 static_filter;
EXTERN u08 alt_filter;
EXTERN u08 kalman_filter;

EXTERN u08 user_pv;               // user requested dynamics filter settings
EXTERN u08 user_static;
EXTERN u08 user_alt;
EXTERN u08 user_kalman;

EXTERN float el_mask;             // minimum acceptable satellite elevation 
EXTERN float amu_mask;            // minimum acceptable signal level
EXTERN float pdop_mask; 
EXTERN float pdop_switch; 
EXTERN u08 foliage_mode;          // 0=never  1=sometime  2=always
EXTERN u08 dynamics_code;         // 1=land  2=sea  3=air  4=stationary

#define set_rcvr_config(x)   config_rcvr((x),  0xFF, -1.0F, -1.0F, -1.0F, -1.0F, 0xFF)
#define set_rcvr_dynamics(x) config_rcvr(0xFF, (x),  -1.0F, -1.0F, -1.0F, -1.0F, 0xFF)
#define set_el_mask(x)       config_rcvr(0xFF, 0xFF, ((x)/(float)RAD_TO_DEG), -1.0F, -1.0F, -1.0F, 0xFF)
#define set_amu_mask(x)      config_rcvr(0xFF, 0xFF, -1.0F, (x),   -1.0F, -1.0F, 0xFF)
#define set_el_amu(el,amu)   config_rcvr(0xFF, 0xFF, ((el)/(float)RAD_TO_DEG), (amu), -1.0F, -1.0F, 0xFF)
#define set_amu_mask(x)      config_rcvr(0xFF, 0xFF, -1.0F, (x),   -1.0F, -1.0F, 0xFF)
#define set_pdop_mask(x)     config_rcvr(0xFF, 0xFF, -1.0F, -1.0F, (x), ((x)*0.75F), 0xFF)
#define set_foliage_mode(x)  config_rcvr(0xFF, 0xFF, -1.0F, -1.0F, -1.0F, -1.0F, (x))
EXTERN u08 single_sat;

EXTERN u08 osc_params;            // flag set if we are monkeying with the osc parameters
EXTERN float time_constant;       // oscillator control values
EXTERN float damping_factor;
EXTERN float osc_gain;
EXTERN float log_osc_gain;
EXTERN int gain_color;
EXTERN float min_volts, max_volts;
EXTERN float min_dac_v, max_dac_v;
EXTERN float jam_sync;
EXTERN float max_freq_offset;
EXTERN float initial_voltage;
EXTERN float osc_pid_initial_voltage;

EXTERN float user_time_constant;  // oscillator control values
EXTERN float user_damping_factor;
EXTERN float user_osc_gain;
EXTERN float user_min_volts, user_max_volts;
EXTERN float user_min_range, user_max_range;
EXTERN float user_jam_sync;
EXTERN float user_max_freq_offset;
EXTERN float user_initial_voltage;

EXTERN u16 critical_alarms;       // receiver alarms
EXTERN u16 minor_alarms;
EXTERN u08 have_alarms;
EXTERN u16 last_critical;
EXTERN u16 last_minor;

EXTERN u08 holdover_seen;         // receiver oscillator holdover control
EXTERN u08 user_holdover;
EXTERN u32 holdover;

EXTERN u08 osc_discipline;        // oscillator disciplining control
EXTERN u08 discipline;
EXTERN u08 last_discipline;
EXTERN u08 discipline_mode;
EXTERN u08 last_dmode;

EXTERN u08 rcvr_mode;             // receiver operating mode
EXTERN u08 last_rmode;
EXTERN u08 configed_mode;

EXTERN int gps_status;            // receiver signal status
EXTERN int last_status;

EXTERN u08 time_flags;            // receiver time status
EXTERN u08 last_time_flags;

EXTERN int set_utc_mode;          // gps or utc time
EXTERN int set_gps_mode;
EXTERN u08 temp_utc_mode;         // used when setting system time from the tbolt
EXTERN u08 timing_mode;           // PPS referenced to GPS or UTC time

EXTERN s08 seconds;               // receiver time info
EXTERN s08 minutes;
EXTERN s08 hours;
EXTERN int day;
EXTERN int month;
EXTERN int year; 
EXTERN s08 last_hours;
EXTERN s08 last_second;
EXTERN s16 utc_offset;
EXTERN int last_utc_offset;
EXTERN u16 gps_week;
EXTERN u32 tow;                   // GPS time of week
EXTERN long this_tow;

EXTERN int pri_seconds;           // time converted to local time zone
EXTERN int pri_minutes;
EXTERN int pri_hours;
EXTERN int pri_day;
EXTERN int pri_month;
EXTERN int pri_year;
EXTERN u32 pri_tow;
EXTERN int force_day;
EXTERN int force_month;
EXTERN int force_year;
EXTERN u08 fraction_time;
EXTERN u08 seconds_time;

EXTERN u08 time_zone_set;         // flag set if time zone is in effect
EXTERN int time_zone_hours;       // offset from GMT in hours
EXTERN int time_zone_minutes;     // time zone minute offset
EXTERN int time_zone_seconds;     // time zone seconds offset
EXTERN int tz_sign;               // the sign of the time zone offset
EXTERN int dst_ofs;               // daylight savings time correction
EXTERN u08 user_set_dst;
EXTERN char custom_dst[64];       // custom daylight savings time zone descriptor

EXTERN u08 use_gmst;              // greenwich mean sidereal time
EXTERN u08 use_lmst;              // local mean siderial time
EXTERN double st_secs;            // fractional seconds of sidereal time

#define TZ_NAME_LEN 5
EXTERN char std_string[TZ_NAME_LEN+1];   // standard time zone name
EXTERN char dst_string[TZ_NAME_LEN+1];   // daylisght savings time zone name
EXTERN char tz_string[TZ_NAME_LEN+1];    // current time zone name

#define USA         1
#define EUROPE      2
#define AUSTRALIA   3
#define NEW_ZEALAND 4
EXTERN int dst_area;

EXTERN int dsm[12+1];             // number of days to start of each month

//  version and production info
#define MANUF_PARAMS   0x01
#define PRODN_PARAMS   0x02
#define VERSION_INFO   0x04
#define INFO_LOGGED    0x80
EXTERN u08 have_info;

EXTERN u16 sn_prefix;            // from manuf_params message
EXTERN u32 serial_num;
EXTERN u16 build_year;
EXTERN u08 build_month;
EXTERN u08 build_day;
EXTERN u08 build_hour;
EXTERN float build_offset;

EXTERN u08 prodn_options;        // from prodn_params message
EXTERN u08 prodn_extn;
EXTERN u16 case_prefix;
EXTERN u32 case_sn;
EXTERN u32 prodn_num;
EXTERN u16 machine_id;
EXTERN u16 hw_code;

EXTERN u08 ap_major, ap_minor;   // from version_info message
EXTERN u08 ap_month, ap_day;
EXTERN u16 ap_year;
EXTERN u08 core_major, core_minor;
EXTERN u08 core_month, core_day;
EXTERN u16 core_year;


EXTERN u08 set_osc_polarity;     // command line settable options
EXTERN u08 user_osc_polarity;
EXTERN u08 osc_polarity;

EXTERN u08 set_pps_polarity;
EXTERN u08 user_pps_polarity;
EXTERN u08 pps_polarity;

EXTERN u08 pps_enabled;
EXTERN u08 user_pps_enable;

EXTERN u08 pps_rate;
EXTERN u08 user_pps_rate;

EXTERN double delay_value;
EXTERN double cable_delay;
EXTERN u08 user_set_delay;

EXTERN int user_set_osc;  // flags which osc params were on the command line
EXTERN float cmd_tc;      // the command line osc param values
EXTERN float cmd_damp;
EXTERN float cmd_gain;
EXTERN float cmd_dac;
EXTERN float cmd_minv;
EXTERN float cmd_maxv;

EXTERN u08 user_set_pps_float;
EXTERN u08 user_set_osc_float;
EXTERN u08 user_set_dac_float;
EXTERN u08 user_set_pps_plot;
EXTERN u08 user_set_osc_plot;
EXTERN u08 user_set_dac_plot;
EXTERN u08 user_set_adev_plot;
EXTERN u08 user_set_watch_plot;
EXTERN u08 user_set_clock_plot;

EXTERN u08 keyboard_cmd;  // flag set of command line option is being set from the keyboard
EXTERN u08 not_safe;      // flag set if user attempts to set a command line option
                          // ... from the keyboard that is not ok change.
                          // ... 1 = option not processed immediately
                          // ... 2 = option cannot be changed safely


#define SURVEY_SIZE  2000L         // default self-survey size
#define SURVEY_BIN_COUNT 60        // number of precise survey bins
EXTERN long survey_length;         // reported survey size
EXTERN u08 survey_save;            // reported survey save flag
EXTERN long do_survey;             // user requested survey size
EXTERN int survey_progress;        // completion percentage of the survey
EXTERN u08 precision_survey;       // do 48 hour precision survey
EXTERN u08 user_precision_survey;  // flag set if user requested precsion survey from command line
EXTERN u08 survey_done;            // flag set after survey completes - used to keep adev tables off screen
EXTERN u08 have_initial_posn;
EXTERN u08 surveying;
EXTERN long survey_minutes;
#define doing_survey (minor_alarms & 0x0020)
#define PRECISE_SURVEY_HOURS do_survey

EXTERN int end_time;               // daily exit time set
EXTERN int end_hh, end_mm, end_ss;
EXTERN int end_date;               // exit date set
EXTERN int end_month, end_day, end_year;
EXTERN long exit_timer;            // exit countdown timer mode
EXTERN long exit_val;              // initial exit countdown timer value

EXTERN u08 set_system_time;        // set flag to keep CPU clock set to GPS clock
EXTERN u08 time_set_char;          // char used to show time has been set
EXTERN u08 time_set_delay;         // used to make sure system is idle for cpu time set
EXTERN u08 force_utc_time;         // if set, make sure to use UTC time for system clock
EXTERN u08 set_time_daily;         // set CPU clock at 04:05:06 local time every day
EXTERN u08 set_time_hourly;        // set CPU clock at xx:05:06 local time every day
EXTERN u08 set_time_minutely;      // set CPU clock at xx:xx:06 local time every day
EXTERN long set_time_anytime;      // set time if CPU clock millisecond is off by over this value
EXTERN long time_sync_offset;      // milliseconds of delay in receiver time message
EXTERN int alarm_time;             // daily alarm time set
EXTERN int alarm_hh, alarm_mm, alarm_ss;
EXTERN int alarm_date;             // alarm date set
EXTERN int alarm_month, alarm_day, alarm_year;
EXTERN u08 sound_alarm;            // flag set to sound the alarm
EXTERN u08 single_alarm;           // set flag to play the alarm file only once

EXTERN u08 cuckoo;                 // cuckoo clock mode - #times per hour
EXTERN u08 singing_clock;          // set flag to sing songs instead of cuckoo
EXTERN u08 cuckoo_hours;           // set flag to cuckoo the hour on the hour
EXTERN u08 cuckoo_beeps;           // cuckoo clock signal counter
EXTERN u08 ticker;                 // used to flash alarm indicators

EXTERN long egg_timer;             // egg timer mode
EXTERN long egg_val;               // initial egg timer value
EXTERN u08 repeat_egg;             // repeat the egg timer

EXTERN int dump_time;              // daily screen dump time set
EXTERN int dump_hh, dump_mm, dump_ss;
EXTERN int dump_date;              // screen dump date set
EXTERN int dump_month, dump_day, dump_year;
EXTERN u08 dump_alarm;
EXTERN u08 single_dump;
EXTERN long dump_timer;            // egg timer mode
EXTERN long dump_val;              // initial egg timer value
EXTERN long dump_number;           // automatic screen dump counter
EXTERN u08 repeat_dump;            // repeat the screen dump
EXTERN u08 do_dump;                // flag set to do the screen dump

EXTERN int log_time;              // daily log dump time set
EXTERN int log_hh, log_mm, log_ss;
EXTERN int log_date;              // log dump date set
EXTERN int log_month, log_day, log_year;
EXTERN u08 log_alarm;
EXTERN u08 single_log;
EXTERN long log_timer;            // egg timer mode
EXTERN long log_val;              // initial egg timer value
EXTERN long log_number;           // automatic screen dump counter
EXTERN u08 repeat_log;            // repeat the screen dump
EXTERN u08 do_log;                // flag set to do the screen dump
EXTERN u08 log_wrap;              // write log on queue wrap

EXTERN u08 show_euro_ppt;          // if flag set,  show exponents on the osc values (default is ppb/ppt)
EXTERN u08 show_euro_dates;        // if flag set,  format mm/dd/yy dates in euro format
EXTERN u08 digital_clock_shown;
EXTERN u08 enable_timer;           // enable windows message timer

#define CHORD_FILE   "c:\\WINDOWS\\Media\\chord.wav"
#define NOTIFY_FILE  "c:\\WINDOWS\\Media\\notify.wav"
#define CHIME_FILE   "c:\\WINDOWS\\Media\\heather_chime.wav"
#define ALARM_FILE   "c:\\WINDOWS\\Media\\heather_alarm.wav"
#define SONG_NAME    "c:\\WINDOWS\\Media\\heather_song%02d.wav"
EXTERN u08 chime_file;    // flag set if user chime file exists
EXTERN u08 alarm_file;    // flag set if user alarm file exists



EXTERN int com_port;           // COM port number to use
EXTERN int lpt_port;           // LPT port number to use for temperature control
EXTERN unsigned port_addr;     // com chip I/O address (for DOS mode) 
EXTERN int int_mask;           // and interrupt level mask (for DOS mode)
EXTERN u08 com_running;        // flag set if com port has been initialized

EXTERN int com_q_in;           // serial port data queue (for DOS mode)
EXTERN int com_q_out;
EXTERN u32 com_errors;

EXTERN u08 read_only;          // if set, block TSIP commands that change the oscillator config
EXTERN u08 just_read;          // if set, only read TSIP data,  don't do any processing
EXTERN u08 no_send;            // if set, block data from going out the serial port
EXTERN u08 process_com;        // clear flag to not process the com port data on startup
                               // useful if just reviewing a log file and no tbolt is connected
                               // and your serial port has other data comming in

EXTERN int break_flag;         // flag set when ctrl-break pressed
EXTERN u08 system_idle;        // flag set when system is not processing lots of messages
EXTERN u08 first_msg;          // flag set when processing first message from com port
                               // (used to get in sync to data without flagging an error)

#define USER_CMD_LEN (256+1)
EXTERN u08 user_init_cmd[USER_CMD_LEN];  // user specified init commands
EXTERN u08 user_init_len;
EXTERN u08 user_pps_cmd[USER_CMD_LEN];   // user_specified pps commands
EXTERN u08 user_pps_len;


#define LLA_X         (LLA_COL+LLA_SIZE/2-1)   // center point of lla plot
#define LLA_Y         (LLA_ROW+LLA_SIZE/2-1)   
#define LLA_DIVISIONS 10       // lla plot grid divisons
EXTERN double LLA_SPAN;        // feet or meters each side of center
EXTERN int LLA_SIZE;           // size of the lla map window (for DOS should be a multiple of TEXT_HEIGHT)
EXTERN int LLA_MARGIN;         // lla plot grid margins in pixels
EXTERN int lla_step;           // pixels per grid division
EXTERN int lla_width;          // size of data area in the grid
EXTERN int LLA_ROW;            // location of the lla map window
EXTERN int LLA_COL;
EXTERN int zoom_lla;           // if set,  zoom the lla plot (and value is the margin in pixels)
EXTERN u08 plot_lla;           // flag set when plotting the lat/lon/alt data
EXTERN u08 graph_lla;          // flag set when graphing the lat/lon/alt data
EXTERN u08 show_fixes;         // set flag to enter 3D fix mode and plot lla scatter
EXTERN u08 fix_mode;           // flag set if receiver is not in overdetermined clock mode
EXTERN u08 reading_lla;        // set flag if reading lla log file
EXTERN u08 reading_signals;    // set flag if reading signal level log file
EXTERN u32 signal_length;      // number of signal level samples logged
EXTERN u08 check_precise_posn; // set flag to do repeated single point surveys
EXTERN u08 check_delay;        // counter to skip first few survey checks
EXTERN u32 precision_samples;  // counter of number of surveyed points
EXTERN double precise_lat;     // the precise position that we are at (or want to be at)
EXTERN double precise_lon;
EXTERN double precise_alt;

EXTERN int WATCH_ROW;     // where the AZEL plot goes, if not in the plot area
EXTERN int WATCH_COL;     // ... also where to draw the analog watch
EXTERN int WATCH_SIZE;

EXTERN int AZEL_SIZE;     // size of the az/el map window (for DOS should be a multiple of TEXT_HEIGHT)
EXTERN int AZEL_ROW;      // location of the az/el map window
EXTERN int AZEL_COL;
EXTERN int AZEL_MARGIN;   // outer circle margin in pixels
#define SHARED_AZEL  (plot_azel && ((AZEL_ROW+AZEL_SIZE) >= PLOT_ROW))
#define AZEL_BOTTOM  (((AZEL_ROW+AZEL_SIZE+TEXT_HEIGHT-1)/TEXT_HEIGHT)*TEXT_HEIGHT)

#define AZEL_X      (AZEL_COL+AZEL_SIZE/2-1)   // center point of plot
#define AZEL_Y      (AZEL_ROW+AZEL_SIZE/2-1)   
#define AZ_INCR     5                    // drawing resolution of the az circles
#define EL_GRID     30                   // grid spacing in degrees
#define AZ_GRID     30   
#define AZEL_COLOR  GREY                 // grid color (turns red if nothing tracked)
#define AZEL_ALERT  DIM_RED

EXTERN u32 this_track;                   // bit vector of currently tracked sats
EXTERN u32 last_track;                   // bit vector of previously tracked sats
EXTERN u32 this_used;                    // bit vector of currently used sats
EXTERN u32 last_used;                    // bit vector of previously used sats
EXTERN u32 last_q1, last_q2, last_q3, last_q4;  // tracks sats that are in each quadrant
EXTERN u32 this_q1, this_q2, this_q3, this_q4;
EXTERN u08 shared_plot;                         // set flag if sharing plot area with az/el map


EXTERN u08 show_min_per_div;  // set flag to show display in minutes/division
EXTERN u08 showing_adv_file;  // flag set if adev file has been loaded
EXTERN u08 old_sat_plot;
EXTERN u08 old_adev_plot;
EXTERN u08 old_keep_fresh;
EXTERN float old_adev_period;
EXTERN u08 debug_screen;
EXTERN char user_view_string[32+1];

EXTERN int dac_dac;          // osc autotune state
EXTERN float gain_voltage;   // initial dac voltage


#define MAX_TEXT_COLS (2560/8)
#define UNIT_LEN 22            // unit name length
#define SLEN  128              // standard string length
EXTERN char blanks[MAX_TEXT_COLS+1]; // 80 of them will blank a screen line
EXTERN char out[MAX_TEXT_COLS+1];    // screen output strings get formatted into here
EXTERN char unit_name[UNIT_LEN+1];   // unit name identifier
EXTERN char clock_name[30+1];
EXTERN char clock_name2[30+1];
EXTERN char *help_path;

EXTERN char plot_title[SLEN+80+1];   // graph title string ////!!!!80
EXTERN char *debug_text;             // used to display debug info in the plot area
EXTERN char *debug_text2;
EXTERN int title_type;
EXTERN u08 greet_ok;
#define NONE      0
#define GREETING  1
#define USER      2
#define OTHER     3

EXTERN char edit_buffer[SLEN+1];     // the text string the user is typing in
EXTERN u08 getting_string;           // the command that will use the stuff we are typing
EXTERN u08 edit_ptr;                 // the edit cursor column
EXTERN int EDIT_ROW, EDIT_COL;       // where to show the edit string

EXTERN char *ppb_string;             // parts per billion identifier
EXTERN char *ppt_string;             // parts per trillion identifier

#ifdef DEBUG_TEMP_CONTROL
   EXTERN char dbg[256];
   EXTERN char dbg2[256];
#endif


//
// function definitions
//
void line(COORD x1,COORD y1, COORD x2,COORD y2, u08 color);
void thick_line(COORD x1,COORD y1, COORD x2,COORD y2, u08 color, u08 thickness);
void dot(COORD x,COORD y, u08 color);
void vidstr(COORD row, COORD col, u08 color, char *s);
u08 get_pixel(COORD x,COORD y);

void find_endian(void);
void init_hardware(void);
void ems_init(void);
u16 ems_free(u16 pid);
void shut_down(int reason);
void error_exit(int reason, char *s);
void init_com(void);

void sendout(u08 val);
void bios_char(u08 c, u08 attr);
void scr_rowcol(COORD row, COORD col);
bool check_incoming_data(void);

void init_screen(void);
void config_screen(void);
void new_screen(u08 c);
int dump_screen(int invert, char *fn);
int dump_bmp_file(int invert);
int dump_gif_file(int invert, FILE *file);
void redraw_screen(void);
void erase_cursor_info(void);
void show_cursor_info(S32 i);
void init_dos_mouse(void);


void draw_plot(u08 refresh_ok);
void update_plot(int draw_flag);
void erase_plot(int full_plot);
void erase_help(void);
void erase_screen(void);
void scale_plots(void);
void show_title(void);
int  show_mouse_info(void);
void show_stat_info(void);
void vchar(int xoffset,int yoffset, u08 erase, u08 color, u08 c);
void vchar_string(int row, int col, u08 color, char *s);
void reset_vstring(void);
float fmt_temp(float t);
void restore_plot_config(void);
void filter_spikes(void);
void adev_mouse(void);

void alloc_queues(void);
void alloc_plot(void);
void alloc_adev(void);
void alloc_fft(void);
void alloc_gif(void);
void reset_queues(int queue_type);
void new_queue(int queue_type);
void write_q_entry(FILE *file, long i);
long find_event(long i, u08 flags);
long goto_event(u08 flags);
void put_plot_q(long i, struct PLOT_Q q);
long next_q_point(long i, int stop_flag);
struct PLOT_Q get_plot_q(long i);
struct PLOT_Q filter_plot_q(long i);

void do_gps(void);
void get_pending_gps(void);
void wait_for_key(void);
void init_messages(void);
void get_tsip_message(void);
u08 tsip_end(u08 report_err);
void unknown_msg(u16 msg_id);
void debug_stream(unsigned c);

void set_survey_params(u08 enable_survey,  u08 save_survey, u32 survey_len);
void start_self_survey(u08 val);
void stop_self_survey();
void request_rcvr_info(void);
void set_filter_config(u08 pv, u08 stat, u08 alt, u08 kalman, int save);
void request_cold_start(void);
void set_discipline_mode(u08 mode);
void save_segment(u08 segment);
void request_timing_mode(void);
void set_timing_mode(u08 mode);
void get_timing_mode();
void request_factory_reset(void);
void request_version(void);
void show_version_header(void);
void set_pps(u08 pps_enable,  u08 pps_polarity,  double cable_delay,  float threshold, int save);
void request_warm_reset(void);
void set_osc_sense(u08 mode, int save);
void set_dac_voltage(float volts);
void calc_osc_gain();
void request_discipline_params(u08 type);
void request_all_dis_params(void);
void set_discipline_params(int save);
void request_survey_params(void);
void request_sat_list(void);
void request_primary_timing();
void request_secondary_timing();
void request_pps();
void do_fixes(int mode);
void start_3d_fixes(int mode);
void config_rcvr(u08 mode, u08 dynamics, float elev, float amu, float pdop_mask, float pdop_switch, u08 foliage);
void request_rcvr_config();
void set_single_sat(u08 prn);
void set_io_options(u08 posn, u08 vel, u08 timing, u08 aux);
void start_self_survey(u08 val);
void save_segment(u08 segment);
void saw_ebolt(void);
void show_osc_params(int row, int col);

void start_3d_fixes_survey(void);
void start_precision_survey(void);
void stop_precision_survey(void);
void abort_precise_survey(void);
void save_precise_posn(int force_save);
int add_to_bin(double val,  double bin[],  int bins_filled);
void plot_lla_point(int color);
void precise_check(void);
int add_survey_point(void);
void analyze_hours(void);
void calc_precise_lla(void);
void update_precise_survey(void);

void plot_lla_axes(void);
void lla_header(char *s, int color);
void set_lla(float lat, float lon, float alt);
void format_lla(double lat, double lon,  double alt,  int row, int col);
void erase_lla(void);
void clear_signals();
void clear_lla_points();
void update_mouse(void);
void hide_mouse(void);
u08 mouse_hit(COORD x1,COORD y1,  COORD x2,COORD y2);
void dos_erase(int left,int top,  int right,int bot);
void open_lla_file(void);
void close_lla_file(void);

void draw_azel_plot(void);
void update_sat_trails(void);
void check_azel_changes(void);
void erase_azel(void);
void dump_trails(void);
void plot_3d_sig(int az, int el);
void log_signal(float azf, float elf, float sig_level, int amu_flag);
void dump_signals(char *fn);
float amu_to_dbc(float sig_level);
float dbc_to_amu(float sig_level);
void label_circles(int row);

void format_plot_title(void);
void show_log_state(void);
void open_log_file(char *mode);
void close_log_file(void);
void sync_log_file(void);
void write_log_readings(FILE *file, long i);
void dump_log(char *name, u08 dump_size);
void write_log_leapsecond(void);
void write_log_changes(void);
void write_log_error(u16 number, u32 val);
void write_log_utc(s16 utc_offset);
void log_posn_bins(void);
void log_saved_posn(int type);
void log_adevs(void);
void show_satinfo(void);
int reload_log(char *fn, u08 cmd_line);
void time_check(int reading_log, u16 interval, int hh,int mm,int ss);
double GetMsecs(void);

void need_time_set();
void init_dsm(void);
void bump_time(void);
void set_time_zone(char *s);
int adjust_tz(void);
void calc_dst_times(char *s);
int dst_offset();
void setup_calendars(void);
void calc_seasons(void);
void calc_moons(int year, int month);
double jdate(int y, int m, int d);
double gmst(int yy, int mo, int dd, int hh, int mm, int ss, int app_flag);
double lmst(int yy, int mo, int dd, int hh, int mm, int ss, int app_flag);

int do_kbd(int c);
void kbd_help(void);
int are_you_sure(int c);
int sure_exit(void);
u08 toggle_value(u08 x);
int option_switch(char *arg); 
int read_config_file(char *name, u08 local_flag);
void config_options(void);
void save_cmd_bytes(char *arg);
void command_help(char *where, char *s, char *cfg_path);
void set_defaults(void);
double hp53131 (char *line);
void close_script(u08 close_all);
int get_script(void);
int adjust_screen_options(void);
void toggle_plot(int id);
void read_calendar(char *s, int erase);
int calendar_count(void);
void read_signals(char *s);
int good_el_level(void);
float set_el_level(void);

u08 string_param(void);
void edit_ref(void);
void edit_scale(void);
int edit_error(char *s);
int start_edit(int c, char *prompt);
u08 edit_string(int c, int row);
void set_osc_units(void);

void check_end_times(void);
void cuckoo_clock(void);
void alarm_clock(void);
void silly_clocks(void);
int  show_greetings(void);
void calc_greetings(void);
void set_cpu_clock(void);
void draw_watch(void);
int show_digital_clock(int row, int col);
char *fmt_date();

float scale_adev(float val);
void reset_adev_bins(void);
void reload_adev_info(void);
void reload_adev_queue(void);
void force_adev_redraw(void);
long next_tau(long tau, int bins);
void find_global_max(void);

void plot_review(long i);
void do_review(int c);
void end_review(u08 draw_flag);
void zoom_review(long i, u08 beep_ok);
void kbd_zoom(void);
void goto_mark(int mark);
void reset_marks(void);
void adjust_view(void);
void new_view(void);
int edit_user_view(char *s);

void SetDtrLine(u08 on);
void SetRtsLine(u08 on);
void SetBreak(u08 on);
void SendBreak();

#ifdef TEMP_CONTROL
   void apply_heat(void);
   void apply_cool(void);
   void hold_temp(void);
   void enable_temp_control();
   void disable_temp_control();
   void control_temp(void);
   unsigned init_lpt(void);
   void update_pwm(void);

   EXTERN u08 do_temp_control;        // if flag is set,  attempt to control unit temperature
   EXTERN u08 temp_control_on;
   EXTERN float  desired_temp;        // desired unit temperature
   EXTERN int lpt_addr;
   EXTERN u08 lpt_val;
#else
   #define update_pwm()
#endif

void calc_k_factors(void);
void show_pid_values();
void clear_pid_display();
void set_default_pid(int num);
int calc_autotune(void);
void analyze_bang_bang();
void reset_osc_pid(int kbd_cmd);
void config_res_t_plots();
void set_pps_mode(int mode);
void request_pps_mode();

EXTERN float test_heat, test_cool;
EXTERN int test_marker;
EXTERN int bang_bang;

EXTERN u08 pid_debug;
EXTERN int crude_temp;
EXTERN float P_GAIN;
EXTERN float D_TC;
EXTERN float FILTER_TC;
EXTERN float I_TC;
EXTERN float FILTER_OFFSET;
EXTERN float KL_TUNE_STEP;
EXTERN float OLD_P_GAIN;

#define PRE_Q_SIZE  60
EXTERN int osc_prefilter;   // OSC pid pre filter (input values to the PID) depth
EXTERN int opq_in;
EXTERN int opq_count;

#define POST_Q_SIZE 600
EXTERN int osc_postfilter;  // OSC pid post filter (between PID and DAC) depth
EXTERN int post_q_count;   
EXTERN int post_q_in;


void calc_osc_k_factors(void);
void show_osc_pid_values();
void clear_osc_pid_display();
void set_default_osc_pid(int num);
void new_postfilter(void);

EXTERN u08 osc_pid_debug;
EXTERN double OSC_P_GAIN;
EXTERN double OSC_D_TC;
EXTERN double OSC_FILTER_TC;
EXTERN double OSC_I_TC;
EXTERN double OSC_FILTER_OFFSET;
EXTERN double OSC_KL_TUNE_STEP;
EXTERN double OLD_OSC_P_GAIN;

void enable_osc_control();
void disable_osc_control();
void osc_pid_filter();
void control_osc();
EXTERN u08 osc_control_on;
EXTERN double osc_rampup;

EXTERN float min_clock;
EXTERN float clock_sum;
EXTERN float clock_count;
EXTERN double avg_pps;
EXTERN double avg_osc;
EXTERN int pps_bin_count;

EXTERN int monitor_pl;              // monitor power line freq
EXTERN unsigned long pl_counter;


EXTERN float dac_drift_rate;
EXTERN u08 show_filtered_values;
EXTERN float d_scale;

EXTERN double trick_scale;
EXTERN int    first_trick;
EXTERN double trick_value;

#define RMS  1
#define AVG  2
#define SDEV 3
#define VAR  4
EXTERN int stat_type;
EXTERN char *stat_id;

struct PLOT_DATA {
   char *plot_id;      // the id string for the plot
   char *units;        // the plot data units
   float ref_scale;    // scale_factor is divided by this to get the units of
                       // measurement to manipulate the scale factor in
   u08 show_plot;      // set flag to show the plot
   int slot;           // the plot units header slot position number
   u08 float_center;   // set flag to allow center line ref val to float
   int plot_color;     // the color to draw the plot in

   float plot_center;  // the calculated center line reference value;
   u08   user_scale;   // flag set if user set the scale factor
   float scale_factor; // the plot scale factor
   float data_scale;   // data plot scale factor
   float invert_plot;  // if (-1.0) invert the plot 

   float sum_x;        // linear regression stuff
   float sum_y;        
   float sum_xx;      
   float sum_yy;       
   float sum_xy;
   float stat_count;
   float a0;
   float a1;
   float drift_rate;   // drift rate to remove from plot
   float sum_change;

   float min_val;      // min and max values of the display window
   float max_val;
   int   last_y;       // last plot y-axis value
   u08 old_show;       // previous value of show_plot flag (used to save/restore options when reading a log file)
   u08 show_trend;     // set flag to show trend line
   u08 show_stat;      // set flag to statistic to show
   int last_trend_y;
};
extern struct PLOT_DATA plot[];
extern int slot_column[];  // the plot header slot screen column
extern int slot_in_use[];  // the plot being shown in the slot

EXTERN int num_plots;      // how many plots we are keeping data for
EXTERN int extra_plots;    // flag set if any of the extra plots are enabled
EXTERN int selected_plot;       // the currently selected plot
EXTERN int last_selected_plot;  // the last selected plot


#ifdef FFT_STUFF
   #define BIGUN HUGE

   typedef struct {   /* COMPLEX STRUCTURE */
      float real, imag;
   } COMPLEX;
   int  logg2(long);
   long calc_fft(int id);
   void set_fft_scale();

   EXTERN long max_fft_len;    // must be a power of two
   EXTERN long fft_length;     // must be a power of 2
   EXTERN long fft_scale;      // expand power specta bins to this many pixels wide
   EXTERN float fps;           // freqency per sample
   EXTERN u08 fft_db;          // if set, calculate FFT results in dB
   EXTERN long fft_queue_0;
   EXTERN float BIGUN *tsignal;     // the fft input data
   EXTERN COMPLEX BIGUN *w;         // the fft w array
   EXTERN COMPLEX BIGUN *cf;        // the fft cf array
   EXTERN COMPLEX BIGUN *fft_out;   // the fft results
#endif

#define first_extra_plot slot_in_use[FIRST_EXTRA_PLOT]


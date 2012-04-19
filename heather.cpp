//
// Thunderbolt TSIP monitor
//
// Copyright (C) 2008,2009,2010,2011,2012 Mark S. Sims - all rights reserved
// Win32 port by John Miles, KE5FX (jmiles@pop.net)
// Temperature and oscillator control algorithms by Warren Sarkinson
// Adev code adapted from Tom Van Baak's adev1.c and adev3.c
// Incremental adev code based upon John Miles' TI.CPP
// Easter and moon phase code from voidware.com
// Nortel NTGS55A recever wakeup research by Sam Sergi
//
// Note: This program calculates adevs based upon the pps and oscillator
//       error values reported by the unit.  These values are not
//       derived from measurements made against an independent reference
//       (other than the GPS signal) and may not agree with adevs calculated
//       by measurements against an external reference signal.
// 
//
// This file contains most of the operating system dependent routines, 
// initialization routines,  serial port I/O,  and the screen I/O and 
// plotting routines.
//
//
// rev 0.01 - 04 Jul 08 - initial release
// rev 0.02 - 13 Jul 08 - lotsa updates
// rev 0.03 - 29 Jul 08 - changed and improved logging
// rev 0.04 - 01 Jan 09 - added logging of changes in UTC offset
// rev 0.05 - 27 Jan 09 - added /v command line option to set screen size
// rev 0.06 - 01 Feb 09 - tweaked John's Windows mods to allow DOS/WINDOWS compiles
// rev 0.07 - 01 Feb 09 - added support for extended adev / plot queue sizes
//                      - added support for time tags in the plot queue
// rev 0.08 - 06 Feb 09 - John added the mouse support to show the time at the cursor
//                        in the plot window.  
//                      - Added #define SMALL_DOS to remove the date from the plot queue entries.
//                      - Blanked the date from the plot header when exiting scroll mode
//
//
// rev 1.00 - 09 Feb 09 - Fixed adev plot scaling for 1280x960 screens.
//                      - Removed temperature spike logging.
//                      - Changed order that adev tables are written to the log file
//                      - Added /m option to adjust plot scale factors.
// rev 1.01 - 13 Feb 09 - Fixed temperature plot scaling.
//                      - Fixed user holdover mode and pps indicators when the mode 
//                        was already active when the program started.   
//                      - Highlighted disabled filters in YELLOW.
// rev 1.02 - 20 Feb 09 - Added keyboard lockout command.
//                      - Added plot interval display.
//                      - Added confirmation for ESC to exit program.
//                      - Disabled DOS version com port timeout check (was corrupting packets)
//                      - Added support for disabling plot by setting queue_interval to 0.0
// rev 1.03 - 22 Feb 09 - Added /r command for reading data from log file.
//                      - Added /w command for setting name of log file to write.
//                      - Added /j command for changing ADEV period 
//                      - Added support for disabling adevs by setting adev_period to 0
//                      - Tweaked positioning of DOS help screen and plot erasing.
// rev 1.04 - 24 Feb 09 - Flagged time interval gaps in plot data.
//                      - Added logging of time/packet errors.
//                      - Added /e command to disabe logging of most errors.
//                      - Rearranged the furniture...
// rev 1.05 - 12 Mar 09 - Fixed Windows mode screen updates every second (was updating every plot interval)
//                      - Added /n command to disable plot/adev updates (useful if reading
//                        in a log file and you don't want incoming data to update
//                        the screen).  Also 'u' key toggles update mode.
//                      - Changed survey command to /ss=# (to reduce chances of
//                        inadvertent self survey.
// rev 1.06 - 13 Mar 09 - Refreshed Windows screen when toggling pause mode (so
//                        ADEV tables show properly).
//                      - Exited plot review mode when unpausing screen updates.
//                      - Exited pause mode when exiting plot review mode.
//
//
// rev 2.00 - 26 Mar 09 - Lots of major and minor changes....
//                      - Split code into three files: a header include file,
//                        main code,  gps controller specific (mostly) code
//                      - Cleaned up comments, etc.
//                      - Added version info to Windows title bar.
//                      - The DOS version now operates in 16 color mode
//                        (previously it was in 256 color mode)
//                      - All queue accesses now go through function calls that
//                        get or put a queue entry.  This allows EMS memory support.
//                      - The DOS version supports using EMS memory for the 
//                        plot queue (16 meg allows over 8 days of data at 
//                        1 second intervals).  If EMS memory is used for the 
//                        plot queue, enough DOS memory is available to
//                        handle around 56,000 adev queue entries.  
//                      - Default adev queue size is now 33,000 entries.  This 
//                        allows HDEVs, MDEVs and TDEVs to tau=10000.  If DOS
//                        mode and no EMS memory seen, it is still 22,000 entries.
//                      - Added mouse support to DOS version
//                      - Left clicking on a column in the graphs will enter
//                        scroll mode and zoom the plot in or out.  The selected 
//                        column will be centered in the plot window (if possible).
//                      - Added auto scaling of graphs.   Individual graphs can 
//                        be controlled from the keybord.  Auto scaled graphs
//                        show the scale factor preceeded by a '~'.  Fixed
//                        scale factors are preceeded by an '='.
//                      - All graphs can now have a floating center line
//                        reference value or the user can set a fixed value.
//                        Floating reference values are shown preceeded by
//                        a '~'.   Fixed refernce values are preceeded by an '='.
//                      - Added support for downsampling the plot queue data to
//                        the plot window.  This allows switching from a short
//                        term view of the data (seconds) to a long term (daily)
//                        view.  Keyboard commands 'v', 'x',  and 'y'
//                        control the view.  V=user set view interval,
//                        X=1 hr/div   Y=1 day/screen (also divides the plot
//                        into 24 divisions)  
//                      - Note that if downscaling is in use that the 
//                        display of time skip markers, etc is not accurate.
//                        Markers or other events that do not happen on a
//                        displayed sample will not be visible because all
//                        non-displayed samples are skipped over.
//                      - Added ability to process strings/parameters for
//                        keyboard commands.
//                      - Queue update interval can be set from the keyboard
//                      - Queue contents can be written to a log file
//                        (the normal log command writes the log as the
//                        data comes in).  Note that the *tow* field in logs
//                        generated from queue data is not the official GPS
//                        time-of-week,  but just a sequential number.
//                      - Log files can be read in from the keyboard or 
//                        command line.  
//                      - Changed Windows mode default plot queue size to 7 days.
//                        (or 30 days with /q).  Default size for DOS version
//                        with EMS memory is 3 days.  One day of data takes
//                        around 2 meg of memory.
//                      - Added support for calculating ADEVS, HDEVS, MDEVS,
//                        and TDEVS.  (MDEVs and TDEVs can also be calculated
//                        with non-overlapping data which is not usually done
//                        for those measurements and gives somewhat bogus data).
//                      - Several keyboard and command line option select
//                        characters have changed.  Review the keybord help
//                        screen and the "heather /?" screen for details.
//                      - The meanings of the plot scrolling characters have
//                        changed (hopefully for the better).  
//                        UP arrow and PageUP means move the start of the
//                        plot forward in time.  DOWN arrow and PageDN, says
//                        to move back in time.
//                        For left arrow and '<' or right arrow and '>'
//                        the meaning is to slide the plot in the direction of
//                        the arrow.
//                      - Added /g# command toggling graph enables
//                        /ga (adevs)   /gc (satellite count)
//                        /gk (constellation change tick marks - undocumented feature) 
//                        /gd (dac voltage)  /go (osc)  /gp (pps)  /gt (temperature) 
//                        /gs (time skip tick marks)  /ge (shows skips as big red lines)
//                        The old /o and /b commands no longer do this.
//                      - The old /g and /u commands for GPS and UTC time
//                        are now /tg and /tu
//                      - You can set the temperature scale to Fahrenheit with
//                        /tf,  Kelvin with /tk,  Rankine with /tr,  Delisle
//                        with /td,  Newton with /tn,  Romer with /to, and
//                        Reaumur with /te,  Celsius with /tc
//                      - You can set the altitude scale to feet with /t'
//                        or meters with /tm
//                      - The DOS version command for forcing text only mode is
//                        now /vt (was just /t)
//                      - The /0 command disables serial port (useful if reading
//                        log files on a system without a Thunderbolt, but with
//                        some other conflicting equipment on the serial port).
//                      - Added '&' keyboard command for changing oscillator
//                        control paramters.
//                      - Added ability to set DAC voltage when disabling
//                        oscillator disciplining.
//                      - Who knows what else changed...  beer was involved...
//
//
// rev 3.00 - 01 Feb 10 - Split code into 5 files (header, main, user interface,
//                        misc major functions,  GPS controller).
//                      - Note that when Heather says to "press any key" that
//                        this means normal typing keys...  some function and
//                        special purpose keys are not recognized.
//                      - Validated more TSIP message data values.
//                      - Improved Windows version operation if no Tbolt is
//                        connected.  Note that if you start the program in a
//                        windowed mode and then go to full screen mode,  you
//                        may need to press the space bar to get the screen
//                        to draw the first time.
//
//                      - Numerous screen formatting tweaks.  Oh, so many 
//                        screen formatting tweaks.  So very, very many screen 
//                        formatting tweaks.   Did I mention there were a lot
//                        of screen formatting tweaks?
//                      - Major restructuring of the plot code so that it is
//                        driven from a table of plot parameters.  This makes
//                        it fairly easy to add more plots.  The extra plots
//                        are selected by the commands G1..G9.  When extra plots
//                        are selected their zero ref/scale factor headers appear
//                        in order from most recently selected to oldest.  Only
//                        as many headers as will fit on the screen are shown
//                        (but all selected plots are drawn).  The selected 
//                        statistic of the most recently selected extra plot 
//                        is also shown.
//                      - Deleted the M (modify graph scale factor) and Z 
//                        (modify graph zero reference) keyboard commands.
//                        These functions are now accessed via the Gx command
//                        (where X is the plot that you want to modify).
//
//                      - Added Graph menu option (L) to display a linear
//                        regression trend line of the parameter.
//
//                      - Added Graph menu option (F) to calculate a FFT of the
//                        selected plot.  The data to be analyzed starts at the
//                        beginning of the plot window and when the max fft size
//                        points or the end of the plot data are reached.  The
//                        data is sampled from the plot data at the viewing
//                        interval.  The FFT data is placed in plot G4.  The max
//                        FFT size is set by the /qf[=#] command line option.
//                        (1024 points if not given,  /qf is 4096 points,
//                        otherwise it is whatever is set (must be a power of 2).
//                        Note that a FFT requires the input data to be bandwidth
//                        limited to below the sample frequency or else you will.  You should set
//                        get spurious values due to aliasing.  Set the display
//                        display filter count to at least the value shown as
//                        the "view interval" in minutes/division.
//
//                      - Tweaked formatting of the plot area so the vertical
//                        size is a multiple of VERT_MAJOR and the horizontal
//                        size is a multiple of HORIZ_MAJOR.
//                      - Text only mode no longer uses video page swapping
//                        to switch between data and help screens.  This
//                        permits the text only mode code to be usable as a
//                        graphics 640x480 mode.
//                      - 800x600 and below mode now display the help info
//                        in full screen mode.
//
//                      - Fixed '_' edit cursor in WINDOWS version
//                      - Added editing to the string input routine.  LEFT and
//                        RIGHT arrows move the cursor within the string. INSERT
//                        toggles insert mode.  HOME moves to the start of the
//                        string.  END moves the end of the string.  DEL deletes
//                        the character at the cursor.  DOWN arrow deletes to 
//                        the end of the line.  UP arrow deletes to the start
//                        of the line. BACKSPACE deletes the character before
//                        the cursor.
//                      - If a keyboard command suggests an input value and
//                        the first character that you enter is a not an  
//                        editing character,  the suggestion is erased.
//                      - You get a beep whenever a write to EEPROM is requested.
//                        If you hear lots of unexpected beeps,  something is 
//                        probably wrong and you may be wearing out the EEPROM.
//                      - Changing any of the PPS/OSC/Cable delay parameters
//                        from the keyboard now stores the values in EEPROM
//                        (segment 6).  The /C, /P, /+, /- or /^ command
//                        line options do not set the EEPROM values.
//                      - You can append an 'f' or 'm' to the cable delay value
//                        to set the delay in feet or meters of 0.66 velocity
//                        factor coaxial cable.
//                      - Command line options that toggle an option on/off may
//                        be followed by a "1" or "0" to set the flag value 
//                        directly. e.g. Use /go1 to force the osc plot on, /go0
//                        to turn it off, or use /go to toggle the osc plot flag.
//                        Note that the /gb and /gm commands are not toggleable
//                        options.  They always turn the azel map on.
//                      - Fixed bug in send_dword argument (was double, 
//                        now is u32).  This affected the self-survey command
//                        and possibly the set dac value command.
//                      - If a self survey is not in process,  the greyed out 
//                        survey display is replaced with the oscillator
//                        disciplining parameters.  
//                      - The satellite elevation mask and signal (AMU) mask
//                        values are displayed by the filter settings (if there
//                        is room on the screen).
//                      - Added ability to set the foliage mode,  PDOP, and
//                        receiver movement dynamics filters.  The PDOP 2D/3D
//                        switchover level is always set to 0.75*PDOP level
//                        value.  These filter values are set from the 'F' menu.
//                        Note that the tbolt firmware automatically disables
//                        the A)ltitude filter is A)ir movement dynamics is 
//                        selected.
//                      - Made exit program key sequence "ESC y" (was ESC ESC).
//                        Use /ke command line option to allow ESC ESC.
//                      - Made mouse disable command line option /km.
//                      - Made beep disable command line option /kb.
//                      - Defaults to not exit on com port errors.
//                      - Added /kc command line option to disable all writes
//                        to the configuration EEPROM.
//                      - Rewrote the serial port i/o routines to not use the
//                        COMBLOCK.H routines.  This greatly improved the
//                        performance and reliabilty.  Virtually eliminated
//                        corrupted messages and time skips.
//                      - Fixed mouse operation if Windows version is running
//                        in the (rather unreadable) downscaled window (when
//                        requested screen size is > physical screen size).
//                      - Mouse no longer responds when Heather is not on top.
//                        (ok, when Heather's window is not on top).
//                      - All temperature info is kept in Celsius (including
//                        values written to the log file.  It is converted
//                        to the user selected temperatue scale only when
//                        displayed (previous versions converted the temp to
//                        the user specified scale when it was read from the
//                        Tbolt)
//                      - Added #OSC_GAIN value to log files.  This allows the
//                        computation of osc drift and tempco from logged data
//                        acquired on a unit that does not have the same osc 
//                        gain (or that is not connected to a tbolt).  If the
//                        osc gain has been loaded from a log file,  it is
//                        shown in yellow.
//                      - log files written from queue data are written in
//                        write mode (not append mode).  Writing queue data to
//                        a log file while also writing live data to a log file
//                        may cause problems...  it seems to work,  but I have
//                        no idea why...
//                      - You can write filtered queue data to a log file.
//                        Use the WF command to enable/disable writing filtered
//                        data.  You also have to have the display filter set.
//                      - if writing the plot window queue data to a log file
//                        AND the plot queue is full and wraping AND the queue
//                        updates are not paused AND the plot window covers
//                        all of the queue data,  then there will be a glitch
//                        at the beginning of the log output file.  Several
//                        seconds of the latest data will appear at the start 
//                        of the log data.
//
//                      - Windows version added the ability to talk to a remote 
//                        unit over a TCP/IP link.  Use /ip=ip_addr command line
//                        option to enable TCP/IP access.  The remote unit
//                        must be connected to a machine running the SERVER.EXE
//                        program.
//                      - Added '?' keyboard command to display the command 
//                        line help dialog.
//                      - Both the TCP/IP link and help dialog features use
//                        a windows message timer to keep the program running
//                        if the help dialog box is active or the screen is
//                        being dragged, etc.   This timer has the potential to
//                        cause very intermittent, random "unhandled exception"
//                        aborts.   You can disable the timer feature with the
//                        /kt command.  Without the timer,  the program will
//                        not process TSIP messages while the help dialog is
//                        being displayed.  Also you can drop the TCP/IP 
//                        connection if the window is being dragged or is
//                        minimized.
//
//                      - Added &a keyboard command to automatically set
//                        the oscillator disciplining parameters to their
//                        "optimum" values.   Wait for the unit to stabilize
//                        with (relatively) steady DAC, OSC, and PPS values
//                        then issue the '&a' command.  Heather will tickle
//                        your oscillator and determine good values for the
//                        osc gain and initial dac voltage.  The time constant
//                        will be set to 500 seconds,  and the damping to 1.0
//                        Also the AMU (signal level) mask will be set to 1.0
//                        and the satellite elevation mask to 25 degrees.
//                        The values will be written to the configuration EEPROM.
//                        You can then manually change any of the parameters
//                        that may not suit your needs.
//                      - the & keyboard char brings up the oscillator 
//                        parameter and satellite max signal level display 
//                        screens.  To return to normal display mode
//                        do a GR command to redraw the screen or press SPACE
//                        twice to toggle the keyboard help menu on and off.
//                        Also another '&' will cancel the oscillator info mode.
//                      - Moved the OSC and PPS polarity and the PPS signal
//                        enable/disable keyboard commands to the '&' 
//                        menu (they were '^',  '+', ' -', and 'P').
//                      - Added osc drift rate and tempco values to the '&'
//                        osc parameter display.  These are calculated from the
//                        data points shown in the plot window.  
//                        For the best results,  select a plot display interval
//                        that covers a fairly long time interval with well 
//                        behaved DAC and/or TEMP values.
//                        The OSC drift rate value makes the most sense if 
//                        the temperature is stable.  The OSC tempco value 
//                        makes the most sense if the temperature 
//                        is allowed to change in a linear ramp.  This is
//                        easy to achieve with the active temperature control
//                        mechanism.
//                      - Modified the C (clear) keyboard command to allow
//                        the plot and adev queues to be cleared individually.
//                      - You can cause the adev queue to to be cleared and then
//                        reloaded from the portion of the plot queue data 
//                        currently being displayed on the screen with the CS
//                        keyboard command.
//
//                      - If you left click on a point in the plots to zoom in/out
//                        it is marked by ^ and v on the top and bottom rows.
//                        You can return to the mark with the @ or 0 keyboard command.
//                      - You can right click on the graph to mark a point
//                        and center the display on it (without zooming).  You
//                        can return to the mark with the '@' or '0' keyboard command.
//                      - You can scroll the plot by a variable amount with each
//                        right mouse click by moving the mouse to the left or
//                        right of the plot center and right clicking. The
//                        further the mouse is from the center line,  the more
//                        it is scrolled.  
//                      - If you hold the right mouse button down, the plot 
//                        continuously scrolls. The further the mouse is from 
//                        the plot center line,  the faster it is scrolled.  
//                        If the display filter is set,  works best on faster 
//                        Windows machines.
//                      - You can also set 9 other markers with the 1..9 keys.
//                        The point at the mouse cursor is marked.  You can also
//                        use the '=' key to set the next unused marker at
//                        the point.  To return to a mark,  enter the marker
//                        number (0-9).  To clear the mark,  return to it by
//                        typing its number followed by '-'.  If you accidentally
//                        go to a marker,  you can return to where you were 
//                        (but centered on the screen) if the next key that you
//                        press is '+'.  The GR screen redraw command clears 
//                        all markers. Markers are also saved/restored when you
//                        write/read a log file.
//
//                      - If the previous key was not a marker command (where
//                        a following + or - key will undo a marker)  you can
//                        use the '+' or '-' keys to move the last selected
//                        graph up or down 0.1 divisions.   That graph's scale
//                        factor and center reference values will be "fixed".
//
//                      - The plot View time is now specified in minutes/division
//                        via the keyboard V command or the /Y= command line option.
//                        You can also set the view time directly in days, hours, 
//                        minutes, or seconds by appending a d, h, m, or s to the
//                        value.  i.e.  /y=5h sets the total view time to 5
//                        hours,  while /y=5 sets view time to 5 minutes/division.
//                        The keyboard VA command will view all the data.
//                        The same method can be used with the keyboard command.
//                        Also,  the old display of the PLOTQ update interval 
//                        value is now the length of time the plot queue can hold.
//
//                      - Added Thunderbolt unit version, serial number, and 
//                        production info to the log file header.
//                      - Added support for writing log file in either append or
//                        write mode.  /W=file command now defaults to write 
//                        mode.  Use /wa=file for append mode.  
//                      - Added WD keyboard command to delete files.
//                      - Also the keyboard L (log) command has been changed.
//                        You can now specify the file to write or append, 
//                        delete the file, or set the log interval.
//
//                      - Modifed the adev code to calculate adevs incrementally
//                        as the data is collected (the older version recalulated
//                        the adev numbers from scratch every 10 seconds).  The
//                        incremental version is MUCH faster.  The incremental
//                        adev code has the ability to calculate the adevs
//                        based on all data points taken during a run,  but to
//                        keep the adev numbers fairly "fresh",  the info 
//                        accumulated since the adev queue overflowed is cleared
//                        out once the queue overflows a second time.  On slow
//                        machines you may notice a pause whenever this happens
//                        because all the adev bins must be recalculated.
//                      - Deleted support of non-overlapping adevs.  Overlapping
//                        adevs make much better use of the available data.
//                      - Modified ADEV graph scaling.  Older versions had a 
//                        fixed top decade of 1.0E-8.  This version calculates
//                        the top decade of the curves from the data.  All ADEV
//                        types use the same ADEV scale.  The top decade line
//                        is determined from the largest ADEV value seen in any
//                        of the ADEV tables.
//                      - The "all adev" commands (AP and AO) now lets you
//                        control what info to display.  All four adev plots
//                        only, all four adev types and the graphs (can be a
//                        bit confusing since the same colors are used for
//                        two different things),  or graphs and the two regular
//                        adev plots.
//                      - For 800x600 mode and below,  the ADEV plot decades 
//                        are scaled to single VERT_MAJOR divisions (not to 
//                        every two divisions)  (also the sat count plot is 
//                        scaled two sats per division)
//                      - If adev plots are disabled,  the ADEV decade lines
//                        are no longer highlighted in CYAN.
//
//                      - Added /VN /VU /VV /VX /VH /VZ command line options for other
//                        screen sizes. Note that if your video driver/monitor
//                        does not support the exact screen res,  the program
//                        may not run or will run in that horrid scaled down
//                        window. The VU command (for undersized) 640x480 screens.
//                      - Added /VC=COLSxROWS command for custom screen sizes
//                        (e.g.  /VC=1920x1200)
//                      - Added ability to specify the VESA video mode number
//                        to use for each screen resolution (like /VH=304) 
//                        in the DOS version of the program.
//                      - Added support in DOS for a small (8x8) font.  Use 
//                        a ':' after the screen res letter (/VM:  or /VH:304)
//                        on the command line.  From the keyboard,  append a 
//                        's' or a ':' after the mode number.
//                        Small fonts allow space for things like the sat info
//                        data plus the big clock.  (Plus, I get a kickback
//                        from your eye doctor).
//                      - Added ability to change screen size with the
//                        '$' keyboard command.  This command was originally
//                        intended only for debugging,  and may not work 
//                        as you might expect or desire... such is life.
//                        Changing video modes may leave options enabled that
//                        are not compatible with the new screen resolution.
//                        You can answer the command confirmation with 
//                        an N to not select the mode, or Y (for the 
//                        windowed screen) or F (for fullscreen attempt).   
//                        You can also select the font size by adding a T 
//                        (tiny 8x8) S (small 8x12)  M (medium 8x14)  or L 
//                        (large 8x16).
//                        !!! NOTE: The Windows version will crash if you start
//                        the program in non full-screen mode,  switch to 
//                        full screen with the F11 key,  then change the screen 
//                        resolution !!!  
//
//                      - Modified Windows vidstr function to draw the 
//                        degrees, up arrow, and down arrow symbols (which were
//                        not in the standard font).
//                      - Sat info shows if sat elevation is rising or falling
//                      - Added satellite azimuth/elevation plot (can be
//                        displayed in either the plot area or in place of 
//                        the adev tables - controlled by GM and GB commands).
//                        For screens 1280 pixels and wider the az/el plot 
//                        and adev tables can both be shown at the same time.
//                        For screens 1600 pixels and wider,  there is always 
//                        room for the az/el map outside of the plot area, so
//                        the GB command is not needed.  
//                      - A dot is drawn on satellite position history trails at 
//                        :00 and :30 minutes (at 22 seconds past the hour, 
//                        if you are picky about such things)  The trail marker
//                        is a solid dot in on the hour,  it is hollow at all 
//                        other times.
//                        
//                      - Flags holdover state with a red line at the top of 
//                        the plot - controlled by GH command.  Holdover is also
//                        indicated when a false temperature sensor spike is
//                        detected during active temperatue control.
//                      - Added /tj command line option to remove the filtering
//                        that the tbolt firmware does to the temperature sensor
//                        readings. (this now defaults to remove the filtering
//                        so use /tj to restore the filtering).  Removing the
//                        filtering makes false sensor reading spikes stand out
//                        better and minimizes their effect on the temperature
//                        control loop.  It does make the temperature display
//                        a little less smooth since you now see the raw sensor
//                        readings that have around 0.01C increments.
//                      - Added '%' keyboard command to move plot to the next
//                        holdover and/or time skip event.  If holdover display
//                        is turned off,  moves to next time skip.  If error
//                        display is turned off,  moves to next holdover event.
//                        If both displays are on or off,  moves to next 
//                        occurance of either.  The event is positioned where
//                        the mouse cursor is.
//                    
//                      - Added GL command to hide precise lat/lon on display.
//                        Useful if you are publishing a screen dump and those
//                        pesky voices in your head say to keep your location 
//                        private.
//                      - Added GG command to add a graph title.  Titles are 
//                        also saved to/loaded from the log file.  If a
//                        plot title contains an '&' then the '&' is replaced
//                        with the oscillator disciplining parameters.  To put
//                        the '&' in a title use &&.
//                      - Added GU command to redraw the plot on every incoming 
//                        point.  The screen is configured to scroll back by
//                        one pixel. This gives a continuously scrolling graph. 
//                        When you are in continuous scroll mode,  the center 
//                        reference line > < markers are CYAN (normally white).
//                        Continuous scroll mode makes sense to do only if you 
//                        are running the Windows version!  The DOS plot 
//                        erase/redraw is rather slow and ugly.  Also it may be
//                        a problem on slower Windows machines if the display
//                        filter is enabled.  /GU on is the default for WINDOWS.
//                      - Added command to plot menu to display a statistical
//                        value (average, rms value,  standard deviation or 
//                        variance) of the plot data.  This is the calculated
//                        from the data currently shown on the screen.
//                        The G/ command sets the statistic type for all plots
//                        or you can use use the '/' command on the plot 
//                        parameter sub-menu for the individual plots.
//                      - The GS keyboard command now toggles the sound flag
//                        (was display time skips as annoyingly large red lines)
//                      - Turning off auto scaling of graphs also turns off
//                        auto centering.
//                      - Plot scale factors can now be negative.  This allows
//                        a plot's direction to be inverted.  You can specify
//                        a scale factor of -0 (or 0-) to invert the plot with
//                        autoscaling enabled.
//                      - Added the MI command to invert the PPS and temperature
//                        plots (so their direction matches the OSC and DAC plots).
//
//                      - Added FL command for setting minimum acceptable signal
//                        level (in AMU units).  Thunderbolt default is 4,  but
//                        a value of 1.0 gives better performace since the
//                        tracked satellite constellation will not change as
//                        often.  Satellite constellation changes have a large
//                        impact on the stability of the output signals. Weak
//                        signals have a fairly small effect.
//                      - Added FE command for setting the satellite elevation
//                        mask angle (in degrees).  Satellites below the mask
//                        angle will not be tracked.  The FE and FL values are
//                        saved in EEPROM when set and do not need to be
//                        re-entered every time the program is run.
//                      - Added GJ keyboard command and /GJ command line option
//                        to disable the sat elevation mask display in the
//                        az/el plot.
//                      - Added commands to the filter menu options to change
//                        the tbolt dynamics and foliage filters
//
//                      - Added antenna survey modes to the Survey menu.  
//                        To use these you should first set the F)ilter 
//                        E)levation mask to 0 degrees.  Signal levels
//                        should be set to dBc.  If the receiver is in
//                        AMU mode the AMU values are converted to dBc 
//                        values via an emprically determined algorithm.
//                        For best results you should collect data for 24 hours.
//                        (note that the program is always collecting signal
//                         data while it is running,  not just when an antenna
//                         survey display is enabled.  You should do an SAC 
//                         command after you set the elevation mask to 0 so
//                         that old data collected at different mask values
//                         is cleared out).  
//         
//                        SAA - displays relative signal level seen at each
//                              azimuth angle
//                        SAW - displays relative signal level seen at each
//                              azimuth angle,  with the signal strength
//                              weighted inversely with elevation angle.  This
//                              highlights low elevation antenna blockages and
//                              low orbit angles that have no satellites.
//                        SAE - displays relative signal level seen at each
//                              elevation angle
//                        SAS - displays color coded map of the absolute signal 
//                              level seen at each azimuth/elevation point.
//                        SAD - shows raw satellite signal level data points
//                              (much like the GM satellite position map)
//                        SAC - clears the old signal level data and starts 
//                              collecting new data.
//
//                        You can use the Z key to zoom the signal survey 
//                        displays to full screen.  The Z command also works
//                        if the digital clock,  the analog watch,  or the
//                        satellite az/el displays are enabled.  If the display
//                        is ZOOMed then any keystroke except '\' returns the
//                        display to normal.  To dump a zoomed display to disk
//                        you must use the '\' command to write the file 
//                        "tbolt.gif"...  you cannot specify a file name.
//
//                        You can use the WZ keyboard command to write 
//                        the az/el signal level data to a file.  You should
//                        always use the .SIG file extension because you can 
//                        read in .SIG files and display the signal level data.
//
//                      - Added FD command to allow filtering (averaging) of 
//                        pps/osc/dac/temp graphs.  Note the filter is applied
//                        only when the plot is redrawn. Live incoming data 
//                        is not filtered until the plot scrolls or is 
//                        redrawn.  Be careful when using large filter values
//                        with long view time displays.  It can take a lot
//                        of time to process the data on slower machines.  
//                      - If the filter count is a negative value (including -0 
//                        or 0-) then the PPS and temperature plots are inverted.
//                        This makes all the plots track in the same direction.
//                        You can also do this with the MI keyboard command.
//                        Also,  you can append the '-' to the filter count 
//                        value.
//                      - Added /X[=#] command line option to set the display
//                        filter count... see above
//                      - If the display filter is turned on,  the values shown
//                        at the mouse cursor are the filtered values.
//
//                      - Added screen image dump to .GIF and .BMP files (this 
//                        changed the meanings of the old 'WS' keyboard command 
//                        (now is WP)).
//                        If the redrawing of the plot area after entering the file 
//                        name causes problems (like erasing the lat/lon/alt map
//                        which cannot be redrawn),  use the '\' keyboard command.
//                        This dumps the screen to TBOLT.GIF without prompting
//                        for a file name.  
//                        Note that screen dumps take quite a while on the DOS 
//                        version (around 20 secs for a 1024x768 screen on a
//                        200 MHz machine).  You get a beep at the start and 
//                        end of the dump.  The screen does not update during
//                        a dump.
//                      - WR command does a "reverse video" screen dump.  Black
//                        and white are swapped so that the screen dump is
//                        better suited to printing.  If you start to select a 
//                        reverse video screen dump (even if you do not complete
//                        it with a ESC key) the reverse video attribute will 
//                        will be used for any following '\' initiated screen 
//                        dumps.  The same applies to normal polarity screen dumps.
//
//                      - Added precise statistical position survey command (SP).
//                        This takes 48 hours to complete unless you specified
//                        a different value.  Works best with 48 hours,  well 
//                        with 24 hours,  OK with 12 hours.  After completion,
//                        the position is saved to a high precision:
//                      - Added SL keyboard command to enter lat/lon/alt. If the 
//                        position error due to using the single precision TSIP
//                        command is under 1 foot lat/lon,  1 meter altitude, 
//                        then the TSIP command is used.  Otherwise repeated 
//                        single point surveys are done until one gets close 
//                        enough to the desired location.  This usually takes 
//                        under 4 hours,  however it can go on forever...  
//                        particularly if the value you entered is not close
//                        to the actual location.  Poorer antennas take a 
//                        longer time since they spread the fixes out over a 
//                        larger area.
//                      - Interrupting a survey or precise position save will 
//                        save the current location using the lower precision 
//                        TSIP command.  So will exiting the program while a 
//                        survey is in progress.
//                      - On the command line, you can use /SP[=#] to start a 
//                        precise survey.specify.  You can specify the 
//                        number (#) of hours to do the precise survey for.
//                        Default is 48.  Other values will give inferior results.
//                        24 or 12 hours makes more sense than other values.
//                      - Added SF command to put unit into 2D/3D fix mode and
//                        plot the dispersion of the fixes around the current 
//                        location. Useful for evaluating antenna performance.
//                        Note that the fix data will be lost if the screen
//                        if the screen res is changed.  Also note that in
//                        fix modes that the unit check the PDOP of the
//                        satellite positions and will go into HOLDOVER mode
//                        if it is too large.  You should not use the fix
//                        modes as a standard operating mode!
//                      - You can put the unit into single sat mode with the 
//                        SO command (thats the letter O,  as in One)
//                      - There is also an S3 command for putting the unit
//                        into 3D only fix mode.  You can also use S0..S7 for
//                        putting the receiver into other operating modes:
//                          S0=2d/3d  S1=single sat  S2=2D mode  S3=3D mode
//                          S4=overdetermined clock  S5=DGPS reference
//                          S6=2d clock hold mode    S7=overdertmined clock
//                        Note that DGPS reference mode disables the receiver
//                        timing functions.
//                      - The SP and SF survey commands also write a LLA.LLA 
//                        lat/lon/alt log file. 
//
//                      - Added ability to read lat/lon/alt logs.  These files
//                        must have an extension of .LLA   Data lines require:
//                          time-of-week  flag  lat  lon  alt
//                          flag=0 means good reading,  otherwise skip it
//                        This format can be generated by TBOLTMON log command.
//                        Other .LLA file commands supported are:
//                           # text comments
//                           #title: Title text (note that the ': ' is required)
//                           #lla: ref_lat  ref_lon  ref_alt (your assumed exact location)
//                        The first line of an LLA file must be a '#' line.
//                      - Added ability to read adev files.  These files
//                        must have an extension of .ADV  These files can have
//                        two independent values per line (identified as PPS
//                        and OSC)  The .ADV files can also can contain:
//                           # text comments
//                           #title: title text (note that the ': ' is required)
//                           #period seconds_between_readings (default=1)
//                           #scale  scale_factor_1  scale_factor_2  
//                        The scale factor value multipled by the data values 
//                        should yield nanoseconds.
//                        Note that the #period command erases any data values
//                        that appeared before it in the file.
//                        The first line of an ADV file must be a '#' line.
//                      - Can also read .TIM files from John Miles' TI.EXE
//                        program.  The CAP/TIM/IMO, PER, and SCA commands are
//                        used.
//                      - Added ability to read script files.  These files must
//                        have an extension of .SCR  Script files mimic typing
//                        commands from the keyboard.  Script file names must
//                        be less than 128 chars long.
//                        Commands that would normally suggest an input do not 
//                        do it when read from a script file.  They behave
//                        like you first entered ESC to erase the suggestion.  
//                        Commands that normally toggle a flag may be followed 
//                        by a "1" or "0" to set the flag value directly. e.g.
//                        you can use MA0 in a script file to force auto 
//                        scaling off. The GS and GM commands do not toggle
//                        a specific value and cannot be used this way.
//                        You can put more than one command on a line (separated
//                        by spaces) until you do a command that expects a
//                        parameter value or string.  
//                        A '~' in a script file pauses reading from the script 
//                        and starts reading from the keyboard until a carriage
//                        return is seen.  Most useful for entering a parameter
//                        value or string.  e.g.  GG~ will pause the script and
//                        prompt for a graph title.
//                        Any text following a ';' or a '#' in a script file 
//                        is a comment and is ignored.
//                        Script files abort upon the first error or any 
//                        unrecognized command seen.  
//                        Script files can be nested up to 5 levels deep.
//                        Scripts can be stopped by pressing any key.
//                      - Added .CFG config file to read command line options 
//                        from.  The DOS version first looks for exe_file_name.CFG 
//                        in the current directory.  If not found, it looks for the 
//                        exe_file_name.CFG file  in the same directory as the 
//                        executable file. For WINDOWS use HEATHER /? to find 
//                        where it is looking.  The default file must be named 
//                        HEATHER.CFG.  You can also read other config
//                        files with the /H=filename command line option.  These
//                        files are processed after the default config file is 
//                        processed.
//                        The .CFG files should contain one command line option
//                        (including the leading "/" in column 1) per line.  
//                        Lines that do not start with  /,-,$, or = are ignored.  
//                      - Added '$' and '=' command line/.CFG file  options for 
//                        building lists of hex bytes to send to the tbolt.  
//                        The '=' list is sent when the program starts.  The '$'
//                        list is sent when a primary timing message is received
//                        (i.e. once each second).
//                            $10,3a,00,10,03   would request doppler/code phase 
//                                              data every second. 
//                            $10,3C,00,10,03   would request satellite status
//                                              and position data every second. 
//                        Note that the '$' and '=' command line options should 
//                        not be preceeded by a '/'.
//                      - If a file name does not have a .extension name or end
//                        in a '.',  the program tries to read a .LOG, .SCR,
//                        .LLA, .ADV, the .TIM files in that order
//
//                      - Added '/' keyboard command for setting command
//                        line options from the keyboard.  Note that some
//                        command line options cannot be changed from the 
//                        keyboard,  others may not be set properly.
//                        To set the '$' and '=' command line options,
//                        remember to backspace over the suggested leading '/'.
//
//                      - Added /tx command line option for showing the osc
//                        values with exponents (normally shows them with
//                        "ppb" and "ppt" which causes europeans to wander off
//                        in a daze of confusion and angst)
//
//                      - Added ability to actively stabilize the device 
//                        temperature. (/TT=degrees or TT command line option).
//                        Uses the serial port RTS and DTR lines.  RTS is the
//                        temperature controller enable (+12=off, -12=on)
//                        DTR is the heat (-12V) / cool (+12V) line.  Simple
//                        implementation:  Isolate tbolt in a box,  set control
//                        temperature below typical unit operating temp and
//                        above normal room temp,  when unit signals COOL turn
//                        on fan to move room air into the box.  You want the 
//                        fan to move enough air to cool the unit,  but not so
//                        much air that the temperature drops by more than 0.01C
//                        per second.  The indicator of too much airflow is a 
//                        temperature curve that spikes down around 0.1C several
//                        times per minute.  Too little airflow shows up as a
//                        curve that oscillates around 0.25C about set point over
//                        a couple of minutes.  Good airflow should show a 
//                        temperature curve stable to with 0.01C with a period of
//                        between 1 and 3 minutes.   Generally you do not want
//                        the fan to blow directly on the unit (mine is surrounded
//                        by foam).  You want to gently move air through the 
//                        box.  It can help to include a large thermal mass in 
//                        the box (I use a 2kg scale weight).  
//
//                      - The Thunderbolt firmware smooths the output of the
//                        temperature sensor with a filtering algorithm. This
//                        filtering can mask and prolong the occasional single
//                        point glitches that temperature sensor produces.  The
//                        default program action is to reverse the filtering
//                        that the firmware does.  This makes the sensor glitches
//                        much more obvious in the temperature plots and 
//                        minimizes their effect in the active temperature
//                        control mode.  The /TJ command line option disables
//                        the sensor un-filtering and the smoothed temperature
//                        data is used.
//
//                      - Added large time clock display - controlled by 
//                        the GZ command.  On small screens the clock will
//                        take the place of the sat info display.
//
//                      - Added analog clock (watch) display - controlled by 
//                        the GW command.  The watch display takes the place of
//                        the satellite az/el map on smaller screens.  Its 
//                        position is controlled with the GB and GM commands.  
//                        The watch display is not well suited to the DOS version.
//                      - You can specify a brand name for the analog watch
//                        with the /TB=name command.  The brand name can be two 
//                        lines long.  Separate the lines with a '/'  
//                        (e.g.  /tb=Patek/Philippe).  If no name is given, the 
//                        current day is used for the first line and the time zone
//                        name is used for the second lind.  To include blanks
//                        in the brand name, use an '_' character.   
//                        /TB with no first name uses the brand name to 'Trimble'.  
//                        To totally disable the brand display use /TB=_/_
//                        The brand display defaults to off for small screens.
//
//                      - Added /D# command line option for showing dates in 
//                        various calendars.
//
//                      - Added support for showing the date and time at
//                        local time zones. You can set the time zone from the
//                        command line with /t=#sssss/ddddd where # is the 
//                        (standard) time zone offset from GMT and sssss is 
//                        up to 5 characters of standard time zone ID and ddddd
//                        is the daylight savings time zone id.  
//                        Fractional time zone offsets would be specified 
//                        like "/t=9:30ACST".  NOTE THAT WESTERN HEMISPHERE
//                        TIME ZONES ARE NEGATIVE NUMBERS! (i.e. /t=-6CDT/CST)
//                        The time zone ID strings can be up to 5 characters
//                        long (default=LOCAL).  If not /ddddd daylight savings
//                        time zone ID is given,  the program does not do
//                        any daylight savings time conversions.
//                        If a standard time zone in use,  the time shows in 
//                        blue.  If a daylight savings time is in use, it shows
//                        in CYAN.  If no local time zone is in use, it shows
//                        in WHITE.
//                      - Time zone string can also be specified in the standard
//                        DOS time zone format like CST6CDT (note: western
//                        hemisphere time zone offsets are positive values in
//                        this format)
//                      - The default daylight savings time switching dates are 
//                        the US standard.  Or use /B=1 for USA, /B=2 for UK/Europe, 
//                        /B=3 for Australia or /B=4 for New Zealand.  /B=0 
//                        turns off daylight savings time.  If the rules change
//                        or you live in a backwater,  you can specify a custom
//                        time zone rule:
//                          /B=n1,start_dow,start_month,n2,end_dow,end_month,hour
//                          n1 = start DST on nth occurance of the day-of-week
//                               if n1 > 0,  count day-of-week from start of month
//                               if n1 < 0   count from end of month
//                          start_dow - start DST day-of-week (0=Sunday, 1=Monday, ... 6=Daturday)
//                          start_month - 1..12 = January..December
//                          n2 = end DST on nth occurance of the day-of-week
//                               if n2 > 0,  count day-of-week from start of month
//                               if n2 < 0   count from end of month
//                          end_dow - end DST day-of-week (0=sunday, 1=monday, ... 6=saturday)
//                          end_month - 1..12 = January..December
//                          hour - local time of switchover (default = 2)
//                        Example:  /B=-1,0,9,2,3,4,6 says to start daylight 
//                                  savings time on the last Sunday in September
//                                  and return to standard time on the second
//                                  Wednesday in April at 6:00 local time.
//                      - Time zones can also be set from the keyboard with 
//                        the TZ command.  
//                      - There are four special time zone names:
//                            GMST - Greenwich Mean Sidereal Time
//                            GAST - Greenwich Apparent Sidereal Time
//                            LMST - Local Mean Sidereal Time
//                            LAST - Local Apparent Sidereal Time
//                        Setting one of these time zones will display the
//                        current sidereal time. The receiver will be put 
//                        into UTC time mode.  Since the sidereal time is
//                        based upon UTC, NOT UT1, it may be off a second or so.
//
//                      - Added TS and /TS commands to set the system clock to
//                        the Thunderbolt time.  Windows sets the system clock 
//                        using the GPS/UTC time from the GPS receiver
//                        and Windows takes care of the system time zone 
//                        conversion.  DOS sets the time using the currently 
//                        set time zone time.  
//                      - From the command line you can use /TSD to set the 
//                        system time every day (at 04:05:06 local time)
//                        or you can use /TSH to set the system time hourly 
//                        (at xx:05:06).  /TSM will set the time every minute
//                        (at xx:xx:06).
//                      - Use /TSA to set the system time anytime the
//                        system time millisecond value is not 0.
//                        (which may be continuously on slower machines!).
//                        Note that small values of /TSA require very fast
//                        system to insure that the proceesing overhead is less
//                        than the /TSA value.  Things like zoomed screens, the analog watch,  and
//                        the signal level map take a long time to compute.
//                        Use /TSA=msecs to set the system time whenever the 
//                        system time millisecond value exceeds msecs.
//                        Whenever a system time set is pending a message is 
//                        shown over the time area.  The /TSA value should be 
//                        greater than the /TW value.  Otherwise the clock will
//                        be continuously reset since the clock error due to the
//                        forced sleeps() will be greater than the /TSA value.
//                      - Use /TSX=msecs to specify the milliseconds of delay
//                        that the receiver time message ENDs after the 1PPS 
//                        output signal (default = 45).  The Tbolt sends the
//                        timing message 20 milliseconds after the 1 PPS signal.
//                        The timing message takes about 25 milliseconds to send
//                        and get processed by Heather.
//                      - If the tbolt is in GPS time mode,  the unit is 
//                        temporarily set to UTC mode to set the system time
//                        unless the last character of the /TS command is a "G".
//                        The program waits for a lull in incoming messages
//                        and activity before setting the time. Note that older
//                        versions of the progam allowed the /ts command line 
//                        option to set degrees/minutes/seconds display mode.
//                        Now use /t".  The program now defaults to UTC 
//                        time (older vsersions defaulted to GPS time).  If the 
//                        receiver is using UTC time,  it does not have to
//                        change time bases and reinitialize the filters when 
//                        it sets the CPU clock from the receiver data.
//                      - added /TP and /TQ commands for displaying the time
//                        as seconds in the day or fraction of the day.
//
//                      - Added /NX=hh:mm:ss command line value for setting a
//                        program exit time. You can also set an exit date 
//                        with /NX=month/day/year.  You can also set the exit
//                        time/date with the keyboard TX command.  Once the
//                        exit time is reached,  the program quits.
//
//                      - Added /NA=hh:mm:ss command line value for setting 
//                        an alarm time. You can also set an alarm date 
//                        with /NA=month/day/year.  You can also set the alarm
//                        time/date with the keyboard TA command.  When the
//                        alarm time is reached,  the program sounds and the
//                        big clock flashes red.  The WINDOWS version looks for
//                        the file "c:\WINDOWS\Media\heather_alarm.wav"  If not
//                        found, it uses the windows "chord" sound.
//
//                      - The alarm clock remains enabled until it is manually
//                        disabled from the keyboard with the TA command.
//
//                      - Alarm and exit times should be specified in the local
//                        time zone.  Note that the time is matched exactly to
//                        the incoming TSIP message time.  If the time message
//                        is skipped or missed,  the event will not trigger.
//                        There is no range checking or validation of date or
//                        time values...  If only a date is given,  the event
//                        will trigger at 00:00:00 on that date.
//
//                      - If no ':' or '/' is seen in the alarm or exit time
//                        string, and the string ends in a s, m, h, or d 
//                        (for seconds, minutes, hours, days) then a countdown 
//                        "egg timer" will be started.  The event triggers 
//                        after the specifed time interval.  If the egg timer 
//                        value ends in an 'r', the timer automatically repeats.
//
//                      - If an alarm time or egg timer string ends in an 'o', 
//                        the alarm tone sounds once, otherwise it sounds every 
//                        second until a key is pressed.  The alarm .wav file 
//                        should normally be under 1 second long,  but if the
//                        ONCE option is specified, it can be any length.
//
//                      - Added an automatic screen dump option.  The syntax
//                        and behavior is the same as the alarm clock/egg timer
//                        commands.  Use /ND... from the command line or TD from
//                        the keyboard.  If the command contains the 'o' character
//                        the file TBDUMP.GIF is re-written each time a dump
//                        occurs.  Without the 'o' character,  the file 
//                        TByyyy-mm-dd-#.GIF is written (where #) is a sequence
//                        number.  Example:  to write the screen image to the
//                        file TBDUMP.GIF every 30 minutes use the command
//                        line option /ND=30mor or "TD 30mor" from the keyboard.
//                        Screen dump mode is indicated on the screen by a '!'
//                        or !! next to the time mode indicator on the first
//                        line of the screen
//
//                      - Added an automatic log dump option.  The syntax
//                        and behavior is the same as the alarm clock/egg timer
//                        commands.  Use /NL... from the command line or TL from
//                        the keyboard.  If the command contains the 'o' character
//                        the file TBLOG.LOG is re-written each time a dump
//                        occurs.  Without the 'o' character,  the file 
//                        TByyyy-mm-dd-#.LOG is written (where #) is a sequence
//                        number.  Example:  to write the log to the
//                        file TBLOG.LOG every 30 minutes use the command
//                        line option /NL=30mor or "TL 30mor" from the keyboard.
//                        Log dump mode is indicated on the screen by a '!'
//                        or !! next to the time mode indicator on the first
//                        line of the screen.   The data that is written to
//                        the log file is the data that is shown in the plot window.  
//                         
//
//                      - Added chiming (cuckoo) clock mode.  /th[=#] from the 
//                        command line or 'th' from the keyboard.  # is the 
//                        number of times per hour to sound the chime 
//                        (and must divide evenly into 60 - default is 4 - 
//                        i.e. at 00, 15, 30, and 45 minutes)
//
//                        If # is followed by 'h' (i.e. /TH=4H or TH4H from the
//                        keyboard),  it chimes out the hour count on the hour.
//                        Otherwise it sounds three times on the hour,  
//                        twice on the half hour,  otherwise once.  The WINDOWS 
//                        version looks for the file:
//                            c:\WINDOWS\Media\heather_chime.wav
//                        If not found, it uses the windows "notify" sound.
//                        The .wav file should be under 1 second long if you
//                        want to chime out the hour count.
//
//                        If # is followed by 's' (i.e. /TH=4S or TH4S from
//                        the keyboard),  the clock is a singing clock.  The
//                        file c:\WINDOWS\Media\heather_song##.wav is
//                        played at each event (where ## is the minute 
//                        number (usually 00,15,30, or 45). If a song file 
//                        does not exist,  the Windows beep is played.
//
//
// It's no longer called Beta!
//
// 3.10 - 19 APR 2012   - Added support for Nortel NTGS55A receivers.  They
//                        should be automatically recognized and enabled.  It
//                        may take a minute or so for the oscillator disciplining
//                        parameters to be updated.  You can specify the /nt
//                        command line option to do a more agressive Nortel
//                        recognition routine.  Or you can do /nt /nt to disable
//                        the Nortel check.  The Nortel receivers use
//                        alternate/undocumented commands to control the 
//                        oscillator disciplining (time constant,
//                        damping, gain, initial voltage).  The /NT command may
//                        also be useful for early versions of the Thunderbolt
//                        firmware that use the undocumented disciplining
//                        commands.  A series of red '*' chars is printed during
//                        the wakeup proceedure.  If the unit hangs while doing
//                        this, you will need to kill the program manually and try 
//                        again.
//
//                      - Added support for Resolution-T and Resolution SMT 
//                        timing receivers.  Many surplus Resolution SMT receivers
//                        are configured to do TEP (Motorola) format messages.
//                        You MUST first use the Trimblemon program to configure
//                        them for TSIP commands and 9600,8,N,1 serial protocol
//                        before Lady Heather can talk to them.  Remember to save
//                        the config into EEPROM.
//
//                        Resolution-T receivers are factory configured for 
//                        9600,8,O,1 serial protocol.  Lady Heather should 
//                        recognize this within 30 seconds,  or you can use 
//                        the /rt command line option,  or use Trimblemon to 
//                        reconfigure them for the Thunderbolt standard 
//                        9600,8,N,1 protocol.  Again, remember to save the
//                        configuration in EEPROM.  Note that it takes several
//                        seconds for a Resolution-T to recover from and update 
//                        values after changing the receiver filer settings
//                        (or probably anything that writes the EEPROM)
//
//                      - There is apparently an undocumented TSIP command for 
//                        reading and setting the allowable EFC DAC range.
//                        You can alter the low and high values with the 
//                        UNDOCUMENTED &L and &H keyboard commands.  These values
//                        might just be for reporting the allowable range to 
//                        the software or they might do something else!
//                        Caveat emptor if you use/change them!
//



//
//  This program requires the following operating system dependent routines:
//
//  init_hardware() - put screen in a high res graphics mode or open a 
//                    graphics window and open the com port (9600,8,N,1)
//
//  dot(x,y, color) - draw a colored dot
//
//  vidstr(row,col, color, string) - draw a colored text string on the screen
//       Note that row and col are in text (usually 8x16 cell) character coordinates
//
//  kill_screen() - close graphics screen or window
//
//  sendout(c)  - send 'c' to the serial port
//
//  SERIAL_DATA_AVAILABLE() - a routine or macro that returns true if 
//                            there is serial port data available
//
//  get_serial_char()  - get a character from the serial port
//
//  kill_com()    - close down serial port
//
//  SetDtrLine(state)
//  SetRtsLine(state)
//  SetBreak(state)
//
//  KBHIT() - a routine or macro that returns true if a keyboard key has
//            been pressed
//
//  GETCH() - a routine or macro that returns the keyboard character
//
//  refresh_page() - copy any virtual screen buffer to the screen
//                   or flip pages if doing screen buffering.  Can be
//                   a null routine if writing directly to the screen.
//
//  BEEP() - sound a beep if beep_on is set
//  alarm_clock() - sound an alarm tone
//  cuckoo_clock() - sound a clock chime tone
//
//  set_cpu_clock() - has OS dependent code for setting the system time
//
//  GetMsecs() - returns an double precision count of milliseconds (can
//               be since program started,  os boot, or any base time
//               reference (used to do pwm control of the heat/cool cycle
//               if doing precise temp control)
//
//  Also the show_mouse_info() routine will need to be updated to handle
//  the system mouse.  You need to get the current mouse coordinates into
//  variables mouse_x and mouse_y and the button state into this_button.
//
//  Also,  you can improve performance with operating system dependent
//  line drawing and area erasing functions.
//
// Note: stupid DOS linker does not allow initialized variables to be declared
//       in more than one file (even if defined as externs).  Therefore any
//       variables that are used in more than one file are initialized
//       in the routine set_defaults();

#define EXTERN
#include "heather.ch"


#ifdef DOS
   u16 plot_pid = 0xDEAD;
   u16 adev_pid = 0xDEAD;
   u16 hash_pid = 0xDEAD;

   #define COM_Q_SIZE 1024           /* serial port input buffer size */
   unsigned char com_q[COM_Q_SIZE];  /* serial port raw input data queue */
#endif


#ifdef WINDOWS
   C8 szAppName[] = "Lady Heather's Disciplined Oscillator Control Program - "VERSION"";

   u08 timer_set;  // flag set if dialog timer has been set
   u08 path_help;  // flag set if help message has been seen before
#endif


#ifdef WIN_VFX
   #include "heathfnt.ch"

   VFX_WINDOW *stage_window = NULL;
   PANE       *stage = NULL;

   VFX_WINDOW *screen_window = NULL;
   PANE       *screen = NULL;

   VFX_FONT *vfx_font = (VFX_FONT *) (void *) &h_font_12[0];

   S32 font_height;
   S32 em_width;

   #ifdef DOS_VFX
      BYTE transparent_font_CLUT[256];
      u08 VFX_driver_loaded;
   #else
      U16 transparent_font_CLUT[256];
   #endif
#endif


struct PLOT_DATA plot[NUM_PLOTS] = {  // plot configurations 
//  ID        units   ref scale  show  slot float  plot color
   {"OSC",    "ppt",  1000.0F,   0,    0,   0,     OSC_COLOR  },
   {"PPS",    "ns",   1.0F,      1,    1,   0,     PPS_COLOR  },
   {"DAC",    "uV",   1.0E6F,    1,    2,   1,     DAC_COLOR  },
   {"TEMP",   "øC",   1000.0F,   1,    3,   1,     TEMP_COLOR },
   {"D\032O", "x",    1.0F,      0,    4,   1,     XXX_COLOR  },
   {"DIF",    "x",    1.0F,      0,    5,   1,     YYY_COLOR  },
   {"D-T",    "x",    1.0F,      0,    6,   1,     ZZZ_COLOR  },
   #ifdef FFT_STUFF
   {"FFT",    "x",    1.0F,      0,    7,   0,     RED  },
   #endif
};

int slot_column[NUM_PLOTS] = { // the column number that the slot is displayed at
   2, 22, 40, 59,
   0,
   0,
   0,
};

int slot_in_use[NUM_PLOTS];



extern char *dst_list[];  // daylight savings time definitions
extern float k6, k7, k8;
int f11_flag;

void config_res_t_plots()
{
   // Configure the plots for Resolution-T data
   // note: we should not change settings the user gave on the command line

   plot[DAC].plot_id = "Corr";
   plot[DAC].units = "ns";
   plot[DAC].ref_scale = 1.0F;
   if(user_set_dac_plot == 0) plot[DAC].show_plot = 0;
   if(user_set_dac_float == 0) plot[DAC].float_center = 1;

   plot[PPS].plot_id = "Bias";
   plot[PPS].units = "us";
   plot[PPS].ref_scale = 1.0F/1000.0F;
   if(user_set_pps_plot == 0) plot[PPS].show_plot = 1;
   if(user_set_pps_float == 0) plot[PPS].float_center = 1;

   plot[OSC].plot_id = "Rate";
   plot[OSC].units = "ppb";
   plot[OSC].ref_scale = 1.0F;
   if(user_set_osc_plot == 0) plot[OSC].show_plot = 1;
   if(user_set_osc_float == 0) plot[OSC].float_center = 1;

   if(user_set_adev_plot == 0) plot_adev_data = 0;

// plot_signals = 4;
   if(0 && (SCREEN_HEIGHT >= 1024)) {
      plot_azel = AZEL_OK;
      if(plot_azel == 0) plot_signals = 4;
      if(user_set_clock_plot == 0) plot_digital_clock = 0;
   }
   else {
      if(user_set_watch_plot == 0) plot_watch = 1;
      if(user_set_clock_plot == 0) plot_digital_clock = 0;
   }

   if(user_set_temp_filter == 0) undo_fw_temp_filter = 0;
}


#ifdef DOS_MOUSE   // sofware based mouse cursor

#define CUR_HEIGHT 10
#define CUR_WIDTH  10
unsigned char cursor_mask[CUR_HEIGHT][CUR_WIDTH];
unsigned char cursor_buf[CUR_HEIGHT][CUR_WIDTH];
int this_x, this_y;
int mouse_xx, mouse_yy;
u08 mouse_moved;

int mouse_check()
{
   reg.wd.ax = 0x0000;
   int86(0x33, &reg, &reg);
   if(reg.wd.ax == 0x0000) return 0;
   else                   return reg.wd.bx;
}

void set_mouse_x(COORD minn, COORD maxx)
{
   reg.wd.ax = 7;
   reg.wd.cx = minn;
   reg.wd.dx = maxx;
   int86(0x33, &reg, &reg);
}

void set_mouse_y(COORD minn, COORD maxx)
{
   reg.wd.ax = 8;
   reg.wd.cx = minn;
   reg.wd.dx = maxx;
   int86(0x33, &reg, &reg);
}

void set_mouse_ratio(COORD x, COORD y)
{
   reg.wd.ax = 0x000F;
   reg.wd.cx = x;
   reg.wd.dx = y;
   int86(0x33, &reg, &reg);
}

void set_mouse_posn(COORD x, COORD y)
{
   mouse_xx = x;
   mouse_yy = y;

   reg.wd.ax = 0x0004;
   reg.wd.cx = x;
   reg.wd.dx = y;
   int86(0x33, &reg, &reg);
}

void make_mouse_cursor()
{
unsigned x, y;

   for(y=0; y<CUR_HEIGHT/2; y++) {
      for(x=0; x<CUR_WIDTH/2-y; x++) {
         cursor_mask[y][x] = MOUSE_COLOR;
         cursor_buf[y][x] =  MOUSE_COLOR;
      }
   }

   for(x=0; x<CUR_WIDTH; x++) {  // the diagonal
      cursor_mask[x][x] = MOUSE_COLOR;
      cursor_buf[x][x] = MOUSE_COLOR;
      if(x > 0) {
         cursor_mask[x][x-1] = MOUSE_COLOR;
         cursor_buf[x][x-1] = MOUSE_COLOR;
      }
      if(x < (CUR_WIDTH-1)) {
         cursor_mask[x][x+1] = MOUSE_COLOR;
         cursor_buf[x][x+1] = MOUSE_COLOR;
      }
   }

   mouse_shown = 0;
}

void swap_block(COORD x, COORD y, COORD w, COORD h, u08 *cursor, u08 *buf)
{
int i;
unsigned char c;
int col;
int ccc, rrr;

   i = 0;
   ccc = w;
   if(ccc >= SCREEN_WIDTH) ccc = SCREEN_WIDTH-1;
   rrr = h;
   if(rrr >= SCREEN_HEIGHT) rrr = SCREEN_HEIGHT-1;
   for(; y<rrr; y++) {
      for(col=x; col<ccc; col++) {
         if(cursor[i]) {
            c = buf[i];
            buf[i] = get_pixel(col,y);  // save screen pixel in buffer
            #ifdef DOS_VFX
               VFX_io_done = 1;
               VFX_pixel_write(stage, col,y, palette[c]);
            #else
               #ifdef DOS_EXTENDER
                  reg.wd.cx = col;
                  reg.wd.dx = y;
                  reg.h.bh = 0;
                  reg.h.ah = 0x0C;
                  reg.h.al = c;
                  int86(0x10, &reg, &reg);
               #else
                  _asm {                      // put dot on screen
                     mov cx,col
                     mov dx,y
                     mov bh,0
                     mov ah,0Ch
                     mov al,c
                     int 10h
                  }
               #endif
            #endif
         }
         ++i;
      }
   }

   mouse_shown ^= 1;
}

void init_dos_mouse()
{
   make_mouse_cursor();

   set_mouse_x(0, SCREEN_WIDTH-1);
   set_mouse_y(0, SCREEN_HEIGHT-1);
   set_mouse_posn(SCREEN_WIDTH/2, SCREEN_HEIGHT/2);
   set_mouse_ratio(8, 8);
}

void get_mouse_posn()
{
   last_button = this_button;

   reg.wd.ax = 0x0003;
   int86(0x33, &reg, &reg);
   this_x = reg.wd.cx;
   this_y = reg.wd.dx;
   this_button = reg.wd.bx;
}

void update_mouse()
{
   get_mouse_posn();
   if((this_x == mouse_xx) && (this_y == mouse_yy)) mouse_moved = 0;
   else                                             mouse_moved = 1;

   if(mouse_shown) {    /* erase the old mouse */
      if(mouse_moved == 0) return;
      swap_block(mouse_xx, mouse_yy, mouse_xx+CUR_WIDTH, mouse_yy+CUR_HEIGHT, &cursor_mask[0][0], &cursor_buf[0][0]);
   }

   mouse_xx = this_x;
   mouse_yy = this_y;
   swap_block(mouse_xx, mouse_yy, mouse_xx+CUR_WIDTH, mouse_yy+CUR_HEIGHT, &cursor_mask[0][0], &cursor_buf[0][0]);
}

void hide_mouse()
{
   if(mouse_shown) {    /* erase the old mouse */
      swap_block(mouse_xx, mouse_yy, mouse_xx+CUR_WIDTH, mouse_yy+CUR_HEIGHT, &cursor_mask[0][0], &cursor_buf[0][0]);
   }
}

u08 mouse_hit(COORD x1,COORD y1,  COORD x2,COORD y2)
{
int t;
   // see if the operation will write over the mouse cursor
   // if so, remove the cursor from the screen
   if(mouse_shown == 0) return 0;
   if(x1 > x2) {
      t = x1;
      x1 = x2;
      x2 = t;
   }
   if(y1 > y2) {
      t = y1;
      y1 = y2;
      y2 = t;
   }

   if((x1 < CUR_WIDTH) && (mouse_xx >= (SCREEN_WIDTH-CUR_WIDTH))) { // cursor is wrapping around
      if((mouse_yy+CUR_HEIGHT >= y1) && (mouse_yy <= y2)) goto hide_it;
   }
   if((mouse_xx+CUR_WIDTH < x1)  || (mouse_xx > x2)) return 0;
   if((mouse_yy+CUR_HEIGHT < y1) || (mouse_yy > y2)) return 0;

   hide_it:
   hide_mouse();
   return 1;
}
#endif    // DOS_MOUSE


#ifdef DOS_IO    // DOS real mode I/O routines

interrupt far ctrl_break()    /* INT 24H (ctrl-break) interrupt handler */
{
   break_flag = 1;     /* set time-to-die flag */
}

interrupt far service_math()  /* math error interrupt handler */
{
   ++math_errors;
}

interrupt far service_irq()   /* COM receive interrupt handler */
{
   com_q[com_q_in] = inp(port_addr);
   if(++com_q_in >= COM_Q_SIZE) com_q_in = 0;
   if(com_q_in == com_q_out) {   /* !!! we just wrap the queue if it overflows */
      ++com_errors;
   }

   outp(0x20, 0x20);         /* signal EOI */
}

void set_vector(int num,  void far *addr)
{
unsigned long temp;

   temp = (unsigned long) addr;
   reg.h.al = num;
   reg.h.ah = 0x25;
   reg.wd.dx = (temp & 0xFFFF);
   seg.ds = (temp >> 16);
   int86x(0x21, &reg, &reg, &seg);
}

void init_irq()
{
int i;
int irq;

   com_q_in = com_q_out = 0;

   if(port_addr == 0x3F8) irq = 4;
   else                   irq = 3;

   int_mask = (0x01 << irq);
   outp(0x21, inp(0x21) | int_mask);  /* disable IRQx */

   set_vector(0x08+irq, service_irq);
   set_vector(0x23,     ctrl_break);
   set_vector(0x00,     service_math);
   set_vector(0x04,     service_math);

   outp(port_addr+3, inp(port_addr+3) & 0x7F);  /* DLAB = 0 */
   outp(port_addr+1, 0x01);   /* enable RCV DATA interrupt in UART */
   outp(port_addr+4, 0x0B);   /* turn on IRQ driver */

   for(i=0; i<32; i++) {  /* flush any pending chars */
      inp(port_addr);
   }

   outp(0x21, inp(0x21) & ~int_mask);    /* enable IRQx */
}

void SetDtrLine(u08 val)
{
u08 reg;

   reg = inp(port_addr+4);
   if(val) reg |= (0x01);
   else    reg &= (~0x01);
   outp(port_addr+4, reg);
}

void SetRtsLine(u08 val)
{
u08 reg;

   reg = inp(port_addr+4);
   if(val) reg |= (0x02);
   else    reg &= (~0x02);
   outp(port_addr+4, reg);
}


void kill_com()
{
int i;

   if(port_addr == 0) return;

   SetDtrLine(0);

   outp(0x21, inp(0x21) | int_mask);  /* disable IRQx */

   outp(port_addr+3, inp(port_addr+3) & 0x7F);  /* DLAB = 0 */
   outp(port_addr+1, 0x00);   /* disable RCV DATA interrupt in UART */
   outp(port_addr+4, 0x03);   /* turn OFF IRQ driver */

   for(i=0; i<32; i++) {  /* flush any pending chars */
      inp(port_addr);
   }
}

u08 get_serial_char()
{
unsigned char c;

   if(process_com == 0) return 0;

   while(com_q_in == com_q_out) {  // wait for char to come into the serial port
      update_pwm();  // if doing pwm temperature control
//      if(KBHIT()) return MSG_TIMEOUT;   //!!! should do proper timeout check
   }

   c = com_q[com_q_out];       // get char from queue
   if(++com_q_out >= COM_Q_SIZE) com_q_out = 0;

   if(log_stream && log_file) debug_stream(c);
   return c;
}

unsigned short peekw(unsigned segval, unsigned ofsval)
{
unsigned short far *ptr;

   ptr = (unsigned short far *) (void far *)  
         ((((unsigned long) segval) << 16) | ((unsigned long) ofsval));
   return *ptr;
}


unsigned get_com_addr()   /* init COM port,  return base I/O address */
{
   com_q_in = com_q_out = 0;
   com_errors = 0;
   int_mask = 0;

// reg.wd.ax = 0x0083        // 1200 baud, 8 bit, no parity, 1 stop
// reg.wd.ax = 0x00E7        // 9600,8,N,2
   reg.wd.ax = 0x00E3;       // 9600,8,N,1
   reg.wd.dx = com_port-1;
   int86(0x14, &reg, &reg);

   return peekw(0x0040, (com_port-1)*2);
}

void init_com(void)
{
   if(port_addr) kill_com();

   port_addr = get_com_addr();
   if(port_addr) {
      init_irq();    /* turn on incoming character interrupts */
   }
   else {
      process_com = 0;
   }

   SetDtrLine(1);
   com_running = 1;
}

#ifdef TEMP_CONTROL
unsigned init_lpt_x()   /* init LPT port,  return base I/O address */
{
   return peekw(0x0040, 0x0008+((lpt_port-1)*2));
}
#endif


int send_char(c)
unsigned c;
{
   if(inp(port_addr+5) & 0x20) {  // uart is not busy
      outp(port_addr, c);
      return 1;
   }
   else return 0;
}


void sendout(u08 val)
{
   if(process_com == 0) return;

   while(send_char(val) == 0) {  //!!! should do timeout check
      update_pwm();   // if doing pwm temperature control
      continue; 
   }
}

int get_kbd()
{
unsigned c;

   c = GETCH();
   if(c == 0x00) {   /* special function key */
      c = GETCH();
      c += 0x0100;
   }
   return c;

}

void init_hardware(void) 
{
   init_com();
   init_screen(); /* set video mode,  clear screen,  sign on */
   need_screen_init = 0;
}

double GetMsecs()
{
u08 hh, mm, sss, milli;
unsigned long ticks;

   reg.h.ah = 0x2C;
   int86(0x21, &reg, &reg);
   hh = reg.h.ch;
   mm = reg.h.cl;
   sss = reg.h.dh;
   milli = reg.h.dl;

   ticks  = ((unsigned long) milli)*10L;  // DOS time is in 10 millisecond increments
   ticks += ((unsigned long) sss) * 1000L;
   ticks += ((unsigned long) mm) * (60L*1000L);
   ticks += ((unsigned long) hh) * (60L*60L*1000L);
   ticks += ((unsigned long) pri_day) * (24L*60L*60L*1000L);
   return (double) ticks;
}

void alarm_clock()
{
   printf("\007");
}

void cuckoo_clock()
{
   printf("\007");
}

#endif // DOS_IO


#ifdef DOS_VIDEO   // DOS real mode (BIOS driven) video I/O routines

void small_dos_font()
{
   reg.wd.ax = 0x1123;        // select small 8x8 font
   reg.h.bl = 0x00;
   reg.h.dl = 0xFF;
   int86(0x10, &reg, &reg);

   TEXT_HEIGHT = 8;
   small_font = 2;    // small (8x8) fixed pitch font
}

void vidmode(unsigned mode)
{
   if(mode >= 0x0100) {
      reg.wd.ax = 0x4F02;
      reg.wd.bx = mode & 0x7FFF;
   }
   else {
      reg.wd.ax = mode;
   }
   
   int86(0x10, &reg, &reg);
}


void init_screen(void)
{
   config_screen();  // initialize screen rendering variables

   if(text_mode) vmode = text_mode;
   else          vmode = GRAPH_MODE;  //1024x768x16 color
   vidmode(vmode);

   small_font = 0;
   if(text_mode) TEXT_HEIGHT = 16;    // can't do 8x8 fonts in true text mode
   else if(user_font_size && (user_font_size <= 12)) small_dos_font();
   else TEXT_HEIGHT = 16;

   if(vmode) {
      reg.wd.ax = 0x0100;        // set cursor shape
      reg.wd.cx = 0x2020;        // so it disappears
      int86(0x10, &reg, &reg);
   }
   else {
/* !!!      scr_rowcol(TEXT_ROWS, 0);  move cursor off screen */
   }

   #ifdef DOS_MOUSE
      if((text_mode == 0) && (full_circle == 0) && (mouse_disabled == 0)) {
         dos_mouse_found = mouse_check();
         if(dos_mouse_found) init_dos_mouse();
      }
   #endif

   #ifdef BUFFER_LLA
      clear_lla_points();
   #endif

   config_screen();  // re-initialize screen rendering variables
}

void kill_screen()
{
   vidmode(2);
}


u08 get_pixel(COORD x,COORD y)
{
  #ifdef DOS_EXTENDER
     reg.wd.cx = x;
     reg.wd.dx = y;
     reg.h.bh = 0;
     reg.h.ah = 0x0D;
     int86(0x10, &reg, &reg);
     return reg.h.al;
  #else
      _asm {
         mov cx,x
         mov dx,y
         mov bh,0
         mov ah,0Dh
         int 10h
         xor ah,ah
      }
   #endif
}

void dot(COORD x,COORD y, u08 color)
{
   #ifdef DOS_MOUSE
      u08 m;
      m = mouse_hit(x,y, x,y);
   #endif

   #ifdef DOS_EXTENDER
      reg.wd.cx = x;
      reg.wd.dx = y;
      reg.h.bh = 0;
      reg.h.ah = 0x0C;
      reg.h.al = color;
      int86(0x10, &reg, &reg);
   #else
      _asm {         // put a dot on the screen
         mov cx,x
         mov dx,y
         mov bh,0
         mov ah,0Ch
         mov al,color
         int 10h
      }
   #endif

   #ifdef dos_mouse
      if(m) update_mouse();
   #endif
}

void vidstr(COORD row, COORD col, u08 attr, char *s)
{
char c;
u08 m;
unsigned lenn;
unsigned ooo, sss;
u08 rrr,ccc;

   if(graphics_coords) {  //!!! this don't work so well in DOS because of fixed text cell locations
      row /= TEXT_HEIGHT;
      col /= TEXT_WIDTH;
   }
   // !!! we should handle blank_underscore,  but DOS does not use VARIABLE_FONTs
   lenn = strlen(s);

   #ifdef DOS_MOUSE
      m = mouse_hit(col*TEXT_WIDTH,row*TEXT_HEIGHT, ((col+lenn)*TEXT_WIDTH)-1, row*TEXT_HEIGHT+TEXT_HEIGHT-1);
   #endif

   ooo = (unsigned) ((unsigned long)s) & 0xFFFF;
   sss = ((unsigned long) s) >> 16;
   rrr = row;
   ccc = col;

   #ifdef DOS_EXTENDER
      while(c=*s++) {
         if((row < TEXT_ROWS) && (col < TEXT_COLS)) {  // see if we are on screen
            if(blank_underscore && (c == '_')) c = ' ';
            reg.h.dh = row;
            reg.h.dl = col;
            reg.wd.bx = 0;
            reg.wd.ax = 0x0200;
            int86(0x10, &reg, &reg);

            reg.h.ah = 9;
            reg.h.al = c;
            reg.h.bh = 0;
            reg.h.bl = attr;
            reg.wd.cx = 1;
            int86(0x10, &reg, &reg);

            ++col;
         }
         else break;
      }
   #else
      _asm {        // output text string using a single BIOS call
         push es
         push bp
         mov ax,1300H
         mov bl,attr    // bh=0
         mov bh,0
         mov cx,lenn
         mov dh,rrr
         mov dl,ccc
         mov si,sss
         mov bp,ooo
         mov es,si
         int 10h
         pop bp
         pop es
      }
   #endif

   #ifdef DOS_MOUSE
      if(m) update_mouse();
   #endif
}

void dos_erase(int left,int top,  int right,int bot)
{
int width;
int row;
u08 m;


   // !!! kludgy way to erase the plot area by writing blanks to the window
   //     (semi-fast way to erase using only video BIOS calls)
   //     Note that screen coordinates need to be on character cell boundaries
   if(left > right) {
      width = left;
      left = right;
      right = width;
   }
   if(top > bot) {
      width = bot;
      bot = top;
      top = width;
   }

   #ifdef DOS_MOUSE
      m = mouse_hit(left,top, right,bot);
   #endif

   width = ((right-left) / TEXT_WIDTH)+1;
   if(width > TEXT_COLS) width = TEXT_COLS;

   left /= TEXT_WIDTH;
   bot  /= TEXT_HEIGHT;
   top  /= TEXT_HEIGHT;

   // Using the bios "write char cx time" function might be faster,  but we use 
   // vidstr to write the blanks because many bioses mess up that function
   // in graphics modes.
   for(row=top; row<=bot; row++) {
      vidstr(row, left, WHITE, &blanks[TEXT_COLS-width]);
   }

   #ifdef DOS_MOUSE
      if(m) update_mouse();
   #endif
}

#endif  // DOS_VIDEO


#ifdef DOS4GW_IO // DOS4GW protected mode I/O routines

int dtr_val;
int rts_val;

void SetDtrLine(u08 val)
{
   if(val) dtr_val = DTR;
   else    dtr_val = 0;
   ioSetHandShake(PortID, rts_val | dtr_val);
}

void SetRtsLine(u08 val)
{
   if(val) rts_val = RTS;
   else    rts_val = 0;
   ioSetHandShake(PortID, rts_val | dtr_val);
}


void kill_com()
{
   if(PortID == 0) return;

   SetDtrLine(0);
   ioClosePort(PortID);
   PortID = 0;
}

u08 get_serial_char()
{
unsigned char c;

   if(process_com == 0) return 0;

   while(ioReadStatus(PortID) == 0) update_pwm();
   c = ioReadByte(PortID) & 0xFF;

   if(log_stream && log_file) debug_stream(c);
   return c;
}


void init_com(void)
{
   if(PortID) kill_com();

   if(com_port == 1) {
      PortID = ioOpenPort(0x3F8, 4);
      port_addr = 0x3F8;
   }
   else if(com_port == 2) {
      PortID = ioOpenPort(0x2F8, 3);
      port_addr = 0x2F8;
   }
   else {
      PortID = 0;
      port_addr = 0;
   }

   if(PortID) {
      ioSetBaud(PortID, 9600);
      ioSetControl(PortID, BITS_8 | NO_PARITY | STOP_1);
      ioSetMode(PortID, BYTE_MODE);
      outp(port_addr+2, 7);   // enable 16550 fifos
      SetDtrLine(1);
   }
   else {
      process_com = 0;
   }

   com_running = 1;
}

#ifdef TEMP_CONTROL
unsigned init_lpt_x()   /* init LPT port,  return base I/O address */
{
// return peekw(0x0040, 0x0008+((lpt_port-1)*2));
return 0x378;
}
#endif


void sendout(u08 val)
{
   if(process_com == 0) return;

   while(ioWriteStatus(PortID)) {  //!!! should do timeout check
      update_pwm();   // if doing pwm temperature control
      continue; 
   }
   ioWriteByte(PortID, val);
}


int get_kbd()
{
unsigned c;

   c = GETCH();
   if(c == 0x00) {   /* special function key */
      c = GETCH();
      c += 0x0100;
   }
   return c;
}

void init_hardware(void) 
{
   init_com();
   init_screen(); /* set video mode,  clear screen,  sign on */
   need_screen_init = 0;
}

double GetMsecs()
{
u08 hh, mm, sss, milli;
unsigned long ticks;

   reg.h.ah = 0x2C;
   int86(0x21, &reg, &reg);
   hh = reg.h.ch;
   mm = reg.h.cl;
   sss = reg.h.dh;
   milli = reg.h.dl;

   ticks  = ((unsigned long) milli)*10L;  // DOS time is in 10 millisecond increments
   ticks += ((unsigned long) sss) * 1000L;
   ticks += ((unsigned long) mm) * (60L*1000L);
   ticks += ((unsigned long) hh) * (60L*60L*1000L);
   ticks += ((unsigned long) pri_day) * (24L*60L*60L*1000L);
   return (double) ticks;
}

void alarm_clock()
{
   printf("\007");
   fflush(stdout);
}

void cuckoo_clock()
{
   printf("\007");
   fflush(stdout);
}

#endif    // DOS4GW_IO


#ifdef DOS_VFX

void VFX_rectangle_fill(PANE *stage,  int x1,int y1,  int x2,int y2, int mode, int color)
{
int t;
u08 m;

   m = 0;

   if(y1 > y2) {
      t = y1;
      y1 = y2;
      y2 = t;
   }

   #ifdef DOS_MOUSE
      m = mouse_hit(x1,y1, x2,y2);
   #endif

   while(y1 <= y2) {
      VFX_line_draw(stage,
                    x1,y1,
                    x2,y1,
                    mode,
                    palette[color]);
      ++y1;
   }

   #ifdef DOS_MOUSE
      if(m) update_mouse();
   #endif
}

#endif // DOS_VFX



#ifdef WINDOWS     // WINDOWS OS dependent I/O routines

#include "timeutil.cpp"
#include "ipconn.cpp"

struct LH_IPCONN : public IPCONN
{
   C8 message[1024];

   virtual void message_sink(IPMSGLVL level,   
                             C8      *text)
      {
      memset(message, 0, sizeof(message));      // copy error/notice message to an array where app can see it
      strncpy(message, text, sizeof(message)-1);

      printf("%s\n", message);
      }

   virtual void on_lengthy_operation(void)
      {
      update_pwm();                             // e.g., while send() is blocking
      }
};

LH_IPCONN *IPC;

HANDLE hSerial = INVALID_HANDLE_VALUE;
DCB dcb = { 0 };

void SetDtrLine(u08 on)
{   
   if(hSerial != INVALID_HANDLE_VALUE) {
      EscapeCommFunction(hSerial, on ? SETDTR : CLRDTR);
   }
}

void SetRtsLine(u08 on)
{   
   if(hSerial != INVALID_HANDLE_VALUE) {
      EscapeCommFunction(hSerial, on ? SETRTS : CLRRTS);
   }
}

void SetBreak(u08 on)
{   
   if(hSerial != INVALID_HANDLE_VALUE) {
      EscapeCommFunction(hSerial, on ? SETBREAK : CLRBREAK);
   }
}


void kill_com(void)
{
   if(com_port != 0) {       // COM port in use: close it
      SetDtrLine(0);
      if(hSerial != INVALID_HANDLE_VALUE) CloseHandle(hSerial);
      hSerial = INVALID_HANDLE_VALUE;
   }
   else {                    // TCP connection: close the connection
      if (IPC != NULL)
         {
         delete IPC;
         IPC = NULL;
         }
   }
}

void init_tcpip()
{
   IPC = new LH_IPCONN();

   IPC->connect(IP_addr, DEFAULT_PORT_NUM);

   if (!IPC->status())
      {
      error_exit(2, IPC->message);
      }
}

void init_com(void)
{
   kill_com();   // in case COM port already open

   if(com_port == 0) {
      //
      // TCP: In Windows, COM0 with process_com=TRUE means we're using TCP/IP 
      //
      if(process_com) {
         init_tcpip();
         com_running = 1;
      }
      return;    
   }

   // kd5tfd hack to handle comm ports > 9
   // see http://support.microsoft.com/default.aspx?scid=kb;%5BLN%5D;115831 
   // for the reasons for the bizarre comm port syntax

   char com_name[20];
   sprintf(com_name, "\\\\.\\COM%d", com_port);
   hSerial = CreateFile(com_name,
                        GENERIC_READ | GENERIC_WRITE, 
                        0,
                        0,
                        OPEN_EXISTING, 
                        FILE_ATTRIBUTE_NORMAL, 
                        0
   );

   if(hSerial == INVALID_HANDLE_VALUE) {
      sprintf(out, "Can't open com port: %s", com_name);
      error_exit(10001, out);
   }

   dcb.DCBlength = sizeof(dcb);
   if(!GetCommState(hSerial, &dcb)) {
      error_exit(10002, "Can't GetCommState()");
   }

   dcb.BaudRate         = 9600;
   dcb.ByteSize         = 8;
   if(res_t_init) dcb.Parity = ODDPARITY;
   else           dcb.Parity = NOPARITY;
   dcb.StopBits         = ONESTOPBIT;
   dcb.fBinary          = TRUE;
   dcb.fOutxCtsFlow     = FALSE;
   dcb.fOutxDsrFlow     = FALSE;
   dcb.fDtrControl      = DTR_CONTROL_ENABLE;
   dcb.fDsrSensitivity  = FALSE;
   dcb.fOutX            = FALSE;
   dcb.fInX             = FALSE;
   dcb.fErrorChar       = FALSE;
   dcb.fNull            = FALSE;
   dcb.fRtsControl      = RTS_CONTROL_ENABLE;
   dcb.fAbortOnError    = FALSE;

   if(!SetCommState(hSerial, &dcb)) {
      error_exit(10003, "Can't SetCommState()");
   }

   // set com port timeouts so we return immediately if no serial port
   // character is available
   COMMTIMEOUTS cto = { 0, 0, 0, 0, 0 };
   cto.ReadIntervalTimeout = MAXDWORD;
   cto.ReadTotalTimeoutConstant = 0;
   cto.ReadTotalTimeoutMultiplier = 0;

   if(!SetCommTimeouts(hSerial, &cto)) {
      error_exit(10004, "Can't SetCommTimeouts()");
   }

   SetDtrLine(1);
   com_running = 1;
}

u08 rcv_buf[1024];
DWORD rcv_byte_count;
DWORD next_serial_byte;

bool check_incoming_data(void)
{
   if(com_error & 0x01) return TRUE; // !!!! return 0;
   if(next_serial_byte < rcv_byte_count) return TRUE;  // we have already read and buffered the char

   rcv_byte_count = 0;
   next_serial_byte = 0;

   if(com_port != 0) {       // COM port in use: read bytes from serial port
      ReadFile(hSerial, &rcv_buf[0], sizeof(rcv_buf), &rcv_byte_count, NULL);
      if(rcv_byte_count == 0) return FALSE;  // no serial port data is available
   }
   else {                     // TCP connection: read a byte from Winsock 
      if (!IPC->status())
         {
         error_exit(2, IPC->message);
         }

      rcv_byte_count = IPC->read_block(rcv_buf, sizeof(rcv_buf));

      if (rcv_byte_count == 0) return FALSE;
   }

   return TRUE;
}

u08 get_serial_char(void)
{
int flag;
u08 c;

   if(com_error & 0x01) return 0;

   if(next_serial_byte < rcv_byte_count) {   // return byte previously fetched by check_incoming_data()
      c = rcv_buf[next_serial_byte++];
      if(log_stream && log_file) debug_stream(c);
      return c;
   }

   rcv_byte_count = 0;
   next_serial_byte = 0;

   while(rcv_byte_count == 0) {  // wait until we have a character
      if(com_port != 0)  {       // COM port in use: read a byte from serial port
         flag = ReadFile(hSerial, &rcv_buf[0], sizeof(rcv_buf), &rcv_byte_count, NULL);
      }
      else {                     // TCP connection: read a byte from Winsock 
         if (!IPC->status())
            {
            error_exit(2, IPC->message);
            }

         flag = rcv_byte_count = IPC->read_block(rcv_buf, sizeof(rcv_buf));
      }

      update_pwm();   // if doing pwm temperature control
   }

   if(flag && rcv_byte_count) {  // succesful read
      c = rcv_buf[next_serial_byte++];
      if(log_stream && log_file) debug_stream(c);
      return c;
   }

   if(com_error_exit) {
      error_exit(222, "Serial receive error");
   }
   else {
      com_error |= 0x01;
      ++com_errors;
   }
   return 0;
}

void sendout(unsigned char val)
{
DWORD written;
int flag;
static U8 xmit_buffer[1024]; 
static S32 x = 0;      // Queue up data until ETX to reduce per-packet overhead 

   if(process_com == 0) return;
   if(com_error & 0x02) return;
if(just_read) return;
if(no_send) return;

   // we buffer up output chars until buffer full or ETX byte seen
   xmit_buffer[x++] = val;
   if((x < sizeof(xmit_buffer)) && (val != ETX)) return;

   update_pwm();   // if doing pwm temperature control

   if(com_port != 0) {       // COM port in use: send bytes via serial port
      flag = WriteFile(hSerial, xmit_buffer, x, &written, NULL);
      if(written == x) written = 1;
      else             written = 0;
   }
   else {                    // TCP connection: send byte via Winsock 
      IPC->send_block(xmit_buffer, x);

      if (!IPC->status())
         {
         error_exit(2, IPC->message);
         }

      flag = 1;      // success
      written = 1;
   }

   x = 0;

   if((flag == 0) || (written != 1)) {
      if(com_error_exit) {
         error_exit(3, "Serial transmit error");
      }
      else {
         com_error |= 0x02;
         ++com_errors;
      }
   }
}


void SendBreak()
{
   // This routine sends a 300 msec BREAK signal to the serial port

   SetBreak(1);
   Sleep(300);
   SetBreak(0);
   Sleep(50);
}

void init_hardware(void) 
{
   init_com();
   init_screen();
   if(nortel == 0) {    // To wake up a Nortel NTGS55A receiver
      SendBreak(); 
   }

   // Arrange to call serve_gps() 5x/second while dragging or displaying the
   // command line help dialog to maintain screen/com updates.
   //
   // !!!  note:  this seems to cause random unhandled exception aborts
   //             maybe due to recursive calls to serve_gps().  Use the /kt
   //             command line option to not use this timer.
   if(enable_timer) {   
      SetTimer(hWnd, 0, 200, NULL);
      timer_set = 1;
   }
   sal_ok = 1;
   need_screen_init = 0;
}

void alarm_clock()
{
   if(alarm_file) PlaySoundA(ALARM_FILE, NULL, SND_ASYNC);
   else           PlaySoundA(CHORD_FILE, NULL, SND_ASYNC);
}

void cuckoo_clock()
{
char fn[128];

   if(singing_clock) {  // sing a song file
      sprintf(fn, SONG_NAME, pri_minutes);
      PlaySoundA(fn, NULL, SND_ASYNC);
      cuckoo_beeps = 0;
   }
   else if(chime_file) PlaySoundA(CHIME_FILE,  NULL, SND_ASYNC);
   else                PlaySoundA(NOTIFY_FILE, NULL, SND_ASYNC);
}

double GetMsecs()
{
   // we could use the high resolution peformance counter here
   // but the standard tick counter works fine
   return (double) (unsigned long) GetTickCount();
}


//****************************************************************************
//
// Window message receiver procedure for application
// 
// We implement a small keystroke queue so that the DOS kbhit()/getch()
// keyboard i/o model works with the Windows message queue i/o model.
// Keystrokes are put into the queue by the Windows message reciever proc.
// They are drained from the queue by the program keyboard handler.
//
//****************************************************************************

#define get_kbd() win_getch()
#define KBD_Q_SIZE 16
int kbd_queue[KBD_Q_SIZE+1];
int kbd_in, kbd_out;

void add_kbd(int key)
{
   if(++kbd_in >= KBD_Q_SIZE) kbd_in = 0;
   if(kbd_in == kbd_out) {  // kbd queue is full
      if(--kbd_in < 0) kbd_in = KBD_Q_SIZE-1;
   }
   else {  // put keystoke in the queue
      kbd_queue[kbd_in] = key;
   }
}

int win_kbhit(void)
{
   return (kbd_in != kbd_out);  // return true if anything is in the queue
}

int win_getch(void)
{
int key;   

   if(kbd_in == kbd_out) return 0;            // no keys in the queue

   if(++kbd_out >= KBD_Q_SIZE) kbd_out = 0;   // get to next queue entry
   key = kbd_queue[kbd_out];                  // get keystroke from queue
   return key;                                // return it
}


#endif  // WINDOWS



#ifdef WIN_VFX

void dot(int x, int y, u08 color)
{
   #ifdef DOS_MOUSE
      u08 m;
      m = mouse_hit(x,y, x,y);
   #endif

   VFX_io_done = 1;
   VFX_pixel_write(stage, x,y, palette[color]);

   #ifdef DOS_MOUSE
      if(m) update_mouse();
   #endif
}

u08 get_pixel(COORD x,COORD y)
{
S32 pixel;
int i;

   VFX_io_done = 1;

   pixel = VFX_pixel_read(stage, x,y) & 0xFFFF;
   for(i=0; i<16; i++) {  // convert screen value to color code
      if(((U32) pixel) == (palette[i]&0xFFFF)) return i;
   }
   return 0;
}

S32 string_width(C8 *text)
{
C8 ch;
S32 len = 0;

   while((ch = *text++) != 0) {
      len += VFX_character_width(vfx_font, ch);
   }

   return len;
}

void kill_screen(void)
{
// TODO: gracefully close the graphics window here

   VFX_io_done = 1;

   if(stage_window) {
      VFX_window_destroy(stage_window);
      stage_window = 0;
   }
   if(stage) {
      VFX_pane_destroy(stage);
      stage = 0;
   }
   if(screen_window) {
      VFX_window_destroy(screen_window);
      screen_window = 0;
   }
   if(screen) {
      VFX_pane_destroy(screen);
      screen = 0;
   }

   #ifdef DOS_VFX
      if(VFX_driver_loaded) VFX_shutdown_driver();
      VFX_driver_loaded = 0;
   #endif
}

void init_screen(void)
{
   #ifdef DOS_VFX
      void *DLL,*drvr;
      VFX_DESC *VFX;
      int i;

      config_screen();  // initialize screen rendering variables
      VFX_io_done = 1;
      kill_screen();    // release any screen data structures currently in use

      DLL = FILE_read("VESA768.DLL",NULL);

      if(DLL == NULL) {
         printf("Missing or invalid 386FX driver\n");
         exit(1);
      }

      drvr = DLL_load(DLL,DLLMEM_ALLOC | DLLSRC_MEM,NULL);

      if(drvr == NULL)  {
         printf("Invalid DLL image\n");
         exit(1);
      }

      free(DLL);

      //
      // Register the driver with the API
      //

      VFX_register_driver(drvr);

      VFX = VFX_describe_driver();

      SCREEN_WIDTH = VFX->scrn_width;
      SCREEN_HEIGHT = VFX->scrn_height;

//    printf("Screen width: %d\n",SCREEN_WIDTH);
//    printf("Screen height: %d\n",SCREEN_HEIGHT);
//    printf("\007");
//    fflush(stdout);
//    getch();
      VFX_init_driver();
      VFX_driver_loaded = 1;

      for(i=0; i<256; i++) palette[i] = i;
      transparent_font_CLUT[0] = 0;     //RGB_TRANSPARENT
      transparent_font_CLUT[1] = WHITE; //RGB_NATIVE(0,0,0);

//    VFX_rectangle_fill(stage, 0,0,  SCREEN_WIDTH-1,SCREEN_HEIGHT-1, LD_DRAW, 0);
//    refresh_page();
   #else
      config_screen();  // initialize screen rendering variables
      VFX_io_done = 1;
      kill_screen();    // release any screen data structures currently in use
      //set new video mode
      if(!VFX_set_display_mode(SCREEN_WIDTH,
                               SCREEN_HEIGHT,
                               16,
                               initial_window_mode,
                               TRUE))
      {  // big problem
         error_exit(4, "VFX cannot set video display mode");
      }


      // setup screen palette
      memset(palette,0xff,sizeof(palette));

      // Note: this palette should match the bmp_pal palette
      palette[BLACK      ] = RGB_NATIVE(0,     0,   0);    //0
      palette[DIM_BLUE   ] = RGB_NATIVE(0,     0, 255);    //1
      palette[DIM_GREEN  ] = RGB_NATIVE(0,    96,   0);    //2 - dim green
      palette[DIM_CYAN   ] = RGB_NATIVE(0,   192, 192);    //3 - dim cyan
      palette[DIM_RED    ] = RGB_NATIVE(128,   0,   0);    //4
//    palette[DIM_MAGENTA] = RGB_NATIVE(192,   0, 192);    //5 - dim magenta
      palette[DIM_MAGENTA] = RGB_NATIVE(140,  96,   0);    //5 - dim magenta is now grotty yellow
      palette[BROWN      ] = RGB_NATIVE(255,  64,  64);    //6
      palette[DIM_WHITE  ] = RGB_NATIVE(192, 192, 192);    //7 - dim white
      palette[GREY       ] = RGB_NATIVE(96,   96,  96);    //8
      palette[BLUE       ] = RGB_NATIVE(64,   64, 255);    //9
      palette[GREEN      ] = RGB_NATIVE(0,   255,   0);    //10
      palette[CYAN       ] = RGB_NATIVE(0,   255, 255);    //11
      palette[RED        ] = RGB_NATIVE(255,   0,   0);    //12
      palette[MAGENTA    ] = RGB_NATIVE(255,   0, 255);    //13
      palette[YELLOW     ] = RGB_NATIVE(255, 255,   0);    //14
      palette[WHITE      ] = RGB_NATIVE(255, 255, 255);    //15

      transparent_font_CLUT[0] = (U16) RGB_TRANSPARENT;
      transparent_font_CLUT[1] = (U16) RGB_NATIVE(0,0,0);
   #endif


   //
   // Setup text fonts
   //
   #ifdef VARIABLE_FONT
      vfx_font = VFX_default_system_font();
      font_height = VFX_font_height(vfx_font);
      if(font_height <= 8) small_font = 1;
      TEXT_HEIGHT = font_height*3/2;  // allow for line spacing
      em_width    = string_width("m");
      TEXT_WIDTH  = em_width;
   #else
      if(user_font_size == 0)  {
         if(big_plot && (ebolt == 0)) vfx_font = (VFX_FONT *) (void *) &h_medium_font[0];
         else vfx_font = (VFX_FONT *) (void *) &h_font_12[0];
      }
      else if(user_font_size <= 8)  vfx_font = (VFX_FONT *) (void *) &h_small_font[0];
      else if(user_font_size <= 12) vfx_font = (VFX_FONT *) (void *) &h_font_12[0];
      else if(user_font_size <= 14) vfx_font = (VFX_FONT *) (void *) &h_medium_font[0];
      else if(user_font_size <= 16) vfx_font = (VFX_FONT *) (void *) &h_large_font[0];
      else                          vfx_font = (VFX_FONT *) (void *) &h_font_12[0];

      font_height = VFX_font_height(vfx_font);
      TEXT_HEIGHT = font_height;
      em_width    = string_width("m");
      TEXT_WIDTH  = em_width;
      if(font_height <= 12) {
         small_font = 2;
      }
      else small_font = 0;
   #endif


   // allocate screen windows and buffers
   stage_window = VFX_window_construct(SCREEN_WIDTH, SCREEN_HEIGHT);

   stage = VFX_pane_construct(stage_window,
                              0,
                              0,
                              SCREEN_WIDTH-1, 
                              SCREEN_HEIGHT-1);

   #ifndef DOS_VFX
      VFX_assign_window_buffer(stage_window,
                               NULL,
                              -1);
   #endif

   screen_window = VFX_window_construct(SCREEN_WIDTH, SCREEN_HEIGHT);

   screen = VFX_pane_construct(screen_window,
                               0,
                               0,
                               SCREEN_WIDTH-1, 
                               SCREEN_HEIGHT-1);


   config_screen();  // re-initialize screen rendering variables
                     // to reflect any changes due to font size

   #ifdef DOS_MOUSE
      if((text_mode == 0) && (full_circle == 0) && (mouse_disabled == 0)) {
         dos_mouse_found = mouse_check();
         if(dos_mouse_found) init_dos_mouse();
      }
   #endif

   #ifdef BUFFER_LLA
      clear_lla_points();
   #endif

   VFX_rectangle_fill(stage, 0,0,  SCREEN_WIDTH-1,SCREEN_HEIGHT-1, LD_DRAW, 0);
   refresh_page();
}

void refresh_page(void)
{  
   // copies the virtual screen buffer to the physical screen
   // (or othwewise flips pages, etc for double buffered graphics)
   if(VFX_io_done == 0) { // nothing touched the screen since the last call
      return;             // so don't waste resources redrawing the page
   }
   VFX_io_done = 0;

   #ifdef DOS_VFX
      VFX_pane_copy(stage,0,0,screen,0,0,NO_COLOR);
      VFX_window_refresh(screen_window,0,0,SCREEN_WIDTH-1,SCREEN_HEIGHT-1);
   #else
      //
      // Lock the buffer and validate the VFX_WINDOW
      //
      VFX_lock_window_surface(screen_window,VFX_BACK_SURFACE);

      //
      // Copy entire staging pane to screen_window
      // 
      VFX_pane_copy(stage,0,0,screen,0,0,NO_COLOR);

      //
      // Release surface and perform page flip
      //
      VFX_unlock_window_surface(screen_window, TRUE);
   #endif
}


#define DEG_WIDTH (5+1)
int deg_char(COORD row, COORD col, u08 color)
{  
   #ifdef DOS_MOUSE
      u08 m;
      m = mouse_hit(col,row, col+TEXT_WIDTH,row+TEXT_HEIGHT);
   #endif

   // erase the old character cell (and probably a little extra)
   VFX_string_draw(stage,
                   col,
                   row,
                   vfx_font,
                   "  ",
                   transparent_font_CLUT);

   // draw the degrees symbol (which is not in the normal font)
   dot(col+2, row+0, color);
   dot(col+3, row+0, color);
   dot(col+1, row+1, color);
   dot(col+4, row+1, color);
   dot(col+1, row+2, color);
   dot(col+4, row+2, color);
   dot(col+2, row+3, color);
   dot(col+3, row+3, color);

   #ifdef DOS_MOUSE
      if(m) update_mouse();
   #endif

   return DEG_WIDTH;  // width
}

#ifdef VARIABLE_FONT
   #define UP_WIDTH (6+1)
   int up_char(COORD row, COORD col, u08 color)
   {  
      #ifdef DOS_MOUSE
         u08 m;
         m = mouse_hit(col,row, col+TEXT_WIDTH,row+TEXT_HEIGHT);
      #endif
      // erase the old character cell (and probably a little extra)
      VFX_string_draw(stage,
                      col,
                      row,
                      vfx_font,
                      "  ",
                      transparent_font_CLUT);

      // draw the up arrow symbol (which is not in the normal font)
      ++row;
      dot(col+3, row+0, color);
      dot(col+3, row+1, color);
      dot(col+3, row+2, color);
      dot(col+3, row+3, color);
      dot(col+3, row+4, color);
      dot(col+3, row+5, color);

      dot(col+2, row+1, color);
      dot(col+4, row+1, color);

      dot(col+1, row+2, color);
      dot(col+5, row+2, color);

      #ifdef DOS_MOUSE
         if(m) update_mouse();
      #endif

      return UP_WIDTH;  // width
   }

   #define DOWN_WIDTH (6+1)
   int down_char(COORD row, COORD col, u08 color)
   {  
      #ifdef DOS_MOUSE
         u08 m;
         m = mouse_hit(col,row, col+TEXT_WIDTH,row+TEXT_HEIGHT);
      #endif
      // erase the old character cell (and probably a little extra)
      VFX_string_draw(stage,
                      col,
                      row,
                      vfx_font,
                      "  ",
                      transparent_font_CLUT);

      // draw the down arrow symbol (which is not in the normal font)
      ++row;
      dot(col+3, row+0, color);
      dot(col+3, row+1, color);
      dot(col+3, row+2, color);
      dot(col+3, row+3, color);
      dot(col+3, row+4, color);
      dot(col+3, row+5, color);

      dot(col+2, row+4, color);
      dot(col+4, row+4, color);

      dot(col+1, row+3, color);
      dot(col+5, row+3, color);

      #ifdef DOS_MOUSE
         if(m) update_mouse();
      #endif

      return DOWN_WIDTH;  // width
   }
#endif  // VARIABLE_FONT


void vidstr(COORD row, COORD col, u08 attr, char *s)
{
char *p;
S32 px, py;
C8 work_str[256];
C8 *chp;
C8 ch;
S32 word_x[32];
S32 n_words;
S32 ux;
C8 last_uch;
C8 last_ch;
C8 uch;
S32 w;
S32 x;
u08 m;

   // this routine recognizes some global variables:
   //   blank_underscore: if set, converts underscore char to blanks
   //   graphics_coords:  if set, row and col are in pixels coordinates
   //                     if clear, they are in character coordinates
   //   no_x_margin:      if set, do not apply the "x" character margin offset
   //   no_y_margin:      if set, do not apply the "y" character margin offset
   //   print_using:      if not null, points to column alignment string

   if(stage == NULL) return;  // screen not initialized


   VFX_io_done = 1;

   if(graphics_coords) {
      px = col;
      py = row;
   }
   else {
      px = col * TEXT_WIDTH;
      py = row * TEXT_HEIGHT;

      if(py < (((PLOT_ROW/TEXT_HEIGHT)*TEXT_HEIGHT) - (TEXT_HEIGHT*2))) {
         if(no_x_margin == 0) px += TEXT_X_MARGIN;   // looks better with margins
         if(no_y_margin == 0) py += TEXT_Y_MARGIN;
      }
   }

   m = 0;
   #ifdef DOS_MOUSE
      m = mouse_hit(px,py, px+TEXT_WIDTH*strlen(s),py+TEXT_HEIGHT);
   #endif

   transparent_font_CLUT[0] = 0;             ////!!!!RGB_NATIVE(0,0,0);
   transparent_font_CLUT[1] = palette[attr];

   if(print_using == NULL) {  // we are not aligning columns
      strcpy(work_str, s);
      #ifdef VARIABLE_FONT
         strcat(work_str,"  ");
      #endif

      chp = work_str;
      while(*chp) {
         if(blank_underscore) {
            if(*chp == '_') *chp = ' ';
         }

         if(*chp == DEGREES) {
            px += deg_char(py,px, attr);
         }
#ifdef VARIABLE_FONT
         else if(*chp == DOWN_ARROW) {
            px += down_char(py,px, attr);
         }
         else if(*chp == UP_ARROW) {
            px += up_char(py,px, attr);
         }
#endif
         else {
            VFX_character_draw(stage,
                               px,
                               py, 
                               vfx_font,
                               *chp,
                               transparent_font_CLUT);

            px += VFX_character_width(vfx_font, *chp);
         }
         ++chp;
      }

      #ifdef DOS_MOUSE
         if(m) update_mouse();
      #endif
      return;
   }

   //
   // At each space->nonspace transition in *s, position cursor at equivalent
   // space->nonspace transition in *print_using
   //
   // This allows table-cell alignment with VFX's proportional font
   //
   // New kludge: the first column is right justified one space before the
   //             second column.
   //

   n_words = 0;
   ux = px;
   last_uch = '\0';

   while(*print_using) {
      uch = *print_using++;

      if(((uch != ' ') && (last_uch == ' ')) || (last_uch == '\0')) {
         word_x[n_words++] = ux;
      }

      last_uch = uch;
      if(blank_underscore) {
         if(uch == '_') uch = ' ';  // _ prints as a blank,  but does not cause a space transition
      }

      if(uch == DEGREES) ux += DEG_WIDTH;  //!!! kludge:  hardcoded char width
#ifdef VARIABLE_FONT
      else if(uch == UP_ARROW) ux += UP_WIDTH;  //!!! kludge:  hardcoded char width
      else if(uch == DOWN_ARROW) ux += DOWN_WIDTH;  //!!! kludge:  hardcoded char width
#endif
      else ux += VFX_character_width(vfx_font, uch);
   }

   w = 0;
   x = px;
   last_ch = '\0';

   while(*s) {
      ch = *s++;

      if((ch != ' ') && (last_ch == ' ') && (w < n_words)) {
         x = word_x[w++];
         if(w == 1) {  // right justify the first column
            x = word_x[w];   // x = where the second column starts
            x -= VFX_character_width(vfx_font, ' ');  // back up a space
            p = s-1;
            while((*p != ' ') && (*p != '\0')) {  // back up the width of the text
               x -= VFX_character_width(vfx_font, *p);
               ++p;
            }
         }
      }

      last_ch = ch;
      if(blank_underscore) {
         if(ch == '_') ch = ' ';
      }

      if(ch == DEGREES) {
         x += deg_char(py,x, attr);
      }
#ifdef VARIABLE_FONT
      else if(ch == UP_ARROW) {
         x += up_char(py,x, attr);
      }
      else if(ch == DOWN_ARROW) {
         x += down_char(py,x, attr);
      }
#endif
      else {
         VFX_character_draw(stage,
                            x,
                            py, 
                            vfx_font,
                            ch,
                            transparent_font_CLUT);

         x += VFX_character_width(vfx_font, ch);
      }
   }

   #ifdef DOS_MOUSE
      if(m) update_mouse();
   #endif
}

#endif // WIN_VFX



void find_endian(void)
{ 
u08 s[2];
u16 v;

   // determine the system byte ordering
   s[0] = 0x34;
   s[1] = 0x12;
   v = *((u16 *) (void *) &s[0]);
   v &= 0xFFFF;
   if((v&0xFFFF) != (0x1234&0xFFFF)) ENDIAN = 1; 
   else ENDIAN = 0;  // intel byte ordering
}

float fmt_temp(float t)
{
   // convert degrees C into user specified measurement system
   if     (DEG_SCALE == 'C') return t;
   else if(DEG_SCALE == 'F') return (t * 1.8F) + 32.0F;
   else if(DEG_SCALE == 'R') return (t + 273.15F) * 1.8F;
   else if(DEG_SCALE == 'K') return t + 273.15F;
   else if(DEG_SCALE == 'D') return (100.0F-t) * 1.5F;
   else if(DEG_SCALE == 'N') return t * 0.33F;
   else if(DEG_SCALE == 'O') return (t * (21.0F / 40.0F)) + 7.5F;
   else if(DEG_SCALE == 'E') return (t * (21.0F / 40.0F)) + 7.5F;
   else                      return t;
}

void end_log()
{
   if(log_file == 0) return;

   fprintf(log_file,"#\n#\n");

   if(com_errors) {
      printf("*** %lu COM PORT ERRORS!!!\n", com_errors);
      if(log_file) fprintf(log_file, "#*** %lu COM PORT ERRORS!!!\n", com_errors);
   }
   if(math_errors) {
      printf("*** %lu MATH ERRORS!!!\n", math_errors);
      if(log_file) fprintf(log_file, "#*** %lu MATH ERRORS!!!\n", math_errors);
   }
   if(1 || bad_packets) {
      printf("%lu TSIP packets processed.  %lu bad packets.\n", packet_count, bad_packets);
      if(log_file) fprintf(log_file, "#*** %lu TSIP packets processed.  %lu bad packets.\n", packet_count, bad_packets);
   }

   close_log_file();  // close the log file
}


void shut_down(int reason)
{
   // clean up in preparation for program exit
#ifdef WINDOWS
   if(timer_set) KillTimer(hWnd, 0);
#endif

#ifdef ADEV_STUFF
   log_adevs();          // write adev info to the log file
#endif

   end_log();            // close out the log file

   #ifdef SIG_LEVELS
      if(0 && plot_signals) {
         dump_signals("tbolt.sig");    ////!!!! debug
      }
   #endif

   #ifdef PRECISE_STUFF
      close_lla_file();  // close the lat/lon/alt file
   #endif

   #ifdef TEMP_CONTROL
      disable_temp_control();
   #endif

   #ifdef OSC_CONTROL
      disable_osc_control();
   #endif

   kill_com();        // turn off incoming interrupts
   kill_screen();     // kill graphics mode/window, return to text

   #ifdef EMS_MEMORY
      if(plot_pid != 0xDEAD) ems_free(plot_pid);
      if(adev_pid != 0xDEAD) ems_free(adev_pid);
      if(hash_pid != 0xDEAD) ems_free(hash_pid);
   #endif

   exit(reason);
}

void error_exit(int num,  char *s)
{
   #ifdef DOS_BASED
      printf("*** ERROR: %s\n", s);
   #endif

   #ifdef WINDOWS
      break_flag = 1;  // inhibit cascading error messages, since WM_TIMER keeps running during SAL_alert_box()
      SAL_alert_box("Error", s);
   #endif

   if(num) shut_down(num);
}



//
//  for increased performance these routines could be made OS dependent
//

void line(COORD x1,COORD y1, COORD x2,COORD y2, u08 color)
{
u08 m;

#ifdef WIN_VFX
   m = 0;
   #ifdef DOS_MOUSE
      m = mouse_hit(x1,y1, x2,y2);
   #endif

   VFX_io_done = 1;

   VFX_line_draw(stage,
                 x1,y1,
                 x2,y2,
                 LD_DRAW,
                 palette[color]);


   #ifdef DOS_MOUSE
      if(m) update_mouse();
   #endif
#endif

#ifdef DOS
int i1, i2;
int d1, d2, d;

   m = 0;
   i1 = i2 = 1;

   d1 = (int) x2 - (int) x1;
   if(d1 < 0) {
      i1 = (-1);
      d1 = 0 - d1;
   }

   d2 = (int) y2 - (int) y1;
   if(d2 < 0) {
      i2 = (-1);
      d2 = 0 - d2;
   }

   if(d1 > d2) {
      d = d2 + d2 - d1;
      while(1) {
         dot(x1,y1, color);
         if(x1 == x2) break;
         if(d >= 0) {
            d = d - d1 - d1;
            y1 += i2;
         }
         d = d + d2 + d2;
         x1 += i1;
      }
   }
   else {
      d = d1 + d1 - d2;
      while (1) {
         dot(x1,y1, color);
         if(y1 == y2) break;
         if(d >= 0) {
            d = d - d2 - d2;
            x1 += i1;
         }
         d = d + d1 + d1;
         y1 += i2;
      }
   }
#endif
}

void xthick_line(COORD x1,COORD y1,  COORD x2,COORD y2, u08 color, u08 thickness)
{
COORD x,y;

   if(thickness == 0) thickness = 1;

   for(x=0; x<thickness; x++) {
      for(y=0; y<thickness; y++) {
         line(x1+x,y1+y, x2+x,y2+y, color);
      }
   }
}

void thick_line(COORD x1,COORD y1,  COORD x2,COORD y2, u08 color, u08 thickness)
{
COORD y;
int i1, i2;
int d1, d2, d;
u08 m;

   m = 0;
   #ifdef DOS_MOUSE
      m = mouse_hit(x1,y1, x2,y2);
   #endif

   if(thickness <= 0) thickness = 1;
   i1 = i2 = 1;

   d1 = (int) x2 - (int) x1;
   if(d1 < 0) {
      i1 = (-1);
      d1 = 0 - d1;
   }

   d2 = (int) y2 - (int) y1;
   if(d2 < 0) {
      i2 = (-1);
      d2 = 0 - d2;
   }

   if(d1 > d2) {
      d = d2 + d2 - d1;
      while(1) {
         for(y=y1; y<y1+thickness; y++) {
            line(x1,y, x1+thickness-1,y, color);
         }
         if(x1 == x2) break;
         if(d >= 0) {
            d = d - d1 - d1;
            y1 += i2;
         }
         d = d + d2 + d2;
         x1 += i1;
      }
   }
   else {
      d = d1 + d1 - d2;
      while (1) {
         for(y=y1; y<y1+thickness; y++) {
            line(x1,y, x1+thickness-1,y, color);
         }
         if(y1 == y2) break;
         if(d >= 0) {
            d = d - d2 - d2;
            x1 += i1;
         }
         d = d + d1 + d1;
         y1 += i2;
      }
   }

   #ifdef DOS_MOUSE
      if(m) update_mouse();
   #endif
}


void erase_plot(int full_plot) 
{
   if(text_mode) return;
   if(full_circle) return;

   #ifdef WIN_VFX
      VFX_io_done = 1;
      // erase area to the left of the plot area
      if(PLOT_COL > PLOT_LEFT) {
         VFX_rectangle_fill(stage, PLOT_LEFT,PLOT_ROW, PLOT_COL-1,SCREEN_HEIGHT-1, LD_DRAW, RGB_TRIPLET(0,0,0));
      }

      if(plot_background) { // slightly highlight the plot background color
         VFX_rectangle_fill(stage, PLOT_COL,PLOT_ROW, PLOT_COL+PLOT_WIDTH-1,SCREEN_HEIGHT-1, LD_DRAW, RGB_TRIPLET(0,0,35));
      }
      else {  // black background
         VFX_rectangle_fill(stage, PLOT_COL,PLOT_ROW, PLOT_COL+PLOT_WIDTH-1,SCREEN_HEIGHT-1, LD_DRAW, RGB_TRIPLET(0,0,0));
      }

      // erase area to the right of the plot area
      if(full_plot && ((PLOT_COL+PLOT_WIDTH) < SCREEN_WIDTH)) {
         VFX_rectangle_fill(stage, PLOT_COL+PLOT_WIDTH,PLOT_ROW, SCREEN_WIDTH-1,SCREEN_HEIGHT-1, LD_DRAW, RGB_TRIPLET(0,0,0));
      }
   #endif

   #ifdef DOS
      if(full_plot) dos_erase(PLOT_LEFT,PLOT_ROW-TEXT_HEIGHT*3,  SCREEN_WIDTH-1,SCREEN_HEIGHT-1);
      else          dos_erase(PLOT_LEFT,PLOT_ROW-TEXT_HEIGHT*3,  PLOT_LEFT+PLOT_WIDTH-1,SCREEN_HEIGHT-1);
   #endif
}

void erase_help() 
{
   if(text_mode) {
      erase_screen();
      return;
   }

   #ifdef WIN_VFX
      VFX_io_done = 1;
      VFX_rectangle_fill(stage, PLOT_LEFT,PLOT_ROW, SCREEN_WIDTH-1,SCREEN_HEIGHT-1, LD_DRAW, RGB_TRIPLET(0,0,0));
   #endif

   #ifdef DOS
      dos_erase(PLOT_LEFT,PLOT_ROW-TEXT_HEIGHT*3,  SCREEN_WIDTH-1,SCREEN_HEIGHT-1);
   #endif
}


void erase_lla() 
{
   if(text_mode) return;
   if(full_circle) return;

   #ifdef WIN_VFX
      VFX_io_done = 1;
      VFX_rectangle_fill(stage, LLA_COL,LLA_ROW, LLA_COL+LLA_SIZE-1,LLA_ROW+LLA_SIZE-1, LD_DRAW, RGB_TRIPLET(0,0,0));
   #endif

   #ifdef DOS
      dos_erase(LLA_COL,LLA_ROW, LLA_COL+LLA_SIZE-1,LLA_ROW+LLA_SIZE-1);
   #endif
}


void erase_screen()
{
   #ifdef WIN_VFX
      VFX_io_done = 1;
      VFX_rectangle_fill(stage, 0,0, SCREEN_WIDTH-1,SCREEN_HEIGHT-1, LD_DRAW, RGB_TRIPLET(0,0,0));
   #endif

   #ifdef DOS
      dos_erase(0,0, SCREEN_WIDTH-1,SCREEN_HEIGHT-1);
   #endif

   #ifdef PRECISE_STUFF
      plot_lla_axes();
   #endif
}



// Note:  you need to keep this palette and the VFX RGB_NATIVE palette the same
u08 bmp_pal[] = {  // blue, green, red, filler
     0,    0,  0,     0,
   255,    0,  0,     0,
     0,   96,  0,     0,
   192,  192,  0,     0,
     0,    0,  128,   0,
// 192,    0,  192,   0,     // dim_magenta
     0,   96,  140,   0,     // dim_magenta is now grotty yellow
    64,   64,  255,   0,
   192,  192,  192,   0,
    96,   96,  96,    0,
   255,   64,  64,    0,
     0,  255,  0,     0,
   255,  255,  0,     0,
     0,    0,  255,   0,
   255,    0,  255,   0,
     0,  255,  255,   0,
   255,  255,  255,   0,
     0,  168,  192,   0   // bmp_yellow - special color used to print yellow on white background
};

#define BM_HEADER_SIZE (0x0036 + 16*4)

//
//   !!! Note:  This code might not be non-Intel byte ordering ENDIAN compatible
//

FILE *bmp_file;

void dump_byte(u08 b)
{
   fwrite(&b, 1, 1, bmp_file);
}

void dump_word(u16 w)
{
   dump_byte((u08) w);
   dump_byte((u08) (w>>8));
}

void dump_dword(u32 d)
{
   dump_byte(d);
   dump_byte(d>>8);
   dump_byte(d>>16);
   dump_byte(d>>24);
}

int dump_bmp_file(int invert)
{
int x, y, c;

   // main header
   dump_byte('B');   dump_byte('M');                          // magic number
   dump_dword(SCREEN_HEIGHT*SCREEN_WIDTH/2L+BM_HEADER_SIZE);  // file size
   dump_dword(0L);              // reserved
   dump_dword(BM_HEADER_SIZE);  // offset of bitmap data in the file

   // info header
   dump_dword(40L);             // info header length
   dump_dword(SCREEN_WIDTH);    // screen size
   dump_dword(SCREEN_HEIGHT);
   dump_word(1);                // number of planes
   dump_word(4);                // bits per pixel
   dump_dword(0L);              // no compression
   dump_dword(SCREEN_WIDTH*SCREEN_HEIGHT/2L);// picture size in bytes
   dump_dword(0L);              // horizontal resolution
   dump_dword(0L);              // vertical resolution 
   dump_dword(16L);             // number of used colors
   dump_dword(16L);             // number of important colors

   // palette
   for(c=0; c<16; c++) {
      x = (c << 2);
      if(invert) {
         if     (c == WHITE)  x = (BLACK << 2);
         else if(c == BLACK)  x = (WHITE << 2);
         else if(c == YELLOW) x = (BMP_YELLOW << 2);
      }
      dump_byte(bmp_pal[x++]);
      dump_byte(bmp_pal[x++]);
      dump_byte(bmp_pal[x++]);
      dump_byte(bmp_pal[x]);
   }

   for(y=SCREEN_HEIGHT-1; y>=0; y--) {
      update_pwm();
      for(x=0; x<SCREEN_WIDTH; x+=2) {
         c = (get_pixel(x, y) & 0x0F) << 4;
         c |= (get_pixel(x+1, y) & 0x0F);
         dump_byte(c);
      }
   }

   return 1;
}

int dump_screen(int invert, char *fn)
{
   strcpy(out, fn);
   if(strstr(out, ".") == 0) {
      #ifdef GIF_FILES
         strcat(out, ".GIF");
      #else
         strcat(out, ".BMP");
      #endif
   }
   strlwr(out);

   bmp_file = fopen(out, "wb");
   if(bmp_file == 0) return 0;

   if(do_dump == 0) {  // keyboard commanded dump
      BEEP();          // beep because this can take a while to do
   }

   #ifdef GIF_FILES
      if(strstr(out, ".gif")) {
         dump_gif_file(invert, bmp_file);
      }
      else 
   #endif
   {
      dump_bmp_file(invert);
   }

   fclose(bmp_file);
   bmp_file = 0;

   if(do_dump == 0) {
      BEEP();
   }
   return 1;
}


#ifdef EMS_MEMORY

#define EMS_PAGE_SIZE  16384L
#define MAX_EMS_FRAMES 4
#define EMS_INT 0x67
char emm_id[] = "EMMXXXX0";

u16 ems_pid;
u16 ems_frame = (-1);
void far *ems_ptr;


#define PQ_PER_PAGE     (EMS_PAGE_SIZE/sizeof(struct PLOT_Q))
#define AQ_PER_PAGE     (EMS_PAGE_SIZE/sizeof(struct ADEV_Q))
#define HASHES_PER_PAGE (EMS_PAGE_SIZE/sizeof(unsigned long))


u16 plot_frame;
u16 current_plot_page;
unsigned adev_frame;
unsigned current_adev_page;
unsigned hash_frame;
unsigned current_hash_page;


u16 ems_version()
{
u32 ems_vector;
int i;

   reg.h.ah = 0x35;     /* get the INT 67 EMS handler vector */
   reg.h.al = EMS_INT;
   seg.es = 0;
   reg.wd.bx = 0;
   int86x(0x21, &reg, &reg, &seg);
   ems_vector = seg.es;
   ems_vector <<= 16;
   ems_vector |= reg.wd.bx;
   if(ems_vector == 0) {
      return 0;    /* null int 67 vector */
   }

   ems_vector &= 0XFFFF0000;
   ems_vector |= 0x000A;
   for(i=0; i<8; i++) {
      if(((char far *) (void far *) ems_vector)[i] != emm_id[i]) return 0;
   }

   reg.h.ah = 0x46;
   int86x(EMS_INT, &reg, &reg, &seg);

   if(reg.h.ah) {
      return 0;
   }
   else {
      return reg.h.al;
   }
}

u16 ems_avail()    /* return number of EMS pages available, 0 if none */
{
   reg.h.ah = 0x42;
   int86x(EMS_INT, &reg, &reg, &seg);
   if(reg.h.ah) return 0;

   return reg.wd.bx;
}

u16 ems_pageframe()   /* return base seg addr of EMS page frame, 0 if error */
{
   reg.h.ah = 0x41;
   int86x(EMS_INT, &reg, &reg, &seg);
   if(reg.h.ah) return 0;

   return reg.wd.bx;
}

u16 ems_create(u16 size)  /* allocate a PID of given size, return handle (0 if error) */
{
   reg.h.ah = 0x43;
   reg.wd.bx = size;
   int86x(EMS_INT, &reg, &reg, &seg);
   if(reg.h.ah) return 0xDEAD;

   return reg.wd.dx;
}

u16 ems_free(u16 pid)   /* return the EMS handle and free the memory */
{
   if(pid) {
      reg.h.ah = 0x45;
      reg.wd.dx = pid;
      int86x(EMS_INT, &reg, &reg, &seg);
      if(reg.h.ah == 0) {   /* we did it all OK */
         ems_pid = 0;
         return 1;
      }
   }

   return 0;
}

void far *ems_alloc(long count, long size)
{
u16 ems_level;
u16 ems_seg;
u16 ems_size;
u16 pages_needed;
u32 x;

   ems_pid = 0xDEAD;
   if(size == 0) return 0;

   pages_needed = (EMS_PAGE_SIZE / size);  // number of entries per page
   if(pages_needed == 0) return 0;
   pages_needed = (count / pages_needed) + 1;

   ems_level = ems_version();
   if(ems_level == 0) return 0;

   ems_seg = ems_pageframe();
   if(ems_seg == 0) return 0;

   ems_size = ems_avail();
   if(ems_size < pages_needed) return 0;   /* must have at least 128Kb of avail EMS memory */

   ems_pid = ems_create(pages_needed);    /* allocate the memory pages */
   if(ems_pid == 0xDEAD) return 0;

   if(++ems_frame >= MAX_EMS_FRAMES) {
      ems_free(ems_pid);
      return 0;
   }

   x = ems_seg;
   x <<= 16;
   x |= (ems_frame * EMS_PAGE_SIZE);

   ems_ptr = (void far *) x;
   return ems_ptr;
}

void ems_init()
{
   if(ems_ok) ems_ok = ems_version();
   if(ems_ok) {
      max_ems_size = ems_avail();
      if(max_ems_size <= 1) ems_ok = 0;
      else max_ems_size = (max_ems_size - 1) * PQ_PER_PAGE;
   }
}
#endif   // EMS_MEMORY


//
//
//  Data queue support
//
//
struct PLOT_Q get_plot_q(long i)
{
   #ifdef EMS_MEMORY
      u16 ems_page, ems_ofs;

      if(plot_pid != 0xDEAD) {
         ems_page = i / PQ_PER_PAGE;
         ems_ofs  = i % PQ_PER_PAGE;
         if(ems_page != current_plot_page) {
            reg.wd.ax = 0x4400 | plot_frame;
            reg.wd.bx = ems_page;
            reg.wd.dx = plot_pid;

            int86x(EMS_INT, &reg, &reg, &seg);
            if(reg.h.ah) {     /* map the page in to its place failed */
               sprintf(out, "*** EMS ERROR %02X: map %04X:%04X to page %02X.\n", reg.h.ah, ems_page, 0, 0);
               error_exit(90, out);
            }
            current_plot_page = ems_page;
         }
if(graph_lla == 0) plot_q[ems_ofs].data[ZZZ] = plot_q[ems_ofs].data[TEMP] - (plot_q[ems_ofs].data[DAC]*d_scale);  //// !!!!
         return *(&plot_q[ems_ofs]);
      }
   #endif

if(graph_lla == 0) plot_q[i].data[ZZZ] = plot_q[i].data[TEMP] - (plot_q[i].data[DAC]*d_scale);  //// !!!!
   return plot_q[i];
}


void put_plot_q(long i, struct PLOT_Q q)
{
   #ifdef EMS_MEMORY
      u16 ems_page, ems_ofs;

      if(plot_pid != 0xDEAD) {
         ems_page = i / PQ_PER_PAGE;
         ems_ofs  = i % PQ_PER_PAGE;
         if(ems_page != current_plot_page) {
            reg.wd.ax = 0x4400 | plot_frame;
            reg.wd.bx = ems_page;
            reg.wd.dx = plot_pid;

            int86x(EMS_INT, &reg, &reg, &seg);
            if(reg.h.ah) {     /* map the page in to its place failed */
               sprintf(out, "*** EMS ERROR %02X: map %04X:%04X to page %02X.\n", reg.h.ah, ems_page, 0, 0);
               error_exit(91, out);
            }
            current_plot_page = ems_page;
         }
         *(&plot_q[ems_ofs]) = q;
         return;
      }
   #endif

   plot_q[i] = q;
   return;
}

void clear_plot_entry(long i)
{
struct PLOT_Q q;
int j;

   for(j=0; j<NUM_PLOTS; j++) q.data[j] = 0.0;

   q.sat_flags = 0;
   q.hh = 0;
   q.mm = 0;
   q.ss = 0;
   q.dd = 0;
   q.mo = 0;
   q.yr = 0;

   put_plot_q(i, q);

   if(i) {   // clear q entry marker, if set
      for(j=0; j<MAX_MARKER; j++) { 
         if(mark_q_entry[j] == i) mark_q_entry[j] = 0;
      }
   }
}

void free_adev()
{
   if(adev_q == 0) return;

   #ifdef DOS
      #ifdef EMS_MEMORY
         if(adev_pid != 0xDEAD) {
            ems_free(adev_pid);
            adev_pid = 0xDEAD;
            adev_q = 0;
            return;
         }
      #endif

      hfree(adev_q);
      adev_q = 0;
      return;
   #else
      free(adev_q);
      adev_q = 0;
      return;
   #endif
}

void alloc_adev()
{
#ifdef ADEV_STUFF
long i;

   // allocate memory for the adev data queue 
   free_adev();
   if(adev_q) return;  // memory already allocated

   if(ems_ok) {
      if(user_set_adev_size == 0) adev_q_size = 33000L;
   }
   else {
     #ifdef DOS
        #ifdef DOS_EXTENDER
           if(user_set_adev_size == 0) adev_q_size = 33000L;
        #else
           if(user_set_adev_size == 0) adev_q_size = 22000L;
        #endif
     #endif
   }

   ++adev_q_size;  // add an extra entry for overflow protection
   #ifdef DOS
      #ifdef EMS_MEMORY
         // adev queue is not suitible for EMS memory because in OADEV
         // mode,  the accesses are not localized and lots of time gets
         // wasted swapping pages
         adev_q = 0; //!!! ems_alloc(adev_q_size+1L, (long) sizeof (struct ADEV_Q));
         if(adev_q) { 
            adev_pid = ems_pid;
            adev_frame = ems_frame;
            current_adev_page = 0xFFFF;
         }
      #endif
      if(adev_q == 0) {  // EMS memory not available,  use normal memort
         i = sizeof (struct ADEV_Q);
         i *= (adev_q_size+1L);
         adev_q = (struct ADEV_Q HUGE *) (void HUGE *) halloc(i, 1);
      }
   #else
       i = 0L;
       adev_q = (struct ADEV_Q *) calloc(adev_q_size+1L, sizeof (struct ADEV_Q));
   #endif

   if(adev_q == 0) {
      sprintf(out, "Could not allocate %ld x %d byte adev queue",
                    adev_q_size+1L, sizeof (struct ADEV_Q));
      error_exit(0, out);
      exit(5);
   }
#endif
}


void free_plot()
{
   if(plot_q == 0) return;

   #ifdef DOS
      #ifdef EMS_MEMORY
         if(plot_pid != 0xDEAD) {
            ems_free(plot_pid);
            plot_pid = 0xDEAD;
            plot_q = 0;
            return;
         }
      #endif

      hfree(plot_q);
      plot_q = 0;
      return;
   #else
      free(plot_q);
      plot_q = 0;
      return;
   #endif
}

void alloc_plot()
{
long i;

   // allocate memory for the plot data queue 
   free_plot();
   if(plot_q) return; // plot queue memory already allocated

   #ifdef DOS
      #ifdef EMS_MEMORY
         if(ems_ok) {
            if(user_set_plot_size == 0) plot_q_size = 3L*24L*3600L;  // go for three days
            if(plot_q_size > max_ems_size) plot_q_size = max_ems_size;
            plot_q = ems_alloc(plot_q_size+1L, (long) sizeof (struct PLOT_Q));
         }
         else {
            if(user_set_plot_size == 0) {
                #ifdef DOS_EXTENDER
                   plot_q_size = 3L*24L*3600L;  // go for two hours  
                #else 
                   plot_q_size = 2L*3600L;  // go for two hours  
                #endif
            }
         }

         if(plot_q) {
            plot_pid = ems_pid;
            plot_frame = ems_frame;
            current_plot_page = 0xFFFF;
         }
      #endif  // EMS_MEMORY
      if(plot_q == 0) {  // EMS memory not available,  try the plain stuff
         i = sizeof (struct PLOT_Q);
         i *= (plot_q_size+1L);
         plot_q = (struct PLOT_Q HUGE *) (void HUGE *) halloc(i, 1);
      }
   #else
       plot_q = (struct PLOT_Q *) calloc(plot_q_size+1L, sizeof (struct PLOT_Q));
       if(plot_q == 0) i = 0;
       else            i = 1;
   #endif

   if(plot_q == 0) {
      sprintf(out, "Could not allocate %ld x %d byte plot queue",
                    plot_q_size+1L, sizeof (struct PLOT_Q));
      error_exit(0, out);
      exit(6);
   }
}

void alloc_gif()
{
#ifdef GIF_FILES
long i;

   // allocate memory for the .GIF file encoder
   if(hash_table) return;  // memory alrady allocated

   #ifdef DOS
       #ifdef EMS_HASH
          // EMS hash table is unusably slow...
          if(ems_ok) {
             hash_table = ems_alloc(HT_SIZE, (long) sizeof (unsigned long));
          }

          if(hash_table) {
             hash_pid = ems_pid;
             hash_frame = ems_frame;
             current_hash_page = 0xFFFF;
          }
       #endif
       if(hash_table == 0) {  // EMS memory not available,  try the plain stuff
          i = sizeof (unsigned long);
          i *= (HT_SIZE+1L);
          hash_table = (unsigned long HUGE *) (void HUGE *) halloc(i, 1);
       }
   #else
       hash_table = (unsigned long *) calloc(HT_SIZE+1L, sizeof (unsigned long));
       if(hash_table == 0) i = 0;
       else                i = 1;
   #endif

   if(hash_table == 0) {
      sprintf(out, "Could not allocate %ld x %d byte GIF hash table",
                    HT_SIZE+1L, sizeof (unsigned long));
      error_exit(0, out);
      exit(6);
   }
#endif
}


void free_fft()
{
#ifdef DOS
   if(tsignal) hfree(tsignal);
   if(fft_out) hfree(fft_out);
   if(w) hfree(w);
   if(cf) hfree(cf);
#else
   if(tsignal) free(tsignal);
   if(fft_out) free(fft_out);
   if(w) free(w);
   if(cf) free(cf);
#endif

   tsignal = 0;
   fft_out = 0;
   w = 0;
   cf = 0;
}


void alloc_fft()
{
#ifdef FFT_STUFF
long i;

   // allocate memory for the FFT routine 
   free_fft();
   if(max_fft_len == 0) max_fft_len = 1024;

   #ifdef DOS
      if(tsignal == 0) {  
         i = sizeof (float);
         i *= (max_fft_len+2);
         tsignal = (float BIGUN *) (void BIGUN *) halloc(i, 1);
      }
      if(fft_out == 0) {  
         i = sizeof (COMPLEX);
         i *= (max_fft_len/2+2);
         fft_out = (COMPLEX BIGUN *) (void BIGUN *) halloc(i, 1);
      }
      if(w == 0) {  
         i = sizeof (COMPLEX);
         i *= (max_fft_len/2+2);
         w = (COMPLEX BIGUN *) (void BIGUN *) halloc(i, 1);
      }
      if(cf == 0) {  
         i = sizeof (COMPLEX);
         i *= (max_fft_len/2+2);
         cf = (COMPLEX BIGUN *) (void BIGUN *) halloc(i, 1);
      }
      if((tsignal == 0) || (fft_out == 0) || (w == 0) || (cf == 0)) {
          sprintf(out, "Could not allocate FFT tables");
          printf("*** ERROR: %s\n", out);
          exit(666);
      }
   #else
      if(tsignal == 0) {  
         i = sizeof (float);
         i *= (max_fft_len+2);
         tsignal = (float *) (void *) calloc(i, 1);
      }
      if(fft_out == 0) {  
         i = sizeof (COMPLEX);
         i *= (max_fft_len/2+2);
         fft_out = (COMPLEX *) (void *) calloc(i, 1);
      }
      if(w == 0) {  
         i = sizeof (COMPLEX);
         i *= (max_fft_len/2+2);
         w = (COMPLEX *) (void *) calloc(i, 1);
      }
      if(cf == 0) {  
         i = sizeof (COMPLEX);
         i *= (max_fft_len/2+2);
         cf = (COMPLEX *) (void *) calloc(i, 1);
      }
      if((tsignal == 0) || (fft_out == 0) || (w == 0) || (cf == 0)) {
         sprintf(out, "Could not allocate FFT tables");
         error_exit(0, out);
         exit(666);
      }
   #endif
#endif
}

void alloc_memory()
{
   if(ems_ok) printf("%lu EMS plot queue entries available\n", max_ems_size);
   printf("\nInitializing memory...");

   alloc_adev();
   alloc_plot();
   alloc_gif();
   alloc_fft();

   reset_queues(0x03);

   printf("\nDone...\n\n");
}

void reset_marks(void)
{
int i;

   for(i=0; i<MAX_MARKER; i++) {  // clear all the plot markers
      mark_q_entry[i] = 0;
   }
}

void reset_queues(int queue_type)
{
#ifdef ADEV_STUFF
struct ADEV_Q q;

   if(queue_type & 0x01) { // reset adev queue
      adev_q_in = 0;
      adev_q_out = 0;
      adev_q_count = 0;
      adev_time = 0;
      pps_adev_time = 0; // ADEV_DISPLAY_RATE;
      osc_adev_time = 0; // ADEV_DISPLAY_RATE;
      last_bin_count = 0;
      pps_bins.bin_count = 0;
      osc_bins.bin_count = 0;

      pps_base_value = 0.0;
      osc_base_value = 0.0;

      q.pps = (OFS_SIZE) (0.0 - pps_base_value); // clear first adev queue entry
      q.osc = (OFS_SIZE) (0.0 - osc_base_value);
      put_adev_q(adev_q_in, q);  // clear first adev queue entry

      reset_adev_bins();
   }
#endif

   if(queue_type & 0x02) {  // reset plot queue
      plot_q_in = plot_q_out = 0;
      plot_q_count = 0;
      plot_q_full = 0;
      plot_time = 0;
      plot_start = 0;
      plot_column = 0;
      stat_count = 0.0F;
      clear_plot_entry((long) plot_q_in);

      plot_title[0] = 0;
      title_type = NONE;
      reset_marks();
   }
}

void new_queue(int queue_type)
{
   // flush the queue contents and start up a new data capture
   reset_queues(queue_type);
   log_loaded = 0;
   end_review(1);
   redraw_screen();
}

long find_event(long i, u08 flags)
{
struct PLOT_Q q;

   // locate the next holdover event or time sequence error in the plot queue
   while(i >= plot_q_size) i -= plot_q_size;
   while(i != plot_q_in) {  // skip over leading queue entries that match the flags
      q = get_plot_q(i);
      if((q.sat_flags & flags) == 0) break;
      ++i;
   }
   if(i == plot_q_in) return i;  // event not found

   while(i != plot_q_in) {  // find the next occurance of the event
      q = get_plot_q(i);
      if((q.sat_flags & flags) != 0) break;
      ++i;
   }

   return i;
}

long goto_event(u08 flags)
{
long val;

   // move plot to the next holdover event or time sequence error

   val = find_event(last_mouse_q, flags);
// val -= (PLOT_WIDTH/2)*view_interval;  // center point on screen
   val -= ((mouse_x * view_interval) / (long) plot_mag); // center point on mouse
   if(val > plot_q_count) val = plot_q_count - 1;
   if(val < 0) val = 0;
   last_mouse_q = val;
   zoom_review(val, 1);

   return val;
}


long next_q_point(long i, int stop_flag)
{
long j;
int wrap;

   // locate the next data point in the plot queue 
   // based upon the display view interval

   plot_column += plot_mag;
   if(stop_flag && (plot_column >= PLOT_WIDTH)) {  // we are at end of plot area
      plot_column -= plot_mag;
      return (-2L); 
   }
   wrap = 0;

   if(slow_q_skips) { //!!! ineffcient way to step over multiple queue entries,  but bug resistant
      j = view_interval;
      while(j--) { 
         if(++i == plot_q_in) return (-1L);
         while(i >= plot_q_size) i -= plot_q_size;
      }
   }
   else {  // more efficent way to do it,  but there might be bugs...
      if(i <= plot_q_in) {
         i += view_interval;
         if(i >= plot_q_in) return (-1L);
      }
      else if(i > plot_q_in) {
         i += view_interval;
         if(i <= plot_q_in) return (-1L);
      }
      while(i >= plot_q_size) {
         i -= plot_q_size;
         ++wrap;
      }
   }
   if(1 && wrap && (i >= plot_q_out)) return (-1L);
   if(i == plot_q_out) return (-1L);

   return i;
}



#ifdef BACK_FILTER
struct PLOT_Q filter_plot_q(long point)
{
struct PLOT_Q avg;
struct PLOT_Q q;
float count;
long i;
int j;

   // average the previous "filter_count" queue entries
   if(plot_q_count < filter_count) i = plot_q_count;
   else                            i = filter_count;

   point -= i;
   if(point < 0) point += plot_q_size;
   if(point >= plot_q_count) point = 0;

   avg = get_plot_q(point);
   count = 1.0F;

   while(--i) {
      ++point;
      while(point >= plot_q_size) point -= plot_q_size;

      q = get_plot_q(point);

      for(j=0; j<NUM_PLOTS; j++) {
         if(j != FFT) avg.data[j] += q.data[j];
      }

      count += 1.0F;
   }

   for(j=0; j<NUM_PLOTS; j++) {
      if(j != FFT) avg.data[j] /= count;
   }

   return avg;
}
#else
struct PLOT_Q filter_plot_q(long point)
{
struct PLOT_Q avg;
struct PLOT_Q q;
float count;
long i;
int j;

   // average the next "filter_count" queue entries
   avg = get_plot_q(point);
   if(point == plot_q_in) return avg;

   count = 1.0F;
   for(i=1; i<filter_count; i++) {
      if(++point == plot_q_in) break;
      while(point >= plot_q_size) point -= plot_q_size;

      q = get_plot_q(point);

      for(j=0; j<NUM_PLOTS; j++) {
         if(j != FFT) avg.data[j] += q.data[j];
      }

      count += 1.0F;
   }
      
   for(j=0; j<NUM_PLOTS; j++) {
      if(j != FFT) avg.data[j] /= count;
   }

   return avg;
}
#endif


//
//
//   Screen format configuration stuff
//
//

void config_undersized()
{
   // setup for 640x480 screen
   if(user_font_size > 12) user_font_size = 12;
   #ifdef DOS
      user_font_size = 8;
   #endif

   HORIZ_MAJOR = 30;
   HORIZ_MINOR = 5;
   VERT_MAJOR = 20;
   VERT_MINOR = 4;
   COUNT_SCALE = (VERT_MAJOR/2);

   if(user_font_size && (user_font_size <= 8)) PLOT_ROW = SCREEN_HEIGHT-VERT_MAJOR*8;
   else PLOT_ROW = SCREEN_HEIGHT-VERT_MAJOR*6;
}

void config_small()
{
   // setup for 800x600 screen
   PLOT_ROW = (400+8);

   FILTER_ROW = 17;
   FILTER_COL = INFO_COL;

   COUNT_SCALE = (VERT_MAJOR/2);

   AZEL_SIZE = LLA_SIZE = (160);       // size of the az/el map area

   if(TEXT_HEIGHT >= 16) eofs = 0;
}


void config_azel()
{
   // how and where to draw the azimuth/elevation map (and analog watch)
   // assume azel plot will be in the adev table area
   last_track = last_used = 0L;

   AZEL_ROW  = ADEV_ROW*TEXT_HEIGHT;
   AZEL_COL  = (ADEV_COL*TEXT_WIDTH);
   if(SCREEN_WIDTH >= ADEV_AZEL_THRESH) {  // adev tables and azel both fit
      AZEL_COL += (TEXT_WIDTH*44);
      AZEL_ROW += TEXT_HEIGHT;
   }
   else if(SCREEN_WIDTH >= 1024) {
      AZEL_COL += (TEXT_WIDTH*4);
      AZEL_ROW += TEXT_HEIGHT;
   }
   else {
      AZEL_COL += (TEXT_WIDTH*1);
      AZEL_ROW += TEXT_HEIGHT*2;
   }
   AZEL_SIZE = (SCREEN_WIDTH-AZEL_COL);

   if((SCREEN_WIDTH >= 1280) && (SCREEN_WIDTH < 1400)) AZEL_SIZE -= (TEXT_WIDTH*2);
   else if(small_font && (SCREEN_WIDTH >= 1024)) AZEL_COL += TEXT_WIDTH;

   if((AZEL_ROW+AZEL_SIZE) >= (PLOT_ROW-128)) {  // az/el map is in plot area
      AZEL_SIZE = (PLOT_ROW-AZEL_ROW-128);
   }
   if(SCREEN_WIDTH > 800) AZEL_MARGIN = 8;
   else                   AZEL_MARGIN = 4;

   if(full_circle) {
      AZEL_SIZE = SCREEN_HEIGHT-(SCREEN_HEIGHT/20);
      AZEL_SIZE = (AZEL_SIZE/TEXT_HEIGHT)*TEXT_HEIGHT;
      AZEL_SIZE -= (TEXT_HEIGHT*2)*2;

      AZEL_ROW  = (SCREEN_HEIGHT-AZEL_SIZE)/2;
      AZEL_ROW  = (AZEL_ROW/TEXT_HEIGHT)*TEXT_HEIGHT;
      if(SCREEN_HEIGHT <= 600) AZEL_ROW -= TEXT_HEIGHT;
      else                     AZEL_ROW -= (TEXT_HEIGHT*2);

      AZEL_COL  = (SCREEN_WIDTH-AZEL_SIZE)/2;
      AZEL_COL  = (AZEL_COL/TEXT_WIDTH)*TEXT_WIDTH;
   }

   // where to put the analog clock
   if(all_adevs && plot_lla && WIDE_SCREEN && (full_circle == 0)) {
      WATCH_ROW = AZEL_ROW;   // we could be drawing the watch here
      WATCH_COL = (FILTER_COL+20+32+32)*TEXT_WIDTH;  //AZEL_COL;
      WATCH_SIZE = (SCREEN_WIDTH-WATCH_COL)/2;
      if(WATCH_SIZE > (PLOT_ROW-TEXT_HEIGHT*4)) WATCH_SIZE = PLOT_ROW-TEXT_HEIGHT*4;
   }
   else {
      WATCH_ROW = AZEL_ROW;   // we could be drawing the watch here
      WATCH_COL = AZEL_COL;
      WATCH_SIZE = AZEL_SIZE;
   }

   if(full_circle) {
      return;
   }

   if(all_adevs || shared_plot) {  // all_adevs or share az/el space with normal plot area
      if(all_adevs && WIDE_SCREEN && (plot_watch == 0) && (plot_lla == 0)) {
         AZEL_COL = SCREEN_WIDTH-AZEL_SIZE;   // and azel goes in the corner
      }
      else {
         AZEL_SIZE = PLOT_HEIGHT;     // default size of the az/el map area
         if(AZEL_SIZE > 320) {        // it's just too big to look at
            AZEL_SIZE = 320;
            AZEL_ROW  = ((PLOT_ROW+TEXT_HEIGHT-1)/TEXT_HEIGHT)*TEXT_HEIGHT;
         }
         else {
            AZEL_ROW  = (SCREEN_HEIGHT-AZEL_SIZE);
         }
         AZEL_COL  = (SCREEN_WIDTH-AZEL_SIZE);
      }
      AZEL_MARGIN = 4;

      if((shared_plot == 0) && (PLOT_WIDTH < SCREEN_WIDTH)) {  // center plot area on the screen
         PLOT_COL = (SCREEN_WIDTH - PLOT_WIDTH) / 2;
      }
   }
   else {   // draw az/el map in the adev table area
      if(PLOT_WIDTH < SCREEN_WIDTH) {  // center plot area on the screen
         PLOT_COL = (SCREEN_WIDTH - PLOT_WIDTH) / 2;
      }
   }

   if(plot_azel || plot_signals || plot_watch) {
      if((AZEL_ROW+AZEL_SIZE) >= PLOT_ROW) {  // az/el map is in the normal plot window
         PLOT_WIDTH -= AZEL_SIZE;             // make room for it...
         PLOT_WIDTH += (HORIZ_MAJOR-1);
         PLOT_WIDTH = (PLOT_WIDTH/HORIZ_MAJOR) * HORIZ_MAJOR;
         if((PLOT_COL+PLOT_WIDTH+AZEL_SIZE) > SCREEN_WIDTH) PLOT_WIDTH -= HORIZ_MAJOR;
         AZEL_COL -= ((AZEL_COL-(PLOT_COL+PLOT_WIDTH)) / 2);
         AZEL_COL = ((AZEL_COL+TEXT_WIDTH-1)/TEXT_WIDTH)*TEXT_WIDTH;
      }
      else if((plot_azel || plot_signals) && plot_watch && (plot_lla == 0) && (all_adevs == 0) && WIDE_SCREEN) {
         AZEL_SIZE = (SCREEN_WIDTH - AZEL_COL) / 2;
         if(AZEL_SIZE > 320) AZEL_SIZE = 320;
         WATCH_SIZE = AZEL_SIZE;
      }
      else if((plot_signals == 3) && (SCREEN_WIDTH >= 1280)) {
         AZEL_COL -= TEXT_WIDTH*2;
      }
   }
}


void config_lla()
{
   // how and where to draw the lat/lon/altitude map
   LLA_ROW  = (ADEV_ROW*TEXT_HEIGHT);  // on small screens the lla map takes the place of the adev tables
   LLA_COL  = (ADEV_COL*TEXT_WIDTH);
   if(zoom_lla) {
      LLA_ROW = zoom_lla;
      LLA_COL = zoom_lla;
   } 
   else if(WIDE_SCREEN) {
      if(all_adevs) {
         if(plot_lla) LLA_COL = WATCH_COL + WATCH_SIZE;
         else         LLA_COL = SCREEN_WIDTH - AZEL_SIZE - (TEXT_WIDTH*2);
      }
      else if(shared_plot) {
         if(plot_watch && (plot_azel || plot_signals)) LLA_COL = WATCH_COL + WATCH_SIZE + (TEXT_WIDTH*2);
         else                                          LLA_COL = SCREEN_WIDTH - AZEL_SIZE - (TEXT_WIDTH*2);
      }
      else {
         LLA_COL = AZEL_COL + AZEL_SIZE + (TEXT_WIDTH*2);
      }
   }
   else if(SCREEN_WIDTH > 1440) {  // both lla map and azel map will fit on big screens
      if(shared_plot) LLA_COL = SCREEN_WIDTH - AZEL_SIZE - (TEXT_WIDTH*2);
      else            LLA_COL = AZEL_COL + AZEL_SIZE + (TEXT_WIDTH*2);
   }
   else if(SCREEN_WIDTH >= 1280) {
      LLA_COL += (44*TEXT_WIDTH); // adevs and lla will fit
   }
   else if(SCREEN_WIDTH >= 1024) {
      LLA_COL += (4*TEXT_WIDTH); 
   }
   else if(SCREEN_WIDTH >= 800) {
      LLA_COL += (2*TEXT_WIDTH);
   }
   else {  // undersized screens
      LLA_ROW = AZEL_ROW;
      LLA_COL = AZEL_COL;
   }

   if(zoom_lla) {
      LLA_SIZE = (SCREEN_HEIGHT-LLA_ROW-TEXT_HEIGHT*4-zoom_lla);
   }
   else {
      LLA_SIZE = (SCREEN_WIDTH-LLA_COL);
      if(SCREEN_WIDTH >= 800) { // lla map maight be in the plot area
         if((LLA_ROW+LLA_SIZE) >= (PLOT_ROW-128)) LLA_SIZE = (PLOT_ROW-LLA_ROW-128);
      }
   }

   LLA_MARGIN = 10;              // border width in pixels
   LLA_SIZE -= (LLA_MARGIN*2);   // tweek size so it matches the grid
   LLA_SIZE /= LLA_DIVISIONS;    // round size to a multiple of the grid size
   LLA_SIZE *= LLA_DIVISIONS;
   lla_step = LLA_SIZE / LLA_DIVISIONS;  // pixels per grid division
   lla_width = lla_step * LLA_DIVISIONS;
   LLA_SIZE += (LLA_MARGIN*2);   // add a margin around the lla circles
   if(zoom_lla) {
      LLA_COL = (SCREEN_WIDTH - LLA_SIZE) / 2;
   }
}

void config_text()
{
int i;
int k;

   // text drawing stuff
   PLOT_TEXT_COL = (PLOT_COL / TEXT_WIDTH);
   PLOT_TEXT_ROW = (PLOT_ROW / TEXT_HEIGHT);

   MOUSE_COL = (PLOT_TEXT_COL)+2;
   MOUSE_ROW = ((PLOT_TEXT_ROW)-6-eofs);
   MOUSE_ROW -= ((TEXT_Y_MARGIN+TEXT_HEIGHT-1)/TEXT_HEIGHT);
   if((TEXT_HEIGHT == 16) && (SCREEN_HEIGHT == 600)) --MOUSE_ROW;
   if(MOUSE_ROW < 0) MOUSE_ROW = 0;

   TEXT_COLS = ((SCREEN_WIDTH+TEXT_WIDTH-1)/TEXT_WIDTH);
   if(TEXT_COLS >= sizeof(blanks)) TEXT_COLS = sizeof(blanks)-1;
   TEXT_ROWS = ((SCREEN_HEIGHT+TEXT_HEIGHT-1)/TEXT_HEIGHT);
   if(text_mode) MOUSE_ROW = TEXT_ROWS;

   EDIT_ROW = PLOT_TEXT_ROW+2;   // where to put the string input dialog
   EDIT_COL = PLOT_TEXT_COL;

   if(text_mode) {
      EDIT_ROW = EDIT_COL = 0;
      #ifdef DOS
         TEXT_COLS = 80;
         if(small_font) TEXT_ROWS = 50;
         else           TEXT_ROWS = 25;
      #endif
      PLOT_ROW += (TEXT_HEIGHT*3);
      PLOT_TEXT_ROW = 100;
   }

   for(i=0; i<TEXT_COLS; i++) blanks[i] = ' ';
   blanks[TEXT_COLS] = 0;

   // assign column positions to the headers for the extra plots
   for(k=FIRST_EXTRA_PLOT; k<NUM_PLOTS; k++) slot_column[k] = (-1);
   i = 79;
   for(k=FIRST_EXTRA_PLOT; k<NUM_PLOTS; k++) {  
      if ((i+16) > TEXT_COLS) break;
      slot_column[k] = i;
      i += 16;
   }
}

void check_full()
{
   if(full_circle == 0) return;
   if(plot_signals) return;
   if(plot_watch) return;
   if(plot_digital_clock) return;
   if(plot_azel) return;
   if(plot_lla && zoom_lla) return;

   full_circle = 0;
   zoom_lla = 0;
}

void config_screen()
{
   // setup variables related to the video screen size
   check_full();       // see if Z zoom command can be used

   VERT_MAJOR = 30;    // the graph axes tick mark spacing (in pixels)
   VERT_MINOR  = 6;    // VERT_MAJOR/VERT_MINOR should be 5

   HORIZ_MAJOR = 60;
   HORIZ_MINOR = 10;

   INFO_COL = 65;
   if((SCREEN_WIDTH <= 800) || text_mode) INFO_COL -= 1;
   else if(small_font == 1) INFO_COL -= 1;

   FILTER_ROW = 18;
   FILTER_COL = INFO_COL;

   AZEL_SIZE = LLA_SIZE = (320);       // size of the az/el map area
   eofs = 1;

   PLOT_COL = 0;

   if(screen_type == 'u') {  // undersized, very small screen
      GRAPH_MODE = 18;
      SCREEN_WIDTH = 640;
      SCREEN_HEIGHT = 480;

      config_undersized();

      if(day_plot >= 24) {
         HORIZ_MAJOR = 24;
         HORIZ_MINOR = 4;
      }
      else if(day_plot >= 12) {
         HORIZ_MAJOR = 48;
         HORIZ_MINOR = 8;
      }
      else day_plot = 0;
   }
   else if(screen_type == 's') {  // small screen
      GRAPH_MODE  = 258;
      SCREEN_WIDTH = 800;
      SCREEN_HEIGHT = 600;

      config_small();

      if(day_plot >= 24) {
         HORIZ_MAJOR = 30;
         HORIZ_MINOR = 5;
      }
      else if(day_plot >= 12) {
         HORIZ_MAJOR = 60;
         HORIZ_MINOR = 15;
      }
      else day_plot = 0;
   }
   else if(screen_type == 'n') {  // netbook screen
      custom_width = 1000;
      custom_height = 540;
      goto customize;
   }
   else if(screen_type == 'l') { // large screen
      GRAPH_MODE  = 262;
      SCREEN_WIDTH = 1280;
      SCREEN_HEIGHT = 1024; // 800, 900, 960, 1024

      PLOT_ROW = (480+VERT_MAJOR*2);

      if(day_plot >= 24) {
         HORIZ_MAJOR = 48;
         HORIZ_MINOR = 12;
      }
      else if(day_plot >= 12) {
         HORIZ_MAJOR = 96;
         HORIZ_MINOR = 24;
      }
      else day_plot = 0;
   }
   else if(screen_type == 'v') { // very large screen
      GRAPH_MODE  = 327;        //!!!!!
      SCREEN_WIDTH = 1400;  //1440;
      SCREEN_HEIGHT = 1050; // 900; 

      PLOT_ROW = (480+VERT_MAJOR*2);

      if(day_plot >= 24) {
         HORIZ_MAJOR = 55;
         HORIZ_MINOR = 11;
      }
      else if(day_plot >= 12) {
         HORIZ_MAJOR = 110;
         HORIZ_MINOR = 22;
      }
      else day_plot = 0;
   }
   else if(screen_type == 'x') { // extra large screen
      GRAPH_MODE  = 304;     // 325
      SCREEN_WIDTH = 1680;  
      SCREEN_HEIGHT = 1050; 

      PLOT_ROW = (480+VERT_MAJOR*2);

      if(day_plot >= 24) {
         HORIZ_MAJOR = 60;
         HORIZ_MINOR = 12;
      }
      else if(day_plot >= 12) {
         HORIZ_MAJOR = 120;
         HORIZ_MINOR = 24;
      }
      else day_plot = 0;
   }
   else if(screen_type == 'h') { // huge screen
      GRAPH_MODE  = 319;   
      SCREEN_WIDTH = 1920;
      SCREEN_HEIGHT = 1080;

      PLOT_ROW = 576;

      if(day_plot >= 24) {
         HORIZ_MAJOR = 75;
         HORIZ_MINOR = 15;
      }
      else if(day_plot >= 16) {
         HORIZ_MAJOR = 150;
         HORIZ_MINOR = 30;
      }
      else day_plot = 0;
   }
   else if(screen_type == 'z') { // really huge screen
      GRAPH_MODE  = 338;   
      SCREEN_WIDTH = 2048;
      SCREEN_HEIGHT = 1536;

      PLOT_ROW = (576);

      if(day_plot >= 24) {
         HORIZ_MAJOR = 80;
         HORIZ_MINOR = 16;
      }
      else if(day_plot >= 16) {
         HORIZ_MAJOR = 160;
         HORIZ_MINOR = 32;
      }
      else day_plot = 0;
   }
   else if(screen_type == 'c') { // custom screen
      customize:
      screen_type = 'c';
      if(custom_width <= 0) custom_width = SCREEN_WIDTH;
      if(custom_height <= 0) custom_height = SCREEN_HEIGHT;

      if(custom_width <= 640) custom_width = 640;
      if(custom_height <= 400) custom_height = 400;

      SCREEN_WIDTH = custom_width;
      SCREEN_HEIGHT = custom_height;

      if     (SCREEN_HEIGHT > 1050) PLOT_ROW = 576;
      else if(SCREEN_HEIGHT > 960)  PLOT_ROW = 480+(2*VERT_MAJOR);
      else if(SCREEN_HEIGHT >= 768) PLOT_ROW = 468;
      else if(SCREEN_HEIGHT >= 600) config_small();
      else                          config_undersized();

      if(day_plot) {
         HORIZ_MAJOR = SCREEN_WIDTH / day_plot;
         HORIZ_MAJOR /= 4;
         HORIZ_MAJOR *= 4;
         HORIZ_MINOR = (HORIZ_MAJOR / 4);
      }
      day_plot = 0;
   }
   else if(screen_type == 't') {  // text only mode
      // In DOS we do this mode as a true text mode
      // In Windows, this is actually a 640x480 graphics mode
      GRAPH_MODE = 18;
      SCREEN_WIDTH = 640;
      SCREEN_HEIGHT = 480;
      PLOT_ROW = (SCREEN_HEIGHT/TEXT_HEIGHT)*TEXT_HEIGHT;  // -VERT_MAJOR*2;
      PLOT_COL = 0;
      day_plot = 0;
   }
   else {   // normal (1024x768) screen
      GRAPH_MODE  = 260;
      SCREEN_WIDTH = 1024;
      SCREEN_HEIGHT = 768;

      PLOT_ROW = 468;
      PLOT_COL = 0;

      if(day_plot >= 24) {
         HORIZ_MAJOR = 40;
         HORIZ_MINOR = 10;
      }
      else if(day_plot >= 12) {
         HORIZ_MAJOR = 80;
         HORIZ_MINOR = 20;
      }
      else day_plot = 0;
   }

   if(user_video_mode) {  // user specified the video mode to use
      GRAPH_MODE = user_video_mode;
   }

   if(small_sat_count) {  // compress sat count to VERT_MINOR ticks per sat
      COUNT_SCALE = VERT_MINOR; 
   }
   else {                 // expand sat count to VET_MAJOR ticks per sat
      COUNT_SCALE = VERT_MAJOR;
   }

   // graphs look best if PLOT_HEIGHT is a multiple of (2*VERT_MAJOR)
   // but small screens don't have room to waste
   PLOT_HEIGHT = (SCREEN_HEIGHT-PLOT_ROW);
   PLOT_HEIGHT /= (VERT_MAJOR*2);  // makes graph prettier to have things a multiple of VERT_MAJOR*2
   PLOT_HEIGHT *= (VERT_MAJOR*2);

   PLOT_CENTER = (PLOT_HEIGHT/2-1);

   PLOT_WIDTH = (SCREEN_WIDTH/HORIZ_MAJOR) * HORIZ_MAJOR;
   if(day_plot) {
      PLOT_WIDTH = HORIZ_MAJOR * day_plot;
      if(interval_set) queue_interval = 3600L / HORIZ_MAJOR;
   }

   // when graph fills, scroll it left this many pixels
   if(continuous_scroll) PLOT_SCROLL = 1;  // live update mode for fast processors
   else                  PLOT_SCROLL = (HORIZ_MAJOR*2);

   if(SCREEN_WIDTH < 800) {  // undersized screen
      if     (plot_azel)     shared_plot = 1;
      else if(plot_signals)  shared_plot = 1;
      else if(plot_watch)    shared_plot = 1;
      else if(plot_lla)      shared_plot = 1;
      else                   shared_plot = 0;
   }

   if(fix_mode && plot_lla && (plot_azel || plot_signals) && (WIDE_SCREEN == 0)) {
      shared_plot = 1;
   }

   config_azel();  // setup for drawing azel map (and analog watch)
   config_lla();   // setup for drawing lat/lon/alt plot
   config_text();  // setup for drawing text

   if(PLOT_ROW >= (480+2*VERT_MAJOR)) big_plot = 1;
   else                               big_plot = 0;
}


//
//
//   Data plotting stuff
//
//

void show_label(int id)
{
u08 c;
float val;
int row, col;

   col = plot[id].slot;    // the slot number of the plot header
   if(col < 0) return;     // the plot has no assigned header slot

   col = slot_column[col]; // column to draw the plot header at
   if(col < 0) return;     // plot header is turned off
   col += PLOT_TEXT_COL;
   row = PLOT_TEXT_ROW - 1;

   no_x_margin = no_y_margin = 1;
   val = plot[id].scale_factor * plot[id].invert_plot;

   // show the plot scale factor
   if(auto_scale && (plot[id].user_scale == 0)) c = '~';
   else c = '=';
   if(id == TEMP) {
      if(plot[id].scale_factor >= 100.0F) {
         sprintf(out, "%s%c(%ld m%c%c/div)     ",  
                      plot[id].plot_id, c, (long) (val+0.5F), DEGREES,DEG_SCALE);
      }
      else if(plot[id].scale_factor < 1.0F) {
         sprintf(out, "%s%c(%.2f m%c%c/div)     ", 
                       plot[id].plot_id, c, val, DEGREES,DEG_SCALE);
      }
      else {
         sprintf(out, "%s%c(%.1f m%c%c/div)     ", 
                       plot[id].plot_id, c, val, DEGREES,DEG_SCALE);
      }
   }
   else if(id == OSC) {
      if(plot[id].scale_factor >= 100.0F) {
         sprintf(out, "%s%c(%ld%s/div)     ", 
                      plot[id].plot_id, c, (long) (val+0.5F), plot[id].units);
      }
      else if(plot[id].scale_factor < 1.0F) {
         sprintf(out, "%s%c(%.2f%s/div)     ", 
                       plot[id].plot_id, c, val, plot[id].units);
      }
      else {
         sprintf(out, "%s%c(%.1f%s/div)     ", 
                       plot[id].plot_id, c, val, plot[id].units);
      }
   }
   else {
      if(plot[id].scale_factor >= 100.0F) {
         sprintf(out, "%s%c(%ld %s/div)     ", 
                      plot[id].plot_id, c, (long) (val+0.5F), plot[id].units);
      }
      else if(plot[id].scale_factor < 1.0F) {
         sprintf(out, "%s%c(%.2f %s/div)     ", 
                       plot[id].plot_id, c, val, plot[id].units);
      }
      else {
         sprintf(out, "%s%c(%.1f %s/div)     ", 
                       plot[id].plot_id, c, val, plot[id].units);
      }
   }
   if(plot[id].show_plot) vidstr(row, col,  plot[id].plot_color, out);
   else                   vidstr(row, col,  GREY, out);

   // now show the plot zero reference line value
   val = plot[id].plot_center;
   if(auto_center && plot[id].float_center) c = '~';
   else                                     c = '=';
   if(id == OSC)  {
      if(res_t) sprintf(out, "ref%c<%.1f%s>     ", c, val, plot[id].units); 
      else      sprintf(out, "ref%c<%.1f%s>     ", c, val*1000.0F, plot[id].units); 
   }
   else if(id == PPS) {
      if(res_t) sprintf(out, "ref%c<%.1f %s>     ", c, val/1000.0F, plot[id].units);
      else      sprintf(out, "ref%c<%.1f %s>     ", c, val, plot[id].units);
   }
   else if(id == DAC)  {
      if(res_t) sprintf(out, "ref%c<%.6f ns>  ", c, val);
      else      sprintf(out, "ref%c<%.6f V>   ", c, val);
   }
   else if(id == TEMP) sprintf(out, " ref%c<%.3f %c%c>   ", c, val, DEGREES,DEG_SCALE);
   else                sprintf(out, "ref%c<%.1f %s>     ", c, val, plot[id].units);
   if(plot[id].show_plot) vidstr(row-1, col, plot[id].plot_color, out);
   else                   vidstr(row-1, col, GREY, out);

   no_x_margin = no_y_margin = 0;
}

void label_plots()
{
COORD row;
struct PLOT_Q q;
char *t;
long i;
int k;

   // label the plot with scale factors, etc
   if(text_mode) return;
   if(full_circle) return;

   t = "";
   row = (PLOT_TEXT_ROW) - 1;

   if(review_mode) {
      if(review_home)  sprintf(out, "  ");
      else             sprintf(out, "%c ", LEFT_ARROW); 
   }
   else {
      if(((plot_q_count*(long)plot_mag)/view_interval) >= PLOT_WIDTH) sprintf(out, "%c ", LEFT_ARROW);
      else sprintf(out, "  ");
   }
   vidstr(row, PLOT_TEXT_COL+0, WHITE, out);

   for(k=0; k<FIRST_EXTRA_PLOT; k++) {  // show the standard plots
      show_label(k);
   }
   if(extra_plots) {  // show the extra plots, if any are turned on
      // Note that we draw the headers in ascending column order since
      // the header text may be greater than the slot width.  This keeps
      // the text to the left of a slot from overwriting the slot to the right.
      for(k=FIRST_EXTRA_PLOT; k<NUM_PLOTS; k++) {
         if(slot_in_use[k] >= 0) show_label(slot_in_use[k]);
      }
   }


   no_x_margin = no_y_margin = 1;
   if(review_mode) {  // we are scrolling around in the plot queue data
      i = plot_q_out + plot_start;
      while(i >= plot_q_size) i -= plot_q_size;
      q = get_plot_q(i);

      pri_hours   = q.hh;
      pri_minutes = q.mm;
      pri_seconds = q.ss;
      pri_day     = q.dd;
      pri_month   = q.mo;
      pri_year    = q.yr;
      if(pri_year >= 80) pri_year += 1900;
      else               pri_year += 2000;
      adjust_tz();  // tweak pri_ time for time zone

      sprintf(out, "Review (DEL to stop): %02d:%02d:%02d  %s %s         ", 
          pri_hours,pri_minutes,pri_seconds,  fmt_date(),
          time_zone_set?tz_string:(q.sat_flags & UTC_TIME) ? "UTC":"GPS");
      vidstr(row-2, PLOT_TEXT_COL+2,  WHITE, out);
   }
   else {  // we are displaying live data
      if(small_font == 1) vidstr(row-2, PLOT_TEXT_COL+2,  WHITE, "                                                                        ");
      else                vidstr(row-2, PLOT_TEXT_COL+2,  WHITE, "                                                             ");
   }
   no_x_margin = no_y_margin = 0;

   if((extra_plots == 0) && (SCREEN_WIDTH >= 1024)) {
      #ifdef ADEV_STUFF
         if((all_adevs == 0) || (mixed_adevs != 1)) {
            if     (ATYPE == OSC_ADEV) t = "ADEV";
            else if(ATYPE == PPS_ADEV) t = "ADEV";
            else if(ATYPE == OSC_MDEV) t = "MDEV";
            else if(ATYPE == PPS_MDEV) t = "MDEV";
            else if(ATYPE == OSC_HDEV) t = "HDEV";
            else if(ATYPE == PPS_HDEV) t = "HDEV";
            else if(ATYPE == OSC_TDEV) t = "TDEV";
            else if(ATYPE == PPS_TDEV) t = "TDEV";
            else                       t = "?DEV";

            sprintf(out, "PPS %s       ", t);
            if(plot_adev_data) vidstr(row, PLOT_TEXT_COL+85,  PPS_ADEV_COLOR, out);
            else               vidstr(row, PLOT_TEXT_COL+85,  GREY,           out);

            sprintf(out, "OSC %s       ", t);
            if(plot_adev_data) vidstr(row, PLOT_TEXT_COL+100, OSC_ADEV_COLOR, out);
            else               vidstr(row, PLOT_TEXT_COL+100, GREY,           out);
         }

         if(plot_sat_count) vidstr(row, PLOT_TEXT_COL+115,  COUNT_COLOR, "Sat count");
         else               vidstr(row, PLOT_TEXT_COL+115,  GREY,        "Sat count");
      #else
         if(plot_sat_count) vidstr(row, PLOT_TEXT_COL+85,  COUNT_COLOR, "Sat count");
         else               vidstr(row, PLOT_TEXT_COL+85,  GREY,        "Sat count");
      #endif
   }
}




void mark_row(int y)
{
int x;
int color;

   // draw > < at the edges of the specified plot row
   if(continuous_scroll) color = CYAN;
   else                  color = WHITE;

   y += PLOT_ROW;
   x = PLOT_COL;
   dot(x+0, y-5, color);   dot(x+0, y+5, color);
   dot(x+1, y-4, color);   dot(x+1, y+4, color);
   dot(x+2, y-3, color);   dot(x+2, y+3, color);
   dot(x+3, y-2, color);   dot(x+3, y+2, color);
   dot(x+4, y-1, color);   dot(x+4, y+1, color);
   dot(x+5, y-0, color);   

   x = PLOT_COL + PLOT_WIDTH - 1;
   dot(x-0, y-5, color);   dot(x-0, y+5, color);
   dot(x-1, y-4, color);   dot(x-1, y+4, color);
   dot(x-2, y-3, color);   dot(x-2, y+3, color);
   dot(x-3, y-2, color);   dot(x-3, y+2, color);
   dot(x-4, y-1, color);   dot(x-4, y+1, color);
   dot(x-5, y-0, color);
}


void plot_mark(int symbol)
{
int x;
int y;
int temp;

   // draw user set data markers in the plot
   y = PLOT_ROW;
   x = PLOT_COL+plot_column;

   if(symbol == 0) {   // the mouse click marker
      temp = MARKER_COLOR;
      dot(x+0, y+5, temp);  
      dot(x+1, y+4, temp);    dot(x-1, y+4, temp); 
      dot(x+2, y+3, temp);    dot(x-2, y+3, temp); 
      dot(x+3, y+2, temp);    dot(x-3, y+2, temp); 
      dot(x+4, y+1, temp);    dot(x-4, y+1, temp); 
      dot(x+5, y+0, temp);    dot(x-5, y+0, temp); 

      y = PLOT_ROW+PLOT_HEIGHT-1;
      dot(x+0, y-5, temp);    
      dot(x+1, y-4, temp);    dot(x-1, y-4, temp); 
      dot(x+2, y-3, temp);    dot(x-2, y-3, temp); 
      dot(x+3, y-2, temp);    dot(x-3, y-2, temp); 
      dot(x+4, y-1, temp);    dot(x-4, y-1, temp); 
      dot(x+5, y-0, temp);    dot(x-5, y-0, temp); 
   }
   else {   // the numeric markers
      #ifdef DIGITAL_CLOCK
         // we can use the vector character code to draw the markers
         x -= VCHAR_SCALE/2;
         if(x < 0) x = 0;
         temp = VCHAR_SCALE;
         if(SCREEN_HEIGHT < 768) VCHAR_SCALE = 0;
         else VCHAR_SCALE = 1;
         vchar(x,y+2, 0, MARKER_COLOR, '0'+symbol);
         VCHAR_SCALE = temp;
      #else
         // use text chars for the markers (may not go exactly where we want in DOS)
         #ifdef WIN_VFX
            graphics_coords = 1;
            x -= TEXT_WIDTH/2;
            if(x < 0) x = 0;
            y += 2;
         #endif
         #ifdef DOS
            x = x / TEXT_WIDTH;
            y = (PLOT_ROW/TEXT_HEIGHT);
         #endif
         sprintf(out, "%c", '0'+symbol);
         vidstr(y,x, MARKER_COLOR, out);
         graphics_coords = 0;
      #endif
   }
}


void show_queue_info()
{
int row, col;
int j;
float queue_time;

   col = FILTER_COL;
   if(text_mode) {
      row = TEXT_ROWS - 2;  // FILTER_ROW+3+eofs+1+3;
   }
   else {
      row = (PLOT_TEXT_ROW-4);
      if((TEXT_HEIGHT <= 14) && (SCREEN_HEIGHT >= 768)) --row;
   }
   j = row;

   if(pause_data) vidstr(j, col, YELLOW, "UPDATES PAUSED");
   else           vidstr(j, col, YELLOW, "              ");
   --j;

   #ifdef ADEV_STUFF
      if(adev_period <= 0.0F)      sprintf(out, "ADEV:  OFF          ");
      else if(adev_period == 1.0F) sprintf(out, "ADEVQ: %ld pts ", adev_q_size-1);
      else if(adev_period < 1.0F)  sprintf(out, "ADEVP: <1 sec       ");
      else                         sprintf(out, "ADEVP: %.1f sec   ", adev_period);
      vidstr(j, col, WHITE, out);
      --j;
   #endif

   queue_time = ((float) plot_q_size * queue_interval);
   if(queue_time >= (3600.0F * 24.0F)) sprintf(out, "PLOTQ: %.1f day  ", queue_time/(3600.0F*24.0F));
   else if(queue_time >= 3600.0F)      sprintf(out, "PLOTQ: %.1f hr   ", queue_time/3600.0F);
   else if(queue_time >= 60.0F)        sprintf(out, "PLOTQ: %.1f min  ", queue_time/60.0F);
   else if(queue_time > 0.0F)          sprintf(out, "PLOTQ: %.1f sec  ", queue_time);
   else                                sprintf(out, "PLOTQ: OFF          ");
// if(queue_interval <= 0) sprintf(out, "PLOTQ: OFF          ");
// else sprintf(out, "PLOTQ: %ld.0 sec   ", queue_interval);
   vidstr(j, col, WHITE, out);
   --j;

   view_row = j;
}

void show_view_info()
{
float show_time;
int col;
int color;

   // tuck the diaplay interval data into whatever nook or crannie 
   // we can find on the screen
   if((all_adevs == 0) || mixed_adevs) {
      col = FILTER_COL;
      if(show_min_per_div) {
         show_time = (float) (view_interval*queue_interval*PLOT_WIDTH);  //seconds per screen
         show_time /= plot_mag;
         show_time /= 60.0F;  // minutes/screen
         show_time /= (((float) PLOT_WIDTH) / (float) HORIZ_MAJOR);
         if(SCREEN_WIDTH < 800) {
            sprintf(out, "%.1f min/div ", show_time);
         }
         else if((SCREEN_WIDTH <= 800) && (small_font != 1)) {
            sprintf(out, "%.1f min/div ", show_time);
         }
         else {
            if(show_time < 1.0F) sprintf(out, "VIEW: %.1f sec/div  ", show_time*60.0F);
            else                 sprintf(out, "VIEW: %.1f min/div  ", show_time);
         }

         if(view_interval != 1) color = BLUE;
         else                   color = WHITE;

         vidstr(view_row, col, color, out);
         --view_row;
         
         show_time = (float) (view_interval*queue_interval*PLOT_WIDTH);  // seconds per screen
         show_time /= plot_mag;
         if(show_time < (2.0F*3600.0F)) sprintf(out, "VIEW: %.1f min     ", show_time/60.0F);
         else sprintf(out, "VIEW: %.1f hrs     ", show_time/3600.0F);
         vidstr(view_row, col, color, out);
         --view_row;
      }

      label_plots();
   }
}

void show_plot_grid()
{
int x, y;
int color;
int col;
int j;

   for(x=0; x<PLOT_WIDTH; x++) {  // draw vertical features
      col = PLOT_COL + x;
      dot(col, PLOT_ROW, WHITE);                // top of graph
      dot(col, PLOT_ROW+PLOT_HEIGHT-1, WHITE);  // bottom of graph

      if((x % HORIZ_MAJOR) == 0) {  // major tick marks
         dot(col, PLOT_ROW+1, WHITE);               // top horizontal axis
         dot(col, PLOT_ROW+2, WHITE);
         dot(col, PLOT_ROW+3, WHITE);

         dot(col, PLOT_ROW+PLOT_HEIGHT-2, WHITE);   // bottom horizontal axis     
         dot(col, PLOT_ROW+PLOT_HEIGHT-3, WHITE);
         dot(col, PLOT_ROW+PLOT_HEIGHT-4, WHITE);

         for(j=0; j<=PLOT_CENTER; j+=VERT_MAJOR) {
            dot(col,   PLOT_ROW+PLOT_CENTER+j-1, WHITE);  // + at intersections
            dot(col,   PLOT_ROW+PLOT_CENTER+j,   WHITE);
            dot(col,   PLOT_ROW+PLOT_CENTER+j+1, WHITE);
            if(col > PLOT_COL) dot(col-1, PLOT_ROW+PLOT_CENTER+j,   WHITE);
            dot(col+1, PLOT_ROW+PLOT_CENTER+j,   WHITE);

            dot(col,  PLOT_ROW+PLOT_CENTER-j-1, WHITE);  // + at intersections 
            dot(col,  PLOT_ROW+PLOT_CENTER-j,   WHITE);
            dot(col,  PLOT_ROW+PLOT_CENTER-j+1, WHITE);
            if(col > PLOT_COL) dot(col-1,PLOT_ROW+PLOT_CENTER-j,    WHITE);
            dot(col+1,PLOT_ROW+PLOT_CENTER-j,    WHITE);
         }

         color = GREY;  // WHITE;
         if(plot_adev_data) {
            if((x % (HORIZ_MAJOR*3)) == 0) color = CYAN;  // ADEV bins
         }
         for(j=VERT_MINOR; j<=PLOT_CENTER; j+=VERT_MINOR) {
            dot(col, PLOT_ROW+PLOT_CENTER+j, color);
            dot(col, PLOT_ROW+PLOT_CENTER-j, color);
         }
      }
      else if((x % HORIZ_MINOR) == 0) {  // minor tick marks
         dot(col, PLOT_ROW+1, WHITE);
         dot(col, PLOT_ROW+PLOT_CENTER, GREY);  // center line tick marks
         dot(col, PLOT_ROW+PLOT_HEIGHT-2, WHITE);

         if(plot_adev_data) color = CYAN;   // subtly highlight ADEV decades
         else               color = GREY;
         for(j=VERT_MAJOR; j<=PLOT_CENTER; j+=VERT_MAJOR) {
            dot(col, PLOT_ROW+PLOT_CENTER+j, color);
            dot(col, PLOT_ROW+PLOT_CENTER-j, color);
            if(SCREEN_HEIGHT > 600) {
               if(plot_adev_data && (color != CYAN)) color = CYAN;
               else color = GREY;  // WHITE;
            }
            else {
               dot(col, PLOT_ROW+PLOT_CENTER, color);
            }
         }
      }
   }

   for(y=0; y<=PLOT_CENTER; y++) {  // draw horizontal features
      dot(PLOT_COL, PLOT_ROW+PLOT_CENTER+y, WHITE);
      dot(PLOT_COL, PLOT_ROW+PLOT_CENTER-y, WHITE);
      dot(PLOT_COL+PLOT_WIDTH-1, PLOT_ROW+PLOT_CENTER+y, WHITE);
      dot(PLOT_COL+PLOT_WIDTH-1, PLOT_ROW+PLOT_CENTER-y, WHITE);

      if((y % VERT_MAJOR) == 0) {
         if((HIGHLIGHT_REF == 0) || (y != 0)) {
            dot(PLOT_COL+1,              PLOT_ROW+PLOT_CENTER+y, WHITE);
            dot(PLOT_COL+1,              PLOT_ROW+PLOT_CENTER-y, WHITE);
            dot(PLOT_COL+PLOT_WIDTH-1-1, PLOT_ROW+PLOT_CENTER+y, WHITE);
            dot(PLOT_COL+PLOT_WIDTH-1-1, PLOT_ROW+PLOT_CENTER-y, WHITE);
            dot(PLOT_COL+2,              PLOT_ROW+PLOT_CENTER+y, WHITE);
            dot(PLOT_COL+2,              PLOT_ROW+PLOT_CENTER-y, WHITE);
            dot(PLOT_COL+PLOT_WIDTH-1-2, PLOT_ROW+PLOT_CENTER+y, WHITE);
            dot(PLOT_COL+PLOT_WIDTH-1-2, PLOT_ROW+PLOT_CENTER-y, WHITE);
            dot(PLOT_COL+3,              PLOT_ROW+PLOT_CENTER+y, WHITE);
            dot(PLOT_COL+3,              PLOT_ROW+PLOT_CENTER-y, WHITE);
            dot(PLOT_COL+PLOT_WIDTH-1-3, PLOT_ROW+PLOT_CENTER+y, WHITE);
            dot(PLOT_COL+PLOT_WIDTH-1-3, PLOT_ROW+PLOT_CENTER-y, WHITE);
         }
      }
      else if((y % VERT_MINOR) == 0) {
         dot(PLOT_COL+1,             PLOT_ROW+PLOT_CENTER+y,  WHITE);
         dot(PLOT_COL+1,             PLOT_ROW+PLOT_CENTER-y,  WHITE);
         dot(PLOT_COL+PLOT_WIDTH-1-1,PLOT_ROW+PLOT_CENTER+y,  WHITE);
         dot(PLOT_COL+PLOT_WIDTH-1-1,PLOT_ROW+PLOT_CENTER-y,  WHITE);
      }
   }

   if(HIGHLIGHT_REF) {   // highlight plot center reference line
      mark_row(PLOT_CENTER);
   }
}


void format_plot_title()
{
int len;
int i, j;
char c;

   j = 0;
   i = 0;
   out[0] = 0;
   len = strlen(plot_title);

   for(i=0; i<len; i++) {
      c = plot_title[i];
      if(c == '&') {
         if(plot_title[i+1] == '&') {
            out[j++] = '&';
            out[j] = 0;
            ++i;
         }
         else {
            sprintf(&out[j], "(TC=%.3f  DAMPING=%.3f  GAIN=%.3f Hz/V)", 
                time_constant, damping_factor, osc_gain);
            j = strlen(out);
         }
      }
      else {
         out[j++] = c;
         out[j] = 0;
         if(c == 0) break;
      }
   }
}

void show_title()
{
int row, col;

   if(text_mode) {
      row = TEXT_ROWS-1;
      col = 0;
      no_x_margin = no_y_margin = 1;
   }
   else {
      row = (PLOT_ROW+PLOT_HEIGHT)/TEXT_HEIGHT-1;
      col = PLOT_TEXT_COL + 1;
   }

   if(plot_title[0]) {
      format_plot_title();
      vidstr(row, col, TITLE_COLOR, out);
      if((text_mode == 0) && (full_circle == 0)) {
         line(PLOT_COL,PLOT_ROW+PLOT_HEIGHT-1,  PLOT_COL+PLOT_WIDTH,PLOT_ROW+PLOT_HEIGHT-1, WHITE);
      }
   }

   if(text_mode) return;
   if(full_circle) return;

   if(debug_text) {
      vidstr(row-1, col, GREEN, debug_text);
   }

   if(debug_text2) {
      vidstr(row-2, col, YELLOW, debug_text2);
   }

   if(filter_count) {
      sprintf(out, "Filter: %ld", filter_count);
      vidstr(PLOT_TEXT_ROW+1, col, WHITE, out);
   }
}


void plot_axes()
{
   // draw the plot background grid and label info
   if(first_key) return;   // plot area is in use for help/warning message

   erase_plot(0);          // erase plot area
   if(read_only || just_read || no_send) {
      show_version_header();
   }
   show_queue_info();      // show the queue stats
   if((text_mode == 0) && (full_circle == 0)) {
      show_view_info();    // display the plot view settings
      show_plot_grid();    // display the plot grid
   }
   show_title();           // display the plot title
}



float round_scale(float val)
{
double decade;

   // round the scale factor to a 1 2 (2.5) 5  sequence
   for(decade=1.0E-6; decade<=1.0E9; decade*=10.0) {
      if(val <= decade)       return (float) decade;
      if(val <= (decade*2.0)) return (float) (decade*2.0);
//    if(val <= (decade*2.5)) if(decade >= 10.0) return (float) (decade*2.5);
      if(val <= (decade*2.5)) return (float) (decade*2.5);
      if(val <= (decade*5.0)) return (float) (decade*5.0);
   }

   return val;
}


void scale_plots()
{
long i;
struct PLOT_Q q;
int k;
float scale[NUM_PLOTS];

   // Calculate plot scale factors and center line reference points.
   // This routine uses the min and max values collected by calc_plot_statistics()

   if((auto_scale == 0) && (auto_center == 0)) return;
   if(queue_interval <= 0) return;

   // calc_queue_stats() already scanned the queue data to find mins and maxes
   // now we calculate good looking scale factors and center points for the plots
   if(auto_scale && plot_q_last_col) {  // we have data to calculate good graph scale factors from
      i = (PLOT_HEIGHT / VERT_MAJOR) & 0xFFFE;  //even number of major vertical divisions
      //  if(SCREEN_HEIGHT >= 768) i -= 1;  // prevents multiple consecutive rescales
      i -= 1;  //!!!!
      if(i <= 0) i = 1;

      // scale graphs to the largest side of the curve above/below
      // the graph center reference line
      for(k=0; k<NUM_PLOTS; k++) {
         if(auto_center && plot[k].float_center) {
            plot[k].plot_center = (plot[k].max_val+plot[k].min_val) / 2.0F;
         }

         if((plot[k].max_val >= plot[k].plot_center) && (plot[k].min_val < plot[k].plot_center)) { 
            if((plot[k].max_val-plot[k].plot_center) >= (plot[k].plot_center-plot[k].min_val)) {
               scale[k] = (plot[k].max_val-plot[k].plot_center);
            }
            else {
               scale[k] = plot[k].plot_center - plot[k].min_val;
            }
         }
         else if(plot[k].max_val >= plot[k].plot_center) {
            scale[k] = (plot[k].max_val - plot[k].plot_center);
         }
         else {
            scale[k] = (plot[k].plot_center - plot[k].min_val);
         }

         scale[k] = (scale[k] / (float) (i/2));
         scale[k] *= plot[k].ref_scale;

         // round scale factors to nice values
         scale[k] = round_scale(scale[k]);
      }

      // set the working scale factors to the new values
      if(peak_scale) {  // don't ever let scale factors get smaller
         for(k=0; k<NUM_PLOTS; k++) {
            if(scale[k] > plot[k].scale_factor) {
               if(plot[k].user_scale == 0) {
                  plot[k].scale_factor = scale[k];
               }
            }
         }
      }
      else {  // scale factors can change to whatever they are
         for(k=0; k<NUM_PLOTS; k++) {
            if(plot[k].user_scale == 0) plot[k].scale_factor = scale[k];
         }
      }

      if(auto_center) {  // center the plots
         for(k=0; k<NUM_PLOTS; k++) {  
            if(plot[k].float_center) {
               plot[k].plot_center = (plot[k].max_val+plot[k].min_val) / 2.0F;  
            }
         }
         last_dac_voltage = plot[DAC].plot_center;
//!!!!   last_temperature = plot[TEMP].plot_center;
      }
   }
   else if(auto_center && (plot_q_last_col >= 1)) {  // center these graphs around the last recorded value
      if(filter_count) q = filter_plot_q(plot_q_last_col-1);
      else             q = get_plot_q(plot_q_last_col-1);

      for(k=0; k<NUM_PLOTS; k++) {
         if(plot[k].float_center) {
            if(k == TEMP) plot[k].plot_center = fmt_temp(q.data[TEMP] / (float) queue_interval);
            else          plot[k].plot_center = q.data[k] / (float) queue_interval;
         }
      }
   }

   // finally we round the center line reference values to multiples of 
   // the scale factor (note that this tweak can allow a plot to go slightly 
   // off scale)
   if(auto_center) {
      for(k=0; k<NUM_PLOTS; k++) {
         if(plot[k].float_center && plot[k].scale_factor) {
            plot[k].plot_center *= plot[k].ref_scale;
            plot[k].plot_center = (LONG_LONG) ((plot[k].plot_center / plot[k].scale_factor)) * plot[k].scale_factor;
            plot[k].plot_center /= plot[k].ref_scale;
         }
      }
   }
}


int last_plot_col;
u08 plot_dot;

int plot_y(float val, int last_y, int color)
{
int py;

   // draw a data point on the plot
   py = (int) (val * (float) VERT_MAJOR);
   if(py >= PLOT_CENTER) {  // point is off the top of the plot area
      off_scale |= 0x01;   
      py = PLOT_ROW; 
   }
   else if(py <= (-PLOT_CENTER)) {  // point is off the bottom of the plot area
      off_scale |= 0x02;   
      py = PLOT_ROW+PLOT_CENTER+PLOT_CENTER; 
   }
   else {  // point is in the plot area
      py = (PLOT_ROW+PLOT_CENTER) - py;
   }

   if(plot_dot) dot(PLOT_COL+plot_column,py, color);
   else         line(PLOT_COL+last_plot_col,last_y,  PLOT_COL+plot_column,py, color);

   return py;
}

void plot_entry(long i)
{
float y;
struct PLOT_Q q;
int col;
int py;
float x;
int k;

   // draw all data points for the next column in the plot

   if(first_key) return;    // plot area is in use for help/warning message
   if(text_mode) return;    // no graphics available in text mode
   if(full_circle) return;
   if(queue_interval <= 0) return;  // no queue data to plot

   if(filter_count) q = filter_plot_q(i);   // plot filtered data
   else             q = get_plot_q(i);      // plot raw data

   for(k=0; k<NUM_PLOTS; k++) {     // compensate for drift rate
      q.data[k] -= ((plot[k].drift_rate*(float)plot_column)*view_interval); 
   }

   col = PLOT_COL + plot_column;    // where we will be plotting
   if(plot_column <= 0) {           // no data or only one point to plot
      last_plot_col = 0;
      plot_dot = 1;
   }
   else {  // with more than one point available, connect the dots with lines
      last_plot_col = plot_column-plot_mag;
      if(last_plot_col < 0) last_plot_col = 0;
      plot_dot = 0;
   }

   if(q.sat_flags & TIME_SKIP) {  // flag skips and stutters in the time stamps
      if(plot_skip_data) {        // use small skip markers
         line(col,PLOT_ROW, col,PLOT_ROW+8, SKIP_COLOR); 
      }
   }

   if(plot_holdover_data && (q.sat_flags & HOLDOVER)) {  // flag holdover events
      if(plot_dot) {
         dot(col,PLOT_ROW+0, HOLDOVER_COLOR);
         dot(col,PLOT_ROW+1, HOLDOVER_COLOR);
      }
      else {
         line(PLOT_COL+last_plot_col,PLOT_ROW+0, col,PLOT_ROW+0, HOLDOVER_COLOR);
         line(PLOT_COL+last_plot_col,PLOT_ROW+1, col,PLOT_ROW+1, HOLDOVER_COLOR);
      }
   }

   // flag satellite constellation changes
   if(plot_const_changes && (q.sat_flags & CONST_CHANGE)) {  
      line(col,PLOT_ROW+PLOT_HEIGHT, col,PLOT_ROW+PLOT_HEIGHT-8, CONST_COLOR); 
//    plot_dot = 1;  // highlights satellite change discontinuites
   }

   if(plot_sat_count) {
      py = (q.sat_flags & SAT_COUNT) * COUNT_SCALE;
      if(py > PLOT_HEIGHT) py = PLOT_HEIGHT;
      py = (PLOT_ROW+PLOT_HEIGHT) - py;
      if(plot_dot) dot(col,py, COUNT_COLOR);
      else         line(PLOT_COL+last_plot_col,last_count_y, col,py, COUNT_COLOR);
      last_count_y = py;
   }

   // draw each of the data plots
   for(k=0; k<NUM_PLOTS; k++) {
      if(plot[k].show_plot) {
         if(k == TEMP) y = (float) ((fmt_temp(q.data[k] / queue_interval) - plot[k].plot_center) * plot[k].ref_scale);  // y = ppt
         else          y = (float) (((q.data[k] / queue_interval) - plot[k].plot_center) * plot[k].ref_scale);  // y = ppt
         y /= (float) (plot[k].scale_factor*plot[k].invert_plot);
         plot[k].last_y = plot_y(y, plot[k].last_y, plot[k].plot_color);
      }

      if(plot[k].show_trend) {
         x = view_interval * queue_interval * (float) plot_column;
         if(plot_mag) x /= (float) plot_mag;
         y = (float) (((plot[k].a0 + (plot[k].a1*x)) - plot[k].plot_center) * plot[k].ref_scale);  // y = ppt
         y /= (float) (plot[k].scale_factor*plot[k].invert_plot);
         plot[k].last_trend_y = plot_y(y, plot[k].last_trend_y, plot[k].plot_color);
      }
   }
}

struct PLOT_Q last_q;

void add_stat_point(struct PLOT_Q *q)
{
int k;
float val;
float x;

   // calculate statistics of the points in the plot window
   x = (stat_count * view_interval * queue_interval);
   for(k=0; k<NUM_PLOTS; k++) { 
      val = q->data[k] / queue_interval;
      plot[k].sum_change += (val - (last_q.data[k]/queue_interval));
      plot[k].sum_y  += val;
      plot[k].sum_yy += (val * val);
      plot[k].sum_xy += (x * val);
      plot[k].sum_xx += (x * x);
      plot[k].sum_x  += x;
      plot[k].stat_count += 1.0F;
   }

   ++stat_count;
}

void calc_queue_stats()
{
long i;
int k;
float sxx, syy, sxy;
struct PLOT_Q q;
float qi;
float val;

   // prepare to calculate the statistics values of the plots
   for(k=0; k<NUM_PLOTS; k++) {  
      plot[k].sum_x      = 0.0F;
      plot[k].sum_y      = 0.0F;
      plot[k].sum_xx     = 0.0F;
      plot[k].sum_yy     = 0.0F;
      plot[k].sum_xy     = 0.0F;
      plot[k].stat_count = 0.0F;
      plot[k].sum_change = 0.0F;
      plot[k].max_val    = (-1.0E30F);
      plot[k].min_val    = (1.0E30F);
   }

   qi = (float) queue_interval;

   if(auto_center) {  // set min/max values in case auto scaling is off
      for(k=0; k<NUM_PLOTS; k++) {
         if(plot[k].float_center == 0) {
            plot[k].min_val = plot[k].max_val = plot[k].plot_center;
            if(k == TEMP) {      // !!!! ref_scale? 
               if(plot[k].plot_center == NEED_CENTER) { 
                  if(last_temperature) plot[k].min_val = plot[k].max_val = plot[k].plot_center = fmt_temp(last_temperature);
               }
            }
            else if(k == DAC) {  // !!!! ref_scale?
               if(plot[k].plot_center == NEED_CENTER) {
                  plot[k].min_val = plot[k].max_val = plot[k].plot_center = last_dac_voltage;
               }
            }
         }
      }
   }

   plot_column = 0;
   stat_count = 0.0F;
   plot_q_last_col = 0;

   i = plot_q_col0;
   while(i != plot_q_in) {  // scan the data that is in the plot window
      if(filter_count) q = filter_plot_q(i);
      else             q = get_plot_q(i);

      if(i == plot_q_col0) last_q = q;
      add_stat_point(&q);   // update statistics values
      last_q = q;

      if(auto_scale && (queue_interval > 0)) {  // find plot min and max value
         for(k=0; k<NUM_PLOTS; k++) {
            if(k == TEMP) val = fmt_temp(q.data[TEMP]/qi);
            else          val = q.data[k]/qi;

            if(val > plot[k].max_val) plot[k].max_val = val;
            if(val < plot[k].min_val) plot[k].min_val = val;
         }
      }

      plot_q_last_col = i;
      i = next_q_point(i, 1);
      if(i < 0) break;      // end of plot data reached
   }

   if(stat_count == 0.0F) return;

   for(k=0; k<NUM_PLOTS; k++) { // calculate linear regression values
      sxy = plot[k].sum_xy - ((plot[k].sum_x*plot[k].sum_y)/stat_count);
      syy = plot[k].sum_yy - ((plot[k].sum_y*plot[k].sum_y)/stat_count);
      sxx = plot[k].sum_xx - ((plot[k].sum_x*plot[k].sum_x)/stat_count);
      plot[k].stat_count = stat_count;
      if(sxx == 0.0F) {
         plot[k].a1 = plot[k].a0 = 0.0F;
      }
      else {
         plot[k].a1 = (sxy / sxx);
         plot[k].a0 = (plot[k].sum_y/stat_count) - (plot[k].a1*(plot[k].sum_x/stat_count));
      }
   }
}


void plot_queue_data()
{
long i;
long last_i;
int j;
u08 ticker[MAX_MARKER];

   //  plot the data points
   plot_column = 0;
   if((all_adevs == 0) || mixed_adevs) {
      for(j=0; j<MAX_MARKER; j++) { // prepare to find what marked points are shown
         ticker[j] = 1;
      }

      i = plot_q_col0;
      while(i != plot_q_in) {  // plot the data that is in the queue
         plot_entry(i);        // plot the data values

         last_i = i;           // go to next point to plot
         i = next_q_point(i, 1);
         if(i < 0) break;      // end of plot data reached

         for(j=0; j<MAX_MARKER; j++) {  // see if the queue entry is marked
            if(ticker[j] && mark_q_entry[j] && (mark_q_entry[j] >= last_i) && (mark_q_entry[j] <= i)) {
               plot_mark(j);
               ticker[j] = 0;
            }
         }
      }
      plot_q_last_col = last_i;

      // see if we have more data in the queue that can be plotted
      if((plot_q_count && (i == plot_q_in)) || (i == (-2L))) {  // more data is available
         sprintf(out, "%c", RIGHT_ARROW);  
      }
      else sprintf(out, " ");
      vidstr(PLOT_TEXT_ROW-1, (SCREEN_WIDTH/TEXT_WIDTH)-2, WHITE, out);
   }
}


void draw_plot(u08 refresh_ok)
{
   // draw everything related to the data plots
   if(first_key) return;   // plot area is in use for help/warning message

   if(text_mode) {         // graphics are not available,  only draw text stuff
      plot_axes();
      #ifdef ADEV_STUFF
         if(all_adevs && (refresh_ok != 2)) {
            show_adev_info();
         }
      #endif
      if(process_com == 0) {
         show_satinfo();
      }
      if(refresh_ok) refresh_page();
      return;   // no plotting in text modes
   }

   // locate the first queue entry we will be plotting
   plot_q_col0 = plot_q_out + plot_start;
   while(plot_q_col0 >= plot_q_size) plot_q_col0 -= plot_q_size;

   #ifdef AZEL_STUFF
      draw_azel_plot();  // draw the satellite location map
   #endif
   if(full_circle) return;

   if(queue_interval <= 0) {
      plot_axes();
      return;  // we have no queued data to plot
   }

   #ifdef FFT_STUFF
      if(show_live_fft) {    // calc FFT over the live data
         calc_fft(live_fft);
      }
   #endif
   calc_queue_stats();  // calc stat info data that will be displayed
   scale_plots();       // find scale factors and the values to center the graphs around 
   plot_axes();         // draw and label the plot grid
   plot_queue_data();   // plot the queue data
   show_stat_info();    // display statistics of the plots

   #ifdef ADEV_STUFF
      if(refresh_ok != 2) {   // draw the adev info
         show_adev_info();
      }
   #endif

   if(process_com == 0) {
      show_satinfo();
   }
   if(refresh_ok) {     // copy buffered screen image to the screen
      refresh_page();
   }
}


void update_plot(int draw_flag)
{
struct PLOT_Q q;

   // add current data values to the plot data queue
   q = get_plot_q(plot_q_in);

   q.data[TEMP] += temperature;
   q.data[DAC]  += dac_voltage;
   q.data[OSC]  += (OFS_SIZE) osc_offset;
   q.data[PPS]  += (OFS_SIZE) pps_offset;

   if(graph_lla) {
      if(XXX < NUM_PLOTS) q.data[XXX] += (float) (lon*RAD_TO_DEG);
      if(YYY < NUM_PLOTS) q.data[YYY] += (float) (lat*RAD_TO_DEG);
      if(ZZZ < NUM_PLOTS) q.data[ZZZ] += (float) alt;
   }
   else {
      if(XXX < NUM_PLOTS) q.data[XXX] += (float) ((dac_voltage-initial_voltage)*osc_gain*(-100000.0F));
      if(YYY < NUM_PLOTS) q.data[YYY] = (float) ((q.data[OSC]*1000.0F)-q.data[XXX]); 
   }
   if(FFT < NUM_PLOTS) q.data[FFT] = 0.0F;

   q.sat_flags &= SAT_FLAGS;       // preserve flag bits
   q.sat_flags |= sat_count;                      // satellite count
   q.sat_flags |= (new_const & CONST_CHANGE);     // constellation change
   if(time_flags & 0x01) q.sat_flags |= UTC_TIME; // UTC/GPS time flag

   if((discipline_mode == 6) && osc_control_on) ;
   else if(discipline_mode != 0) q.sat_flags |= HOLDOVER;

   if(spike_delay) q.sat_flags |= TEMP_SPIKE;

   if(flag_faults && (msg_fault || tsip_error)) q.sat_flags |= TIME_SKIP;

   q.hh = (u08) hours;
   q.mm = (u08) minutes;
   q.ss = (u08) seconds;
   q.dd = (u08) day;
   q.mo = (u08) month;
   q.yr = (u08) (year%100);

   put_plot_q(plot_q_in, q);

   new_const = 0;

   if(++plot_time < queue_interval) {  // not yet time to draw
      if(draw_flag) refresh_page();    // make sure text parts of screen keep updating
      return;  
   }
   plot_time = 0;

   if(review_mode == 0) {
      review = plot_q_count;  
   }

   ++view_time;
   if((view_time >= view_interval) && ((review_mode == 0) || (plot_column < (PLOT_WIDTH-plot_mag)))) {
      if(plot_q_in == plot_q_col0) last_q = q;
      add_stat_point(&q);
      last_q = q;
      if(draw_flag == 0) goto scroll_it;

      plot_entry(plot_q_in);  // plot latest queue entry
      plot_column += plot_mag;
      if(plot_column >= PLOT_WIDTH) {   // it's time to scroll the plot left
         plot_column -= (PLOT_SCROLL*plot_mag);
         plot_start = plot_q_in - (((PLOT_WIDTH - PLOT_SCROLL)*view_interval)/plot_mag) - plot_q_out;
         if(plot_start < 0) {
             if(plot_q_full) {
                plot_start += plot_q_size;
                if(plot_start < 0) plot_start = 0;
             }
             else plot_start = 0;
         }
         draw_plot(0);
      }
      else if(auto_scale && off_scale) {  // a graph is now off scale,  redraw the plots to rescale it
         draw_plot(0);
         off_scale = 0;
      }
      else if(continuous_scroll) draw_plot(0);
   }
   else {
      scroll_it:
      if(continuous_scroll) draw_plot(0);
   }
   if(view_time >= view_interval) view_time = 0;


   // prepare queue for next point
   if(++plot_q_count >= plot_q_size) plot_q_count = plot_q_size;
   if(++plot_q_in >= plot_q_size) {
      plot_q_in = 0;  
   }
   if(plot_q_in == plot_q_out) {  // plot queue is full
      if(++plot_q_out >= plot_q_size) plot_q_out = 0;
      plot_q_count = plot_q_size;
      plot_q_full = 1;
   }
   clear_plot_entry((long) plot_q_in);

   if(draw_flag) {
      show_stat_info();  // update plot statistics display
      refresh_page();
   }
}


void redraw_screen()
{
   // redraw everything on the screen
   #ifdef DOS_MOUSE
      hide_mouse();          // refresh the mouse pointer
      make_mouse_cursor();
      update_mouse();
   #endif

   #ifdef DIGITAL_CLOCK
      reset_vstring();       // cause the vector char string to be redrawn
   #endif

   survey_done = 0;
   if((check_precise_posn == 0) && (precision_survey == 0) && (show_fixes == 0)) {
      plot_lla = 0;
////  if(doing_survey == 0) plot_lla = 0;
   }

   erase_screen();
   show_log_state();
   request_rcvr_info();

   draw_plot(1);  // redraw the plot area
}


float get_stat_val(int id)
{
float val;

   if(stat_count == 0.0F) return 0.0F;

   if(plot[id].show_stat == RMS) {
      stat_id = "rms";
      return sqrt(plot[id].sum_yy/stat_count);
   }
   else if(plot[id].show_stat == AVG) {
      stat_id = "avg";
      return plot[id].sum_y/stat_count;
   }
   else if(plot[id].show_stat == SDEV) {
      stat_id = "sdv";
      val = ((plot[id].sum_yy/stat_count) - ((plot[id].sum_y/stat_count)*(plot[id].sum_y/stat_count)) );
//    val = ( plot[id].sum_yy - ((plot[id].sum_y*plot[id].sum_y)/stat_count) ) / stat_count;
      if(val < 0.0F) val = 0.0F - val;
      return sqrt(val);
   }
   else if(plot[id].show_stat == VAR) {
      stat_id = "var";
      val = ( (plot[id].sum_yy/stat_count) - ((plot[id].sum_y/stat_count)*(plot[id].sum_y/stat_count)) );
//    val = ( plot[id].sum_yy - ((plot[id].sum_y*plot[id].sum_y)/stat_count) ) / stat_count;
      return val;
   }
   else {
      stat_id = "???";
      return 0.0F;
   }
}

void fmt_osc_val(char *id, float val)
{
   if(res_t) {
      if((val >= 100000.0F) || (val <= (-10000.0F))) {
         sprintf(out, "%s:%11.3f%s  ", id, val, ppt_string);
      }
      else if((val >= 10000.0F) || (val <= (-1000.0F))) {
         sprintf(out, "%s:%11.4f%s  ", id, val, ppt_string);
      }
      else if((val >= 1000.0F) || (val <= (-100.0F))) {
         sprintf(out, "%s:%11.5f%s  ", id, val, ppt_string);
      }
      else {
         sprintf(out, "%s:%11.6f%s  ", id, val, ppt_string);
      }
   }
   else {
      val *= 1000.0F;

      if((val >= 100000.0F) || (val <= (-10000.0F))) {
         sprintf(out, "%s:%10.3f%s  ", id, val, ppt_string);
      }
      else if((val >= 10000.0F) || (val <= (-1000.0F))) {
         sprintf(out, "%s:%10.4f%s  ", id, val, ppt_string);
      }
      else if((val >= 1000.0F) || (val <= (-100.0F))) {
         sprintf(out, "%s:%10.5f%s  ", id, val, ppt_string);
      }
      else {
         sprintf(out, "%s:%10.6f%s  ", id, val, ppt_string);
      }
   }
}

void show_stat_info()
{
int col;
float val;

   // print statistics values of the data show on the screen
   if(text_mode || (first_key && (SCREEN_HEIGHT < 600))) return;
   if(full_circle) return;
   if(all_adevs && (mixed_adevs == 0)) return;
   if(stat_count == 0.0) return;

   col = MOUSE_COL;
   no_x_margin = 1;

   if(plot[OSC].show_stat) {
      val = get_stat_val(OSC);
      fmt_osc_val(stat_id, val);
      vidstr(MOUSE_ROW+3, col, plot[OSC].plot_color, out);
   }

   if(plot[PPS].show_stat) {
      val = get_stat_val(PPS);
      if(res_t) {
         val /= 1000.0F;
         if(val <= (-10000.0F))     sprintf(out, "%s:%11.4f us ", stat_id, val);
         else if(val <= (-1000.0F)) sprintf(out, "%s:%11.5f us ", stat_id, val);
         else if(val >= 10000.0F)   sprintf(out, "%s:%11.4f us ", stat_id, val);
         else                       sprintf(out, "%s:%11.6f us ", stat_id, val);
      }
      else {
         if(val <= (-10000.0F))     sprintf(out, "%s:%11.4f ns ", stat_id, val);
         else if(val <= (-1000.0F)) sprintf(out, "%s:%11.5f ns ", stat_id, val);
         else if(val >= 10000.0F)   sprintf(out, "%s:%11.4f ns ", stat_id, val);
         else                       sprintf(out, "%s:%11.6f ns ", stat_id, val);
      }
      vidstr(MOUSE_ROW+3, col+20,plot[PPS].plot_color, out);
   }

   if(plot[TEMP].show_stat) {
      val = get_stat_val(TEMP);
      sprintf(out, "%s:%10.6f %c%c", stat_id, fmt_temp(val), DEGREES, DEG_SCALE);
      vidstr(MOUSE_ROW+2, col+40, plot[TEMP].plot_color, out);
   }

   if(plot[DAC].show_stat) {
      val = get_stat_val(DAC);
      if(res_t) {
         sprintf(out, "%s:%10.6f ns", stat_id, val);
      }
      else {
         sprintf(out, "%s:%10.6f V", stat_id, val);
      }
      vidstr(MOUSE_ROW+3, col+40, plot[DAC].plot_color, out);
   }


   // show the RMS value of the first extra plot displayed
   // (i.e. the last one that was selected)
   if(extra_plots && (first_extra_plot >= FIRST_EXTRA_PLOT)) { 
      if(plot[first_extra_plot].show_stat) {
#ifdef FFT_STUFF
         if(plot[FFT].show_plot == 0) {
#else
         {
#endif
            val = get_stat_val(first_extra_plot);
            sprintf(out, "%s:%10.6f %s", stat_id, val, plot[first_extra_plot].units);
            vidstr(MOUSE_ROW+1, col+40, plot[first_extra_plot].plot_color, out);
         }
      }
   }

   no_x_margin = 0;
}


//
//
//   Show data at the mouse cursor when the mouse is in the plot area
//
//
void show_cursor_info(S32 i)
{
struct PLOT_Q q;
float val;
int col;
int k;

   // show the data of the point the mouse cursor is pointing at in the plots
   if(text_mode || (first_key && (SCREEN_HEIGHT < 600))) return;
   if(full_circle) return;
   if(filter_count) q = filter_plot_q(i);
   else             q = get_plot_q(i);

   pri_hours   = q.hh;
   pri_minutes = q.mm;
   pri_seconds = q.ss;
   pri_day     = q.dd;
   pri_month   = q.mo;
   pri_year    = q.yr;
   if(pri_year >= 80) pri_year += 1900;
   else               pri_year += 2000;
   adjust_tz();  // tweak pri_ time for time zone

   col = MOUSE_COL;
   no_x_margin = 1;

   sprintf(out, "Cursor time: %02d:%02d:%02d   %s            ",
       pri_hours,pri_minutes,pri_seconds, fmt_date(),
       time_zone_set?tz_string:(q.sat_flags & UTC_TIME) ? "UTC":"GPS");
   vidstr(MOUSE_ROW+0, col, MOUSE_COLOR, out);

   if(res_t) {
      sprintf(out, "%s:%10.6f ns   ", plot[DAC].plot_id, q.data[DAC]/(float) queue_interval);
   }
   else {
      sprintf(out, "%s:%10.6f V    ", plot[DAC].plot_id, q.data[DAC]/(float) queue_interval);
   }
   vidstr(MOUSE_ROW+1, col, plot[DAC].plot_color, out);

   sprintf(out, "%s:%10.6f %c%c", plot[TEMP].plot_id, 
                fmt_temp(q.data[TEMP]/(float) queue_interval), DEGREES,DEG_SCALE);
   #ifdef VARIABLE_FONT
      strcat(out, "    ");
   #endif
   vidstr(MOUSE_ROW+1, col+20,plot[TEMP].plot_color, out);

   val = (float) (q.data[OSC]/(OFS_SIZE) queue_interval);
   fmt_osc_val(plot[OSC].plot_id, val);
   vidstr(MOUSE_ROW+2, col, plot[OSC].plot_color, out);

   val = (float) (q.data[PPS]/(OFS_SIZE) queue_interval);
   if(res_t) {
      val /= 1000.0F;
      if(val <= (-10000.0F))     sprintf(out, "%s:%11.4f us", plot[PPS].plot_id,val);
      else if(val <= (-1000.0F)) sprintf(out, "%s:%11.5f us", plot[PPS].plot_id,val);
      else if(val >= 10000.0F)   sprintf(out, "%s:%11.4f us", plot[PPS].plot_id,val);
      else                       sprintf(out, "%s:%11.6f us", plot[PPS].plot_id,val);
   }
   else {
      if(val <= (-10000.0F))     sprintf(out, "%s:%11.4f ns", plot[PPS].plot_id,val);
      else if(val <= (-1000.0F)) sprintf(out, "%s:%11.5f ns", plot[PPS].plot_id,val);
      else if(val >= 10000.0F)   sprintf(out, "%s:%11.4f ns", plot[PPS].plot_id,val);
      else                       sprintf(out, "%s:%11.6f ns", plot[PPS].plot_id,val);
   }
   #ifdef VARIABLE_FONT
      strcat(out, "      ");
   #endif
   vidstr(MOUSE_ROW+2, col+20, PPS_COLOR, out);

#ifdef FFT_STUFF
   if(plot[FFT].show_plot && fft_scale) {
      i -= fft_queue_0;
      if(i < 0) i += plot_q_size;
      i /= (S32) fft_scale;
      val = (float) i * fps;
      if(i > (fft_length/2)) sprintf(out, "              ");
      else if(val) {
         val = 1.0F/val;
         sprintf(out, "%s: %-.1f sec     ", plot[FFT].plot_id, val);
      }
      else sprintf(out, "%s: DC blocked    ", plot[FFT].plot_id, val);
      vidstr(MOUSE_ROW+1, MOUSE_COL+40, plot[FFT].plot_color, out);
   }
#endif

   no_y_margin = 1;
   for(k=FIRST_EXTRA_PLOT; k<NUM_PLOTS; k++) {  // show the values for the extra plots
      col = plot[k].slot;     // the slot number of the plot header
      if(col < 0) continue;   // the plot has no assigned slot
      col = slot_column[col]; // column to draw the plot header at
      if(col < 0) continue;   // the slot has no assigned column
      if(plot[k].show_plot == 0) continue;
      val = q.data[k]/queue_interval;
      sprintf(out, "%10.6f", val);

      vidstr(PLOT_TEXT_ROW-3, PLOT_TEXT_COL+col, plot[k].plot_color, out);
   }

   no_x_margin = no_y_margin = 0;

   show_stat_info();
}

void erase_cursor_info()
{
int col;
int k;

   // erase the old data of the point the mouse cursor 
   // is pointing at in the plots
   if(text_mode || (first_key && (SCREEN_HEIGHT < 600))) return;
   if(full_circle) return;
   col = MOUSE_COL;

   no_x_margin = 1;
   #ifdef VARIABLE_FONT
      if(small_font == 1) strcpy(out, &blanks[TEXT_COLS-76]);
      else                strcpy(out, &blanks[TEXT_COLS-61]);
   #endif
   #ifdef FIXED_FONT
      strcpy(out, &blanks[TEXT_COLS-40]);
   #endif

   vidstr(MOUSE_ROW+0, col, WHITE, out);
   vidstr(MOUSE_ROW+1, col, WHITE, out);
   vidstr(MOUSE_ROW+2, col, WHITE, out);
   if(plot_stat_info) {
      vidstr(MOUSE_ROW+3, col, WHITE, out);
   }

   strcpy(out, &blanks[TEXT_COLS-17]);
   no_y_margin = 1;
   for(k=FIRST_EXTRA_PLOT; k<NUM_PLOTS; k++) {  // erase the values for the extra plots
      col = plot[k].slot;     // the slot number of the plot header
      if(col < 0) continue;   // the plot has no assigned slot
      col = slot_column[col]; // column to draw the plot header at
      if(col < 0) continue;   // the slot has no assigned column

      vidstr(PLOT_TEXT_ROW-3, PLOT_TEXT_COL+col, WHITE, out);
   }

   no_x_margin = no_y_margin = 0;
}

int last_mouse_x, last_mouse_y;
volatile int timer_serve;

int show_mouse_info()
{
S32 i;
u08 new_mouse_info;
float val;

   // this routine shows the queue data at the mouse cursor in the plot window
   if(queue_interval <= 0) return 0;
   if(timer_serve) return 0;  // don't let windows timer access this routine

   mouse_time_valid = 0;
   new_mouse_info = 0;

   // get mouse coordinates here
#ifdef WINDOWS
   SAL_serve_message_queue();
   Sleep(0);

   if(SAL_is_app_active()) {
      SAL_WINAREA wndrect;
      SAL_client_area(&wndrect);    // Includes menu if any

      POINT cursor;
      GetCursorPos(&cursor);

      mouse_x = cursor.x - wndrect.x;
      mouse_y = cursor.y - wndrect.y;
      // adjust position in case screen area is scaled down
      val = ((float) mouse_x * (float) SCREEN_WIDTH) / (float) wndrect.w;
      mouse_x = (int) val;
      val = ((float) mouse_y * (float) SCREEN_HEIGHT) / (float) wndrect.h;
      mouse_y = (int) val;

      last_button = this_button;
      if     (GetKeyState(VK_LBUTTON) & 0x8000) this_button = 1;
      else if(GetKeyState(VK_RBUTTON) & 0x8000) this_button = 2;
      else                                      this_button = 0;

      if(mouse_disabled) return 0;
#endif

#ifdef DOS_BASED
   {
      if(mouse_disabled) return 0;
      val = 0.0F;
      #ifdef DOS_MOUSE
         if(dos_mouse_found == 0) return 0;   // no dos mouse found
         update_mouse();              
         mouse_x = mouse_xx;
         mouse_y = mouse_yy;
      #else
         return 0;     // mouse code not compiled
      #endif
#endif

      mouse_x -= PLOT_COL;

      if((mouse_x >= 0) && (mouse_x < plot_column) && (mouse_y >= PLOT_ROW) && (mouse_y < (PLOT_ROW+PLOT_HEIGHT))) { // mouse is in the plot area
         if(last_mouse_x != mouse_x) new_mouse_info = 1;
         else if(last_mouse_y != mouse_y) new_mouse_info = 1;

         i = plot_q_col0 + ((view_interval * mouse_x)/plot_mag);
         while(i >= plot_q_size) i -= plot_q_size;
         last_mouse_q = i;
         mouse_time_valid = 1;

         if((this_button == 1) && (last_button == 0)) {  // zoom plot
            right_time = 0;
            kbd_zoom();
         }
         else if((this_button == 2) && (last_button == 0)) {  // mark plot and center on it
            right_time = 0;
            right_click:
            mark_q_entry[0] = last_mouse_q;  // mark the point
            goto_mark(0);  // center plot on the marked point
         }
         else if((this_button == 2) && (last_button == 2)) {
            if(++right_time >= RIGHT_DELAY) goto right_click;
         }
         else right_time = 0;
      }   
//    else if(last_mouse_time_valid) {  // mouse just moved out of plot area
//       new_mouse_info = 1;  // force page redraw if no serial port connected
//    }

      last_mouse_x = mouse_x;
      last_mouse_y = mouse_y;
   }

   if(mouse_time_valid) {  // show the data that is at the mouse cursor
      show_cursor_info(i);
   }
   else if(last_mouse_time_valid) {  // erase old data from screen
      erase_cursor_info();
   }

   last_mouse_time_valid = mouse_time_valid;

   if(new_mouse_info) {
      refresh_page();
      return 1;
   }
   return 0;
}


//
//
//  stuff to position plot queue data onto the screen
//
//
void plot_review(long i)
{
   // start viewing queue data at point *i*
   if(i <= 0) review_home = 1;
   else       review_home = 0;

   if(i >= plot_q_count) {  // we are past the end of the data, back up a minor tick
      while(i >= plot_q_count) i = plot_q_count - (HORIZ_MINOR*view_interval) / (long) plot_mag;
      if(i < 0) i = 0;
      BEEP();
   }
   else if(i < 0) {  // we are at the start of the data in the queue
      i = 0;
      BEEP();
   }

   review = i;
   review_mode = 1;
   plot_start = review;

   draw_plot(1);
}

void zoom_review(long i, u08 beep_ok)
{
   i -= plot_q_out;

   if(i >= plot_q_count) {  // we are past the end of the data, back up a minor tick
      i -= plot_q_count;
      if(beep_ok) BEEP();
   }
   if(i < 0) {  // we are at the start of the data in the queue
      i = 0;
      if(beep_ok) BEEP();
   }

   review = i;
   review_mode = 1;
   plot_start = review;

   draw_plot(0);

   show_mouse_info();  // update cursor info in case we are not doing live data
   refresh_page();
}

void kbd_zoom()
{
u32 val;

   // toggle view interval from 1 sec/pixel to longer view
   if(mouse_time_valid) {
      if((view_interval == 1L) && (queue_interval > 0)) {
         if(user_view) view_interval = user_view;
         else view_interval = (3600L/HORIZ_MAJOR)/queue_interval;  // 1 hr per division
      }
      else {
         day_plot = 0;
         view_interval = 1L;
      }

      mark_q_entry[0] = last_mouse_q;  // marker 0 is mouse click marker
      val = last_mouse_q;
      val -= (((PLOT_WIDTH/2)*view_interval) / (long) plot_mag);  // center point on screen
      if(last_mouse_q < plot_q_out) val += plot_q_size;
      zoom_review(val, 1);
   }
}

void goto_mark(int i)
{
long val;

   // center plot window on the marked point
   if(mark_q_entry[i]) { // a queue entry is marked
      last_q_place = last_mouse_q;
      val = mark_q_entry[i];
      val -= (((PLOT_WIDTH/2)*view_interval) / (long) plot_mag);  // center point on screen
      if(mark_q_entry[i] < plot_q_out) val += plot_q_size;
      zoom_review(val, 0);
   }
}


void end_review(u08 draw_flag)
{
   // exit plot review mode
   review_mode = 0;
   review = plot_q_count;

   plot_start = plot_q_in - (((PLOT_WIDTH - PLOT_SCROLL)*view_interval)/(long)plot_mag) - plot_q_out;
   if(plot_start < 0) {
       if(plot_q_full) {
          plot_start += plot_q_size;
          if(plot_start < 0) plot_start = 0;
       }
       else plot_start = 0;
   }

   pause_data = 0;
   restore_plot_config();
   if(draw_flag) draw_plot(1);
}

void do_review(int c)
{
   // move the view around in the plot queue
   if(c == HOME_CHAR)  {  // start of plot data 
      plot_review(0L);
   }
   else if(c == END_CHAR) { // end of plot data
      review_end:
      review = plot_q_count - ((PLOT_WIDTH*view_interval)/(long)plot_mag) + 1;
      if(review < 0) review = 0;
      plot_review(review);
   }
   else if(c == LEFT_CHAR) {  // scroll one major division
      if(review_mode == 0) goto review_end;
      plot_review(review + ((HORIZ_MAJOR*view_interval)/(long)plot_mag));
   }
   else if(c == RIGHT_CHAR) {
      if(review_mode == 0) goto review_end;
      plot_review(review - ((HORIZ_MAJOR*view_interval)/(long)plot_mag));
   }
   else if(c == PAGE_UP) {  // scroll one screen
      if(review_mode == 0) goto review_end;
      plot_review(review + ((PLOT_WIDTH*view_interval)/(long)plot_mag));
   }
   else if(c == PAGE_DOWN)  {
      if(review_mode == 0) goto review_end;
      plot_review(review - ((PLOT_WIDTH*view_interval)/(long)plot_mag));
   }
   else if(c == UP_CHAR) {  // scroll one hour
      if(review_mode == 0) goto review_end;
      if(queue_interval > 0) plot_review(review + (3600L/queue_interval));
   }
   else if(c == DOWN_CHAR) {
      if(review_mode == 0) goto review_end;
      if(queue_interval > 0) plot_review(review - (3600L/queue_interval));
   }
   else if(c == '<') {      // scroll one day
      if(review_mode == 0) goto review_end;
      if(queue_interval > 0) plot_review(review+(long) (24L*3600L/queue_interval));
   }
   else if(c == '>') {  
      if(review_mode == 0) goto review_end;
      if(queue_interval > 0) plot_review(review-(long) (24L*3600L/queue_interval));
   }
   else if(c == '[') {      // scroll one pixel
      if(review_mode == 0) goto review_end;
      if(queue_interval > 0) plot_review(review+view_interval);
   }
   else if(c == ']') {  
      if(review_mode == 0) goto review_end;
      if(queue_interval > 0) plot_review(review-view_interval);
   }
   else {  // terminate scroll mode
      end_review(1);
   }
}

void new_view()
{
   // set up display for the new view parameters
   if((view_interval == 1L) && (queue_interval > 0)) {
      view_interval = (3600L/HORIZ_MAJOR)/queue_interval;  // 1 hr per division
   }
   else {
      day_plot = 0;
      view_interval = 1L;
      config_screen();
   }
   if(view_interval <= 0L) view_interval = 1L;
   user_view = 0;

   redraw_screen();
}

void adjust_view()
{
   // configure screen for the view the user specified on the command line
   if(set_view == 1) {
      config_screen();
      day_plot = SCREEN_WIDTH / HORIZ_MAJOR;

      if((view_interval == 1L) && (queue_interval > 0)) {
         view_interval = (3600L/HORIZ_MAJOR)/queue_interval;  // 1 hr per division
      }
      else {
         day_plot = 0;
         view_interval = 1L;
         config_screen();
      }
      if(view_interval <= 0L) view_interval = 1L;
      user_view = 0;
//    user_view = day_plot;
   }
}


//
//
//  Script and config file processing
//
//

void wait_for_key()
{
   if(timer_serve) return; // don't let windows timer access this routine
   first_key = ' ';  // make sure plot area is not disturbed
   while(break_flag == 0) {
      #ifdef WINDOWS
         SAL_serve_message_queue();
         Sleep(0);
      #endif
      if(KBHIT()) break;

      if(process_com) {
         get_pending_gps();   //!!!! possible recursion
      }
   }
}

int get_script()
{
int i;

   // read a simulated keystroke from a script file
   i = fgetc(script_file);
   if(i < 0) {  // end of file
      close_script(0);
      return 0;
   }
   else if(i == 0x00) { // two byte cursor char
      i = fgetc(script_file);
      if(i < 0) {
         close_script(0);
         return 0;
      }
      i += 0x0100;
   }
   else if((skip_comment == 0) && (i == '~')) { // switch input to keyboard
      script_pause = 1;
      return 0;
   }

   if(i == 0x0A) i = 0x0D; // CR and LF are the same
   if(i == 0x0D) {         // end of line
      ++script_line;
      script_col = 0;
      skip_comment = 0;
   }
   else ++script_col;

   if(skip_comment) return 0;
   return i;
}

int process_script_file()
{
int i;

   if(script_fault) {  // error in script,  abort
      close_script(1);
      getting_string = 0;
      return 0;
   }
   else if(KBHIT()) {  // key pressed,  abort script
      i = edit_error("Script aborted...  key pressed.");
      close_script(1);
      getting_string = 0;
      return 0;
   }
   else {   // get keyboard command from the script file
      i = get_script();
      if(i) do_kbd(i);
      return i;
   }
}


int read_config_file(char *name, u08 local_flag)
{
char fn[128+1];
char *dot;
FILE *file;
int error;

   // read command line options from a .CFG file
   strcpy(fn, name);
   dot = strstr(fn, ".");
   if(dot) {   // file name has a .EXTension
               // replace with .CFG,  unless user gave the name on the command line
      if(local_flag == 1) {  // cfg file name is based upon the .EXE file name
         strcpy(dot, ".CFG"); 
      }
      else if(local_flag == 2) {
         strcpy(dot, ".CAL"); 
      }
   }
   else {
      if(local_flag == 1) strcat(fn, ".CFG");  // no extension given,  use .CFG
      else                strcat(fn, ".CAL");  // no extension given,  use .CAL
   }

   if(local_flag) {  // cfg file name is based upon the .EXE file name
      dot = strstr(fn, ".");     
      while(dot >= &fn[0]) {  // look in current directory first
         if(*dot == '\\') break;
         --dot;
      }
      file = fopen(dot+1, "r");
      if(file) {
         if(local_flag == 2) printf("Reading calendar file: %s\n", dot+1);
         else                printf("Reading config file: %s\n", dot+1);
      }
   }
   else file = 0;

   if(file == 0) {  // now look in .EXE file directory
      file = fopen(fn, "r");
      if(file == 0) { // config file not found
         if(local_flag) return 1;
         sprintf(blanks, "\nConfig file %s not found.", fn);
         out[0] = 0;
         command_help(blanks, out, 0); 
         exit(10);
      }
      printf("Reading config file: %s\n", fn);
   }

   if(local_flag == 2) {
      fclose(file);
      #ifdef GREET_STUFF
         read_calendar(fn, 1);
      #endif
      return 0;
   }

   while(fgets(out, sizeof out, file) != NULL) {
      if((out[0] != '/') && (out[0] != '-') && (out[0] != '=') && (out[0] != '$')) continue;
      error = option_switch(out);
      if(error) {
         sprintf(blanks, "in config file %s: ", fn);
         command_help(blanks, out, 0); 
         exit(10);
      }
   }

   fclose(file);
   return 0;
}


void config_options(void)
{
int k;

   // configure system for any screen related command line options that were set
   if(need_screen_init) {
      need_screen_init = 0;
      init_screen();
   }

   if(keyboard_cmd == 0) {
      if(SCREEN_WIDTH < 1024) {  // blank clock name for small screens
         if(clock_name[0] == 0)  strcpy(clock_name,  " ");
         if(clock_name2[0] == 0) strcpy(clock_name2, " "); 
      }
   }

   // set where the first point in the graphs will be
   last_count_y = PLOT_ROW + (PLOT_HEIGHT - VERT_MINOR);
   for(k=0; k<NUM_PLOTS; k++) {
      plot[k].last_y = PLOT_ROW+PLOT_CENTER;
      plot[k].last_trend_y = PLOT_ROW+PLOT_CENTER;
   }

   if(user_view_string[0]) {  // user set a plot view interval on the command line
      edit_user_view(user_view_string);
   }
   else if(user_view && queue_interval) {   // set user specified plot window time (in hours)
      view_interval = (user_view * 3600L) / queue_interval;
      view_interval /= PLOT_WIDTH;
      if(view_interval <= 1L) view_interval = 1L;
      user_view = view_interval;
   }                                                 

   // user set a log param on the command line, start writing the log file
   if(user_set_log) { 
      log_file_time = log_interval + 1;
      open_log_file(log_mode);
   }

   // user set a daylight savings time definition
   if(user_set_dst && have_year) {
      calc_dst_times(dst_list[dst_area]);
   }

   // if user did not enable/disable the big clock,  turn it on if it will fit
   if(user_set_bigtime == 0) {  
      if(SCREEN_WIDTH > 800) plot_digital_clock = 1; 
      else if((TEXT_HEIGHT <= 8) && (text_mode == 0)) plot_digital_clock = 1; ////
      else if((small_font == 1) && text_mode) plot_digital_clock = 1;
   }

   #ifdef TEMP_CONTROL
      if(do_temp_control) {  // user wants to actively stabilize the unit temperature
         if(desired_temp) enable_temp_control();
         else             disable_temp_control();
      }
   #endif

   if(not_safe) {
      init_messages();
   }
}


void set_defaults()
{
double angle;
int i;
int j;
FILE *f;

   // This routine is used to give initial values to variables.
   // The QuickC linker does not let one initialize variables from more than 
   // one file (even if they are declared extern).

   f = 0;
   #ifdef WINDOWS
      // See if user sound files exist for the cuckoo clock and alarm clock
      // If so, we play them.  Otherwise we use the default sounds.
      f = fopen(CHIME_FILE, "r");   // see if chime sound file exists
      if(f) {
         chime_file = 1;
         fclose(f);
      }
      f = fopen(ALARM_FILE, "r");   // see if alarm sound file exists
      if(f) {
         alarm_file = 1;
         fclose(f);
      }

      continuous_scroll = 1;        // windows defaults to continuous scroll mode

      IP_addr[0] = 0;     // TCP
      enable_timer = 1;   // enable windows dialog timer
   #endif

   angle = 0.0;
   #ifdef SIN_TABLES
      // precalculate sin and cos values - uses memory DOS version can't spare
      for(i=0; i<=360; i+=1) {
         angle = ((double) i) * DEG_TO_RAD;
         sin_table[i] = (float) sin(angle);
         cos_table[i] = (float) cos(angle);
      }
   #endif

   TEXT_WIDTH = 8;        // font size
   TEXT_HEIGHT = 16;
   VCHAR_SCALE = 6;       // default vector char size multiplier

   TEXT_COLS = 80;        // screen size (in text charactrers)
   TEXT_ROWS = 25;
   INFO_COL = 65;

   screen_type = 'm';     // medium size screen
   SCREEN_WIDTH = 1024;
   SCREEN_HEIGHT = 768;
   PLOT_ROW = 468;

   beep_on = 1;           // allow beeps

   com_port = 1;          // use COM1
   process_com = 1;
   first_msg = 1;        

   max_sats = 8;          // used to format the sat_info data
   temp_sats = 8;
   eofs = 1;

   pv_filter = 1;         // default power-up dynamics filters
   static_filter = 1;
   alt_filter = 1;
   kalman_filter = 0;

   user_pv = 1;           // user requested dynamics filters
   user_static = 1;
   user_alt = 1;
   user_kalman = 1;

   cmd_tc = 500.0F;       // default disciplining params
   cmd_damp = 1.000F;
   cmd_gain = 1.400F;
   cmd_dac = 3.000F;
   cmd_minv = 0.0F;
   cmd_maxv = 5.0F;

   foliage_mode = 1;      // sometimes
   dynamics_code = 4;     // stationary
   pdop_mask = 8.0F;
   pdop_switch = pdop_mask * 0.75F;

   user_pps_enable = 1;   // output signal controls
   pps_polarity = 0;
   osc_polarity = 0;
   osc_discipline = 1;

   set_utc_mode = 1;      // default to UTC mode
   set_gps_mode = 0;

   delay_value = 50.0F * (1.0F / (186254.0F * 5280.0F)) / 0.66F;  // 50 feet of 0.66 vp coax
   dac_drift_rate = 0.0F; // volts per second

   last_rmode = 7;
   last_hours = 99;
   last_second = 99;
   last_utc_offset = (s16) 0x8000;
   force_utc_time = 1;
   first_sample = 1;       // flag cleared after first data point has been received
   time_color = RED;
   time_set_char = ' ';
   time_sync_offset = 45L; // milliseconds from 1PPS output to end of receiver timing message

   queue_interval = 1;     // seconds between queue updates
   log_interval = 1;       // seconds between log file entries
   log_header = 1;         // write timestamp headers to the log file
   view_interval = 1;      // plot window view time
   show_min_per_div = 1;
   day_size = 24;          // assumed day plot size
   plot_mag = 1;           // plot magnifier (for more than one pixel per second)
   plot_column = 0;
   stat_count = 0.0F;

   log_errors = 1;         // if set, log data errors
   strcpy(log_name, "TBOLT.LOG");
   log_mode = "w";

   #ifdef ADEV_STUFF
      ATYPE = OSC_ADEV;
      adev_period = 1.0F;
      bin_scale = 5;        // 1-2-5 adev bin sequence
      n_bins = ADEVS;
      min_points_per_bin = 4;
      keep_adevs_fresh = 1;
   #endif

   // flags to control what and what not to draw
   plot_adev_data = 1;      // adevs
   plot_skip_data = 1;      // time sequence and message errors
   plot_sat_count = 1;      // satellite count
   small_sat_count = 1;     // used compressed sat count plot
   plot_const_changes = 0;  // satellite constellation changes
   plot_holdover_data = 1;  // holdover status
   plot_digital_clock = 0;  // big digital clock
   plot_loc = 1;            // actual lat/lon/alt
   stat_id = "RMS";
   plot_dops = 0;           // dilution of precision (in place of filters on small screens)
   plot_azel = 0;           // satellite azimuth/elevation map
   plot_signals = 0;        // satellite signal levels
   plot_el_mask = 1;        // show elevation angle mask in the azel plot
   map_vectors = 1;         // draw satellite movement vectors
   plot_background = 1;     // highlight plot area background color (WINDOWS)
   shared_plot = 0;         // share az/el or lla maps with the plot area

   show_euro_ppt = 0;
   set_osc_units();

   extra_plots = 0;
   num_plots = NUM_PLOTS;
   plot_stat_info = 0;
   for(i=0; i<num_plots; i++) {     // initialize default plot parameters
      plot[i].invert_plot  = 1.0F;      // normal graph polarity
      plot[i].scale_factor = 1.0F;      // unity data scale factor
      plot[i].show_stat = RMS;          // statistic to show
      slot_in_use[i] = i;
      if(i >= FIRST_EXTRA_PLOT) {
         extra_plots |= plot[i].show_plot;
         plot[i].slot = (-1);
         slot_in_use[i] = (-1);
      }
      plot_stat_info |= plot[i].show_stat;
   }

   plot[OSC].units = ppt_string;
   plot[DAC].plot_center = plot[TEMP].plot_center = NEED_CENTER;

   auto_scale = 1;             // auto scale the plots
   auto_center = 1;            // and auto center them

   deg_string[0] = DEGREES;
   deg_string[1] = 0;
   DEG_SCALE = 'C';            // Celcius
   alt_scale = "m";            // meters
   dms = 0;                    // decimal degrees
   strcpy(tz_string,  "LOC");  // current time zone name
   strcpy(std_string, "LOC");  // normal time zone name
   strcpy(dst_string, "LOC");  // daylight savings time zone name

   LLA_SPAN = 50.0;              // lla plot scale in feet each side of center
   ANGLE_SCALE = 2.74e-6;        // degrees per foot
   angle_units = "ft";
// LLA_SPAN = 20.0;
// ANGLE_SCALE = (2.74e-6*3.28); // degrees per meter
// angle_units = "m";

   #ifdef DOS
      #ifdef DOS_EXTENDER
         adev_q_size = (33000L);        // good for 10000 tau
         plot_q_size = (3600L*24L*3L);  // 3 days of data
         ems_ok = 0;
      #else
         adev_q_size = (22000L);        // good for 10000 tau
         plot_q_size = (3600L*3L);      // 3 hours of data (gets boosted to 3 days if EMS memory seen)
         ems_ok = 1;
      #endif
      mouse_shown = 0;
   #else
      adev_q_size = (33000L);         // good for 10000 tau
      plot_q_size = (3600L*24L*3L);   // 72 hours of data
      ems_ok = 0;
      mouse_shown = 1;
   #endif

   // convert __DATE__ into Lady Heather's offical date format
   i = j = 0;
   while(__DATE__[i] && (__DATE__[i] != ' ')) ++i;
   while(__DATE__[i] && (__DATE__[i] == ' ')) ++i;
   while(__DATE__[i] && (__DATE__[i] != ' ') && (j < VSTRING_LEN-8)) { 
      date_string[j++] = __DATE__[i++];
   }
   if(j == 1) {
      date_string[j++] = date_string[0];
      date_string[0] = '0';
   }
   date_string[j++] = ' ';
   date_string[j++] = __DATE__[0];
   date_string[j++] = __DATE__[1];
   date_string[j++] = __DATE__[2];
   date_string[j++] = ' ';
   while(__DATE__[i] && (__DATE__[i] == ' ')) ++i;
   while(__DATE__[i] && (__DATE__[i] != ' ') && (j < VSTRING_LEN-2)) { 
      date_string[j++] = __DATE__[i++];
   }
   date_string[j++] = 0;

   #ifdef TEMP_CONTROL
      do_temp_control = 0;     // don't attempt temperature control
      temp_control_on = 0;
      desired_temp = 40.0;     // in degrees C
      lpt_port = 0;
      port_addr = 0x0000;
      #ifdef DOS
//       lpt_port = 1;
//       port_addr = 0x378;
      #endif
   #endif
   mayan_correlation = MAYAN_CORR;

   init_dsm();        // set up the days to start of month table
   plot_title[0] = 0;
   title_type = NONE;
   greet_ok = 1;

   log_stream = 0;    // don't write serial data stream to the log file
   flag_faults = 1;   // show message errors as time skips

   debug_text = debug_text2 = 0;

   test_heat = test_cool = (-1.0F);
   test_marker = 0;
   spike_threshold = 0.04F;
   spike_mode = 1;    // filter temp spikes from pid data

   set_default_pid(0);
   KL_TUNE_STEP = 0.20F;

   #ifdef OSC_CONTROL
      set_default_osc_pid(0);
      OSC_KL_TUNE_STEP = 0.0;
   #endif

   #ifdef SIG_LEVELS
      clear_signals();
   #endif

   #ifdef GREET_STUFF
      calendar_entries = calendar_count();
   #endif

   d_scale = 1.0F;
   osc_gain = user_osc_gain = (-3.5);
   gain_color = YELLOW;

   undo_fw_temp_filter = 1;
   idle_sleep = DEFAULT_SLEEP;
}



//
//
//   Process the GPS receiver data
//
//
void get_pending_gps()
{
u08 old_disable_kbd;

   // process all the data currently in the com port input buffer
   old_disable_kbd = disable_kbd;
   disable_kbd = 2; // (so that do_kbd() won't do anything when it's called by WM_CHAR during get_pending_gps())
   system_idle = 0; // we are doing other things besides waiting for serial data

   while((break_flag == 0) && (take_a_dump == 0) && process_com && SERIAL_DATA_AVAILABLE()) {
      #ifdef WINDOWS
         SAL_serve_message_queue();
         Sleep(0);
      #endif
      get_tsip_message();
   }

   disable_kbd = old_disable_kbd;
}


S32 serve_gps(void)  // called from either foreground loop or WM_TIMER (e.g., while dragging)
{
S32 i;

   if(break_flag) return 0;        // ctrl-break pressed, so exit the program
   if(take_a_dump) return 1;

// if(in_service) return 1;

   while(1) {
      #ifdef WINDOWS
         if (!timer_serve)
            {
         SAL_serve_message_queue();     // OK to re-enter serve_gps() via WM_TIMER invoked from this call...
         Sleep(0);                      // (in fact, that will usually be the case) 
            }
      #endif
      #ifdef DOS_VFX
         if(mouse_shown && mouse_moved) {
            refresh_page();
            mouse_moved = 0;
         }
      #endif

      if(process_com == 0) i = 0;
      else i = SERIAL_DATA_AVAILABLE(); // check if serial port has data (and stay in loop until it's exhausted)

      if(i == 0) {    // no data available,  we are not too busy
         system_idle = 1; 
         update_pwm();
         show_mouse_info();
         if(process_com == 0) refresh_page();
         break;
      }

      if(process_com && i) {          // we have serial data to process
         get_tsip_message();          // process the incoming GPS message
         system_idle = 0;
      }
      if(show_mouse_info() == 0) {    // show queue data at the mouse cursor
//       if(i == 0) refresh_page();   // keep refreshing page in case no serial data
      }
   }

   return 1;
}

void show_com_state()
{
   if(process_com) {       // we are using the serial port
      #ifdef WINDOWS
         if(com_port == 0) {   // TCP
            sprintf(out, "WAITING FOR %s", IP_addr);  
         }
         else
      #endif
      if(nortel) sprintf(out, "Waking up Nortel receiver on COM%d", com_port);
      else       sprintf(out, "NO COM%d SERIAL PORT DATA SEEN", com_port);
      vidstr(0,0, RED, out);
      refresh_page();

      set_single_sat(0x00);
      init_messages();     // send init messages and request various data
      if(SERIAL_DATA_AVAILABLE()) tsip_end(0);  // skip data till we see an end of message code
   }
   else {                  // user has disabled the serial port
      vidstr(0,0, YELLOW, "SERIAL PORT PROCESSING DISABLED");
      refresh_page();
   }
}

void get_log_file()
{
int i;

   if(read_log[0]) {       // user specified a log file to read in
      first_key = ' ';
      i = reload_log(read_log, 1);
      if(i == 0) {         // log file found
         plot_review(0L);  // enter plot review mode to start plot at first point
         #ifdef ADEV_STUFF
            force_adev_redraw();    // and make sure adev tables are showing
         #endif
         pause_data = user_pause_data^1;
      }
      first_key = 0;
      draw_plot(1);
   }
}


int qwe;

void do_gps()
{
int i;

   while(1) {    // the main processing loop...  repeat until exit requested
      if(!serve_gps()) {
         break;
      }
      if(f11_flag) {  //// !!!! debug
//       BEEP();
         f11_flag = 0;
      }

      if(script_file && (script_pause == 0)) {  // reading chars from script file
         system_idle = 0;
         while(script_file) {  // process a full line of commands
            i = process_script_file();
            if((i == 0) || (i == 0x0D)) break;
            update_pwm();
         }
      }
      else if(KBHIT()) {   // key pressed 
         system_idle = 0;
         i = get_kbd();    // get the keyboard character
         i = do_kbd(i);    // process the character
         if(i) break;      // it's time to stop this madness
      }
      else {
         #ifdef WINDOWS
            if(idle_sleep && (set_system_time == 0)) Sleep(idle_sleep);
         #endif
      }
   }
}


#ifdef PLM
   // This code was a failed attempt to monitor long term power line
   // frequency changes.  There does not appear to be a viable way to have a 
   // callback routine called when the modem signal chanes state.  So 
   // this works by polling the RI modem control line for changes.
   // It uses a high-frequency (1KHz) timer event to poll the signal
   // for changes.  Unfortunately Windoze cannot guarantee the required
   // latency.  
   HANDLE hTimer = NULL;
   HANDLE hTimerQueue = NULL;
   int    pl_arg = 1;
   DWORD  last_ring;

   VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired)
   {
   DWORD ring;
   DWORD dwModemStatus;

      if(!GetCommModemStatus(hSerial, &dwModemStatus)) {
         return;  // Error in GetCommModemStatus;
      }

      ring = MS_RING_ON & dwModemStatus;
      if(ring != last_ring) {
         last_ring = ring;
         ++pl_counter;
      }
   //++pl_counter;
   }

   void kill_pl_timer()
   {
       // Delete all timers in the timer queue.
       if(hTimerQueue) {
          DeleteTimerQueue(hTimerQueue);
          timeEndPeriod(1);
       }
   }

   void init_pl_timer()
   {
       if(monitor_pl <= 0) return;

       timeBeginPeriod(1);

       hTimerQueue = CreateTimerQueue();
       if(hTimerQueue == NULL) {
          monitor_pl = (-1);
          return;
       }

       // Set a timer to call the timer routine every 5 msecs starting in 1 second.
       if(!CreateTimerQueueTimer( &hTimer, hTimerQueue,
               (WAITORTIMERCALLBACK)TimerRoutine, &pl_arg , 1000, 1, WT_EXECUTEINTIMERTHREAD)) {
          kill_pl_timer();
          monitor_pl = (-2);
       }
       return;
   }
#endif //PLM


void clean_up()
{
   // leave receiver in a nice stand-alone state
   #ifdef PRECISE_STUFF
      abort_precise_survey();  // shut down any surveys in progress,  save best position
   #endif

   if(show_fixes) {
      set_rcvr_config(7);  // return to overdetermined clock mode
   }

   #ifdef PLM
     kill_pl_timer();
   #endif
}

void dump_stream()
{
int row, col;
unsigned i;

   again:
   erase_screen();
   vidstr(0,0, GREEN, "Stream dump:");
   refresh_page();
   init_messages();

   row = 2;
   col = 0;
   while(1) {
      SAL_serve_message_queue();
      Sleep(0);
      if(SERIAL_DATA_AVAILABLE()) {
         i = get_serial_char();
         sprintf(out, "%02X(%c) ", i,i);
         vidstr(row,col, WHITE, out);
         refresh_page();
         col += 6;
         if(col >= 32*6) {
            col = 0;
            ++row;
            if(row > 32) row = 2;
         }
      }
      SAL_serve_message_queue();
      Sleep(0);
      if(KBHIT()) {
         i = GETCH();
         if(i == 0x1B) {
            clean_up();
            shut_down(0);
            exit(666);
         }
         else goto again;
      }
   }
}


int main(int argc, char *argv[])
{
int i;
int error;

   set_defaults();     // initialize global variables

   #ifdef EMS_MEMORY
      #ifdef DOS_EXTENDER
         ems_ok = 0;
      #else
         ems_init();      // for DOS, see if we can use EMS memory
      #endif
   #endif

   read_config_file(argv[0], 1);    // process options from default config file
   read_config_file(argv[0], 2);    // process calendar file
   help_path = argv[0];

   for(i=1; i<argc; i++) {          // process the command line 
      if((argv[i][0] == '/') || (argv[i][0] == '-') || (argv[i][0] == '=') || (argv[i][0] == '$')) {
         error = option_switch(argv[i]);
      }
      else error = 1;

      if(error) {
         command_help("on command line: ", argv[i], argv[0]);
         exit(10);
      }
   }

   find_endian();     // determine machine byte ordering 
   alloc_memory();    // allocate memory for plot and adev queues, etc
   adjust_view();     // tweak screen for user selected view interval, etc


   init_hardware();   // initialize the com port, screen, and any other hardware

   if(take_a_dump) {
      dump_stream();
   }

   #ifdef PLM
      init_pl_timer();   // setup to monitor power line freq
   #endif
   config_options();  // configure system for command line options

   plot_axes();       // draw graph axes
   show_version_header();
   show_log_state();  // show logging state
   show_com_state();  // show com port state and flush buffer
   get_log_file();    // read in any initial log file

   do_gps();          // run the receiver until something says stop 

   clean_up();        // leave the receiver in a nice stand-alone state
   shut_down(0);      // clean up remaining details and exit
   return 0;
}


#ifdef WINDOWS       // startup code

//****************************************************************************
//
// Exit handlers must be present in every SAL application
//
// These routines handle exits under different conditions (exit() call, 
// user request via GUI, etc.)
//
//****************************************************************************

static int exit_handler_active = 0;

void WINAPI WinClean(void)
{
   if(exit_handler_active) {
      return;
   }

   exit_handler_active = 1;

   SAL_shutdown();
}

void WINAPI WinExit(void)
{
   if(!exit_handler_active) {
      WinClean();
   }
   exit(0);
}

void AppExit(void)
{
   if(!exit_handler_active) {
      WinClean();
   }
   return;
}

long FAR PASCAL WindowProc(HWND   hWnd,   UINT   message,   //)
                           WPARAM wParam, LPARAM lParam)
{
   switch (message)
      {
      case WM_KEYDOWN:
         {
         switch (wParam)
            {
            case VK_HOME:   add_kbd(HOME_CHAR);  break;
            case VK_UP:     add_kbd(UP_CHAR);    break;
            case VK_PRIOR:  add_kbd(PAGE_UP);    break;
            case VK_LEFT:   add_kbd(LEFT_CHAR);  break;
            case VK_RIGHT:  add_kbd(RIGHT_CHAR); break;
            case VK_END:    add_kbd(END_CHAR);   break;
            case VK_DOWN:   add_kbd(DOWN_CHAR);  break;
            case VK_NEXT:   add_kbd(PAGE_DOWN);  break;
            case VK_INSERT: add_kbd(INS_CHAR);   break;
            case VK_DELETE: add_kbd(DEL_CHAR);   break;
            case VK_CANCEL: break_flag = 1;      break;
            case VK_F1:
            case VK_F2:
            case VK_F3:
            case VK_F4:
            case VK_F5:
            case VK_F6:
            case VK_F7:
            case VK_F8:
            case VK_F9:
//          case VK_F10:
            case VK_F11:
               f11_flag = 1;
               break;
            case VK_F12:
               add_kbd(0);
               break;
            }
         break;
         }

      case WM_CHAR:
         {
         add_kbd(wParam);
         break;
         }

      case WM_CLOSE:
         {
         break_flag = 1;
         return 0;
         }

      case WM_TIMER:
         {
         timer_serve++;
         if ((timer_serve == 1) && (reading_log == 0))
            {
            serve_gps();
            }
         timer_serve--;
         break;
         }
      }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

//****************************************************************************
//
// Windows main() function
//
//****************************************************************************
int main(int argc, char *argv[]);

int PASCAL WinMain(HINSTANCE hInst, 
                   HINSTANCE hPrevInst,
                   LPSTR     lpCmdLine,     
                   int       nCmdShow)
{
#define MAX_ARGS 30
   //
   // Initialize system abstraction layer -- must succeed in order to continue
   //

   VFX_io_done = 1;
   hInstance = hInst;

   IPC = NULL;   // TCP

   SAL_set_preference(SAL_USE_PARAMON, NO);
                                           
   if (!SAL_startup(hInstance,
                    szAppName,
                    TRUE,
                    WinExit))
      {
      return 0;
      }

   //
   // Create application window
   // 

   SAL_set_preference(SAL_ALLOW_WINDOW_RESIZE, NO);
   SAL_set_preference(SAL_MAXIMIZE_TO_FULLSCREEN, YES);
   SAL_set_preference(SAL_USE_DDRAW_IN_WINDOW, NO);

   SAL_set_application_icon((C8 *) IDI_ICON);

   hWnd = SAL_create_main_window();

   if(hWnd == NULL) {
      SAL_shutdown();
      return 0;
   }

   initial_window_mode = VFX_WINDOW_MODE;

   //
   // Register window procedure
   // 

   SAL_register_WNDPROC(WindowProc);
   SAL_show_system_mouse();

   //
   // Register exit handler and validate command line
   //

   atexit(AppExit);

   //
   // Get current working directory and make sure it's not the Windows desktop
   // (We don't want to drop our temp files there)
   //
   // If it is, try to change the CWD to the current user's My Documents folder
   // Otherwise, leave the CWD alone to permit use of the "Start In" field on a
   // desktop shortcut
   //
   // (Also do this if the current working directory contains "Program Files")
   //

   C8 docs[MAX_PATH] = "";
   C8 desktop[MAX_PATH] = "";

   SHGetSpecialFolderPath(HWND_DESKTOP,
                          desktop,
                          CSIDL_DESKTOPDIRECTORY,
                          FALSE);

   SHGetSpecialFolderPath(HWND_DESKTOP,
                          docs,
                          CSIDL_PERSONAL,
                          FALSE);

   C8 CWD[MAX_PATH] = "";

   if(GetCurrentDirectory(sizeof(CWD), CWD)) {
       _strlwr(CWD);
       _strlwr(desktop);
    
      if((!_stricmp(CWD,desktop)) ||
            (strstr(CWD,"program files") != NULL)) {
         SetCurrentDirectory(docs);
         strcpy(CWD, docs);
      }
   }

   //
   // Pass Windows command line as DOS argv[] array
   //

   static char *argv[MAX_ARGS];

   S32 argc = 1;
   strcpy(root, CWD);
   root_len = strlen(root);
   strcat(root, "\\heather.exe");
   argv[0] = &root[0];
//  argv[0] = _strdup("heather.exe");

   C8 cli[MAX_PATH];
   strcpy(cli, lpCmdLine);

   C8 *src = cli;

   for(argc=1; (argc < MAX_ARGS) && (src != NULL); argc++) {
      if(*src == 0) break;
      while(*src == ' ') ++src; 

      C8 *next = NULL;
      C8 *term = strchr(src,' ');

      if(term != 0) {
         *term++ = 0;
         next = term;
      }

      argv[argc] = _strdup(src);
      src = next;
   }

   return main(argc, argv);
}

#endif  // WINDOWS


#ifdef WINDOWS
BOOL CALLBACK CLIHelpDlgProc (HWND   hDlg,  
                              UINT   message,
                              WPARAM wParam,
                              LPARAM lParam)
{
   switch (message)
      {
      case WM_INITDIALOG:
         {
         //
         // Center dialog on screen
         //

         SAL_FlipToGDISurface();

         S32 screen_w = GetSystemMetrics(SM_CXSCREEN); 
         S32 screen_h = GetSystemMetrics(SM_CYSCREEN); 

         LPNMHDR pnmh = (LPNMHDR) lParam;

         RECT r;

         GetWindowRect(hDlg, &r);

         r.right  -= r.left;
         r.bottom -= r.top;

         r.left = (screen_w - r.right)  / 2;
         r.top  = (screen_h - r.bottom) / 2;

         MoveWindow(hDlg, r.left, r.top, r.right, r.bottom, TRUE);

         //
         // Set caption for window
         //

         SetWindowText(hDlg, "Help for Command Line Options");

         //
         // Set initial control values
         //

         HWND hDT = GetDlgItem(hDlg, IDC_CMDHELP);

         SetWindowText(hDT, (C8 *) lParam);

         SendMessage(hDT, WM_SETFONT, (WPARAM) GetStockFont(ANSI_FIXED_FONT), (LPARAM) true);         

         return TRUE;
         }

      case WM_COMMAND:
         {
         S32 ID = LOWORD(wParam);

         switch (ID)
            {
            case IDOK:
               {
               EndDialog(hDlg, 1);
               if(downsized) go_fullscreen();
               return TRUE;
               }

            case IDREADME:
               {
               EndDialog(hDlg, 0);
               if(downsized) go_fullscreen();
               return TRUE;
               }
            }
         break;
         }
      }

   return FALSE;
}

//****************************************************************************
//
// Launch HTML page from program directory
//
//****************************************************************************

void WINAPI launch_page(C8 *filename)
{
   C8 path[MAX_PATH];

   GetModuleFileName(NULL, path, sizeof(path)-1);

   _strlwr(path);

   C8 *exe = strstr(path,"heather.exe");

   if(exe != NULL) {
      strcpy(exe, filename);
   }
   else {
      strcpy(path, filename);
   }

   ShellExecute(NULL,    // hwnd
               "open",   // verb
                path,    // filename
                NULL,    // parms
                NULL,    // dir
                SW_SHOWNORMAL);
}

#endif

#ifdef DOS_BASED
int help_line;

void show(char *s)
{
int old_text_mode;
int old_circle;
int old_plot_text_row;
int old_edit_row;
int old_zoom_lla;

   if(keyboard_cmd) {
      old_text_mode = text_mode;
      old_circle = full_circle;
      old_plot_text_row = PLOT_TEXT_ROW;
      old_edit_row = EDIT_ROW;
      old_zoom_lla = zoom_lla;
      if(old_text_mode) {
         PLOT_TEXT_ROW = 20;
         EDIT_ROW = 20+2;
      }
      text_mode = 2;
      full_circle = 0;
      zoom_lla = 0;

      if(help_line > (PLOT_TEXT_ROW-4)) {
         edit_error("");
         erase_screen();
         help_line = 0;
      }
      else if(help_line == 0) erase_screen();
      vidstr(help_line, 0, WHITE, s);

      ++help_line;
      text_mode = old_text_mode;
      full_circle = old_circle;
      zoom_lla = old_zoom_lla;
      PLOT_TEXT_ROW = old_plot_text_row;
      EDIT_ROW = old_edit_row;
   }
   else printf("%s\n", s);
}
#endif // DOS_BASED

void command_help(char *where, char *s, char *cfg_path)
{
#ifdef DOS_BASED
unsigned long x;

    help_line = 0;
    show("Lady Heather's Disciplined Oscillator Control Program");
    sprintf(out, "Trimble Thunderbolt Version: %s - %s %s", VERSION, date_string, __TIME__);
    show(out);
    show("");
    show("Copyright (C) 2009 Mark S. Sims - all rights reserved.");
    show("Permission for free non-commercial use and redistribution is granted.");
    show("Original Windows port and TCP/IP support by John Miles.");
    show("Adev code from Tom Van Baak's ADEV3.C (modified by John Miles for incremental adevs)");
    show("Temperature and oscillator control algorithms by Warren Sarkinson.");
    show("");
    sprintf(out, "Valid startup command line options are:", where, s);
    show(out);
    show("   /0      - disable com port processing (if just reading a log file)");
    show("   /2      - use COM2");
    show("   /a[=#]  - number of points to calc Adevs over (default=330000)");
    show("             If 0,  then all adev calculations are disabled.");
    show("   /b[=#]  - set daylight savings time area (1=USA,2=EURO,3=AUST,4=NZ)");
    show("   /b=nth,start_day,month,nth,end_day,month,hour - set custom DST rule");
    show("             day:0=Sun..6=Sat  month:1=Jan..12=Dec");
    show("             nth>0 = from start of month  nth<0 = from end of month");
    show("   /c[=#]  - set Cable delay to # ns (default=77.03 ns (50 ft std coax))");
    show("   /c=#f     set Cable delay in feet of 0.66Vp coax");
    show("   /c=#m     set Cable delay in meters of 0.66Vp coax");
#ifdef GREET_STUFF
    show("   /d#     - show dates in calendar #)\n" );
    show("                      A)fghan   haaB)     C)hinese  D)ruid   H)ebrew\n");
    show("                      I)slamic  J)ulian   K)urdish  M)jd    iN)dian\n");
    show("                      P)ersian  iS)o      T)zolkin  X)iuhpohualli  \n");
    show("                      maY)an    aZ)tec Tonalpohualli\n");
#endif
    show("   /e      - do not log message/time Errors and state changes");
    show("   /f      - do not use EMS memory for the plot queue");
    show("   /f[psak]- toggle Pv,Static,Altitude,Kalman filter");
    show("   /g[#]   - toggle Graph enable (#= a,b,c,d,e,h,l,m,o,p,s,t,u,x,z)");
    show("             (Adevs  Both map and adev tables  sat_Count  Dac  Errors");
    show("              Holdover  K(constallation changes)  L(hide Location)");
    show("              Map,no adev tables   Osc  Pps  R(RMS info)   Sound  Temperature");
    show("              Update/scroll plot continuously  Watch  X(dops)  Z(clock)");
    show("              j(el mask)  n(disable holiday greetings)");
    show("   /h=file - read command line options from .CFG config file");
    show("   /i[=#]  - set plot Interval to # seconds (default=24 hour mode)");
    show("   /j[=#]  - set ADEV sample period to # seconds (default=10 seconds)");
    show("   /k      - disable Keyboard commands");
    show("   /kb     - disable Beeps");
    show("   /kc     - disable all writes to Config EEPROM");
    show("   /km     - disable mouse");
    show("   /k[?]   - set temp control parameter '?'");
    show("   /l[=#]  - write Log file every # seconds (default=1)");
    show("   /lh     - don't write timestamp headers in the log file");
    show("   /m[=#]  - Multiply all plot scale factors by # (default is to double)");
    show("   /ma     - toggle Auto scaling");
    show("   /md[=#] - set DAC plot scale factor (microvolts/divison)");
    show("   /mi     - invert pps and temperature plots");
    show("   /mo[=#] - set OSC plot scale factor (parts per trillion/divison)");
    show("   /mp[=#] - set PPS plot scale factor (nanoseconds/divison)");
    show("   /mt[=#] - set TEMPERATURE plot scale factor (millidegrees/divison)");
    show("   /n=hh:mm:ss  - exit program at specified time (optional: /n=month/day/year)");
    show("   /na=hh:mm:ss - sound alarm at specified time (optional: /n=month/day/year)");
    show("   /na=#?       - sound alarm every #s secs,  #m mins,  #h hours  #d=days");
    show("   /na=#?o      - sound alarm Once in #so secs,  #mo mins,  #ho hours  #d=days");
    show("   /nd=hh:mm:ss - dump screen at specified time (optional: /n=month/day/year)");
    show("   /nd=#?       - dump screen every #s secs,  #m mins,  #h hours  #d=days");
    show("   /nd=#?o      - dump screen Once in #so secs,  #mo mins,  #ho hours  #d=days");
    show("   /nl=hh:mm:ss - dump log at specified time (optional: /n=month/day/year)");
    show("   /nl=#?       - dump log every #s secs,  #m mins,  #h hours  #d=days");
    show("   /nl=#?o      - dump log Once in #so secs,  #mo mins,  #ho hours  #d=days");
    show("   /nt          - attempt to wake up Nortel NTGxxxx receivers");
    show("   /nx=hh:mm:ss - exit program at specified time (optional: /n=month/day/year)");
    show("   /nx=#?o      - exit program in #s secs,  #m mins,  #h hours  #d=days");
#ifdef ADEV_STUFF
    show("   /o[#]  - select ADEV type (#=A,H,M,T, O,P)");
    show("            Adev  Hdev  Mdev  Tdev  O=all osc types  P=all pps types");
#endif
    show("   /p     - disable PPS output signal");
    x = plot_q_size;
    if(max_ems_size > x) x = max_ems_size;
    sprintf(out, "   /q[=#]    - set size of plot Queue (default=%ld (%ld hours))", x, x/3600L);
    show(out);
    show("   /qf[=#]   - set max size of FFT (default=4096)");
    show("   /r[=file] - Read file (default=TBOLT.LOG)");
    show("               .LOG=log   .SCR=script   .LLA=lat/lon/altitude");
    show("               .ADV=adev  .TIM=ti.exe time file");
    show("   /rt       - use Resolution-T serial port config (9600,8,O,1)");
    show("   /ss[=#]   - do Self Survey (# fixes,  default=2000)");
    show("   /sp[=#]   - do Precison Survey (# hours,  default/max=48)");
    show("   /sf       - enter 2D/3D fix mode and map fixes");
    show("   /t=#SSSSS/DDDDD  - show time at local time zone - #=gmt offset");
    show("               SSSSS=standard time zone id   DDDDD=daylight time zone id");
    show("               (note:  western hemisphere # is negative:  /T=-6CST/CDT)");
    show("   /ta       - show dates in European dd.mm.yy format.");
    show("   /tb=string- set analog clock brand name.");
    show("   /tc       - show Celcius temperatures");
    show("   /td       - show DeLisle temperatures");
    show("   /te       - show Reaumur temperatures");
    show("   /tf       - show Fahrenheit temperatures");
    show("   /tg       - sync outputs to GPS Time (default is UTC time)");
    show("   /th[=#]   - chime clock mode.  Chimes # times per hour.");
    show("               Tries to play \\WINDOWS\\MEDIA\\HEATHER_CHIME.WAV,  else uses alarm sound.");
    show("   /th[=#H]  - cuckoo clock mode.  Sings .WAV files # times per hour.");
    show("               Tries to play \\WINDOWS\\MEDIA\\HEATHER_CHIME.WAV,  else uses alarm sound.");
    show("               Chimes the hour number on the hour,  one chime at other times.");
    show("   /th[=#S]  - singing chime clock mode.  Sings .WAV files # times per hour.");
    show("               Tries to play \\WINDOWS\\MEDIA\\HEATHER_SONGxx.WAV (xx=minute)");
    show("   /tj       - don't remove effects of tbolt firmware temperature smoothing");
    show("   /tk       - show Kelvin temperatures");
    show("   /tm       - show altitude in meters");
    show("   /tn       - show Newton temperatures");
    show("   /to       - show Romer temperatures");
    show("   /tp       - show time as fraction of a day");
    show("   /tq       - show time as total seconds of the day");
    show("   /ts[odhm] - set operating system time (once,daily,hourly,every minute)");
    show("   /tsa[=#]  - set operating system time anytime difference exceeds # msecs");
    show("   /tsx[=#]  - compensate for delay between 1PPS output and receiver timing message.");
    show("               Value is in milliseconds.  Default is a +45 millisecond delay");
    show("   /tt=#     - set active temp control setpoint to # degrees C");
    show("   /tu       - sync outputs to UTC time (default is UTC time)");
    show("   /tx       - show osc values with eXponent (default is ppb/ppt)");
    show("   /t'       - show altitude in feet");
    show("   /t\"       - use degrees.minutes.seconds for lat/lon");
    show("   /u        - toggle plot/adev queue Updates");
    show("   /vu[=#]   - Undersized (640x480) Video screen (mode #256)");
    show("   /vs[=#]   - Small (800x600) Video screen (mode# 258)");
    show("   /vn[=#]   - Netbook (1000x540) Video screen (mode# ???)");
    show("   /vm[=#]   - Medium (1024x768) Video screen (default) (mode# 260)");
    show("   /vl[=#]   - Large (1280x1024) Video screen (mode# 262)");
    show("   /vv[=#]   - Very large (1440x900) Video screen (mode# 262)");
    show("   /vx[=#]   - eXtra large (1680x1050) Video screen (mode# 304)");
    show("   /vh[=#]   - Huge (1920x1080) Video screen (mode# 304)");
    show("   /vt[=#]   - Text only Video screen (mode# 2)");
    show("   /vc=rowsXcols - custom screen size (e.g. /vc=1200x800)");
    show("   /wa=file  - set log file name to Append to (default=TBOLT.LOG)");
    show("   /w=file   - set log file name to Write (default=TBOLT.LOG)");
    show("   /x[=#]    - set plot display data filter count (default=10)");
    show("   /y        - optimize plot grid for 24 hour display (/y /y = 12hr)");
    show("   /y=#      - set plot view time to # minutes/division");
    show("   /z[#][=val] - toggle or set graph zero line ref value (#=d,o,p,t)");
    show("                 (D)ac volts  (O)sc   (P)ps ns  (T)temp deg");
    show("   /+ - sync PPS signal rising edge to time");
    show("   /- - sync PPS signal falling edge to time");
    show("   /^ - sync OSC signal falling edge to time");
    if (strchr(s,'?') == NULL)
      {
      sprintf(out, "Invalid option seen %s%s", where, s);
      show(out);
      }

    if(cfg_path) {
       show("");
       sprintf(out, "Put .CFG file in directory with %s", cfg_path);
       show(out);
    }
    if(keyboard_cmd) {
       BEEP();
       help_line = 999;
       show("");
       redraw_screen();
    }
#endif

#ifdef WINDOWS
    downsized = 0;
    if(sal_ok) {  // SAL package has been initialized
       if(SAL_window_status() == SAL_FULLSCREEN) {
          // (it's extremely painful to make GDI dialogs work properly in DirectDraw fullscreen mode, so we don't try)
          go_windowed();  // try to go windowed
          if(SAL_window_status() == SAL_FULLSCREEN) {
             downsized = 0;
             erase_plot(1);
             edit_error("Sorry, startup command line help dialog is not available in fullscreen mode");
             return;
          }
       }
    }

    static char help_msg[32768] = {
         "Lady Heather's Disciplined Oscillator Control Program\r\n"
         "Trimble Thunderbolt Version "VERSION" - "__DATE__" "__TIME__"\r\n"
         "\r\n"
         "Copyright (C) 2009 Mark S. Sims - all rights reserved.\r\n"
         "Permission for free non-commercial use and redistribution is granted.\r\n"
         "Original Windows port and TCP/IP support by John Miles.\r\n"
         "Adev code from Tom Van Baak's ADEV3.C (modified by John Miles for incremental adevs)\r\n"
         "Temperature and oscillator control algorithms by Warren Sarkinson.\r\n"
         "\r\n"
         "Startup command line options should be placed on the TARGET line in the\r\n"
         "Lady Heather program PROPERTIES or in the HEATHER.CFG file.  Most can also\r\n"
         "be executed with the keyboard / command.  Valid startup command options are:\r\n"
         "   /#               - use COM# instead of COM1 default (#=1..99)\r\n"
         "   /0               - disable com port processing (if just reading a log file)\r\n"
         "   /ip=addr[:port#] - connect to TSIP server instead of local COM port\r\n"
         "   /a[=#]           - number of points to calc Adevs over (default=330000)\r\n"
         "                      If 0,  then all adev calculations are disabled.\r\n"
         "   /b[=#]           - set daylight savings time area (1=USA,2=EURO,3=AUST,4=NZ)\r\n"
         "   /b=nth,start_day,month,nth,end_day,month,hour - set custom DST rule\r\n"
         "                      day:0=Sun..6=Sat  month:1=Jan..12=Dec\r\n"
         "                      nth>0 = from start of month  nth<0 = from end of month\r\n"
         "   /c[=#]           - set Cable delay to # ns (default=77.03 ns (50 ft std coax))\r\n"
         "   /c=#f              set Cable delay in feet of 0.66Vp coax\r\n"
         "   /c=#m              set Cable delay in meters of 0.66Vp coax\r\n"
#ifdef GREET_STUFF
         "   /d#              - show dates in calendar #)\r\n"
         "                      A)fghan   haaB)     C)hinese  D)ruid   H)ebrew\r\n"
         "                      I)slamic  J)ulian   K)urdish  M)jd    iN)dian\r\n"
         "                      P)ersian  iS)o      T)zolkin  X)iuhpohualli  \r\n"
         "                      maY)an    aZ)tec Tonalpohualli\r\n"
#endif
         "   /e               - do not log message/time Errors and state changes\r\n"
         "   /f               - start in Fullscreen mode\r\n"
         "   /f[psak]         - toggle Pv,Static,Altitude,Kalman filter\r\n"
         "   /g[#]            - toggle Graph enable (#= a,b,c,d,e,h,l,m,o,p,s,t,u,x,z)\r\n"
         "                      (Adevs  Both map and adev tables  sat_Count  Dac  Errors\r\n"
         "                       Holdover  K(constallation changes)  L(hide Location)\r\n"
         "                       Map,no adev tables   Osc  Pps  R(RMS info)   Sound  Temperature\r\n"
         "                       Update/scroll plot continuously  Watch  X(dops)  Z(clock)\r\n"
         "                       j(el mask)  n(disable holiday greetings)\r\n"
         "   /h=file          - read command line options from .CFG config file\r\n"
         "   /i[=#]           - set plot Interval to # seconds (default=24 hour mode)\r\n"
         "   /j[=#]           - set ADEV sample period to # seconds (default=10 seconds)\r\n"
         "   /k               - disable Keyboard commands\r\n"
         "   /kb              - disable Beeps\r\n"
         "   /kc              - disable all writes to Config EEPROM\r\n"
         "   /km              - disable mouse\r\n"
         "   /kt              - disable Windows dialog/message timer\r\n"
         "   /k[?=#]          - set temp control parameter '?'\r\n"
         "   /l[=#]           - write Log file every # seconds (default=1)\r\n"
         "   /lh              - don't write timestamp headers in the log file\r\n"
         "   /m[=#]           - Multiply all plot scale factors by # (default is to double)\r\n"
         "   /ma              - toggle Auto scaling\r\n"
         "   /md[=#]          - set DAC plot scale factor (microvolts/divison)\r\n"
         "   /mi              - invert pps and temperature plots\r\n"
         "   /mo[=#]          - set OSC plot scale factor (parts per trillion/divison)\r\n"
         "   /mp[=#]          - set PPS plot scale factor (nanoseconds/divison)\r\n"
         "   /mt[=#]          - set TEMPERATURE plot scale factor (millidegrees/divison)\r\n"
         "   /n=hh:mm:ss      - exit program at specified time (optional: /n=month/day/year)\r\n"
         "   /na=hh:mm:ss     - sound alarm at specified time (optional: /n=month/day/year)\r\n"
         "   /na=#?           - sound alarm every #s secs,  #m mins,  #h hours  #d=days\r\n"
         "   /na=#?o          - sound alarm Once in #so secs,  #mo mins,  #ho hours  #d=days\r\n"
         "   /nd=hh:mm:ss     - dump screen at specified time (optional: /n=month/day/year)\r\n"
         "   /nd=#?           - dump screen every #s secs,  #m mins,  #h hours  #d=days\r\n"
         "   /nd=#?o          - dump screen Once in #so secs,  #mo mins,  #ho hours  #d=days\r\n"
         "   /nl=hh:mm:ss     - dump log at specified time (optional: /n=month/day/year)\r\n"
         "   /nl=#?           - dump log every #s secs,  #m mins,  #h hours  #d=days\r\n"
         "   /nl=#?o          - dump log Once in #so secs,  #mo mins,  #ho hours  #d=days\r\n"
         "   /nt              - attempt to wake up Nortel NTGxxxx receivers\r\n"
         "   /nx=hh:mm:ss     - exit program at specified time (optional: /n=month/day/year)\r\n"
         "   /nx=#?o          - exit program in #s secs,  #m mins,  #h hours  #d=days\r\n"
#ifdef ADEV_STUFF            
         "   /o[#]            - select ADEV type (#=A,H,M,T, O,P)\r\n"
         "                      Adev  Hdev  Mdev  Tdev  O=all osc types  P=all pps types\r\n"
#endif                       
         "   /p               - disable PPS output signal\r\n"
         "   /q[=#]           - set size of plot Queue in seconds (default=30 days)\r\n"
         "   /qf[=#]          - set max size of FFT (default=4096)\r\n"
         "   /r[=file]        - Read file (default=TBOLT.LOG)\r\n"
         "                      .LOG=log   .SCR=script   .LLA=lat/lon/altitude\r\n"
         "                      .ADV=adev  .TIM=ti.exe time file\r\n"
         "   /rt              - use Resolution-T serial port config (9600,8,O,1)\r\n"
         "   /ss[=#]          - do Self Survey (# fixes,  default=2000)\r\n"
         "   /sp[=#]          - do Precison Survey (# hours,  default/max=48)\r\n"
         "   /sf              - enter 2D/3D fix mode and map fixes\r\n"
         "   /t=#SSSSS/DDDDD  - show time at local time zone - #=gmt offset\r\n"
         "                      SSSSS=standard time zone id   DDDDD=daylight time zone id\r\n"
         "                      (note:  western hemisphere # is negative:  /T=-6CST/CDT)\r\n"
         "   /ta              - show dates in European dd.mm.yy format.\r\n"
         "   /tb=string       - set analog clock brand name.\r\n"
         "   /tc              - show Celcius temperatures\r\n"
         "   /td              - show DeLisle temperatures\r\n"
         "   /te              - show Reaumur temperatures\r\n"
         "   /tf              - show Fahrenheit temperatures\r\n"
         "   /tg              - sync outputs to GPS Time (default is UTC time)\r\n"
         "   /tj              - don't remove effects of tbolt firmware temperature smoothing\r\n"
         "   /th[=#]          - chime clock mode.  Chimes # times per hour.\r\n"
         "                      Tries to play \\WINDOWS\\MEDIA\\HEATHER_CHIME.WAV,  else uses alarm sound.\r\n"
         "   /th[=#H]         - cuckoo clock mode.  Sings .WAV files # times per hour.\r\n"
         "                      Tries to play \\WINDOWS\\MEDIA\\HEATHER_CHIME.WAV,  else uses alarm sound.\r\n"
         "                      Chimes the hour number on the hour,  one chime at other times.\r\n"
         "   /th[=#S]         - singing chime clock mode.  Sings .WAV files # times per hour.\r\n"
         "                      Tries to play \\WINDOWS\\MEDIA\\HEATHER_SONGxx.WAV (xx=minute)\r\n"
         "   /tk              - show Kelvin temperatures\r\n"
         "   /tm              - show altitude in meters\r\n"
         "   /tn              - show Newton temperatures\r\n"
         "   /to              - show Romer temperatures\r\n"
         "   /tp              - show time as fraction of a day\r\n"
         "   /tq              - show time as total seconds of the day\r\n"
         "   /ts[odhm]        - set operating system time to UTC (once,daily,hourly,every minute)\r\n"
         "   /tsa[=#]         - set operating system time to UTC anytime difference exceeds # msecs\r\n"
         "   /tsx[=#]         - compensate for delay between 1PPS output and receiver timing message\r\n"
         "                      Value is in milliseconds.  Default is a +45 millisecond delay\r\n"
         "   /tt=#            - set active temp control setpoint to # degrees C\r\n"
         "   /tu              - sync outputs to UTC Time (default is UTC time)\r\n"
         "   /tw[=#]          - Sleep() for this many milliseconds when idle (default=10)\r\n"
         "   /tx              - show osc values with eXponent (default is ppb/ppt)\r\n"
         "   /t'              - show altitude in feet\r\n"
         "   /t\"              - use degrees.minutes.seconds for lat/lon\r\n"
         "   /u               - toggle plot/adev queue Updates\r\n"
         "   /vt              - Text only Video screen\r\n"
         "   /vu              - Undersized (640x480) Video screen\r\n"
         "   /vs              - Small (800x600) Video screen\r\n"
         "   /vn              - Netbook (1000x540) Video screen\r\n"
         "   /vm              - Medium (1024x768) Video screen (default)\r\n"
         "   /vl              - Large (1280x1024) Video screen\r\n"
         "   /vv              - Very large (1440x900) Video screen\r\n"
         "   /vx              - eXtra large (1680x1050) Video screen\r\n"
         "   /vh              - Huge (1920x1080) Video screen\r\n"
         "   /vc=rowsXcols    - custom screen size (e.g. /vc=1200x800)\r\n"
         "   /wa=file         - set log file name to Append to (default=TBOLT.LOG)\r\n"
         "   /w=file          - set log file name to Write (default=TBOLT.LOG)\r\n"
         "   /x[=#]           - set plot display data filter count (default=10)\r\n"
         "   /y               - optimize plot grid for 24 hour display (/y /y = 12hr)\r\n"
         "   /y=#             - set plot view time to # minutes/division\r\n"
         "   /z[#][=val]      - toggle or set graph zero line ref value (#=d,o,p,t)\r\n"
         "                      (D)ac volts  (O)sc   (P)ps ns  (T)temp deg\r\n"
         "   /+               - sync PPS signal rising edge to time\r\n"
         "   /-               - sync PPS signal falling edge to time\r\n"
         "   /^               - sync OSC signal falling edge to time\r\n"
    };

    if(path_help == 0) {
      if (strchr(s,'?') == NULL)
         {
         strcat(help_msg, "\r\nInvalid option seen ");
         strcat(help_msg, where);
         strcat(help_msg, s);
         }

       if(cfg_path) {
          strcat(help_msg, "\r\nPut .CFG file, if any, in directory ");
          cfg_path[root_len] = 0;
          strcat(help_msg, cfg_path);
          strcat(help_msg, "\r\n");
       }
       path_help = 1;
    }

    if(!DialogBoxParam(hInstance,
                        MAKEINTRESOURCE(IDD_CMDHELP),
                        hWnd,
                        CLIHelpDlgProc,
                        (LPARAM) help_msg)) {
       launch_page("readme.htm");
    } 

#endif
}


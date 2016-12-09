// *****************************************************************************
//
// TIMEUTIL: Time/date-related utility classes
//
// john@miles.io 18-Mar-10
//
// *****************************************************************************

#pragma once
#include <time.h>

//
// Configuration
//

#ifndef TIMEUTIL_MAX_LOG_ENTRY_STRING
   #define TIMEUTIL_MAX_LOG_ENTRY_STRING 8192
#endif

//
// Globals
//

const S64 SECONDS_PER_MINUTE  =  60LL;
const S64 SECONDS_PER_HOUR    =  SECONDS_PER_MINUTE * 60LL;
const S64 SECONDS_PER_DAY     =  SECONDS_PER_HOUR   * 24LL;
const S64 SECONDS_PER_YEAR    = (SECONDS_PER_DAY    * 36524218967LL) / 100000000LL;

const S64 SYS_TIME_1970 = 11644473600LL;    // seconds between Jan 1, 1601 and Jan 1, 1970, for NT/Unix time conversion
const S64 MJD_TIME_1970 = 40587LL;          // days between Nov 17, 1858 and Jan 1, 1970, for MJD/Unix time conversion

#ifndef TIMEUTIL_THREAD_SAFE                // lock-protect us() and other calls unless explicitly requested not to
#define TIMEUTIL_THREAD_SAFE 1
#endif

void ___________________________________________________________USTIMER____________________________________________________(void) 
{}

class USTIMER
{
   BOOL   QPC_OK;
   DOUBLE q_multiplier;     
   S64    relative_time;   

   S64 q_first;    
   S32 t_first;
   S32 m_first;

   S64 q_last;    
   S32 t_last;
   S64 m_last;

   S64       last_result;
   DWORD_PTR thread_mask;
   
#if TIMEUTIL_THREAD_SAFE
   CRITICAL_SECTION time_lock;
#endif

   C8   logfile_name[MAX_PATH];
   BOOL log_previous_newline;
   C8   log_output_string[TIMEUTIL_MAX_LOG_ENTRY_STRING];

   S64 smallest_disagreement;
   S64 largest_disagreement;
   S32 num_disagreements;
   S64 largest_retrograde;
   S32 num_retrogrades;

   virtual void reset_timebase(void)
      {
      QPC_OK       = FALSE;
      q_multiplier = 0.0;
      q_first      = 0;
      m_first      = timeGetTime();

      S64 temp = 0;

      if (QueryPerformanceFrequency((LARGE_INTEGER *) &temp))
         {
         QPC_OK       = TRUE;
         q_multiplier = 1E6 / ((DOUBLE) temp);

         QueryPerformanceCounter((LARGE_INTEGER *) &q_first);
         }

      q_last = q_first;
      m_last = m_first;
      }

public:

   void lock(void)
      {
#if TIMEUTIL_THREAD_SAFE
      EnterCriticalSection(&time_lock);
#endif
      }

   void unlock(void)
      {
#if TIMEUTIL_THREAD_SAFE
      LeaveCriticalSection(&time_lock);
#endif
      }

   USTIMER(BOOL force_single_core = FALSE)
      {
#if TIMEUTIL_THREAD_SAFE
      InitializeCriticalSection(&time_lock);
#endif

      thread_mask = 0;

      if (force_single_core)
         {
         thread_mask = SetThreadAffinityMask(GetCurrentThread(), 1);
         }

      reset_timebase();

      t_first = t_last = GetTickCount();       

      relative_time = 0;
      last_result   = 0;

      memset(logfile_name, 0, sizeof(logfile_name));
      memset(log_output_string, 0, sizeof(log_output_string));
      log_previous_newline = TRUE;

      smallest_disagreement = 0;
      largest_disagreement  = 0;
      num_disagreements     = 0;
      largest_retrograde    = 0;
      num_retrogrades       = 0;
      }

   virtual ~USTIMER()
      {
      if (thread_mask)
         {
         SetThreadAffinityMask(GetCurrentThread(), thread_mask);
         }

      if ((num_disagreements > 0) || (num_retrogrades > 0))
         {
         printf("TIMEUTIL: %d disagreements (%I64d to %I64d), %d retrograde jumps, largest=%I64d\n",
            num_disagreements, 
            smallest_disagreement,
            largest_disagreement,
            num_retrogrades,
            largest_retrograde);
         }

#if TIMEUTIL_THREAD_SAFE
      DeleteCriticalSection(&time_lock);
#endif
      }

   //
   // High-resolution timer, similar to timeGetTime() but returns microsecond count
   // rather than milliseconds
   //
   // May be called safely from multiple threads
   //
   // Monotonic, but can potentially return zero delta times in successive calls
   // if the high-res timer needs to be corrected.  Clock rate can vary with 
   // power-management settings and PCI bus load (KB274323); the routine will do 
   // its best to compensate
   //
   // Timebase begins at zero at construction time, and (if QPF is supported with 
   // typical values of QPF=0x000000009000000) rolls over past 2^63 in approx. 290K years.
   // Consequently it's OK in most applications to use comparisons rather than 
   // subtraction when performing relative-time calculations
   //

   virtual S64 us(void)
      {
      lock();

      //
      // Get # of fine-grained ticks since last call
      //

      S64 hi_res_delta;
      S64 hi_res_result;

      if (QPC_OK)
         {
         //
         // Derive microsecond timer from QueryPerformanceCount() result
         //
         // q_first can be anything, so we can't maintain good precision
         // without resorting to doubles.  E.g., (q_time-q_first)*1000000 
         // wrapped to negative values in about an hour in one trial
         //
      
         S64 q_time;
         QueryPerformanceCounter((LARGE_INTEGER *) &q_time);

         hi_res_result = (S64) (((DOUBLE) (q_time - q_first)) * q_multiplier);
         hi_res_delta  = (S64) (((DOUBLE) (q_time - q_last )) * q_multiplier);
         q_last = q_time;
         }
      else
         {
         //
         // Platform doesn't support high-res counter; return millisecond-precise count 
         // instead (which will roll over past 2^31 in 24 days)
         //
         // (This path is not used on any known system)
         //
   
         S32 m_time = timeGetTime();

         hi_res_result = ((S64) (m_time - m_first)) * 1000LL;
         hi_res_delta  = ((S64) (m_time - m_last))  * 1000LL;
         m_last = m_time;
         }

      //
      // Get # of coarse-grained ticks since last call
      //
      // Note that this will cause rollover problems if successive calls to us() occur
      // more than 24 days apart!
      //
      
      S32 t_time = GetTickCount();

      S64 lo_res_result = ((S64) (t_time - t_first)) * 1000LL;
      S64 lo_res_delta  = ((S64) (t_time - t_last))  * 1000LL;
      t_last = t_time;

      //
      // Rebase the high-resolution timer at the current low-resolution tick count 
      // if they disagree by more than 200 ms
      //
      
      S64 d = lo_res_delta - hi_res_delta;    
      
      if ((d < -200000LL) || (d > 200000LL))
         {
         if (d < smallest_disagreement) smallest_disagreement = d;
         if (d > largest_disagreement)  largest_disagreement  = d;
         num_disagreements++;

         relative_time = lo_res_result;
         hi_res_result = 0;
         reset_timebase();
         }
      
      S64 result = hi_res_result + relative_time;

      //
      // Disallow any retrograde jumps
      //
      // Large jumps should be corrected above; smaller ones will
      // result in the timer stalling briefly
      //
      
      d = result - last_result;

      if (d < 0LL)
         {
         if (d < largest_retrograde) largest_retrograde = d;
         ++num_retrogrades;

         result = last_result;
         }
      else
         {
         last_result = result;
         }

      unlock();
      return result;
      }                             

   //
   // UTC-based file-modification time
   //
   // Returns # of 1-us intervals since 1-Jan-1601 UTC, or 
   // -1 on failure
   //

   virtual S64 file_time_us (C8 *filename, S64 *creation_time = NULL)
      {
      union TU
         {
         FILETIME ftime;
         S64      itime;
         };

      TU T,C;
   
      T.itime = 0;
      C.itime = 0;
   
      HANDLE infile = CreateFile(filename,
                                 GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_FLAG_SEQUENTIAL_SCAN,
                                 NULL);
   
      if (infile == INVALID_HANDLE_VALUE)
         {
         return -1;
         }
   
      if (!GetFileTime(infile, 
                      &C.ftime,
                       NULL, 
                      &T.ftime))
         {
         CloseHandle(infile);
         return -1;
         }
   
      CloseHandle(infile);

      if (creation_time != NULL) 
         {
         *creation_time = C.itime / 10;
         }

      return T.itime / 10;
      }

   //
   // UTC-based calendar time
   //
   // Returns # of 1-us intervals since 1-Jan-1601 UTC
   // 
   // This time is not guaranteed to increase monotonically, due to
   // DST or manual clock changes
   //

   virtual S64 system_time_us (void)
      {
      union
         {
         FILETIME ftime;
         S64      itime;
         }
      T;

      T.itime = 0;

      GetSystemTimeAsFileTime(&T.ftime);

      return T.itime / 10;
      }

   //
   // Convert a file- or system-based time value in microseconds to
   // a locale-specific time string
   //

   virtual C8 *time_text (S64 time_us, 
                          C8 *text, 
                          S32 text_array_size)
      {
      FILETIME   ftime;
      FILETIME   lftime;
      SYSTEMTIME stime;

      assert(text != NULL);
   
      S64 file_time = time_us * 10;

      ftime.dwLowDateTime = S32(file_time & 0xffffffff);
      ftime.dwHighDateTime = S32(U64(file_time) >> 32);

      FileTimeToLocalFileTime(&ftime,  &lftime);
      FileTimeToSystemTime   (&lftime, &stime);

      memset(text, 0, text_array_size);

      GetTimeFormat(LOCALE_SYSTEM_DEFAULT,
                    0,
                   &stime,
                    NULL,
                    text,
                    text_array_size);
      return text;
      }

   //
   // Convert a file- or system-based time value in microseconds to
   // a locale-specific date string
   //

   virtual C8 *date_text (S64 time_us, 
                          C8 *text, 
                          S32 text_array_size)
      {
      FILETIME   ftime;
      FILETIME   lftime;
      SYSTEMTIME stime;

      assert(text != NULL);

      S64 file_time = time_us * 10;

      ftime.dwLowDateTime = S32(file_time & 0xffffffff);
      ftime.dwHighDateTime = S32(U64(file_time) >> 32);

      FileTimeToLocalFileTime(&ftime,  &lftime);
      FileTimeToSystemTime   (&lftime, &stime);

      memset(text, 0, text_array_size);

      GetDateFormat(LOCALE_SYSTEM_DEFAULT,
                    0,
                   &stime,
                    NULL,
                    text,
                    text_array_size);
      return text;
      }

   //                
   // Text date/timestamp
   //                

   virtual C8 *timestamp(C8 *text,
                         S32 text_array_size,
                         S64 at_time_us = 0)
      {
      assert(text != NULL);

      if (!at_time_us)
         {
         at_time_us = system_time_us();
         }

      memset(text, 0, text_array_size);

      C8 d[1024];
      C8 t[1024];

      _snprintf(text,
                text_array_size-1,
                "%s %s",
                date_text(at_time_us, d, sizeof(d)),
                time_text(at_time_us, t, sizeof(t)));

      return text;
      }

   //
   // Duration string (Yy,Dd,Hh,Mm,Ss)
   //

   virtual C8 *duration_string(S64  us, 
                               C8  *text,
                               S32  text_array_size,
                               bool show_msec = FALSE)
      {
      assert(text != NULL);

      memset(text, 0, text_array_size);
      S32 n = text_array_size - 1;

      C8 *dest = text;

      if (us < 0)
         {
         us = -us;
         strcpy(text,"-");
         dest += strlen(text);
         }

      S64 ms   = (us + 500LL)    / 1000LL;
      S64 secs = (us + 500000LL) / 1000000LL;

      S64 years = secs / SECONDS_PER_YEAR;
      secs %= SECONDS_PER_YEAR;

      S64 days =  secs / SECONDS_PER_DAY;
      secs %= SECONDS_PER_DAY;

      S64 hours = secs / SECONDS_PER_HOUR;
      secs %= SECONDS_PER_HOUR;

      S64 mins =  secs / SECONDS_PER_MINUTE;
      secs %= SECONDS_PER_MINUTE;

      if (years > 0)
         {
         _snprintf(dest, n, "%I64dy %I64dd %I64dh %I64dm %I64ds", years, days, hours, mins, secs);
         }
      else if (days > 0)
         {
         _snprintf(dest, n, "%I64dd %I64dh %I64dm %I64ds", days, hours, mins, secs);
         }
      else if (hours > 0)
         {
         _snprintf(dest, n, "%I64dh %I64dm %I64ds", hours, mins, secs);
         }
      else if (mins > 0)
         {
         _snprintf(dest, n, "%I64dm %I64ds", mins, secs);
         }
      else if ((secs > 0) || (!show_msec))
         {
         _snprintf(dest, n, "%I64d s", secs);
         }
      else
         {
         _snprintf(dest, n, "%I64d ms", ms);
         }

      return text;
      }

   //
   // Convert MJD to Windows file time (# of 1-us intervals since 1-Jan-1601 UTC)
   //

   S64 MJD_to_us(DOUBLE MJD_time)
      {
      DOUBLE secs_since_1970 = (MJD_time - MJD_TIME_1970) * SECONDS_PER_DAY;
      return (S64) ((secs_since_1970 * 1E6) + (SYS_TIME_1970 * 1000000LL));
      }

   //
   // Convert Windows file time to MJD
   //

   DOUBLE us_to_MJD(S64 us)
      {
      DOUBLE secs_since_1970 = ((DOUBLE) us / 1E6) - SYS_TIME_1970;
      return (secs_since_1970 / SECONDS_PER_DAY) + MJD_TIME_1970;
      }

   //
   // Get PC time (UTC) in fractional MJD format
   // >= 5 digits of precision are needed to represent whole-number seconds
   //
   // (Code from daytime.c by Tom Van Baak)
   //

   DOUBLE current_MJD(SYSTEMTIME *st = NULL)
      {
      SYSTEMTIME St;
      GetSystemTime(&St);

      if (st != NULL) *st = St;

      DOUBLE seconds = (((St.wHour * 60) + St.wMinute) * 60) + St.wSecond;
      seconds += St.wMilliseconds / 1E3;

      return date_to_MJD(St.wYear, St.wMonth, St.wDay) + (seconds / 86400.0);
      }

   //
   // Return whole-number Modified Julian Day (MJD) given
   // calendar year, month (1-12), and day (1-31).
   // - Valid for Gregorian dates from 17-Nov-1858.
   // - Adapted from sci.astro FAQ.
   //
   // (Code from daytime.c by Tom Van Baak)
   //

   DOUBLE date_to_MJD(S32 year, S32 month, S32 day)
      {
      return 367 * year
             - 7 * (year + (month + 9) / 12) / 4
             - 3 * ((year + (month - 9) / 7) / 100 + 1) / 4
             + 275 * month / 9
             + day
             + 1721028
             - 2400000;
      }              

   //
   // Set a new log filename
   //
   // By default, text passed to log_printf() will not be written
   // to a file
   //

   virtual void set_log_filename(C8 *filename)
      {
      memset(logfile_name, 0, sizeof(logfile_name));

      if (filename != NULL)
         {
         strncpy(logfile_name, filename, sizeof(logfile_name)-1);
         }
      }

   //
   // Write timestamped record to logfile, returning composited string that was
   // written 
   //
   // (If no logfile has been declared with set_log_filename(), the string will
   // still be returned)
   //
   // Warning: returned string is global to the TIMEUTIL instance
   //

   virtual C8 * __cdecl log_printf(C8 *fmt, ...)
      {
      va_list ap;

      va_start(ap, 
               fmt);

      C8 *result = log_vprintf(fmt,
                               ap);
      va_end(ap);

      return result;
      }

   virtual C8 * __cdecl log_vprintf(C8 *fmt, va_list ap)
      {
      lock();

      C8 work_string[TIMEUTIL_MAX_LOG_ENTRY_STRING];

      if (fmt == NULL)
         {
         strcpy(work_string, "(String missing or too large)\n");
         }
      else
         {
         memset(work_string, 0, sizeof(work_string));
    
         _vsnprintf(work_string, 
                    sizeof(work_string)-1,
                    fmt, 
                    ap);
         }

      memset(log_output_string, 0, sizeof(log_output_string));

      if (!log_previous_newline)
         {
         strcpy(log_output_string, work_string);
         }
      else
         {
         if (work_string[0] == '\n')      // (used to add a blank or otherwise-unstamped line)
            {
            _snprintf(log_output_string, 
                      sizeof(log_output_string)-1,
                     "%s",
                      work_string);
            }
         else
            {
            C8 stamp[64] = "";

            _snprintf(log_output_string, 
                      sizeof(log_output_string)-1,
                     "[%s] %s",
                      timestamp(stamp,sizeof(stamp)),
                      work_string);
            }
         }

      if (logfile_name[0])
         {
         FILE *log = fopen(logfile_name,"a+t");

         if (log != NULL)
            {
            fprintf(log, "%s", log_output_string);
            fclose(log);
            }
         }

      log_previous_newline = (work_string[strlen(work_string)-1] == '\n');

      unlock();
      return log_output_string;
      }
};

struct SCOPETIME
{
   C8  name[256];
   S64 start;
   USTIMER timer;

   SCOPETIME(C8 *_name=NULL)
      {
      if (_name != NULL)
         strcpy(name, _name);
      else 
         name[0] = 0;

      start = timer.us();
      }

   virtual ~SCOPETIME()
      {
      S64 duration = timer.us() - start;

      if (name[0])
         printf("%s: %I64d us\n",name, duration);
      else
         printf("%I64d us\n",duration);
      }
};

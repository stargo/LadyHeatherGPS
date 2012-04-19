#define EXTERN extern
#include "heather.ch"

// Trimble Thunderbolt GPSDO TSIP monitor program
//
// Copyright (C) 2008,2009 Mark S. Sims - all rights reserved
// Win32 port by John Miles, KE5FX (jmiles@pop.net)
//
// Temperature and oscillator control algorithms by Warren Sarkison
//
// Adev code adapted from Tom Van Baak's adev1.c and adev3.c
// Incremental adev code based upon John Miles' TI.CPP
//
// Easter,  moon phase,  and Islamic algorithims from voidware.com
//
// The LZW encoder used in this file was derived from code written by Gershon
// Elber and Eric S. Raymond as part of the GifLib package.  And by Lachlan
// Patrick as part of the GraphApp cross-platform graphics library.
//
//
// Note: This program calculates adevs based upon the pps and oscillator
//       error values reported by the unit.  These values are not
//       derived from measurements made against an independent reference
//       (other than the GPS signal) and may not agree with adevs calculated
//       by measurements against an external reference signal (particularly
//       at smaller values of tau).
// 
//
//  This file contains code for various major functional groups:
//     Log file I/O
//     ADEVs
//     Precise position survey
//     AZ/EL map display
//     Daylight savings time support and other time related stuff
//     Digital clock display
//     Analog watch display
//     Calendars and greetings
//     GIF file output
//     Active temperature control
//     Alternate oscillator control algorithm
//     

extern char *months[];
extern u08 bmp_pal[];



double cos_factor = 0.82;     // cosine of the latitude

#define AZEL_RADIUS ((AZEL_SIZE/2)-AZEL_MARGIN)
int azel_grid_color;
int last_minute = 99;

#define MIN_SIGNAL   30.0F   // minimum signal level to plot
#define LEVEL_STEP   2.0F    // dBc per signal level step
#define LEVEL_COLORS 11      // how many signal level steps are available
#define LOW_SIGNAL   GREY    // show signals below MIN_SIGNAL in this color

int level_color[] = {        // converts signal level step to dot color
   BLACK,
   // LOW_SIGNAL is at this step
   DIM_RED,      // 1 = 30
   RED,
   BROWN,
   DIM_MAGENTA,  // actually grotty yellow
   YELLOW,       // 5 = 38
   DIM_BLUE,     // 6 = 40
   BLUE,
   CYAN,
   DIM_GREEN,
   GREEN,
   DIM_WHITE     // 11 = 50
};

int sat_colors[] = {
   BROWN,
   BLUE,
   GREEN,
   CYAN,
   DIM_BLUE,
   MAGENTA,
   YELLOW,
   WHITE,
   DIM_GREEN,
   DIM_CYAN,
   DIM_RED,
   DIM_MAGENTA,
   GREY,
   RED,
   DIM_WHITE
};


// used for both precise survey and oscillator autotuning
int hour_lats;
int hour_lons;
double lat_hr_bins[SURVEY_BIN_COUNT+1];
double lon_hr_bins[SURVEY_BIN_COUNT+1];

char *days[] = {
   "Sunday",
   "Monday",
   "Tuesday",
   "Wednesday",
   "Thursday",
   "Friday",
   "Saturday",
};


int add_to_bin(double val,  double bin[],  int bins_filled)
{
int i;

   if(bins_filled >= SURVEY_BIN_COUNT) {
     return bins_filled;
   }

   bin[bins_filled] = val;
   for(i=bins_filled-1; i>=0; i--) {
      if(bin[i] > val) {
         bin[i+1] = bin[i];
         bin[i] = val;
      }
      else break;
   }

   bins_filled += 1;
   return bins_filled;
}

//
//
// Log stuff
//
//
void show_log_state()
{
char *mode;

   if(text_mode && first_key) return;
   if(full_circle) return;
   if(log_wrap) {
      vidstr(VER_ROW+5, VER_COL, WHITE, "Log: on queue wrap");
   }
   else if(log_date || log_time) {
      vidstr(VER_ROW+5, VER_COL, WHITE, "Log: timed");
   }
   else if(log_file) {
//    sprintf(out, "Log: %5ld sec", log_interval);
      vidstr(VER_ROW+5, VER_COL, WHITE, "                       ");
      if(log_mode[0] == 'a') mode = "Cat";
      else if(log_stream)    mode = "Dbg";
      else                   mode = "Wrt";
      if(strlen(log_name) > 16) {
         sprintf(out, "%s:...%s", mode, &log_name[strlen(log_name)-16]);
      }
      else {
         sprintf(out, "%s: %s", mode, log_name);
      }
      vidstr(VER_ROW+5, VER_COL, WHITE, out);
   }
   else if(log_stream) {  // debug packed dump mode
      if(log_loaded) vidstr(VER_ROW+5, VER_COL, WHITE, "Dbg: loaded       ");
      else           vidstr(VER_ROW+5, VER_COL, WHITE, "Dbg: OFF          ");
   }
   else {
      if(log_loaded) vidstr(VER_ROW+5, VER_COL, WHITE, "Log: loaded       ");
      else           vidstr(VER_ROW+5, VER_COL, WHITE, "Log: OFF          ");
   }
}

void open_log_file(char *mode)
{
   if(log_file) return;  // log already open

   log_file = fopen(log_name, mode);
   if(log_header) log_written = 0;
   log_loaded = 0;
   show_log_state();
   log_file_time = (log_interval+1);
}

void close_log_file()
{
   if(log_file) fclose(log_file);

   log_file = 0;
   if(log_header) log_written = 0;
   show_log_state();
}

void sync_log_file()
{
   if(log_file == 0) return;

   close_log_file();
   open_log_file("a");
}

void write_log_readings(FILE *file, long x)
{
int i;

   if(file == 0) return;  // log not open
   if(tow == 0) return;   // no data from GPS yet
   if(log_stream) return; // dumping raw tsip data

   if(have_info < (MANUF_PARAMS | PRODN_PARAMS | VERSION_INFO)) return;

   if(have_info == (MANUF_PARAMS | PRODN_PARAMS | VERSION_INFO)) {
      have_info |= INFO_LOGGED;
      fprintf(file, "#\n");
      if(unit_name[0]) {
         fprintf(file, "#   Unit type:    Trimble %s\n", unit_name);
      }
      else if(saw_nortel) {
         fprintf(file, "#   Unit type:    Nortel NTGS50AA\n");
      }
      else {
         fprintf(file, "#   Unit type:    Trimble Thunderbolt%s\n", ebolt?"-E":"");
      }

      fprintf(file, "#   Serial number: %u.%lu\n", sn_prefix, serial_num);
      fprintf(file, "#   Case s/n:      %u.%lu\n", case_prefix, case_sn);
      fprintf(file, "#   Prodn number:  %lu.%u\n", prodn_num, prodn_extn);
      fprintf(file, "#   Prodn options: %u\n", prodn_options);        // from prodn_params message
      fprintf(file, "#   Machine id:    %u\n", machine_id);
      if(ebolt) {
         fprintf(file, "#   Hardware code: %u\n", hw_code);
      }

      fprintf(file, "#   App firmware:  %2d.%-2d  %02d %s %02d\n", 
         ap_major, ap_minor,  ap_day, months[ap_month], ap_year);   //!!! docs say 1900
      fprintf(file, "#   GPS firmware:  %2d.%-2d  %02d %s %02d\n", 
         core_major, core_minor,  core_day, months[core_month], core_year);  //!!! docs say 1900
      fprintf(file, "#   Mfg time:      %02d:00  %02d %s %04d\n", 
         build_hour, build_day, months[build_month], build_year);

      fprintf(file, "#\n");
      if(plot_title[0] && (title_type == USER)) {
         format_plot_title();
         fprintf(file, "#TITLE: %s\n", out);
         fprintf(file, "#\n");
      }
   }

   if(hours != last_hours) {  // sync log data to disk every hour
      if(file == log_file) sync_log_file();
      last_hours = hours;
      if(log_header) log_written = 0;
   }

   if(log_written == 0) {  // write header each time log opens
      log_written = 1;
      fprintf(file,"#\n");

      //!!! Note: if you change the content or spacing of this line,  change reload_log() to match
      fprintf(file,"#  %02d:%02d:%02d %s   %02d %s %04d - interval %u seconds\n", 
         hours,minutes,seconds, (time_flags&0x01)?"UTC":"GPS", day,months[month],year, log_interval);

      fprintf(file,"#\n");

      if(res_t) {
         fprintf(file,"# tow\t\t\tbias(sec)  \trate(%s)\tcorr(ns) \ttemp(C)\t\tsats\n", ppb_string);
      }
      else {
         fprintf(file,"# tow\t\t\tpps(sec) \tosc(%s)\tdac(v)  \ttemp(C)\t\tsats\n", ppb_string);
      }
   }

   if(log_errors && spike_threshold && last_temperature && (ABS(temperature - last_temperature) >= spike_threshold)) {
//    fprintf(file,"#! temperature spike: %f at tow %lu\n", (temperature-last_temperature), tow);
   }

//fprintf(file, "%ld: ", x);
   if(res_t) {
//    fprintf(file, "%02d:%02d:%02d  %6lu\t%10.6f\t%f\t%f\t%f\t%d \n", 
//       hours,minutes,seconds, tow, (float) (pps_offset/1.0E9*1.0E6), (float) osc_offset, dac_voltage, temperature, sat_count);
      fprintf(file, "%02d:%02d:%02d  %6lu\t%g\t%f\t%f\t%f\t%d \n", 
         hours,minutes,seconds, tow, (float) (pps_offset/1.0E9), (float) osc_offset, dac_voltage, temperature, sat_count);
   }
   else {
      fprintf(file, "%02d:%02d:%02d  %6lu\t%g\t%f\t%f\t%f\t%d \n", 
         hours,minutes,seconds, tow, (float) (pps_offset/1.0E9), (float) osc_offset, dac_voltage, temperature, sat_count);
   }

   if(log_db && (file == log_file)) {
      for(i=1; i<32; i++) {
         if(sat[i].sig_level && (sat[i].azimuth || sat[i].elevation)) {
            fprintf(file, "#SIG %02d  %5.1f  %-4.1f  %5.1f\n", 
              i, sat[i].azimuth, sat[i].elevation, sat[i].sig_level);
         }
      }
   }
}


// #define SCI_LOG "sci.log"  // for testing SCILAB code that calcs oscillator params
FILE *sci_file;   
long nnn;


void write_q_entry(FILE *file, long i)
{
struct PLOT_Q q;
int j;
int hh,mm,ss,dd,mo,yy;
u32 tow_save;
int count_save;
u08 flag_save;
double o_save, p_save;
float d_save, t_save;

    hh = hours;   
    mm = minutes; 
    ss = seconds; 
    dd = day;     
    mo = month;   
    yy = year;    
    tow_save = tow;
    o_save = osc_offset;
    p_save = pps_offset;
    d_save = dac_voltage;
    t_save = temperature;
    count_save = sat_count;
    flag_save = time_flags;

    if(filter_log && filter_count) q = filter_plot_q(i);
    else                           q = get_plot_q(i);
       
    hours   = q.hh;
    minutes = q.mm;
    seconds = q.ss;
    day     = q.dd;
    month   = q.mo;
    year    = q.yr;
    if(year >= 80) year += 1900;
    else           year += 2000;
 
    // fake the time of week
    tow = 0;
    tow += 365L*(long) (year-2009);
    tow += (long) dsm[month];
    tow += (long) day;            // t = day number of the year
    tow *= (24L*60L*60L);         // * secs per day
    tow += (60L*60L) * (long) hours;  // + secs per hour
    tow += (60L) * (long) minutes;    // + secs per minute
    tow += (long) seconds;            // + seconds

    osc_offset  = q.data[OSC]  / (OFS_SIZE) queue_interval;
    pps_offset  = q.data[PPS]  / (OFS_SIZE) queue_interval;
    dac_voltage = q.data[DAC]  / (float) queue_interval;
    temperature = q.data[TEMP] / (float) queue_interval;

    sat_count = q.sat_flags & SAT_COUNT;
    if(q.sat_flags & UTC_TIME) time_flags = 0x01;
    else                       time_flags = 0x00;

#ifdef SCI_LOG
if(sci_file == 0) sci_file = fopen(SCI_LOG, "w");
if(sci_file) fprintf(sci_file, "%ld %12.6f %10.6f %10.6f\n", nnn, osc_offset*100.0, temperature, dac_voltage);
nnn += queue_interval;
#endif

    if(file) {
       for(j=0; j<MAX_MARKER; j++) {
          if(i && (mark_q_entry[j] == i)) {
             fprintf(file, "#MARKER %d\n", j);
          }
       }
    }
    
    write_log_readings(file, i);

    hours       = hh;
    minutes     = mm;
    seconds     = ss;
    day         = dd;
    month       = mo;
    year        = yy;
    tow         = tow_save;
    sat_count   = count_save;
    time_flags  = flag_save;
    osc_offset  = o_save; 
    pps_offset  = p_save;
    dac_voltage = d_save; 
    temperature = t_save; 
}

void dump_log(char *name, u08 dump_size)
{
FILE *file;
char temp_name[128];
long temp_interval;
u08 temp_flags;
u08 temp_pause;
u08 temp_info;
u08 temp_written;
long i;
long counter;
long val;
int row;
char *s;
char filter[32];

   if(queue_interval <= 0) return;

   row = PLOT_TEXT_ROW+4;
   if(dump_size == 'p') s = "plot area";
   else                 s = "queue";

   filter[0] = 0;
   if(filter_log && filter_count) sprintf(filter, "%d point filtered ", filter_count); 

   temp_interval = log_interval;
   temp_flags = time_flags;
   temp_pause = pause_data;
   temp_info = have_info;
   temp_written = log_written;

   log_written = 0;
   strcpy(temp_name, log_name);

   log_interval = queue_interval;
   strcpy(log_name, name);
   if(!strstr(log_name, ".")) strcat(log_name, ".LOG");

   erase_help();
   file = fopen(log_name, log_mode);
   if(file == 0) {
      sprintf(out, "Cannot open file: %s", log_name);
      edit_error(out);
      goto dump_exit;
   }
   fprintf(file, "#TITLE: From log file %s (%s%s data)\n", log_name, filter, s);
   if(res_t == 0) {
      fprintf(file, "#OSC_GAIN %f\n", osc_gain);
   }

   if(log_mode[0] == 'a') sprintf(out, "Appending %s%s data to file: %s", filter, s, log_name);
   else                   sprintf(out, "Writing %s%s data to file: %s", filter, s, log_name);
   vidstr(row, PLOT_TEXT_COL, PROMPT_COLOR, out);

   pause_data = 1;
   counter = 0;
   if(dump_size == 'p') {
      i = plot_q_col0;  // dumping the plot area's data
   }
   else {
      i = plot_q_out;   // dumping the full queue
   }
   while((i != plot_q_in) || (dump_size == 'p')) {
      write_q_entry(file, i);
      have_info |= INFO_LOGGED;
      update_pwm();   // if doing pwm temperature control

      if((++counter & 0xFFF) == 0x0000) {   // keep serial data from overruning
         get_pending_gps();  //!!!! possible recursion
      }
      if((counter % 1000L) == 1L) {
         sprintf(out, "Line %ld", counter-1L);
         vidstr(row+2, PLOT_TEXT_COL, PROMPT_COLOR, out);
         refresh_page();
      }

      if(dump_size == 'p') {
         val = view_interval * (long) PLOT_WIDTH;
         val /= (long) plot_mag;
         if(counter >= val) break;
         if(counter >= (plot_q_count-1)) break;
      }
      if(++i >= plot_q_size) i = 0;
   }

   #ifdef SCI_LOG
      if(sci_file) fclose(sci_file);
      sci_file = 0;
   #endif

   #ifdef ADEV_STUFF
      log_adevs();
   #endif

   fclose(file);      // close this log and restore original one

   dump_exit:
   log_interval = temp_interval;
   log_written = temp_written;
   time_flags = temp_flags;
   pause_data = temp_pause;
   have_info = temp_info;
   strcpy(log_name, temp_name);
   show_log_state();
}

void write_log_leapsecond()
{
   if(log_file) {
      fprintf(log_file,"#\n");
      fprintf(log_file,"#! Leapsecond: %02d:%02d:%02d %s   %02d %s %04d - interval %u secs\n", 
         hours,minutes,seconds, (time_flags&0x01)?"UTC":"GPS", day,months[month],year, log_interval);
      fprintf(log_file,"#\n");
   }
}

void write_log_changes()
{
u16 change;

   if(log_file == 0) return;
   if(log_errors == 0) return;

   if((rcvr_mode != last_rmode) && (check_precise_posn == 0)) {
      fprintf(log_file,"#! new reciever mode: ");
      if     (rcvr_mode == 0) fprintf(log_file, "2D/3D");
      else if(rcvr_mode == 1) fprintf(log_file, "Single satellite");
      else if(rcvr_mode == 3) fprintf(log_file, "2D");
      else if(rcvr_mode == 4) fprintf(log_file, "3D");
      else if(rcvr_mode == 5) fprintf(log_file, "DGPS reference");
      else if(rcvr_mode == 6) fprintf(log_file, "2D clock hold");
      else if(rcvr_mode == 7) fprintf(log_file, "Overdetermined clock");
      else                    fprintf(log_file, "?%u?", rcvr_mode);
      fprintf(log_file, ": at tow %lu\n", tow);
   }
   last_rmode = rcvr_mode;


   if((gps_status != last_status) && (check_precise_posn == 0)) {
      fprintf(log_file,"#! new gps status: ");
      if(gps_status == 0x00)      fprintf(log_file, "Doing fixes");
      else if(gps_status == 0x01) fprintf(log_file, "No GPS time");
      else if(gps_status == 0x03) fprintf(log_file, "PDOP too high");
      else if(gps_status == 0x08) fprintf(log_file, "No usable sats");
      else if(gps_status == 0x09) fprintf(log_file, "1 usable sat");
      else if(gps_status == 0x0A) fprintf(log_file, "2 usable sats");
      else if(gps_status == 0x0B) fprintf(log_file, "3 usable sats");
      else if(gps_status == 0x0C) fprintf(log_file, "chosen sat unusable");
      else if(gps_status == 0x10) fprintf(log_file, "TRAIM rejected fix");
      else                        fprintf(log_file, "?%02X?", gps_status);
      fprintf(log_file, ": at tow %lu\n", tow);
   }
   last_status = gps_status;


   if((discipline_mode != last_dmode) && (check_precise_posn == 0)) { 
      // if you change any of this text,  you must also change reload_log();
      fprintf(log_file,"#! new discipline mode: ");
      if(discipline_mode == 0)      fprintf(log_file, "Normal");
      else if(discipline_mode == 1) fprintf(log_file, "Power-up");
      else if(discipline_mode == 2) fprintf(log_file, "Auto holdover");
      else if(discipline_mode == 3) fprintf(log_file, "Manual holdover");
      else if(discipline_mode == 4) fprintf(log_file, "Recovery mode");
      else if(discipline_mode == 6) fprintf(log_file, "Disabled");
      else                          fprintf(log_file, "?%u?", discipline_mode);
      fprintf(log_file, ": at tow %lu\n", tow);
   }
   last_dmode = discipline_mode;


   if((discipline != last_discipline) && (check_precise_posn == 0)) { 
      fprintf(log_file,"#! new discipline state: ");
      if(discipline == 0)      fprintf(log_file, "Phase locking");
      else if(discipline == 1) fprintf(log_file, "Warming up");
      else if(discipline == 2) fprintf(log_file, "Frequency locking");
      else if(discipline == 3) fprintf(log_file, "Placing PPS");
      else if(discipline == 4) fprintf(log_file, "Initializing loop filter");
      else if(discipline == 5) fprintf(log_file, "Compensating OCXO");
      else if(discipline == 6) fprintf(log_file, "Inactive");
      else if(discipline == 8) fprintf(log_file, "Recovery");
      else fprintf(log_file, "?%02X?", discipline);
      fprintf(log_file, ": at tow %lu\n", tow);
   }
   last_discipline = discipline;


   if(last_critical != critical_alarms) {
      fprintf(log_file, "#! new critical alarm state %04X:  ", critical_alarms);

      change = last_critical ^ critical_alarms;
      if(change & 0x0001) {
         if(critical_alarms & 0x0001)   fprintf(log_file, "  ROM:BAD");
         else                           fprintf(log_file, "  ROM:OK");
      }
      if(change & 0x0002) {
         if(critical_alarms & 0x0002)   fprintf(log_file, "  RAM:BAD");
         else                           fprintf(log_file, "  RAM:OK");
      }
      if(change & 0x0004) {
         if(critical_alarms & 0x0004)   fprintf(log_file, "  Power:BAD");
         else                           fprintf(log_file, "  Power:OK ");
      }
      if(change & 0x0008) {
         if(critical_alarms & 0x0008)   fprintf(log_file, "  FPGA:BAD");
         else                           fprintf(log_file, "  FPGA:OK ");
      }
      if(change & 0x0010) {
         if(critical_alarms & 0x0010)   fprintf(log_file, "  OSC: BAD");
         else                           fprintf(log_file, "  OSC: OK ");
      }
      fprintf(log_file, ": at tow %lu\n", tow);
   }
   last_critical = critical_alarms;


   change = last_minor ^ minor_alarms;
   change &= (~0x1000);  // PP2S pulse skip is probably not an error
   if(change && (check_precise_posn == 0)) {
      fprintf(log_file, "#! new minor alarm state %04X:  ", minor_alarms);
      if(change & 0x0001) {
         if(minor_alarms & 0x0001)  fprintf(log_file, "OSC age alarm   ");
         else                       fprintf(log_file, "OSC age normal   ");
      }
      if(change & 0x0006) {
         if(minor_alarms & 0x0002)       fprintf(log_file, "Antenna open   ");
         else if(minor_alarms & 0x0004)  fprintf(log_file, "Antenna short   ");
         else                            fprintf(log_file, "Antenna OK   ");
      }
      if(change & 0x0008) {
         if(minor_alarms & 0x0008)  fprintf(log_file, "No sats usable   ");
         else                       fprintf(log_file, "Tracking sats   ");
      }
      if(change & 0x0010) {
         if(minor_alarms & 0x0010)  fprintf(log_file, "Undisciplined   ");
         else                       fprintf(log_file, "Discipline OK   ");
      }
      if(change & 0x0020) {
         if(minor_alarms & 0x0020)  fprintf(log_file, "Survey started  ");
         else                       fprintf(log_file, "Survey stopped  ");
      }
      if(change & 0x0040) {
         if(minor_alarms & 0x0040)  fprintf(log_file, "No saved posn   ");
         else                       fprintf(log_file, "Position saved   ");
      }
      if(change & 0x0080) {
         if(minor_alarms & 0x0080)  fprintf(log_file, "LEAP PENDING!    ");
         else                       fprintf(log_file, "No leap second   ");
      }
      if(change & 0x0100) {
         if(minor_alarms & 0x0100)  fprintf(log_file, "Test mode set    ");
         else                       fprintf(log_file, "Normal op mode   ");
      }
      if(change & 0x0200) {
         if(minor_alarms & 0x0200)  fprintf(log_file, "Saved posn BAD   ");
         else                       fprintf(log_file, "Saved posn OK    ");
      }
      if(change & 0x0400) {
         if(minor_alarms & 0x0400)  fprintf(log_file, "EEPROM corrupt   ");
         else                       fprintf(log_file, "EEPROM data OK   ");
      }
      if(change & 0x0800) {
         if(minor_alarms & 0x0800)  fprintf(log_file, "No almanac    ");
         else                       fprintf(log_file, "Almanac OK    ");
      }
      fprintf(log_file, ": at tow %lu\n", tow);
   }
   last_minor = minor_alarms;
}

void write_log_error(u16 number, u32 val)
{
   if(log_file == 0) return;
   if(log_errors == 0) return;

   fprintf(log_file, "#! tsip packet %04X error: %08lX\n", number, val);
}

void write_log_utc(s16 utc_offset)
{
   if(log_file == 0) return;
   if((have_info & INFO_LOGGED) == 0) { // log file header not written yet   
      if(user_set_log) return;   // keeps bogus UTC entry out of log file
   }

   fprintf(log_file,"#\n");
   fprintf(log_file,"#! New UTC offset: %d seconds\n", (int) utc_offset);
   fprintf(log_file,"#\n");
}

void log_saved_posn(int type)
{
double d_lat, d_lon, d_alt;
float x;

   if(log_file == 0) return;  

   if(type < 0) {  // position saved via repeated single point surveys
      fprintf(log_file, "#Position saved via repeated single point surveys.\n");
   }
   else {  // position saved via TSIP message
      x = (float) precise_lat;   lat = (double) x;
      x = (float) precise_lon;   lon = (double) x;
      x = (float) precise_alt;   alt = (double) x;
      if(type == 1)      fprintf(log_file, "#User stopped precise save of manually entered position.  TSIP message used.\n");
      else if(type == 2) fprintf(log_file, "#User stopped precise save of surveyed position.  TSIP message used.\n");
      else if(type == 3) fprintf(log_file, "#User stopped precise survey.  Averaged position saved using TSIP message.\n");
      else if(type)      fprintf(log_file, "#Precise survey stopped.  Reason=%d.\n", type);
      else {
         fprintf(log_file, "#Position saved via TSIP message.  Roundoff error is small enough.\n");
      }
   }

   // log how and why we saved a receiver position
   d_lat = (lat-precise_lat)*RAD_TO_DEG/ANGLE_SCALE;
   d_lon = (lon-precise_lon)*RAD_TO_DEG/ANGLE_SCALE*cos_factor;
   d_alt = (alt-precise_alt);

   fprintf(log_file, "#DESIRED POSITION: %.9lf  %.9lf  %.9lf\n",
                        precise_lat*RAD_TO_DEG, precise_lon*RAD_TO_DEG, precise_alt);
   fprintf(log_file, "#SAVED POSITION:   %.9lf  %.9lf  %.9lf\n",
                        lat*RAD_TO_DEG, lon*RAD_TO_DEG, alt);
   fprintf(log_file, "#ROUNDOFF ERROR:   lat=%.8lf %s   lon=%.8lf %s   rms=%.8lf %s\n", 
      d_lat,angle_units,  d_lon,angle_units,  sqrt(d_lat*d_lat + d_lon*d_lon),angle_units);
}

void restore_plot_config()
{
int i;

   if(showing_adv_file) { // restore all plots
      showing_adv_file = 0;
      for(i=0; i<num_plots; i++) plot[i].show_plot = plot[i].old_show;
      plot_sat_count = old_sat_plot;
      plot_adev_data = old_adev_plot;
      adev_period = old_adev_period;
      keep_adevs_fresh = old_keep_fresh;
   }
}

void close_script(u08 close_all)
{
int i;

   if(script_file == 0) return;

   if(close_all) {
      for(i=0; i<script_nest; i++) fclose(scripts[i].file);
      script_file = 0;
      script_pause = 0;
      script_nest = 0;
   }
   else if(script_nest) {
      fclose(script_file);
      --script_nest;
      strcpy(script_name, scripts[script_nest].name);
      script_file   = scripts[script_nest].file;
      script_line   = scripts[script_nest].line;
      script_col    = scripts[script_nest].col;
      script_err    = scripts[script_nest].err;
      script_fault  = scripts[script_nest].fault;
      script_pause  = scripts[script_nest].pause;
   }
   else {
     fclose(script_file);
     script_file  = 0;
     script_pause = 0;
   }
}

void open_script(char *fn)
{
   if(script_file) { // nested script files
      if(script_nest < SCRIPT_NEST) {
         strcpy(scripts[script_nest].name, script_name);
         scripts[script_nest].file  = script_file;
         scripts[script_nest].line  = script_line;
         scripts[script_nest].col   = script_col;
         scripts[script_nest].fault = script_fault;
         scripts[script_nest].err   = script_err;
         scripts[script_nest].pause = script_pause;
         ++script_nest;
      }
      else {
         edit_error("Script files nested too deep");
         close_script(1);
         return;
      }
   }

   strncpy(script_name, fn, SCRIPT_LEN);
   script_file  = fopen(fn, "r");
   script_line  = 1;
   script_col   = 0;
   script_fault = 0;
   script_pause = 0;
   skip_comment = 0;
   return;
}

FILE *open_it(char *line, char *fn)
{
FILE *file;

    strcpy(line, fn);
    file = fopen(line, "r");
    if(file) return file;

    if(strstr(fn, ".")) return file; // extension given,  we are done trying

    // try to open file with default extensions
    strcpy(line, fn);
    strcat(line, ".LOG");
    file = fopen(line, "r");
    if(file) return file;

    strcpy(line, fn);
    strcat(line, ".SCR");
    file = fopen(line, "r");
    if(file) return file;

    strcpy(line, fn);
    strcat(line, ".LLA");
    file = fopen(line, "r");
    if(file) return file;

    strcpy(line, fn);
    strcat(line, ".CAL");
    file = fopen(line, "r");
    if(file) return file;

    strcpy(line, fn);
    strcat(line, ".SIG");
    file = fopen(line, "r");
    if(file) return file;

    strcpy(line, fn);
    strcat(line, ".ADV");
    file = fopen(line, "r");
    if(file) return file;

    strcpy(line, fn);
    strcat(line, ".TIM");
    file = fopen(line, "r");
    if(file) return file;

    return 0;
}

void time_check(int reading_log, u16 interval, int hours,int minutes,int seconds)
{
long t;
int warn;
COORD row, col;
struct PLOT_Q q;

   // This routine verfies that time stamps are sequential
   if(log_errors == 0) return;

   t = 0;
   t += 365L*(long) (year-2009);
   t += (long) dsm[month];
   t += (long) day;            // t = day number of the year
   t *= (24L*60L*60L);         // * secs per day
   t += (60L*60L) * (long) hours;  // + secs per hour
   t += (60L) * (long) minutes;    // + secs per minute
   t += (long) seconds;            // + seconds

   // see if we have a skip in the time stamp sequence
   if(time_checked == 0) warn = 0;   // it's the first time 
   else if((t-last_stamp) == 0) warn = 1;   // duplicate time stamp
   else if((reading_log == 0) && ((t-last_stamp) != 1)) warn = 2; // checking live readings
   else if(reading_log && ((t-last_stamp) != interval)) warn = 3; // checking log readings
   else warn = 0;   // properly consecutive time stamps

   if(warn) {  // we have a duplicate or missing time stamp
      q = get_plot_q(plot_q_in);
      q.sat_flags |= TIME_SKIP;  
      put_plot_q(plot_q_in, q);

      if(log_file && log_errors && (reading_log == 0)) {
         if(warn == 1)      fprintf(log_file, "#! time stamp duplicated.\n");
         else if(warn == 2) fprintf(log_file, "#! time stamp skipped.  t=%ld  last=%ld\n", t, last_stamp);
         else if(warn == 3) fprintf(log_file, "#! time stamp skipped in log file\n");
         else               fprintf(log_file, "#! time stamp sequence error type: %d\n", warn);
      }

      if(reading_log && log_errors) {
         if(text_mode) {
            row = EDIT_ROW;
            col = EDIT_COL;
         }
         else {
            row = PLOT_TEXT_ROW;
            col = PLOT_TEXT_COL;
         }
         sprintf(out, "#! time stamp skip %d in log.  interval=%u  prev=%-9ld this=%-9ld  ", warn, interval, last_stamp, t);
         vidstr(row+4, col, RED, out);
         refresh_page();
      }
   }

   last_stamp = t;
   time_checked = have_time;
}


float llt;  // last log temperature;
float tvb;


int reload_log(char *fn, u08 cmd_line)
{
FILE *file;
char line[SLEN];
char ti[20];
char *s;
long tow;
u16 log_interval;
u16 i, j;
int hh,mm,ss;
u08 temp_pause;
u32 counter;
COORD row, col;
int color;
u08 valid_log;
u08 temp_dmode;
u08 temp_tflags;
int temp_day, temp_month, temp_year;
double pps_scale, osc_scale;
double pps_val, osc_val;
double pps_ref, osc_ref;
double vp, vo;
u08 lla_seen;
u08 saw_osc;
u08 saw_title;
u08 have_ref;
u08 old_fixes;
u08 tim_file;
FILE *afile;
int mark_number;
afile = 0;  // fopen("ADEV.XXX", "w");

    // returns 1=bad file name
    //         2=bad file format
    //         3=lla file
    //         4=script file opened
    //         0=all other file types
    color = 0;
    adev_log = lla_log = lla_seen = have_ref = 0;
    pps_scale = osc_scale = 1.0;
    pps_val = osc_val = 0.0;
    pps_ref = osc_ref = 0.0;
    saw_osc = 0;
    saw_title = 0;
    tim_file = 0;

    temp_pause = pause_data;
    if(text_mode) {
       row = EDIT_ROW;
       col = EDIT_COL;
    }
    else {
       row = PLOT_TEXT_ROW+4;
       col = PLOT_TEXT_COL;
    }
    erase_help();

    
    file = open_it(line, fn);
    llt = 0.0F;
    if(file == 0) {  // file not found
       sprintf(out, "Cannot open file: %s", fn);
       edit_error(out);
       pause_data = temp_pause;
       return 1;
    }

    strcpy(fn, line);
    strupr(line);
    if(cmd_line && (strstr(read_log, ".SCR") == 0)) {
       pause_data ^= 1;  
       temp_pause ^= 1;
    }
    if(strstr(line, ".TIM")) {  // file is a TI.EXE .TIM file
       tim_file = 1;
       sprintf(out, "Reading TI.EXE .TIM file: %s", line);
       goto read_adev_file;
    }
    else if(strstr(line, ".ADV")) {  // file is an adev file
       sprintf(out, "Reading adev file: %s", line);
       read_adev_file:
       adev_log = 3;
       if(showing_adv_file == 0) {
          old_adev_period = adev_period;
          old_keep_fresh = keep_adevs_fresh;
       }
       adev_period = 1.0;
       keep_adevs_fresh = 0;
// hours = minutes = seconds = day = month = year = 0;
// afile = fopen("adev.adv", "w");
//if(afile) fprintf(afile, "#\n");
    }
    else if(strstr(line, ".SCR")) {  // file is a script file
       fclose(file);
       open_script(fn);
       return 4;
    }
    else if(strstr(line, ".LLA")) { // file is a lat/lon/altitude file
       lla_log = 3;
       plot_lla = 1;
zoom_lla = 20;
       if(zoom_lla) full_circle = 1;
       else         full_circle = 0;
       reading_lla = 1;
       all_adevs = 0;
       first_key = 0;
       plot_signals = 0;
       if((shared_plot == 0) && (WIDE_SCREEN == 0)) plot_azel = 0;
       if(SCREEN_WIDTH < 800) {
          shared_plot = plot_azel = 1;
       }
       config_screen();

       old_fixes = show_fixes;
       show_fixes = 1;
       redraw_screen();
       show_fixes = old_fixes;

       sprintf(out, "Reading lat/lon/alt file: %s", line);
//start_precision_survey();
    }
#ifdef GREET_STUFF
    else if(strstr(line, ".CAL")) { // file is a calendar file
       read_calendar(line, 0);
       return 0;
    }
#endif
#ifdef SIG_LEVELS
    else if(strstr(line, ".SIG")) { // file is a signal level file
       read_signals(line);
       return 0;
    }
#endif
    else {    // file is a data log file
       sprintf(out, "Reading log file: %s", line);
       for(mark_number=0; mark_number<MAX_MARKER; mark_number++) {
          mark_q_entry[mark_number] = 0;
       }
    }
    if(reading_lla && zoom_lla) {
       vidstr(TEXT_ROWS-5, TEXT_COLS/2, PROMPT_COLOR, out);
    }
    else {
       vidstr(row, col, PROMPT_COLOR, out);
    }
    refresh_page();

    log_loaded = 1;
    valid_log = 0;
    new_const = 0;
    log_interval = 1;
    have_time = 3;
    counter = 0;
    pause_data = 1;

    restore_plot_config();
    reset_queues(0x03);    // clear out the old data

    reading_log = 1;
    time_checked = 0;
    while(fgets(line, sizeof line, file) != NULL) {
        update_pwm();   // if doing pwm temperature control
        if(script_file) ;
        else if(KBHIT()) {
           GETCH();
           i = edit_error("Reading paused...  press ESC to stop");
           if(i == 0x1B) break;
           if(reading_lla && zoom_lla) {
              vidstr(TEXT_ROWS-4, TEXT_COLS/2, PROMPT_COLOR, &blanks[TEXT_COLS-80]);
           }
           else {
              vidstr(EDIT_ROW+3, EDIT_COL, PROMPT_COLOR, &blanks[TEXT_COLS-80]);
           }
           refresh_page();
        }

        if((counter == 0) && (line[0] != '#') && (tim_file == 0)) {  // it ain't no log file
           not_log:
           edit_error("File format not recognized.  First line must start with '#'.");
           fclose(file);
           pause_data = temp_pause;
           lla_log = 0;
           reading_lla = 0;
           reading_log = 0;
           time_checked = 0;
           return 2;
        }

        if((++counter % 1000L) == 1L) {
           sprintf(out, "Line %ld", counter-1L);
           if(reading_lla && zoom_lla) {
              vidstr(TEXT_ROWS-2, TEXT_COLS/2, PROMPT_COLOR, out);
           }
           else {
              vidstr(row+3, col, PROMPT_COLOR, out);
           }
           refresh_page();
        }

        // Parse the log file comment lines for relevent data.
        // !!! This is a VERY crude parser that depends upon fixed spacing.
        // !!! If you change any of the log file output formats,  you will need
        // !!! to make changes in this code also.
        if((line[0] == '#') || (line[0] == ';') || (line[0] == '*') || (line[0] == '/')) { 
           if((line[5] == ':') && (line[8] == ':')) {  // time comment
              if(strstr(line, "seconds") == 0) continue;
              s = strstr(line, "interval");
              if(s == 0) continue;

              if((line[12] == 'G') && (line[13] == 'P') && (line[14] == 'S')) {
                 time_flags = 0x00;
              }
              else if((line[12] == 'U') && (line[13] == 'T') && (line[14] == 'C')) {
                 time_flags = 0x01;
              }

              sscanf(&s[9], "%lu", &log_interval);
              sscanf(&line[18], "%d %s %d", &day, &ti[0], &year);
              for(month=1; month<=12; month++) {
                 if(!strcmp(months[month], &ti[0])) goto got_month;
              }
              month = 0;

              got_month:
              valid_log = 1;
           }
           else if((line[1] == '!') && strstr(line, "discipline mode")) {  // discipline mode comment
              discipline_mode = 0;
              if     (strstr(line, "Normal"))          discipline_mode = 0;
              else if(strstr(line, "Power-up"))        discipline_mode = 1; 
              else if(strstr(line, "Auto holdover"))   discipline_mode = 2; 
              else if(strstr(line, "Manual holdover")) discipline_mode = 3; 
              else if(strstr(line, "Recovery mode"))   discipline_mode = 4; 
              else if(strstr(line, "Disabled"))        discipline_mode = 6; 
           }
           else if(!strnicmp(line, "#TITLE", 6)) {
              strcpy(plot_title, &line[7]);
              set_title:
              i = strlen(plot_title);
              if(i && (plot_title[i-1] == 0x0D)) plot_title[i-1] = 0;
              if(i && (plot_title[i-1] == 0x0A)) plot_title[i-1] = 0;
              if(plot_title[0]) title_type = USER;
              else              title_type = NONE;
              saw_title = 1;
              show_title();
              refresh_page();
           }
           else if(!strnicmp(line, "#SCALE", 6)) {
              pps_scale = osc_scale = 1.0;
              sscanf(&line[7], "%lf %lf", &pps_scale, &osc_scale);
           }
           else if(!strnicmp(line, "#OSC_GAIN", 9)) {
              osc_gain = (-3.5);
              sscanf(&line[10], "%f", &osc_gain);
              user_osc_gain = osc_gain;
              log_osc_gain = osc_gain;
              gain_color = YELLOW;
           }
           else if(!strnicmp(line, "#INTERVAL", 9)) {
              adev_period = 1.0F;
              sscanf(&line[10], "%f", &adev_period);
           }
           else if(!strnicmp(line, "#PERIOD", 7)) {
              adev_period = 1.0F;
              sscanf(&line[8], "%f", &adev_period);
           }
           else if(!strnicmp(line, "#LLA", 4)) {
              sscanf(&line[5], "%lf %lf %lf", &precise_lat, &precise_lon, &precise_alt);
              precise_lon /= RAD_TO_DEG;
              precise_lat /= RAD_TO_DEG;
              cos_factor = cos(precise_lat);
           }
           else if(!strnicmp(line, "#MARKER", 7)) {
              mark_number = 0;
              sscanf(&line[8], "%d", &mark_number);
              mark_q_entry[mark_number] = plot_q_in;
           }
           continue;
        }

        // filter out or parse lines that don't start with numbers
        j = strlen(line);
        for(i=0; i<j; i++) {
           if((line[i] >= '0') && (line[i] <= '9')) goto good_line;
           else if((line[i] == '.') || (line[i] == '+') || (line[i] == '-')) goto good_line;
           else if((line[i] == ' ') || (line[i] == '\t')) continue;
           else if(tim_file) {
              if(!strnicmp(&line[i], "CAP", 3)) {
                 if(strlen(&line[4])+4 < SLEN) {
                    if(saw_title) strcat(plot_title, " | ");
                    strcat(plot_title, &line[i+4]);
                    goto set_title;
                 }
              }
              else if(!strnicmp(&line[i], "TIM", 3)) {
                 if(strlen(&line[4])+4 < SLEN) {
                    if(saw_title) strcat(plot_title, " | ");
                    strcat(plot_title, &line[i+4]);
                    goto set_title;
                 }
              }
              else if(!strnicmp(&line[i], "IMO", 3)) {
                 if(strlen(&line[4])+4 < SLEN) {
                    if(saw_title) strcat(plot_title, " | ");
                    strcat(plot_title, &line[i+4]);
                    goto set_title;
                 }
              }
              else if(!strnicmp(&line[i], "PER", 3)) {
                 adev_period = 1.0F;
                 sscanf(&line[i+4], "%f", &adev_period);
                 break;
              }
              else if(!strnicmp(&line[i], "SCA", 3)) {
                 pps_scale = osc_scale = 1.0;
                 sscanf(&line[i+4], "%lf %lf", &pps_scale, &osc_scale);
                 break;
              }
              else break;
           }
           else break;
        }
        continue;

        good_line:
        if(lla_log) {  // read lat/lon/altitude info
           #ifdef PRECISE_STUFF
              sscanf(line, "%ld %d %lf %lf %lf", &this_tow, &gps_status, &lat, &lon, &alt);
              if(gps_status == 0) {
                 if(lla_seen == 0) plot_lla_axes();
                 lla_seen = 1;
                 lat /= RAD_TO_DEG;
                 lon /= RAD_TO_DEG;
                 color = counter / 3600L;
                 color %= 14;
                 plot_lla_point(color+1);
              }
           #endif  // PRECISE_STUFF
        }
        else if(adev_log) {  // read adev values
              pps_val = osc_val = 0.0;
              sscanf(line, "%le %le", &pps_val, &osc_val);
//if(afile) fprintf(afile, "%.11le %.11le\n", pps_val, osc_val);
              if(have_ref == 0) {  // we can remove the first data point as a constant offset from all points
                 pps_ref = pps_val;
                 osc_ref = osc_val;
                 have_ref = 1;
              }
              if(osc_val != 0.0) saw_osc = 1;

              if(subtract_base_value == 2) {
                 pps_val -= pps_ref;
                 osc_val -= osc_ref;
              }

              pps_val /= (1.0e-9);           // convert to ns
              pps_offset = (pps_val * pps_scale);

              osc_val /= (100.0 * 1.0e-9);
              osc_offset = (osc_val * osc_scale);

              #ifdef ADEV_STUFF
                 add_adev_point(osc_offset, pps_offset);
              #endif

              if(1) {
                 dac_voltage = 0.0F;
                 temperature = 0.0F;
                 bump_time();
//if(afile) fprintf(afile, "%.11le %.11le\n", pps_offset, osc_offset);
                 update_plot(0);
              }
        }
        else {  // read log info
           if(valid_log == 0) goto not_log;

           sscanf(line, "%02d%c%02d%c%02d %ld %lf %lf %f %f %d", 
             &hh,&ti[0],&mm,&ti[0],&ss, &tow, 
             &vp,&vo,&dac_voltage,&temperature, &sat_count
           );
if(llt == 0.0F) llt = temperature;
if(undo_fw_temp_filter) {
   tvb = (SENSOR_TC * temperature) - ((SENSOR_TC-1.0F) * llt);
   llt = temperature;
   temperature = tvb;
}

           osc_offset  = vo;
           pps_offset  = vp * 1.0E9;

           hours = hh;
           minutes = mm;
           seconds = ss;

           tsip_error = msg_fault = 0;
           time_check(1, log_interval, hh,mm,ss);
           for(i=0; i<log_interval; i++) {
              update_plot(0);
              #ifdef ADEV_STUFF
                 if(adev_period > 0.0F) {
                    if(++adev_time >= (int) (adev_period+0.5F)) {  // add this data point to adev data queue
                       add_adev_point(osc_offset, pps_offset);   
                       adev_time = 0;
                    }
                 }
              #endif
           }
        }

        // every so often,  process any pending GPS messages 
        // to keep the serial port buffer from overflowing
        if((counter & 0xFFF) == 0x0000) {  
           temp_dmode = discipline_mode;
           temp_tflags = time_flags;
           temp_day = day;   temp_month = month;   temp_year = year;

           get_pending_gps();  //!!!! possible recursion

           day = temp_day;   month = temp_month;   year = temp_year;
           time_flags = temp_tflags;
           discipline_mode = temp_dmode;
        }
    }
    pause_data = temp_pause;

    reading_log = 0;
    time_checked = 0;
    reading_lla = 0;

    if(saw_title == 0) {
       sprintf(plot_title, "From file: %s", fn);
       title_type = USER;
       show_title();
       refresh_page();
    }

    #ifdef ADEV_STUFF
       find_global_max();
    #endif

    if(file) fclose(file);

    if(lla_log) {
       lla_log = 0;
       return 3;
    }
    else if(adev_log) {
       adev_log = 0;
       if(showing_adv_file == 0) {  // remember the current plot setup
          showing_adv_file = 1;
          for(i=0; i<num_plots; i++) plot[i].old_show = plot[i].show_plot;
          old_sat_plot = plot_sat_count;
          old_adev_plot = plot_adev_data;
       }
       for(i=0; i<num_plots; i++) plot[i].show_plot = 0;
       plot[PPS].show_plot = 1;
       plot[OSC].show_plot = saw_osc;
       plot_sat_count = 0;
       plot_adev_data = 1;
       keep_adevs_fresh = 0;  // allow adevs to be calculated over all points
       return 0;
    }
    return 0;
}


//
// 
//  Date and time related stuff
//
//
int dst_hour = 2;     // the hour to switch the time at
int dst_start_day;    // day and month of daylight savings time start
int dst_start_month;
int dst_end_day;      // day and month of daylight savings time end
int dst_end_month;
int down_under;       // start and stop times are reversed in the southern hemisphere

// string decribes when daylight savings time occurs
char *dst_list[] = {  // start: day_count,day_of_week number,start_month,
                      // end:   day_count,day_of_week number,end_month,
                      //        switchover_hour
                      //
                      //   day_count = nth occurence of day 
                      //               (if > 0, from start of month)
                      //               (if < 0, from end of month)
                      //   day_of_week 0=SUN, 1=MON, 2=TUE, ... 6=SAT
                      //   month 1=JAN ... 12=DEC
                      //   hour that the time switches over

   "",                 // zone 0 = no dst
   " 2,0,3,1,0,11,2",  // zone 1 = USA
   "-1,0,3,-1,0,10,2", // zone 2 = Europe
   " 1,0,10,1,0,4,2",  // zone 3 = Australia
   "-1,0,9,1,0,4,2",   // zone 4 = New Zealand
   custom_dst          // zone 5 = custom zone definition goes here
};

int dim[] = {   // days in the month
   0,
   31,   //jan
   28,   //feb
   31,   //mar
   30,   //apr
   31,   //may
   30,   //jun
   31,   //jul
   31,   //aug
   30,   //sep
   31,   //oct
   30,   //nov
   31    //dec
};

char *months[] = {   /* convert month to its ASCII abbreviation */
   "???",
   "Jan", "Feb", "Mar", "Apr", "May", "Jun",
   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

int leap_year(int year)
{
   if(year % 4) return 0;        // not a leap year
   if((year % 100) == 0) {       // most likely,  not a leap year
      if(year % 400) return 0;   // not a leap year
   }
   return 1;
}

void init_dsm()
{
int month;

   // days to start of month
   dsm[0]  = 0;
   dsm[1]  = 0;         // Jan
   dsm[2]  = 0+31;      // Feb
   dsm[3]  = 0+31+28;   // March ...
   dsm[4]  = 0+31+28+31;
   dsm[5]  = 0+31+28+31+30;
   dsm[6]  = 0+31+28+31+30+31;
   dsm[7]  = 0+31+28+31+30+31+30;
   dsm[8]  = 0+31+28+31+30+31+30+31;
   dsm[9]  = 0+31+28+31+30+31+30+31+31;
   dsm[10] = 0+31+28+31+30+31+30+31+31+30;
   dsm[11] = 0+31+28+31+30+31+30+31+31+30+31;
   dsm[12] = 0+31+28+31+30+31+30+31+31+30+31+30;

   dim[2] = 28;

   if(leap_year(year)) {
      for(month=2; month<=12; month++) dsm[month] += 1;
      dim[2] += 1;
   }
}

u08 day_of_week(int d, int m, int y)      /* 0=Sunday  1=Monday ... */
{
static u08 dow_info[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

   if(m < 3) y -= 1;
   return (y + y/4 - y/100 + y/400 + dow_info[m-1] + d) % 7;
}

int nth_dow_in(int nth, int dow, int month, int year)
{
int sundays;
int d;

   sundays = 0;
   if(nth < 0) {  // nth day of week from end of month
      nth = 0 - nth;
      for(d=dim[month]; d>=1; d--) {
         if(day_of_week(d, month, year) == dow) {
            ++sundays;
            if(sundays == nth) return d;  // last sunday in month, etc
         }
      }
   }
   else { // nth day of week from start of month
      for(d=1; d<=dim[month]; d++) {
         if(day_of_week(d, month, year) == dow) {
            ++sundays;
            if(sundays == nth) return d;  // 2nd sunday in month, etc
         }
      }
   }
   return 0;
}

double jdate(int y, int m, int d)
{
long j;
long j1;
long j2;
long j3;
long c;
long x;

   // return the julian date of the start of a given day
   if(m < 3) {
      m = m + 12;
      j = y - 1;
   }
   else j = y;

   c = (j / 100L);
   x = j - (100L * c);
   j1 = (146097L * c) / 4L;
   j2 = (36525L * x) / 100L;
   j3 = ((153L*(long)m) - 457L) / 5L;
   return (1721119.5-1.0) + (double) j1 + (double) j2 + (double) j3 + (double) d;
}

double jtime(int hh, int mm, int ss)
{
double h;

   h = ((double) hh) + (((double) mm)/60.0) + (((double) ss)/3600.0);
   return (h / 24.0);
}

double gmst(int yy, int mo, int dd, int hh, int mm, int ss, int app_flag)
{
double jd0;
double h;
double d0;
double jd;
double st;
double t;
double d;    // oh, my
double omega;
double epsilon;
double l;
double delta;
double eqeq;

   // calculate Greenwich mean or apparent sidereal time
   jd0 = jdate(yy,mo,dd);
   h = jtime(hh,mm,ss);

   jd = jd0 + h;
   d = jd - 2451545.0; 

   if(0) {  // supposedly more accurate,  but totally bogus results
      d0 = jd0 - 2451545.0;
      t = (d / 36525.0);
      st = 6.697374558 + (0.06570982441908*d0) + (1.00273790935*h) + (0.000026*t*t);
   }
   else {
      st = 18.697374558 + (24.06570982441908 * d);
   }
   st = fmod(st, 24.0);

   if(app_flag) { // calculate nutation for apparent sidereal time
      omega = 125.04 - (0.052954 * d);
      l = 280.47 + (0.98565 * d); 
      epsilon = 23.4393 - (0.0000004 * d); 
      delta = (-0.000319*sin(omega)) - (0.000024*sin(l+l));
      eqeq = delta * cos(epsilon);
      st += eqeq;
      st = fmod(st, 24.0);
   }

   while(st < 0.0)   st += 24.0;
   while(st >= 24.0) st -= 24.0;
   return st;
}

double lmst(int yy, int mo, int dd, int hh, int mm, int ss, int app_flag)
{
double st;

   // calculate Greenwich mean or apparent local time
   if(lon > 0.0) st = 360.0 - lon*RAD_TO_DEG;  // st = lon west of Greenwich
   else          st = 0.0 - lon*RAD_TO_DEG;
   st = st * 24.0 / 360.0;   // st is the hour offset based upon longitude

   st = gmst(yy,mo,dd, hh,mm,ss, app_flag) - st;

   while(st < 0.0)   st += 24.0;
   while(st >= 24.0) st -= 24.0;
   return st;
}

void calc_dst_times(char *s)
{
int start_n, end_n;
int start_dow, end_dow;

   // calculate when daylight savings time starts and ends
   dst_start_month = dst_end_month = 0;
   dst_start_day = dst_end_day = 0;
   down_under = 0;

   #ifdef GREET_STUFF
      setup_calendars();   // recalc calendars for the new time zone
   #endif

   if(dst_area == 0) return;        // no dst area set
   if(s == 0) return;               // no dst area definition string
   if(s[0] == 0) return;            // empty dst area definition string
   if(dst_string[0] == 0) return;   // no DST time zone name given

   start_n = end_n = 0;
   start_dow = end_dow = 0;
   dst_hour = 2;
   sscanf(s, "%d,%d,%d,%d,%d,%d,%d", &start_n,&start_dow,&dst_start_month,
                                     &end_n,&end_dow,&dst_end_month,&dst_hour);
   if(dst_start_month > dst_end_month) { // start month>end_month means southern hemisphere
      down_under = 1;                    // re-scan the string to swap the values
      dst_start_month = dst_end_month = 0;
      start_n = end_n = 0;
      start_dow = end_dow = 0;
      dst_hour = 2;
      sscanf(s, "%d,%d,%d,%d,%d,%d,%d", &end_n,&end_dow,&dst_end_month,
                                        &start_n,&start_dow,&dst_start_month,&dst_hour);
   }

   dst_start_day = nth_dow_in(start_n, start_dow, dst_start_month, pri_year);
   dst_end_day   = nth_dow_in(end_n,   end_dow,   dst_end_month,   pri_year);

   #ifdef GREET_STUFF
      setup_calendars();   // recalc calendars for the new time zone
   #endif

//sprintf(plot_title, "DST(%s): %02d/%02d .. (%s) %02d/%02d  invert=%d -> %d.  area=%d: %s", 
//  dst_string, dst_start_month, dst_start_day, std_string, dst_end_month, dst_end_day, down_under, dst_offset(), dst_area, dst_list[dst_area]);
}

int dst_enabled()
{
   if(time_zone_set == 0)   return 0;
   if(dst_start_day == 0)   return 0;
   if(dst_start_month == 0) return 0;
   if(dst_end_day == 0)     return 0;
   if(dst_end_month == 0)   return 0;
   if(dst_string[0] == 0)   return 0;
   if(dst_area == 0)        return 0;

   return 1;
}


int dst_offset()
{
int before_switch_hour;
int after_switch_hour;

   // calculate time zone adjustment for daylight savings time settings
   strcpy(tz_string, std_string);

   if(dst_enabled() == 0) {   // dst switchover times not set, use standard time
      return 0;
   }

   if(down_under) {  // southern hemisphere spins backwards
      if(dst_string[0]) strcpy(tz_string, dst_string);
      before_switch_hour = dst_hour-1;
      after_switch_hour = dst_hour;
   }
   else {
      before_switch_hour = dst_hour;
      after_switch_hour = dst_hour-1;
   }

   // see if it is before dst start time
   if(pri_month < dst_start_month) return down_under;
   else if(pri_month == dst_start_month) {
      if(pri_day < dst_start_day)  return down_under; 
      else if(pri_day == dst_start_day) {
         if(pri_hours < before_switch_hour) return down_under;
      }
   }

   
   // it is after dst start time,  is it before dst end time?
   if(pri_month > dst_end_month) return down_under; 
   else if(pri_month == dst_end_month) {
      if(pri_day > dst_end_day) return down_under; 
      else if(pri_day == dst_end_day) {
         if(pri_hours >= after_switch_hour) return down_under; 
      }
   }


   if(down_under) { // daylight savings time is not in effect
      if(std_string[0]) strcpy(tz_string, std_string);
      return 0;
   }
   else {           // daylight savings time is in effect
      if(dst_string[0]) strcpy(tz_string, dst_string);
      return 1;
   }
}

int adjust_tz()
{
double st;

   dst_ofs = 0;
   if(use_gmst || use_lmst) {
      if(use_gmst) st = gmst(pri_year,pri_month,pri_day, pri_hours,pri_minutes,pri_seconds, use_gmst-1);
      else         st = lmst(pri_year,pri_month,pri_day, pri_hours,pri_minutes,pri_seconds, use_lmst-1);

      pri_hours = (int) st;
      st = (st - pri_hours);       // decimal hour
      st *= 60.0;

      pri_minutes = (int) st;
      st = (st - pri_minutes);     // decimal minutes
      st *= 60.0;

      pri_seconds = (int) st;
      st = (st - pri_seconds);     // decimal seconds
      st_secs = st;                // remainder seconds

      strcpy(tz_string, std_string);
   }
   else {
      if(time_zone_set) {   // adjust date and time for time zone
         pri_seconds += time_zone_seconds;
         if(pri_seconds < 0) {
            pri_seconds += 60;
            --pri_minutes;
         }
         else if(pri_seconds >= 60) {
            pri_seconds -= 60;
            ++pri_minutes;
         }
         
         pri_minutes += time_zone_minutes;
         if(pri_minutes < 0) {
            pri_minutes += 60;
            --pri_hours;
         }
         else if(pri_minutes >= 60) {
            pri_minutes -= 60;
            ++pri_hours;
         }

         pri_hours += time_zone_hours;
         if(pri_hours < 0) {   // adjust hour by the time zone offset
            pri_hours += 24;
            pri_day -= 1;
            if(pri_day <= 0) {
               if(--pri_month <= 0) {
                  pri_month = 12;
                  --pri_year;
               }
               pri_day = dim[pri_month];
            }
         }
         else if(pri_hours >= 24) {
            pri_hours -= 24;
            pri_day += 1;
            if(pri_day > dim[pri_month]) {
               pri_day = 1;
               if(++pri_month > 12) {
                  pri_month = 1;
                  ++pri_year;
               }
            }
         }
      }

      dst_ofs = dst_offset();
      if(dst_ofs) {
         if(++pri_hours >= 24) {
            pri_hours = 0;
            if(++pri_day > dim[pri_month]) {
               pri_day = 1;
               if(++pri_month > 12) {
                  pri_month = 1;
                  ++pri_year;
               }
            }
         }
      }
   }
   return 1;
}

void check_end_times()
{
   if(exit_timer) {  // countdown timer set
      if(--exit_timer == 0) goto time_exit;
   }

   // see if user wants us to exit at this time every day
   if(end_time || end_date) {    
      if((pri_hours == end_hh) && (pri_minutes == end_mm) && (pri_seconds == end_ss)) {
         if(end_date) {
            if((pri_month == end_month) && (pri_day == end_day) && (pri_year == end_year)) {
               goto time_exit;
            }
         }
         else {
            time_exit:
            printf("\nUser set exit time has been reached\n");
            shut_down(100);
         }
      }
   }

   if(alarm_time || alarm_date) {  // alarm clock has been set
      if((pri_hours == alarm_hh) && (pri_minutes == alarm_mm) && (pri_seconds == alarm_ss)) {
         if(alarm_date) {
            if((pri_month == alarm_month) && (pri_day == alarm_day) && (pri_year == alarm_year)) {
               sound_alarm = 1;
            }
         }
         else {
            sound_alarm = 1;
         }
      }
   }

   if(dump_time || dump_date) {  // screen dump clock has been set
      if((pri_hours == dump_hh) && (pri_minutes == dump_mm) && (pri_seconds == dump_ss)) {
         if(dump_date) {
            if((pri_month == dump_month) && (pri_day == dump_day) && (pri_year == dump_year)) {
               do_dump = 1;
            }
         }
         else {
            do_dump = 1;
         }
      }
   }

   if(log_time || log_date) {  // log dump clock has been set
      if((pri_hours == log_hh) && (pri_minutes == log_mm) && (pri_seconds == log_ss)) {
         if(log_date) {
            if((pri_month == log_month) && (pri_day == log_day) && (pri_year == log_year)) {
               do_log = 1;
            }
         }
         else {
            do_log = 1;
         }
      }
   }
}


char *get_tz_value(char *s)
{
int c;
u08 saw_colon;

   if(s == 0) return s;

   saw_colon = 0;
   while(1) { // get time zone hour offset value
      c = *s;
      if(c == 0) {
         s = 0;
         break;
      }
      if     (c == '+') tz_sign = (0+tz_sign); 
      else if(c == '-') tz_sign = (0-tz_sign);
      else if(c == ':') ++saw_colon;
      else if(isdigit(c)) {
         if     (saw_colon == 0) time_zone_hours = (time_zone_hours * 10) + (c-'0');
         else if(saw_colon == 1) time_zone_minutes = (time_zone_minutes * 10) + (c-'0');
         else if(saw_colon == 2) time_zone_seconds = (time_zone_seconds * 10) + (c-'0'); 
      }
      else break;
      ++s;
   }

   time_zone_hours %= 24;
   time_zone_hours *= tz_sign;
   time_zone_minutes %= 60;
   time_zone_minutes *= tz_sign;
   time_zone_seconds %= 60;
   time_zone_seconds *= tz_sign;

   return s;
}

char *get_std_zone(char *s)
{
int c;
int j;

   if(s == 0) return s;
   if(*s == 0) return 0;

   c = 0;
   std_string[0] = 0;
   j = 0;
   while(j < 20) {
      c = *s;
      if(c == '/') break;
      if(c == '-') break;
      if(c == '+') break;
      if(isdigit(c)) break;

      ++s;
      if(c == ' ') continue;
      if(c == '\t') continue;

      std_string[j++] = toupper(c);
      std_string[j] = 0;

      if(j >= TZ_NAME_LEN) break;
      else if(*s == 0)    { s=0; break; }
      else if(*s == 0x0D) { s=0; break; }
      else if(*s == 0x0A) { s=0; break; }
   }

   if(std_string[0] == 0) strcpy(std_string, tz_string);
   return s;
}

char *get_dst_zone(char *s)
{
int c;
int j;

   if(dst_area == 0) dst_area = USA;
   dst_string[0] = 0;
   j = 0;
   while(j < 20) {
      c = *s;
      if(c == 0) { s=0; break; }
      ++s;

      if(c == '/') break;
      if(c == ' ') continue;
      if(c == '\t') continue;

      dst_string[j++] = toupper(c);
      dst_string[j] = 0;

      if     (j >= TZ_NAME_LEN) break;
      else if(*s == 0)    { s=0; break; }
      else if(*s == 0x0D) { s=0; break; }
      else if(*s == 0x0A) { s=0; break; }
   }

   return s;
}

void set_time_zone(char *s)
{
char c;

   time_zone_set = 0;
   use_gmst = use_lmst = 0;
   tz_string[0] = dst_string[0] = 0;
   if(s == 0) return;
   if((s[0] == 0) || (s[0] == 0x0A) || (s[0] == 0x0D)) {  // clear time zone
      return;
   }

   // set time zone
   strncpy(std_string, "LOCAL", TZ_NAME_LEN);
   dst_string[0] = 0;
   strcpy(tz_string, dst_string);

   time_zone_set = 1;
   tz_sign = 1;
   time_zone_hours = 0;
   time_zone_minutes = 0;
   time_zone_seconds = 0;

   while(*s) {  // find first char of time zone string
      c = *s;
      if(c == 0)    { break; }
      if(c == ' ')  { ++s; continue; }
      if(c == '\t') { ++s; continue; }
      if(c == '=')  { ++s; continue; }

      if((c == '+') || (c == '-') || isdigit(c)) {  // tz string is -6CST/CDT format
         s = get_tz_value(s);
         if(s) s = get_std_zone(s);
         if(s) {
            if(*s == '/') get_dst_zone(s+1);
         }
      }
      else {   // string is in CST6CDT format
         s = get_std_zone(s);
         if(s) {
            tz_sign = 0 - tz_sign;
            s = get_tz_value(s);
         }
         if(s) get_dst_zone(s);
      }
      break;
   }

   if     (!strcmp(std_string, "LMST")) use_lmst = 1;
   else if(!strcmp(std_string, "LAST")) use_lmst = 2;
   else if(!strcmp(std_string, "GMST")) use_gmst = 1;
   else if(!strcmp(std_string, "GAST")) use_gmst = 2;
   if(use_gmst || use_lmst) {  // receiver needs to be in UTC mode for sidereal time to work
      set_gps_mode = 0;
      set_utc_mode = 1;
   }
}

void bump_time(void)
{
  // add one second to current time value
  if(++seconds >= 60) {
     seconds = 0;
     if(++minutes >= 60) {
        minutes = 0;
        if(++hours >= 24) {
           hours = 0;
           if(++day > dim[month]) {
              day = 1;
              if(++month > 12) {
                 month = 1;
                 ++year;
              }
           }
        }
     }
  }
}

void silly_clocks()
{
char s[32];

   #ifdef GREET_STUFF
      if(greet_ok || ((pri_hours == GREET_HOUR) && (pri_minutes == GREET_MINUTE) && (pri_seconds == GREET_SECOND))) {
         show_greetings();
         greet_ok = 0;
      }
   #endif

   // gratuitously silly cuckcoo clock mode
   if(cuckoo && (pri_seconds == CUCKOO_SECOND)) {  
      if(cuckoo_hours && (pri_minutes == 0)) {  // cuckoo the hour on the hour
         cuckoo_beeps = (pri_hours % 12);
         if(cuckoo_beeps == 0) cuckoo_beeps = 12;
      }
      else if((pri_minutes%(60/cuckoo)) == 0) {  // cuckoo on the marks
         if     (cuckoo_hours)      cuckoo_beeps = 1;
         else if(pri_minutes == 0)  cuckoo_beeps = 3;
         else if(pri_minutes == 30) cuckoo_beeps = 2;
         else                       cuckoo_beeps = 1;
      }
   }

   if(egg_timer) {
      if(--egg_timer == 0) {
         sound_alarm = 1;
         if(repeat_egg) egg_timer = egg_val;
      }
   }

   if(dump_timer) {
      if(--dump_timer == 0) {
         do_dump = 1;
         if(repeat_dump) dump_timer = dump_val;
      }
   }

   if(log_timer) {
      if(--log_timer == 0) {
         do_log = 1;
         if(repeat_log) log_timer = log_val;
      }
   }

   if(sound_alarm) {  // sounding the alarm clock
      alarm_clock();  
      if(single_alarm) {  // sound alarm once,  useful if playing long sound file
         sound_alarm = 0;
         #ifdef DIGITAL_CLOCK
            reset_vstring();
         #endif
      }
   }

   
   if(do_dump) {
      ++dump_number;
      if(single_dump) sprintf(s, "TBDUMP");
      else {
        sprintf(s, "TB%04d-%02d-%02d-%ld", pri_year,pri_month,pri_day,dump_number);
      }
      dump_screen(invert_video, s);
      do_dump = 0;
   }
   
   if(do_log) {
      ++log_number;
      if(single_log) sprintf(s, "TBLOG.LOG");
      else {
         sprintf(s, "TB%04d-%02d-%02d-%ld.LOG", pri_year,pri_month,pri_day,log_number);
      }
      dump_log(s, 'q');
      do_log = 0;
   }

   if(cuckoo_beeps) { // cuckoo clock
      cuckoo_clock(); 
      if(cuckoo_beeps) --cuckoo_beeps;
   }
}

void need_time_set()
{
   set_system_time = 1;
   time_set_char = '*';
// time_set_delay = 1;
}


void set_cpu_clock()
{
long milli;
int hhh, mmm, sss;

// SYSTEMTIME t;
// GetSystemTime(&t);
// sprintf(plot_title, "GPS=%02d:%02d  sys=%02d:%02d.%03d", 
//    minutes, seconds, t.wMinute, t.wSecond, t.wMilliseconds);

   // set the system time from the GPS receiver
   milli = 0;
   if(set_time_minutely) {  // we do it a xx:xx:06 local time every minute
      if(pri_seconds == SYNC_SECOND) {
         need_time_set();
      }
   }
   else if(set_time_hourly) {  // we do it a xx:05:06 local time every hour
      if((pri_minutes == SYNC_MINUTE) && (pri_seconds == SYNC_SECOND)) {
         need_time_set();
      }
   }
   else if(set_time_daily) {  // we do it a 4:05:06 local time every day
      if((pri_hours == SYNC_HOUR) && (pri_minutes == SYNC_MINUTE) && (pri_seconds == SYNC_SECOND)) {
         need_time_set();
      }
   }
   else if(set_time_anytime) {  // set system clock anytime x milliseconds of drift is seen
      #ifdef WINDOWS
         SYSTEMTIME t;
         GetSystemTime(&t);
         milli = (long) t.wSecond * 1000L;
         milli += t.wMilliseconds;
         milli -= time_sync_offset;
      #endif
      #ifdef DOS_BASED
         reg.h.ah = 0x2C;
         int86(0x21, &reg, &reg);
         milli = (long) reg.h.dl * 10L;
         milli += (long) reg.h.dh * 1000L;
         milli -= time_sync_offset;
      #endif

      // This code assumes system clock and GPS clock are synced within 20 seconds.
      // This should always be the case since we forced a time sync when 
      // set_time_anytime mode was activated.  If it is not the case,  we may
      // get a few extra time syncs.
      // It also assumes that time zone offset does not include seconds!!!
      if((milli > 40000L) && (pri_seconds < 20)) {  // CPU time and GPS time are near minute wraparound
         milli -= 60000L;
      }
      milli -= (long) pri_seconds * 1000L;
      if(milli < 0L) milli = 0L - milli;
      if(milli && (milli >= set_time_anytime)) {
         need_time_set();
      }
      // !!! we should check hours and minutes but that has time zone issues
      //     anyway,  time offset should always be under a second.
   }

   if(set_system_time == 0) return;   // we are not setting the time
   if(have_time == 0) return;         // we have no GPS time
   if(time_flags & 0x1C) return;      // GPS time is not valid
   if(system_idle == 0) return;       // only set time when system is not too busy
   if(time_set_delay) {               // waiting for sleeps to expire
      --time_set_delay;
      return;
   }

   if(force_utc_time && ((time_flags & 0x01) == 0x00)) {  // unit is in GPS time mode
      if(temp_utc_mode == 0) {
         temp_utc_mode = 1;
         set_timing_mode(0x03);       // temporarily go to UTC mode
         request_timing_mode();
      }
      return;
   }

   #ifdef WINDOWS
      hhh = hours;
      mmm = minutes;
      sss = seconds;
      milli = 0;
   #endif
   #ifdef DOS_BASED
      hhh = pri_hours;
      mmm = pri_minutes;
      sss = pri_seconds;
      milli = 0;
   #endif

   // adjust receiver time for message offset delay to get true time
   if(time_sync_offset) {
      milli =  ((long) hhh * 60L);        // minutes to start of hour
      milli += ((long) mmm);              // minutes to start of minute
      milli =  (milli * 60L) + (long)sss; // seconds
      milli *= 1000L;                     // time of day in milliseconds
      milli += time_sync_offset;          // adjust receiver time for message delay
      if(milli < 0) return;               // adjusted time would cause day change
      if(milli >= (24L*60L*60L*1000L)) return;  // ... so wait for another second
      hhh = milli / (60L*60L*1000L);      // convert adjusted time back to hh:mm:ss.msec
      milli -= hhh * (60L*60L*1000L);
      mmm = milli / (60L*1000L);
      milli -= mmm * (60L*1000L);
      sss = milli / 1000L;
      milli -= (sss * 1000L);
   }

   set_system_time = 0;

   #ifdef WINDOWS
      SYSTEMTIME t;
      t.wYear = year;
      t.wMonth = month;
      t.wDay = day;
      t.wHour = hhh;
      t.wMinute = mmm;
      t.wSecond = sss;
      t.wMilliseconds = (int) milli;
      SetSystemTime(&t);
   #endif

   #ifdef DOS_BASED
      reg.h.ch = hhh;
      reg.h.cl = mmm;
      reg.h.dh = sss;
      reg.h.dl = (milli/10L);
      reg.h.ah = 0x2D;     // set the time
      int86(0x21, &reg, &reg);

      reg.wd.cx = pri_year;
      reg.h.dh = pri_month;
      reg.h.dl = pri_day;
      reg.h.ah = 0x2B;     // set the date
      int86(0x21, &reg, &reg);
   #endif

// BEEP();
   if(temp_utc_mode) {     // exit UTC mode back to GPS mode
      set_timing_mode(0x00);
      request_timing_mode();
      temp_utc_mode = 0;
   }
}

#ifdef GREET_STUFF

double spring;          // Julian date of the seasons
double summer;
double fall;
double winter;

char spring_s[80];      // season greeting strings
char summer_s[80];
char autumn_s[80];
char winter_s[80];
char china_year[80];

#define SPRING       100+0   // greetings that must be calculated
#define SUMMER       100+1
#define AUTUMN       100+2
#define WINTER       100+3
#define TAX_DAY      100+4 
#define ELECTION     100+5 
#define CLOCK_FWD    100+6 
#define CLOCK_BACK   100+7 
#define ASH          100+8 
#define PALM_SUNDAY  100+9 
#define GOOD_FRIDAY  100+10
#define EASTER       100+11
#define GRANNY       100+12
#define ROSH         100+13
#define YOM          100+14
#define PASSOVER     100+15
#define HANUKKAH     100+16
#define PURIM        100+17
#define MARDI_GRAS   100+18
#define AL_HIJRA     100+19
#define ASURA        100+20
#define RAMADAN      100+21
#define EID          100+22
#define CHINA_NY     100+23
#define MAYAN_NY     100+24
#define AZTEC_NY     100+25
#define DOOMSDAY     100+26
#define SUKKOT       100+27
#define SHAVUOT      100+28
#define Q4_TAXES     100+29
#define Q2_TAXES     100+30
#define Q3_TAXES     100+31
#define FINAL_TAXES  100+32
#define UNIX_CLOCK   100+33

#define MAX_HOLIDAYS 366

struct HOLIDAY {
   int nth;
   int day;
   int month;
   char *text;
} holiday[MAX_HOLIDAYS] = {
    { SPRING,       0,  3,  &spring_s[0] },  // the first few holidays are calculated
    { SUMMER,       0,  6,  &summer_s[0] },
    { AUTUMN,       0,  9,  &autumn_s[0] },
    { WINTER,       0, 12,  &winter_s[0] },
    { TAX_DAY,      0,  4,  "It's time to pay your dear Uncle Sam his pound of flesh!" },
    { Q2_TAXES,     0,  6,  "Quarterly taxes are due! Uncle Sam wants all your quarters!" },
    { Q3_TAXES,     0,  9,  "Quarterly taxes are due! Uncle Sam wants all your quarters!" }, 
    { Q4_TAXES,     0,  1,  "Quarterly taxes are due! Uncle Sam wants all your quarters!" }, 
    { FINAL_TAXES,  0, 10,  "Your income tax extension expires today!  Pay up!" }, 
    { ELECTION,     0, 11,  "Vote early, vote often!  It's Throw the Bastards Out Day!" },
    { CLOCK_FWD,    0,  3,  "Set your clocks forward one hour tonight!" },
    { CLOCK_BACK,   0,  3,  "Set your clocks back one hour tonight!" },
    { ASH,          0,  3,  "It's Ash Wedneday..." },
    { PALM_SUNDAY,  0,  3,  "It's Palm Sunday..." },
    { GOOD_FRIDAY,  0,  3,  "It's Good Friday..." },
    { EASTER,       0,  3,  "Happy Easter!" },
    { GRANNY,       0,  9,  "It's Grandparent's day..." },
    { ROSH,         0,  9,  "Rosh Hashanha starts tonight..." },
    { YOM,          0,  9,  "Yom Kipper starts tonight..." },
    { PASSOVER,     0,  3,  "Passover starts tonight..." },
    { HANUKKAH,     0, 11,  "Chappy Chanukah!" },
    { PURIM,        0,  3,  "Purim start tonight!" },
    { MARDI_GRAS,   0,  3,  "It's Mardi Gras... time to party hardy!" },
    { AL_HIJRA,     0,  1,  "Happy Islamic New Year!" },
    { ASURA,        0,  1,  "It's Asura!" },
    { RAMADAN,      0,  1,  "Happy Ramadan!" },
    { EID,          0,  1,  "Eid mubarak!" },
    { CHINA_NY,     0,  3,  &china_year[0] },
    { MAYAN_NY,     0,  1,  "Happy Mayan New Year!" },
    { AZTEC_NY,     0,  1,  "Happy Aztec New Year!  Sacrifice your enemies!  Eat their hearts!" },
    { DOOMSDAY,     0,  1,  "The world ends tomorrow...  wear clean underwear" },
    { SUKKOT,       0,  1,  "Sukkot start tonight..." },
    { SHAVUOT,      0,  1,  "Shavout start tonight..." },
    { UNIX_CLOCK,   0,  1,  "Tomorrow is UNIX time doomstime...  wear a clean pocket protector" },

    { 0,  1,  1,  "Happy New Year!" },
    { 3,  1,  1,  "Happy Birthday, Martin!" },
    { 0,  2,  2,  "Happy Groundhog Day!" },
    { 1,  0,  2,  "Hope you didn't bet on the losers..." },
    { 0, 14,  2,  "Happy Valentine's Day!" },
    { 3,  1,  2,  "Greetings Mr. Presidents!  Happy Family Day!" },
    { 0, 13,  3,  "Happy birthday Pluto!  You're still our favorite planet.  The IAU sucks!" },
    { 0, 15,  3,  "Beware the Ides of March!" },
    { 0, 17,  3,  "Happy Saint Patrick's Day!" },
    { 0,  1,  4,  "April Fools!" },
    {-1,  5,  4,  "It's Arbor Day... go hug a tree!" },
    { 0, 22,  4,  "Happy Earth Day..." },
    { 0,  5,  5,  "Happy Cinco de Mayo!" },
    { 2,  0,  5,  "Happy Mother's Day, Mom!" },
    { 3,  6,  5,  "It's Armed Forces Day!" },
    {-1,  1,  5,  "Happy Memorial Day!" },
    { 1,  1,  6,  "Happy Birthday, Queenie!" },
    { 0, 14,  6,  "Happy Flag Day!" },
    { 3,  0,  6,  "Happy Father's Day, Dad!" },
    { 0,  1,  7,  "Canada, Oh Canada..." },
    { 0,  4,  7,  "When in the Course of human events..." },
    { 0, 14,  7,  "Happy Bastille Day!" },
    { 0, 15,  8,  "Happy Independence Day, India!" },
    { 1,  1,  9,  "Happy Labor Day!" },
    { 0,  2, 10,  "Happy Birthday, Mr. Ghandi..." },
    { 0,  9, 10,  "Way to go, Leif!" },
    { 0, 11,  9,  "Another day that will live in infamy..." },
    { 0, 17,  9,  "We the People of the United Sates of America..." },
    { 0, 19,  9,  "Aaargh matey... it be talk like a pirate day!" },
    { 2,  1, 10,  "In 1492, Columbus sailed the ocean blue..." },
    { 3,  5, 10,  "Woohoo! It's New Jersey Credit Union Day!"},
    { 0, 24, 10,  "It's United Nations Day!" },
    { 0, 31, 10,  "Happy Halloween!" },
    { 0,  1, 11,  "Buenos Dias Los Muertos!" },
    { 0, 11, 11,  "Thanks to all the world's veterans..." },
    { 4,  4, 11,  "Happy Thanksgiving!" },
    { 0,  5, 11,  "Remember, remember the Fifth of November!" },
    { 0, 19, 11,  "It's World Toilet Day!" },
    { 0,  7, 12,  "A day that will live in infamy..." },
    { 0, 17, 12,  "Congratulations to the Brothers Wright..." },
    { 0, 24, 12,  "Hey kids,  he's checking his list!" },
    { 0, 25, 12,  "Merry Christmas!" },
    { 0, 26, 12,  "Happy Boxing Day!" },
    { 0, 31, 12,  "Stay home tonight,  there are too many crazy people out there..." },
    { 0,  0, 0,   ""}
};

char *zodiac[] = {  // Chinese years
   "Monkey",
   "Rooster",
   "Dog",
   "Pig",
   "Mouse",
   "Ox",
   "Tiger",
   "Rabbit",
   "Dragon",
   "Snake",
   "Horse",
   "Goat" 
};

char *jmonths[] = {  // convert jewish month to its ASCII abbreviation
   "Tev", "Ad1", "Adr", "Nis", "Iyr", "Siv", 
   "Tam", "Av ", "Elu", "Tis", "Che", "Kis", 
   "Tev", "Shv"
};
double jtimes[14+1];  // start jdate of each month

char *imonths[] = {  // convert Indian month to its ASCII abbreviation
   "Pau",
   "Mag", "Pha", "Cai", "Vai", "Jya", "Asa",
   "Sra", "Bha", "Asv", "Har", "Pau", "Mag", "Pha"
};
double itimes[13+1];

char *mmonths[] = {  // convert Muslim month to its ASCII abbreviation
   "Muh", "Saf", "Raa", "Rat", "JiU",  "JaT", 
   "Raj", "Sbn", "Ram", "Swl", "DiQ",  "DiH",
   "Muh", "Saf", "Raa", "Rat", "JiU",  "JaT", 
   "Raj", "Sbn", "Ram", "Swl", "DiQ",  "DiH",
   "Muh", "Saf", "Raa", "Rat", "JiU",  "JaT", 
   "Raj", "Sbn", "Ram", "Swl", "DiQ",  "DiH"
};
double mtimes[36+1];
int myears[36+1];

char *pmonths[] = {  // Persian months
   "Far", "Ord", "Kho", "Tir", "Mor", "Sha",
   "Meh", "Aba", "Aza", "Dey", "Bah", "Esf",
   "Far", "Ord", "Kho", "Tir", "Mor", "Sha",
   "Meh", "Aba", "Aza", "Dey", "Bah", "Esf"
   "Far", "Ord", "Kho", "Tir", "Mor", "Sha",
   "Meh", "Aba", "Aza", "Dey", "Bah", "Esf"
};
double ptimes[36+1];
int pyears[36+1];

char *amonths[] = {  // Afghan months
   "Ham", "Saw", "Jaw", "Sar", "Asa", "Sun",
   "Miz", "Aqr", "Qaw", "Jad", "Dal", "Hou",
   "Ham", "Saw", "Jaw", "Sar", "Asa", "Sun",
   "Miz", "Aqr", "Qaw", "Jad", "Dal", "Hou",
   "Ham", "Saw", "Jaw", "Sar", "Asa", "Sun",
   "Miz", "Aqr", "Qaw", "Jad", "Dal", "Hou" 
};

char *kmonths[] = {  // Kurdish months
   "Xak", "Gol", "Joz", "Pos", "Glw", "Xer",
   "Rez", "Glz", "Ser", "Bef", "Reb", "Res",
   "Xak", "Gol", "Joz", "Pos", "Glw", "Xer",
   "Rez", "Glz", "Ser", "Bef", "Reb", "Res",
   "Xak", "Gol", "Joz", "Pos", "Glw", "Xer",
   "Rez", "Glz", "Ser", "Bef", "Reb", "Res" 
};

char *dmonths[] = {
   "Beth    ", "Luis    ", "Nuin    ", "Fearn   ",  "Saille  ", "Huath   ",
   "Duir    ", "Tinne   ", "Coll    ", "Muin    " , "Gort    ", "Ngetal  ",
   "Ruis    ", "Nuh     ", "Beth    ", "Luis    ",  "Nuin    ", "Fearn   "
};
double dtimes[18+1];
int dyears[18+1];

struct CHINA {
   u16 month_mask;
   u08 leap_month;
   float jd;
} china_data[] = {
   { 0x1A93,  5, 2454857.5 },  // 4706 2009
   { 0x0A95,  0, 2455241.5 },  // 4707 2010
   { 0x052D,  0, 2455595.5 },  // 4708 2011
   { 0x0AAD,  4, 2455949.5 },  // 4709 2012
   { 0x0AB5,  0, 2456333.5 },  // 4710 2013
   { 0x15AA,  9, 2456688.5 },  // 4711 2014
   { 0x05D2,  0, 2457072.5 },  // 4712 2015
   { 0x0DA5,  0, 2457426.5 },  // 4713 2016
   { 0x1D4A,  6, 2457781.5 },  // 4714 2017
   { 0x0D4A,  0, 2458165.5 },  // 4715 2018
   { 0x0C95,  0, 2458519.5 },  // 4716 2019
   { 0x152E,  4, 2458873.5 },  // 4717 2020
   { 0x0556,  0, 2459257.5 },  // 4718 2021
   { 0x0AB5,  0, 2459611.5 },  // 4719 2022
   { 0x11B2,  2, 2459966.5 },  // 4720 2023
   { 0x06D2,  0, 2460350.5 },  // 4721 2024
   { 0x0EA5,  6, 2460704.5 },  // 4722 2025
   { 0x0725,  0, 2461088.5 },  // 4723 2026
   { 0x064B,  0, 2461442.5 },  // 4724 2027
   { 0x0C97,  5, 2461796.5 },  // 4725 2028
   { 0x0CAB,  0, 2462180.5 },  // 4726 2029
   { 0x055A,  0, 2462535.5 },  // 4727 2030
   { 0x0AD6,  3, 2462889.5 },  // 4728 2031
   { 0x0B69,  0, 2463273.5 },  // 4729 2032
   { 0x1752, 11, 2463628.5 },  // 4730 2033
   { 0x0B52,  0, 2464012.5 },  // 4731 2034
   { 0x0B25,  0, 2464366.5 },  // 4732 2035
   { 0x1A4B,  6, 2464720.5 },  // 4733 2036
   { 0x0A4B,  0, 2465104.5 },  // 4734 2037
   { 0x04AB,  0, 2465458.5 },  // 4735 2038
   { 0x055D,  5, 2465812.5 },  // 4736 2039
   { 0x05AD,  0, 2466196.5 },  // 4737 2040
   { 0x0B6A,  0, 2466551.5 },  // 4738 2041
   { 0x1B52,  2, 2466906.5 },  // 4739 2042
   { 0x0D92,  0, 2467290.5 },  // 4740 2043
   { 0x1D25,  7, 2467644.5 },  // 4741 2044
   { 0x0D25,  0, 2468028.5 },  // 4742 2045
   { 0x0A55,  0, 2468382.5 },  // 4743 2046
   { 0x14AD,  5, 2468736.5 },  // 4744 2047
   { 0x04B6,  0, 2469120.5 },  // 4745 2048
   { 0x05B5,  0, 2469474.5 },  // 4746 2049
   { 0x0DAA,  3, 2469829.5 },  // 4747 2050
   { 0x0EC9,  0, 2470213.5 },  // 4748 2051
   { 0x1E92,  8, 2470568.5 },  // 4749 2052
   { 0x0E92,  0, 2470952.5 },  // 4750 2053
   { 0x0D26,  0, 2471306.5 },  // 4751 2054
   { 0x0A56,  6, 2471660.5 },  // 4752 2055
   { 0x0A57,  0, 2472043.5 },  // 4753 2056
   { 0x0556,  0, 2472398.5 },  // 4754 2057
   { 0x06D5,  4, 2472752.5 },  // 4755 2058
   { 0x0755,  0, 2473136.5 },  // 4756 2059
   { 0x0749,  0, 2473491.5 },  // 4757 2060
   { 0x0E93,  3, 2473845.5 },  // 4758 2061
   { 0x0693,  0, 2474229.5 },  // 4759 2062
   { 0x152B,  7, 2474583.5 },  // 4760 2063
   { 0x052B,  0, 2474967.5 },  // 4761 2064
   { 0x0A5B,  0, 2475321.5 },  // 4762 2065
   { 0x155A,  5, 2475676.5 },  // 4763 2066
   { 0x056A,  0, 2476060.5 },  // 4764 2067
   { 0x0B65,  0, 2476414.5 },  // 4765 2068
   { 0x174A,  4, 2476769.5 },  // 4766 2069
   { 0x0B4A,  0, 2477153.5 },  // 4767 2070
   { 0x1A95,  8, 2477507.5 },  // 4768 2071
   { 0x0A95,  0, 2477891.5 },  // 4769 2072
   { 0x052D,  0, 2478245.5 },  // 4770 2073
   { 0x0AAD,  6, 2478599.5 },  // 4771 2074
   { 0x0AB5,  0, 2478983.5 },  // 4772 2075
   { 0x05AA,  0, 2479338.5 },  // 4773 2076
   { 0x0BA5,  4, 2479692.5 },  // 4774 2077
   { 0x0DA5,  0, 2480076.5 },  // 4775 2078
   { 0x0D4A,  0, 2480431.5 },  // 4776 2079
   { 0x1C95,  3, 2480785.5 },  // 4777 2080
   { 0x0C96,  0, 2481169.5 },  // 4778 2081
   { 0x194E,  7, 2481523.5 },  // 4779 2082
   { 0x0556,  0, 2481907.5 },  // 4780 2083
   { 0x0AB5,  0, 2482261.5 },  // 4781 2084
   { 0x15B2,  5, 2482616.5 },  // 4782 2085
   { 0x06D2,  0, 2483000.5 },  // 4783 2086
   { 0x0EA5,  0, 2483354.5 },  // 4784 2087
   { 0x0E4A,  4, 2483709.5 },  // 4785 2088
   { 0x068B,  0, 2484092.5 },  // 4786 2089
   { 0x0C97,  8, 2484446.5 },  // 4787 2090
   { 0x04AB,  0, 2484830.5 },  // 4788 2091
   { 0x055B,  0, 2485184.5 },  // 4789 2092
   { 0x0AD6,  6, 2485539.5 },  // 4790 2093
   { 0x0B6A,  0, 2485923.5 },  // 4791 2094
   { 0x0752,  0, 2486278.5 },  // 4792 2095
   { 0x1725,  4, 2486632.5 },  // 4793 2096
   { 0x0B45,  0, 2487016.5 },  // 4794 2097
   { 0x0A8B,  0, 2487370.5 },  // 4795 2098
   { 0x149B,  2, 2487724.5 },  // 4796 2099
   { 0x04AB,  0, 2488108.5 },  // 4797 2100 
   { 0x095B,  7, 2488462.5 },  // 4798 2101 
   { 0x05AD,  0, 2488846.5 },  // 4799 2102 
   { 0x0BAA,  0, 2489201.5 },  // 4800 2103 
   { 0x1B52,  5, 2489556.5 },  // 4801 2104 
   { 0x0D92,  0, 2489940.5 },  // 4802 2105 
   { 0x0D25,  0, 2490294.5 },  // 4803 2106 
   { 0x1A4B,  4, 2490648.5 },  // 4804 2107 
   { 0x0A55,  0, 2491032.5 },  // 4805 2108 
   { 0x14AD,  9, 2491386.5 }   // 4806 2109 
};

double ctimes[40];
int cyear[40];
int cmonth[40];

int g_month;     // Gregorian date gets saved here by gregorian(jdate)
int g_day;
int g_year;
int g_hours;
int g_minutes;
int g_seconds;
double g_frac;

int kin;      // mayan date
int uinal;
int tun;
int katun;
int baktun;
int pictun;

char *tzolkin[] = {  // Mayan Tzolkin months
   "Ahau   ",  "Imix   ",  "Ik     ",  "Akbal  ",  "Kan    ",
   "Cicchan",  "Mimi   ",  "Manik  ",  "Lamat  ",  "Muluc  ",
   "Oc     ",  "Chuen  ",  "Eb     ",  "Ben    ",  "Ix     ",
   "Min    ",  "Cib    ",  "Caban  ",  "Eiznab ",  "Caunac "
};

char *haab[] = {  // Mayan haab months
   "Pop   ", "Uo    ", "Zip   ", "Zotz  ",  "Tzec  ", "Xul   ",
   "Yakin ", "Mol   ", "Chen  ", "Yax   ",  "Zac   ", "Ceh   ",
   "Mac   ", "Kankin", "Muan  ", "Pax   ",  "Kayab ", "Cumku ", 
   "Uayeb "
};

char *aztec[] = {  // Aztec months (Tzolkin)
   "Monster",  "Wind   ", "House  ",  "Lizard ",
   "Snake  ",  "Death  ", "Rabbit ",  "Deer   ",
   "Water  ",  "Dog    ", "Monkey ",  "Grass  ",
   "Reed   ",  "Jaguar ",  "Eagle  ", "Vulture",
   "Quake  ",  "Flint  ",  "Rain   ", "Flower "
};

char *aztec_haab[] = {  // Aztec (haab) months
   "Izcalli ",
   "Cuauhitl",   "Tlacaxip",   "Tozozton",
   "Huey Toz",   "Toxcatl ",   "Etzalcua",
   "Tecuilhu",   "Huey Tec",   "Miccailh",
   "Huey Mic",   "Ochpaniz",   "Teotleco",
   "Tepeilhu",   "Quecholl",   "Panquetz",
   "Atemoztl",   "Tititl  ",   
   "Nemontem" 
};


int set_holiday(int index, int m, int d)
{
int i;

   // search the holiday table for the specially calculated event
   // and set the month and day that it happens on in this year
   for(i=0; i<MAX_HOLIDAYS; i++) {
      if(holiday[i].nth == index) {
         if(holiday[i].month) {  // do not enable a message that has been disabled
            holiday[i].month = m;
            holiday[i].day = d;
         }
         return i;
      }
   }
   return (-1);
}

int calendar_count()
{
int start;

   // see how many dates are in the greetings list
   start = 0;
   while(start < MAX_HOLIDAYS) {
      if((holiday[start].month == 0) && (holiday[start].day == 0) && (holiday[start].nth == 0)) break;
      ++start;
   }
   return start;
}

void clear_calendar()
{
int start;

   // clear the greetings list
   start = 0;
   while(start < MAX_HOLIDAYS) {
      holiday[start].month = 0;
      holiday[start].day = 0;
      holiday[start].nth = 0;
      ++start;
   }
   calendar_entries = 0;
}


void read_calendar(char *s, int erase)
{
FILE *cal_file;
int i;
int nth, day, month;
char text[128+1];
int len;
char *buf;
int field;
int text_ptr;
int val;
int sign;
int skipping;
char c;

   // Replace default greetings with data from HEATHER.CAL
   cal_file = fopen(s, "r");
   if(cal_file == 0) return;

   if(erase) clear_calendar();   // clear out the old calendar

   while(fgets(out, sizeof out, cal_file) != NULL) {
      if((out[0] == '*') || (out[0] == ';') && (out[0] != '#') || (out[0] == '/')) {
         if(!strnicmp(out, "#CLEAR", 6)) clear_calendar();
         continue;  // comment line
      }

      field = 0;
      text[0] = 0;
      text_ptr = 0;
      val = 0;
      skipping = 1;
      sign = (+1);

      len = strlen(out);
      for(i=0; i<len; i++) {
         c = out[i];
         if     (c == 0x00) break;
         else if(c == 0x0D) break;
         else if(c == 0x0A) break;
         else if(field < 4) {  // getting a number value
            if     (c == '-') { sign = (-1); skipping=0; }
            else if(c == '+') { sign = (+1);  skipping=0; }
            else if((c >= '0') && (c <= '9')) {
               val = (val*10) + (c-'0');
               skipping = 0;
            }
            else if((c == ' ') || (c == '\t') || (c == ',')) {
               if(skipping) continue;
               ++field;
               if     (field == 1) nth = val*sign;
               else if(field == 2) day = val*sign;
               else if(field == 3) { month = val*sign; ++field; }
               skipping = 1;
               sign = 1;
               val = 0;
            }
            else {  // invalid char in a number, ignore the line
               break;
            }
         }
         else if(((c == ' ') || (c == '\t')) && skipping) {
            continue;
         }
         else {  // get the text string
            text[text_ptr++] = c;
            text[text_ptr] = 0;
            skipping = 0;
            field = 4;
         }
      }
      if(field != 4) continue;

      buf = (char *) calloc(len+1, 1);
      if(buf == 0) break;

      strcpy(buf, text);
      holiday[calendar_entries].nth = nth;
      holiday[calendar_entries].day = day;
      holiday[calendar_entries].month = month;
      holiday[calendar_entries].text = buf;

      if(++calendar_entries >= MAX_HOLIDAYS) break;
   }

   fclose(cal_file);

   calc_greetings();
   show_greetings();
}


void gregorian(double jd)
{
long z;
long w;
long x;
long a,b,c,d,e,f;
double t;

   // convert Julian date to Gregorian
   z = (long) (jd + 0.50);
   w = (long) (((double) z - 1867216.25) / 36524.25);
   x = w / 4;
   a = z + 1 + w - x;
   b = a + 1524;
   c = (long) ((((double) b) - 122.1) / 365.25);
   d = (long) (((double) c) * 365.25);
   e = (long) (((double) (b-d)) / 30.6001);
   f = (long) (((double) e) * 30.6001);

   g_day   = (int) (b - d - f);
   g_month = (int) (e - 1);
   if(g_month > 12) g_month = (int) (e - 13);

   if(g_month < 3) g_year = (int) (c - 4715);
   else            g_year = (int) (c - 4716);

   t = jd - jdate(g_year,g_month,g_day);
   t *= 24.0;

   g_hours = (int) t;         // convert decimal hours to hh:mm:ss
   t = (t - g_hours);
   g_minutes = (int) (t * 60.0);
   t = (t * 60.0) - g_minutes;
   g_seconds = (int) (t * 60.0);
   g_frac = (t * 60.0) - g_seconds;
}

long adjust_mayan(long mayan)
{
   // adjsut mayan date for different correlation factor
   mayan += (MAYAN_CORR-mayan_correlation);  // adjust for new correlation constant
   while(mayan < 0) mayan += (20L*20L*20L*18L*20L);  // make result positive
   return mayan;
}

void get_mayan_date()
{
long mayan;

   mayan = (long) (jdate(pri_year,pri_month,pri_day)-jdate(1618,9,18));
   mayan = adjust_mayan(mayan);

   kin = (int) (mayan % 20L);  mayan /= 20L;
   uinal = (int) (mayan % 18L);  mayan /= 18L;
   tun = (int) (mayan % 20L);  mayan /= 20L;
   katun = (int) (mayan % 20L);  mayan /= 20L;
   baktun = (int) ((mayan % 20L)+1L);  mayan /= 20L;
   pictun = (int) (mayan % 20L);  mayan /= 20L;
}

void adjust_season(double season)
{
int temp_hours;
int temp_minutes;
int temp_seconds;
int temp_day;
int temp_month;
int temp_year;

   // adjust season Julian date for local time zone
   temp_hours   = pri_hours;    // save current time
   temp_minutes = pri_minutes;
   temp_seconds = pri_seconds;
   temp_day     = pri_day;
   temp_month   = pri_month;
   temp_year    = pri_year;

   gregorian(season);           // convert Julian season value to Gregorian

   pri_day     = g_day;         // set current time values to UTC season values
   pri_month   = g_month;
   pri_year    = g_year;
   pri_hours   = g_hours;        
   pri_minutes = g_minutes;
   pri_seconds = g_seconds;

   adjust_tz();                 // convert UTC season values to local time zone
   strcpy(out, time_zone_set?tz_string:(time_flags&0x01)?"UTC":"GPS");

   g_year    = pri_year;        // save season time values
   g_month   = pri_month;
   g_day     = pri_day;
   g_hours   = pri_hours;
   g_minutes = pri_minutes;
   g_seconds = pri_seconds;

   pri_hours   = temp_hours;    // restore the current time
   pri_minutes = temp_minutes;
   pri_seconds = temp_seconds;
   pri_day     = temp_day;
   pri_month   = temp_month;
   pri_year    = temp_year;
}



#define         SMALL_FLOAT     (1e-12)

double sun_position(double j)
{
double n,x,e,l,dl,v;
int i;

    n = (360.0/365.2422) * j;
    i = (int) (n / 360.0);
    n = n - 360.0 * (double) i;
    x = n - 3.762863;
    if(x < 0.0) x += 360.0;
    x *= DEG_TO_RAD;
    e = x;

    i = 0;
    do {
        dl = e - (0.016718*sin(e)) - x;
        e  = e - (dl / (1.0 - (.016718*cos(e))));
        if(++i > 100) break;
    } while (fabs(dl) >= SMALL_FLOAT);

    v = (360.0 / PI) * atan(1.01686011182*tan(e/2));
    l = v + 282.596403;
    i = (int) (l / 360.0);
    l = l - 360.0 * (double) i;
    return l;
}

double moon_position(double j, double ls)
{
double ms,l,mm,n,ev,sms,ae,ec;
int i;

    /* ls = sun_position(j) */
    ms = 0.985647332099*j - 3.762863;
    if(ms < 0) ms += 360.0;
    l = 13.176396*j + 64.975464;
    i = (int) (l / 360.0);
    l = l - (360.0*(double) i);
    if(l < 0) l += 360.0;
    mm = l - 0.1114041*j - 349.383063;
    i = (int) (mm / 360.0);
    mm -= 360.0 * (double) i;
    n = 151.950429 - 0.0529539*j;
    i = (int) (n / 360.0);
    n -= 360.0 * (double) i;
    ev = 1.2739 * sin((2*(l-ls)-mm)*DEG_TO_RAD);
    sms = sin(ms*DEG_TO_RAD);
    ae = 0.1858 * sms;
    mm += ev - ae - 0.37*sms;
    ec = 6.2886 * sin(mm*DEG_TO_RAD);
    l += ev + ec - ae + 0.214*sin(2.0*mm*DEG_TO_RAD);
    l= 0.6583 * sin(2*(l-ls)*DEG_TO_RAD)+l;

    return l;
}

double moon_phase(double j)
{
double ls;
double lm;
double t;

   //
   //  Calculates the phase of the moon at the given julian date.
   //  returns the moon phase as a real number (0-1)
   //

   j  -= 2444238.5;
   ls = sun_position(j);
   lm = moon_position(j, ls);

   t = lm - ls;
   if(t < 0) t += 360.0;
   return (1.0 - cos((lm-ls)*DEG_TO_RAD))/2.0;
}


void calc_moons(int year, int month)
{
double p, last_p;
double jd, last_jd;
int rising;
int blue_moon;
int black_moon;
int hour;
int gd;

   last_jd = jdate(year,month,1);
   last_p  = moon_phase(last_jd);
   rising  = 0;

   blue_moon  = 0;
   black_moon = 0;

   for(gd=1; gd<=dim[month]; gd++) {
      moons[gd] = "";
      for(hour=0; hour<24; hour++) {
         jd = jdate(year,month,gd) + (hour/24.0);
         p = moon_phase(jd);

         gregorian(last_jd);
         g_seconds += (int) (g_frac+0.50);
         if(g_seconds >= 60) { ++g_minutes; g_seconds-=60; }
         if(g_minutes >= 60) { ++g_hours; g_minutes-=60; }

         if(DABS(p-0.50) < 0.02) moons[gd] = "Half Moon";  // really a quarter moon,  but that cramps the watchface

         if(rising > 0) {
            if(p < last_p) {
               if(blue_moon) moons[gd] = "Blue Moon";
               else          moons[gd] = "Full Moon";
               ++blue_moon;
            }
         }
         else if(rising < 0) {
            if(p > last_p) {
               if(black_moon) moons[gd] = "Black Moon";
               else           moons[gd] = "New Moon";
               ++black_moon;
            }
         }

         if     (p > last_p) rising = (+1);
         else if(p < last_p) rising = (-1);

         last_p = p;
         last_jd = jd;
      }
   }
}

void easter(int y)
{
int g, c, d, h, k, i, j;
int m;
double jd;

    // Oudin's algorithm for calculating easter
    g = y % 19;
    c = y / 100;
    d = c - c/4;
    h = (d - ((8*c+13)/25) + (19*g) + 15) % 30;
    k = h / 28;
    i = h - k*(1 - k*(29/(h+1))*(21-g)/11);
    j = (y + y/4 + i + 2 - d) % 7;
    d = 28 + i - j;

    if(d > 31) {
       d -= 31;
       m = 4;
    }
    else m = 3;

    set_holiday(EASTER, m, d);

    // calculate Ash Wednesday
    jd = jdate(y, m, d);
    gregorian(jd-46.0);
    set_holiday(ASH, g_month, g_day);

    gregorian(jd-47.0);
    set_holiday(MARDI_GRAS, g_month, g_day);

    // calculate Palm Sunday
    gregorian(jd-7.0);
    set_holiday(PALM_SUNDAY, g_month, g_day);

    // calculate Good Friday
    gregorian(jd-2.0);
    set_holiday(GOOD_FRIDAY, g_month, g_day);
}

double rosh_jd(int y)
{
int g;
double r;
double jd;
int i, j;
int day, month;

   // return Julian date of Rosh Hashanha
   g = (y % 19) + 1;
   j = (12 * g) % 19;
   i = (y/100) - (y/400) - 2;
   r = (double) (int) (y%4);
   r = (765433.0/492480.0*(double) j) + (r/4.0) - ((313.0 * ((double) y) + 89081.0)/98496.0);
   r += (double) i;
// r = 6.057778996 + (1.554241797*(double) j) + 0.25*r - 0.003177794*(double)y,

   day = (int) r;
   r -= (double) day;
   month = 9;
   if(day > 30) { day-=30; ++month; }

   i = day_of_week(day, month, y);
   jd = jdate(y, month, day);

   // apply the postponements
   if((i == 0) || (i == 3) || (i == 5)) {
      jd += 1.0;
   }
   else if(i == 1) {
      if((r >= 23269.0/25920.0) && (j > 6)) jd += 1.0;
   }
   else if(i == 2) {
      if((r >= 1367.0/2160.0) && (j > 6)) jd += 2.0;
   }
   return jd;
}

void init_hebrew(int year)
{
double t;
double rosh;

   rosh = rosh_jd(year);        // our reference is Rosh Hashahna 
                                // once you know Rosh Hashahna,  all is revealed

   t = rosh_jd(year+1) - rosh;  // number of days in the Jewish year

   jtimes[9] = rosh;
   jtimes[10] = jtimes[9]+30.0; // che
   if((DABS(t-355.0) < 0.1) || (DABS(t-385.0) < 0.1)) {  // shalim years
      rosh += 1.0;  // kis - the month of Cheshvan has an extra day in shalim years
      jtimes[11] = jtimes[10]+30.0;
   }
   else jtimes[11] = jtimes[10]+29.0;

   if((DABS(t-353.0) < 0.1) || (DABS(t-383.0) < 0.1)) {  // hasser years
      jtimes[12] = jtimes[11]+29.0; // tev - Kislev has one less day in hasser years
   }
   else jtimes[12] = jtimes[11]+30.0;
   jtimes[13] = jtimes[12]+29.0;    // shv
   jtimes[14] = jtimes[13]+30.0;    // end of shv
   jtimes[8]  = jtimes[9]-29.0;     // elu
   jtimes[7]  = jtimes[8]-30.0;     // av
   jtimes[6]  = jtimes[7]-29.0;     // tam
   jtimes[5]  = jtimes[6]-30.0;     // siv
   jtimes[4]  = jtimes[5]-29.0;     // iyr
   jtimes[3]  = jtimes[4]-30.0;     // nis
   jtimes[2]  = jtimes[3]-29.0;     // adr
   if(DABS(t) < 370.0) {            // previous year not a leap year
      jtimes[1]  = jtimes[2]-30.0;  // Adar I in leap years
      jmonths[1] = "Ad1";
      jmonths[2] = "Ad2";
   }
   else {                           // previous year was a leap year, this one is not
      jtimes[1] = 0.0F;             // no Adar I
      jtimes[1]  = jtimes[2]-30.0;     
      jmonths[1] = "Shv";
      jmonths[2] = "Adr";
   }
   jtimes[0] = jtimes[1]-29.0;
}

void islam_to_greg(int h, int m, int d)
{
long jd, al, be, b;
long c, d1, e1;

    // convert islamic date to gregorian
    long n = (59*(m-1)+1)/2 + d;
    long q = h / 30;
    long r = h - q*30;
    long a = (11*r + 3) / 30;
    long w = 404*q + 354*r + 208+a;
    long q1 = w / 1461;
    long q2 = w - 1461*q1;
    long g = 621 + 4*(7*q+q1);
    long k = (long) (q2 / 365.2422);
    long e = (long) (k * 365.2422);
    long j = q2 - e + n - 1;
    long x = g + k;
    
    if ((j > 366) && !(x&3)) { j -= 366; ++x; }
    if ((j > 365) && (x&3))  { j -= 365; ++x; }
    
    jd = (long) (365.25*(x-1));
    jd += 1721423 + j;
    al = (long) ((jd - 1867216.25) / 36524.25);
    be = jd + 1 + al - al/4;
    if (jd < 2299161) be = jd;
    b = be + 1524;
    c = (long) ((b - 122.1) / 365.25);
    d1 = (long) (365.25 * c);
    e1 = (long) ((b - d1) / 30.6001);
    
    g_day = b - d1;
    g_day -= (long) floor(30.6001*e1);
    if(e1 < 14) g_month = e1 - 1;
    else        g_month = e1 - 13;
    
    if(g_month > 2) g_year = c - 4716;
    else            g_year = c - 4715;
    return;
}

int get_islam_day(int yy, int mm, int dd)
{
// islam_to_greg(yy-581, mm, dd);
// if(g_year == yy) return 581;

   islam_to_greg(yy-580, mm, dd);
   if(g_year == yy) return 580;

   islam_to_greg(yy-579, mm, dd);
   if(g_year == yy) return 579; // not in this year, try next year

   islam_to_greg(yy-578, mm, dd);
   if(g_year == yy) return 578; // not in this year, try next year

   return 0;
}

void init_islamic(int year)
{
int mm;
int iyear;

   // calculate start of each islamic month
   iyear = year - 578;
   while(iyear > 0) {  // find islamic year that starts on or before this gregorian year
      islam_to_greg(iyear, 1, 1);
      if(g_year < pri_year) break;
      else if((g_year == pri_year) && (g_month == 1) && (g_day == 1)) break;
      --iyear;
   }

   for(mm=0; mm<=35; mm++) {  // find start of the next 36 islam months
      islam_to_greg(iyear+(mm/12), (mm%12)+1, 1);
      myears[mm] = iyear+(mm/12);
      mtimes[mm] = jdate(g_year, g_month, g_day);
   }
}

void init_druid(int year)
{
double jd;
int mm;

   jd = jdate(year-1, 12, 24);  // Druid year starts on 24 Dec
   jd += (double) druid_epoch;  // kludge to allow tweak to start of calendar
   for(mm=0; mm<=13; mm++) {    // the Juilan time of the start of each month
      dyears[mm] = year;        // assume druid year is the Gregorian year
      dtimes[mm] = jd;          // ... that holds most of the Druid year
      jd += 28.0;               // Druid months are 28 days long
   }

   dtimes[14] = jdate(year, 12, 24);
   dtimes[14] += (double) druid_epoch; 
   dyears[14] = year+1;
   dtimes[15] = dtimes[14]+28.0;
   dyears[15] = year+1;
}


void init_persian(int year)
{
double jd;
int leap;
int mm;

// gregorian(spring-365.24238426);    // time of spring in GMT of last year;
// jd = jdate(g_year, g_month, g_day);   // jdate of start of spring
// if((jd - (3.5/24.0) + (12.0/24.0)) > spring) {  // it's after noon in Tehran
//    jd += 1.0;  //!!! kludge - we should be using solar noon in Tehran
// }

   leap = year-621-1;               // start calculating from last year
   leap %= 33;                      // leap years on 33 year cycles
   if     (leap == 1)  leap = 20;   // leap years start on 20 March
   else if(leap == 5)  leap = 20;
   else if(leap == 9)  leap = 20;
   else if(leap == 13) leap = 20;
   else if(leap == 17) leap = 20;
   else if(leap == 22) leap = 20;
   else if(leap == 26) leap = 20;
   else if(leap == 30) leap = 20;
   else                leap = 21;

   jd = jdate(year-1, 3, leap);  // first day of the previous persian year

   ptimes[0] = jd;
   pyears[0] = year-621-1;

   for(mm=0+1; mm<=36; mm++) { // calculate next 36 months
      if     ((mm%12) <= 6)  ptimes[mm] = ptimes[mm-1] + 31.0;
      else if((mm%12) <= 11) ptimes[mm] = ptimes[mm-1] + 30.0;
      else if((mm%12) == 12) {  // first month of the next year
         if(leap == 20)  ptimes[mm] = ptimes[mm-1] + 30.0;  // set length of the previous month
         else            ptimes[mm] = ptimes[mm-1] + 29.0;
      }
      pyears[mm] = (year-621-1) + (mm/12);
   }
}

void init_indian(int year)
{
   // India civil calendar
   itimes[0] = jdate(year-1, 12, 22);  // start of Pau the year before
   itimes[1] = itimes[0] + 30.0;           // Mag
   itimes[2] = itimes[1] + 30.0;           // Pha
   itimes[3] = itimes[2] + 30.0;           // start of Cai
   if(leap_year(year-79+78)) itimes[3] -= 1.0;  // adjust for leap years
   itimes[4] = itimes[3] + 31.0;           // Vai
   itimes[5] = itimes[4] + 31.0;           // Jya
   itimes[6] = itimes[5] + 31.0;           // Asa
   itimes[7] = itimes[6] + 31.0;           // Sra
   itimes[8] = itimes[7] + 31.0;           // Bha
   itimes[9] = itimes[8] + 31.0;           // Asv

   itimes[10] = itimes[9]  + 30.0;         // Kar
   itimes[11] = itimes[10] + 30.0;         // Agr
   itimes[12] = itimes[11] + 30.0;         // Pau
   itimes[13] = itimes[12] + 30.0;         // Mag
}

int load_china_year(int year, int mm)
{
int i;
int index;
int month, months;
double days;

   index = year - 2009;
   if(china_data[index].leap_month) months = 13;
   else                             months = 12;
   year = year + 4635 - 1998 + 60;  // note +60 years is common in the US

   ctimes[mm] = china_data[index].jd;
   month = 1;
   for(i=mm; i<mm+months; i++) {
      cyear[i]  = year;
      cmonth[i] = month;
      if(china_data[index].leap_month) {  // this year has a leap month
         if((i-mm+1) != china_data[index].leap_month) ++month;
      }
      else {
         ++month;
      }

      if(china_data[index].month_mask & (1 << (i-mm))) days = 30.0;
      else                                             days = 29.0;
      ctimes[i+1] = ctimes[i] + days;
   }
   if(china_data[index].leap_month) {
      cmonth[china_data[index].leap_month] *= (-1);
   }

   return months;
}

void init_chinese(int year)
{
int month;

   if((year < 2010) || (year > 2100)) return;
   month = 0;
   month += load_china_year(year-1, month);
   month += load_china_year(year+0, month);
   month += load_china_year(year+1, month);
//FILE *f;
//int i;
//f = fopen("abcd", "w");
//for(i=0; i<40; i++) {
//   fprintf(f, "%.1f %d %d\n", ctimes[i], cyear[i], cmonth[i]);
//}
//fclose(f);
}


double chinese_ny()
{
double jd;
double p, last_p;
double li_chun;
int hour;
int new_moons;
int rising;
double first_new;
  
   // calculate the date of the Chinese new year as the closest new moon
   // to the midpoint between winter and spring (li chun)

   if((pri_year >= 2009) && (pri_year <= 2100)) {  // we cheat, get date from table
      return china_data[pri_year-2009].jd;
   }

   jd = winter - 365.24274306;      // previous winter solstice
   li_chun = (spring + jd) / 2.0;   // li chun is midway between winter and spring

   rising  = 0;
   new_moons = 0;
   first_new = 0.0;
   for(hour= (-24*32); hour<24*32; hour++) {  // look for new moon closet to li chun
      jd = li_chun + ((double) hour)/24.0;
      if(new_moons == 0) last_p = moon_phase(jd);
      new_moons = 1;
      p = moon_phase(jd);

      if((rising < 0) && (p > last_p)) {  // new moon
         if(jd < li_chun) first_new = jd; // before li_chun
         else if(jd > li_chun) break;     // first moon after li chun
      }

      if     (p > last_p) rising = (+1);
      else if(p < last_p) rising = (-1);
      last_p = p;
   }

   if     (pri_year == 2034) p = jd; 
   else if(pri_year == 2053) p = jd;
   else if((jd-li_chun) > (li_chun - first_new)) p = first_new;
   else                                          p = jd;

   p += 7.0/24.0;   // china time ahead of GMT
   return p;
}

int tax_day(int year, int month, int day)
{
int gd;

   // return next tax due day - first weekday on or after the 15th of a month
   gd = day;
   if     (day_of_week(day, month, year) == 6) gd += 2;  // the 15th is on Sat
   else if(day_of_week(day, month, year) == 0) gd += 1;  // the 15th is on Sun
   return gd;
}

void calc_greetings()
{
double t;
int gd;
int i;
double rosh, po, purim;

   // calculate a few useful greetings dates 

   // convert Julian times to Gregorian and update the greetings table
   adjust_season(spring);
   sprintf(spring_s, "Spring starts today around %02d:%02d %s", g_hours,g_minutes, out);
   i = set_holiday(SPRING, g_month, g_day);
   if(i >= 0) holiday[i].text = &spring_s[0];

   adjust_season(summer);
   sprintf(summer_s, "Summer starts today around %02d:%02d %s", g_hours,g_minutes, out);
   i = set_holiday(SUMMER, g_month, g_day);
   if(i >= 0) holiday[i].text = &summer_s[0];

   adjust_season(fall);
   sprintf(autumn_s, "Autumn starts today around %02d:%02d %s", g_hours,g_minutes,out);
   i = set_holiday(AUTUMN, g_month, g_day);
   if(i >= 0) holiday[i].text = &autumn_s[0];

   adjust_season(winter);
   sprintf(winter_s, "Winter starts today around %02d:%02d %s", g_hours,g_minutes,out);
   i = set_holiday(WINTER, g_month, g_day);
   if(i >= 0) holiday[i].text = &winter_s[0];

   t = chinese_ny();
   gregorian(t);
   sprintf(china_year, "Happy Chinese New Year!  It's the Year of the %s.", zodiac[pri_year%12]);
   i = set_holiday(CHINA_NY, g_month, g_day);
   if(i >= 0) holiday[i].text = &china_year[0];

   easter(pri_year);       // a few easter based holidays

   if(dst_enabled()) {    // daylight savings time days
      t = jdate(pri_year, dst_start_month, dst_start_day) - 1.0;
      gregorian(t);
      set_holiday(CLOCK_FWD, g_month, g_day);
      t = jdate(pri_year, dst_end_month, dst_end_day) - 1.0;
      gregorian(t);
      set_holiday(CLOCK_BACK, g_month, g_day);
   }
   else {  // disable DST message
      set_holiday(CLOCK_FWD, 3, 0);
      set_holiday(CLOCK_BACK, 11, 0);
   }

   // grandprents day
   gd = nth_dow_in(1, 1, 9, pri_year);  // first Monday in sept = Labor Day
   t = jdate(pri_year, 9, gd) + 6.0;    // the next sunday
   gregorian(t);
   set_holiday(GRANNY, g_month, g_day);

   // election day
   gd = nth_dow_in(1, 1, 11, pri_year) + 1;
   set_holiday(ELECTION, 11, gd);

   // tax days
   gd = tax_day(pri_year,    4,  15);
   set_holiday(TAX_DAY,      4,  gd);
   gd = tax_day(pri_year,    6,  15);
   set_holiday(Q2_TAXES,     6,  gd);
   gd = tax_day(pri_year,    9,  15);
   set_holiday(Q3_TAXES,     9,  gd);
   gd = tax_day(pri_year,    1,  15);
   set_holiday(Q4_TAXES,     1,  gd);
   gd = tax_day(pri_year,    10, 15);
   set_holiday(FINAL_TAXES,  10, gd);

   // calculate approximate Islamic holidays (approximate since they can only
   // be offically determined by visual observation of the crescent moon)
   if(get_islam_day(year, 1, 1)) {    // Al-Hijra - new year
      set_holiday(AL_HIJRA, g_month, g_day);
   }

   if(get_islam_day(year, 1, 10)) {   // Asura
      set_holiday(ASURA, g_month, g_day);
   }

   if(get_islam_day(year, 9, 1)) {    // Ramadan - new year
      set_holiday(RAMADAN, g_month, g_day);
   }

   if(get_islam_day(year, 12, 10)) {  // Eid al Addha
      set_holiday(EID, g_month, g_day);
   }

   // calculate some jewish holidays
   rosh = rosh_jd(year);             // our reference is Rosh Hashahna 
                                     // once you know Rosh Hashahna,  all is revealed

   gregorian(rosh-1.0);              // Rosh Hashahna (party starts the night before)
   set_holiday(ROSH, g_month, g_day);

   gregorian(rosh+9.0-1.0);          // Yom Kipper is nine days after Rosh Hashanah
   set_holiday(YOM, g_month, g_day);

   po = rosh - 29.0 - 30.0 - 29.0 - 30.0 - 29.0 - 30.0;  // Julian date of Nisan 1
   purim = po - 29.0 + 13.0;   // Purim is Adar 14

   purim -= 1.0;               // start observance the night before
   gregorian(purim);
   set_holiday(PURIM, g_month, g_day);

   po += 14.0;                 // Passover is Nisan 15
   po -= 1.0;                  // but starts the night before
   gregorian(po);              // convert Julian passover to Gregorian terms
   set_holiday(PASSOVER, g_month, g_day);

   rosh += 30.0 + 29.0 + 24.0;   // Hanukkah is Kislev 25
   rosh -= 1.0;                  // start observance the night before
   gregorian(rosh);
   set_holiday(HANUKKAH, g_month, g_day);

   t = jtimes[9] + 15.0 - 1.0;   // 15 Tishrei
// rosh -= 1.0;                  // start observance the night before
   gregorian(t);
   set_holiday(SUKKOT, g_month, g_day);

   t = jtimes[5] + 6.0 - 1.0;    // 6 Sivan
// rosh -= 1.0;                  // start observance the night before
   gregorian(t);
   set_holiday(SHAVUOT, g_month, g_day);

// get_mayan_date();  // Mayan Tzolkin new year
// t = (360 - ((uinal * 20) + kin)) % 360;
// t += jdate(pri_year, pri_month, pri_day);
// gregorian(t);
// set_holiday(MAYAN_NY, g_month, g_day);

   t = jdate(2010,5,27);      // t = known Tzolkin new year
   t += (float) (MAYAN_CORR-mayan_correlation);
   po = jdate(pri_year,pri_month,pri_day);  // today
   g_year = 2010;
   while(g_year <= (pri_year+2)) {  // find Mayan Tzolkin new year that starts on or after today
      if(t >= po) break;
      t += 260.0;
      ++g_year;
   }
   gregorian(t);
   set_holiday(MAYAN_NY, g_month, g_day);

   t = jdate(2012, 12, 20);
   t += (float) (MAYAN_CORR-mayan_correlation);
   gregorian(t);
   if(g_year == pri_year) set_holiday(DOOMSDAY, g_month, g_day);
   else                   set_holiday(DOOMSDAY, 0, 0);

   t = jdate(2009,10,7);      // t = known Aztec new year
   t += (double) aztec_epoch;
   po = jdate(pri_year,pri_month,pri_day);  // today
   g_year = 2009;
   while(g_year <= (pri_year+2)) {  // find Aztec new year that starts on or after today
      if(t >= po) break;
      t += 365.0;
      ++g_year;
   }
   gregorian(t);
   set_holiday(AZTEC_NY, g_month, g_day);

   if(pri_year == 2038) set_holiday(UNIX_CLOCK, 1, 18);
   else                 set_holiday(UNIX_CLOCK, 0, 0);
}

void calc_seasons()
{
double t;

   // calculate/Julian date/time when each season starts
   spring = jdate(2010,3,20)  + jtime(17, 32, 00);
   spring += ((double) (pri_year-2010)) * 365.24238426;

   summer = jdate(2010,6,21)  + jtime(11, 28, 00);
   summer += ((double) (pri_year-2010)) * 365.24163194;

   fall   = jdate(2010,9,22)  + jtime(3,  9, 00);
   fall   += ((double) (pri_year-2010)) * 365.24202546;

   winter = jdate(2010,12,21) + jtime(23, 38, 00);
   winter += ((double) (pri_year-2010)) * 365.24274306;

   if(lat < 0.0) {   // down under the world is bass ackwards
      t = fall;
      fall = spring;
      spring = t;

      t = winter;
      winter = summer;
      summer = t;
   }
}

int show_greetings()
{
int i;
int day;

   // see if today is a day of greetings
   if(no_greetings) return 0;         // user is a grinch...
   if(title_type == USER) return 0;   // don't override the user's title

   for(i=0; i<calendar_entries; i++) { // is it a special day?
      if(holiday[i].month == 0) continue;
      if(pri_month != holiday[i].month) continue;

      day = holiday[i].day;
      if((holiday[i].nth != 0) && (holiday[i].nth < 100)) {
         day = nth_dow_in(holiday[i].nth, day, pri_month, pri_year);
      }
      if(day != pri_day) continue;
      sprintf(plot_title, "%s", holiday[i].text);
      title_type = GREETING;
      return day;
   }

   if(title_type == GREETING) {  // erase old greeting
      plot_title[0] = 0;
      title_type = NONE;
   }
   return 0;
}

void setup_calendars()
{
  #ifdef GEN_CHINA_DATA
     void load_china_data();
     void load_hk_data();
     load_china_data();              // process data from projectpluto.com "chinese.dat"
     load_hk_data();                 // process data from Hong Kong Observatory files
  #endif

   calc_seasons();                   // calc jdates of the equinoxes
   calc_moons(pri_year, pri_month);  // moon phases for the current month

   init_chinese(pri_year);           // setup chinese calendar
   init_hebrew(pri_year);            // setup Hebrew calendar (oy, vey! it be complicated)
   init_islamic(pri_year);           // setup (appoximate) Islamic calendar
   init_persian(pri_year);           // setup Persian/Afghan/Kurdish calendars
   init_indian(pri_year);            // setup Indian civil calendar
   init_druid(pri_year);             // setup pseudo-Druid calendar

   calc_greetings();                 // calculate seasonal holidays and insert into table
   show_greetings();                 // display any current greeting
}
#endif   // GREET_STUFF



char *fmt_date()
{
static char date[32];
double jd;
long x;
int month;
int day;
int year;
char date_flag;

   date_flag = ' ';

#ifdef GREET_STUFF
   if(alt_calendar == HEBREW) {  // convert current date to Jewish Standard Time
      jd = jdate(pri_year,pri_month,pri_day);
      for(month=0; month<=13; month++) {
         if(jtimes[month] == 0.0) continue;  // its not a leap year
         if((jd >= jtimes[month]) && (jd < jtimes[month+1])) {
            day = ((int) (jd - jtimes[month])) + 1;
            year = 5770 + (pri_year-2010);
            if(month >= 9) ++year;
            sprintf(date, "%02d %s %04d%c", day, jmonths[month], year, date_flag);
            return &date[0];
         }
      }
   }
   else if(alt_calendar == INDIAN) {  // convert date to Indian civil calendar
      jd = jdate(pri_year,pri_month,pri_day);
      for(month=0; month<=12; month++) {
         if((jd >= itimes[month]) && (jd < itimes[month+1])) {
            day = ((int) (jd - itimes[month])) + 1;
            year = (pri_year-78-1);
            if(jd >= itimes[3]) ++year;
            sprintf(date, "%02d %s %04d%c", day, imonths[month], year, date_flag); 
            return &date[0];
         }
      }
   }
   else if(alt_calendar == ISLAMIC) {
      jd = jdate(pri_year,pri_month,pri_day);
      for(month=0; month<36; month++) {
         if((jd >= mtimes[month]) && (jd < mtimes[month+1])) {
            day = ((int) (jd - mtimes[month])) + 1;
            year = myears[month];
            sprintf(date, "%02d %s %04d%c", day, mmonths[month], year, date_flag); 
            return &date[0];
         }
      }
   }
   else if(alt_calendar == CHINESE) {
      jd = jdate(pri_year,pri_month,pri_day);
      for(month=0; month<36; month++) {
         if((jd >= ctimes[month]) && (jd < ctimes[month+1])) {
            day = ((int) (jd - ctimes[month])) + 1;
            year = cyear[month] + (int) chinese_epoch;
            if(cmonth[month] < 0)  sprintf(out, "%d*", 0-cmonth[month]);
            else                   sprintf(out, "%d", cmonth[month]);
            sprintf(date, "%04d/%s/%02d%c   ", year, out, day, date_flag); 
            return &date[0];
         }
      }
   }
   else if((alt_calendar == PERSIAN) || (alt_calendar == AFGHAN) || (alt_calendar == KURDISH)) {
      jd = jdate(pri_year,pri_month,pri_day);
      for(month=0; month<36; month++) {
         if((jd >= ptimes[month]) && (jd < ptimes[month+1])) {
            day = ((int) (jd - ptimes[month])) + 1;
            year = pyears[month];
            if     (alt_calendar == PERSIAN) sprintf(date, "%02d %s %04d%c", day, pmonths[month], year, date_flag); 
            else if(alt_calendar == KURDISH) sprintf(date, "%02d %s %04d%c", day, kmonths[month], year, date_flag); 
            else                             sprintf(date, "%02d %s %04d%c", day, amonths[month], year, date_flag); 
            return &date[0];
         }
      }
   }
   else if(alt_calendar == DRUID) {  // pseudo-Druid calendar
      jd = jdate(pri_year,pri_month,pri_day);
      for(month=0; month<15; month++) {
         if((jd >= dtimes[month]) && (jd < dtimes[month+1])) {
            day = ((int) (jd - dtimes[month])) + 1;
            year = dyears[month];
//          sprintf(date, "%02d %s %04d", day, dmonths[month], year);
            sprintf(date, "%02d %s%c", day, dmonths[month], date_flag); 
            return &date[0];
         }
      }
   }
   else if(alt_calendar == MJD) {
      jd = jdate(pri_year,pri_month,pri_day) - 2400000.5;
      sprintf(date, "MJD:  %5ld%c", (long) (jd+0.5), date_flag); 
      return &date[0];
   }
   else if(alt_calendar == JULIAN) {
      jd = jdate(pri_year,pri_month,pri_day);
      sprintf(date, "JD%9.1f%c", jd, date_flag); 
      return &date[0];
   }
   else if(alt_calendar == ISO) {
      jd = jdate(pri_year,pri_month,pri_day)-jdate(pri_year,1,1)+1;
      sprintf(date, "%04d-%03d%c   ", pri_year,(int)jd, date_flag); 
      return &date[0];
   }
   else if(alt_calendar == MAYAN) {  // long count
      get_mayan_date();
      sprintf(date, "%02d.%02d.%02d.%02d%c", katun,tun,uinal,kin, date_flag); 
      return &date[0];
   }
   else if(alt_calendar == HAAB) {
      x = (long) (jdate(pri_year, pri_month, pri_day) - jdate(2009, 4, 3));
      x = adjust_mayan(x);
      x = (x % 365L);
      day = (int) (x % 20L);
      month = (int) ((x / 20L) % 19L);
      sprintf(date, "%02d %s%c  ", day, haab[month], date_flag); 
      return &date[0];
   }
   else if(alt_calendar == TZOLKIN) {
      get_mayan_date();
      x = (long) (jdate(pri_year, pri_month, pri_day) - jdate(2001, 4, 3));
      x = adjust_mayan(x);
      x %= 13L;
      day = (int) (x+1);
      sprintf(date, "%02d %s%c ", day, tzolkin[kin], date_flag); 
      return &date[0];
   }
   else if(alt_calendar == AZTEC) {  // Tzolkin type dates
      x = (long) (jdate(pri_year, pri_month, pri_day) - jdate(2009,9,9));
      x += aztec_epoch;
      x %= (260L);
      day   = (int) (x % 13);
      month = (int) (x % 20L);
      sprintf(date, "%02d %s%c ", day+1, aztec[month], date_flag); 
      return &date[0];
   }
   else if(alt_calendar == AZTEC_HAAB) {  // Aztec haab style dates
      x = (long) (jdate(pri_year, pri_month, pri_day) - jdate(2009, 10, 7));
      x += aztec_epoch;
      x = (x % 365L);
      day = (int) (x % 20L);
      month = (int) ((x / 20L) % 19L);
      sprintf(date, "%02d %s%c  ", day+1, aztec_haab[month], date_flag); 
      return &date[0];
   }
#endif

   sprintf(date, "%02d %s %04d%c", pri_day, months[pri_month], pri_year, date_flag);
   return &date[0];
}



#ifdef DIGITAL_CLOCK
//
//  vector char stuff for showing big digital clock
//
#define PROGMEM
#define VCHAR_W  8 // elemental width and height of character pattern vectors
#define VCHAR_H  8 
u08 vg_2B[] PROGMEM = { 0x03,0xC3, 0x21,0xAD };
u08 vg_2C[] PROGMEM = { 0x12,0xA6, 0x25,0xAE };
u08 vg_2D[] PROGMEM = { 0x03,0xCB };
u08 vg_2E[] PROGMEM = { 0x25,0xAE };
u08 vg_2F[] PROGMEM = { 0x06,0xD8 };
u08 vg_30[] PROGMEM = { 0x01,0x90,0xC0,0xD1,0xD5, 0x01,0x85,0x96,0xC6,0xD5, 0x05,0xD9 };
u08 vg_31[] PROGMEM = { 0x11,0xA0,0xA6, 0x16,0xBE };
u08 vg_32[] PROGMEM = { 0x01,0x90,0xB0,0xC1,0xC2, 0x05,0xA3,0xB3,0xC2, 0x05,0x86,0xCE };
u08 vg_33[] PROGMEM = { 0x01,0x90,0xB0,0xC1,0xC2, 0x05,0x96,0xB6,0xC5, 0x23,0xB3,0xC4,0xC5,
                        0x33,0xCA };
u08 vg_34[] PROGMEM = { 0x03,0xB0,0xC0,0xC6, 0x03,0x84,0xDC };
u08 vg_35[] PROGMEM = { 0x00,0xC0, 0x00,0x82,0xB2,0xC3,0xC5, 0x05,0x96,0xB6,0xCD };
u08 vg_36[] PROGMEM = { 0x02,0xA0,0xB0, 0x02,0x85,0x96,0xB6,0xC5, 0x03,0xB3,0xC4,0xCD };
u08 vg_37[] PROGMEM = { 0x00,0xC0,0xC2, 0x00,0x81, 0x24,0xC2, 0x24,0xAE };
u08 vg_38[] PROGMEM = { 0x01,0x90,0xB0,0xC1,0xC2, 0x01,0x82,0x93,0xB3,0xC4,0xC5,
                        0x04,0x93, 0x04,0x85,0x96,0xB6,0xC5, 0x33,0xCA };
u08 vg_39[] PROGMEM = { 0x01,0x90,0xB0,0xC1,0xC4, 0x01,0x82,0x93,0xC3, 0x16,0xA6,0xCC };
u08 vg_3A[] PROGMEM = { 0x21,0xA2, 0x25,0xAE };

u08 *vgen[] = {  //  + , - . / 0..9 and :
   &vg_2B[0],
   &vg_2C[0],
   &vg_2D[0],
   &vg_2E[0],
   &vg_2F[0],
   &vg_30[0],
   &vg_31[0],
   &vg_32[0],
   &vg_33[0],
   &vg_34[0],
   &vg_35[0],
   &vg_36[0],
   &vg_37[0],
   &vg_38[0],
   &vg_39[0],
   &vg_3A[0]
};

void vchar_erase(int x, int y)
{
int height;

   height = VCHAR_SCALE;

   #ifdef WIN_VFX
      VFX_io_done = 1;
      VFX_rectangle_fill(stage, x,y, x+VCharWidth-1+VCharThickness,y+VCharHeight-1+VCharThickness, LD_DRAW, RGB_TRIPLET(0,0,0));
   #endif

   #ifdef DOS
      if(TEXT_HEIGHT <= 8) height *= 2;
      height *= TEXT_HEIGHT;
//    dos_erase(x,y, x+TEXT_WIDTH*VCHAR_SCALE-1+VCharThickness,y+height-1);
      dos_erase(x,y, x+VCharWidth-1+VCharThickness,y+VCharHeight-1);
   #endif
}


void vchar(int xoffset,int yoffset, u08 erase, u08 color, u08 c)  // draw a vector character
{ 
int x1,y1, x2,y2;  
u08 VByte;
u08 *VIndex;  // char gen table offset

   if(erase) vchar_erase(xoffset, yoffset);
   if(c == ' ') return;
   if((c < 0x2B) || (c > ':')) return;

   x2 = y2 = 0;

   VIndex = vgen[c-0x2B];  // pointer to strokes for the char
  
   while(1) {  // draw the character strokes
      VByte = *VIndex;
      if(VByte == 0xFF) break;
      ++VIndex;

      x1 = (VByte >> 4) & 0x07;
      y1 = (VByte & 0x07);

      x1 *= VCharWidth;
      if((c >= 0xB0) && (c <= 0xDF)) {  // make sure line drawing chars touch
         if(((VByte>>4) & 0x07) == (VCHAR_W-1)) x1 += (VCharWidth-1);
      }
      x1 /= VCHAR_W;

      y1 *= VCharHeight;
      if((c >= 0xB0) && (c <= 0xDF)) { // make sure line drawing chars touch
         if((VByte & 0x07) == (VCHAR_H-1)) y1 += (VCharHeight-1);
      }
      y1 /= VCHAR_H;
   
      if((VByte & 0x80) == 0x00) {  // move to point and draw a dot
         x2 = x1;   // we do dots as a single point line
         y2 = y1;
      }
       
      thick_line(x1+xoffset,y1+yoffset, x2+xoffset,y2+yoffset, color, VCharThickness);

      if(VByte & 0x08) break;  // end of list

      x2 = x1;  // prepare for next stroke in the character
      y2 = y1;
   }
}  

void reset_vstring()
{
int i;

   // cause all chars in the vstring to be redrawn
   for(i=0; i<VSTRING_LEN; i++) last_vstring[i] = 0;
}


void vchar_string(int row, int col, u08 color, char *s)
{
u08 c;
int i;

   #ifdef DOS
      // WINDOWS text mode is actually 640x480 graphics, so vchars do work
      if(text_mode) return;
   #endif

   row *= TEXT_HEIGHT;  //!!!!!!
   col *= TEXT_WIDTH;
   i = 0;
   while(c = *s++) {  // to save time,  we only draw the chars that changed
      if(c != last_vstring[i]) {
         vchar(col+TEXT_X_MARGIN, row+TEXT_Y_MARGIN, 1, color, c);
         last_vstring[i] = c;
      }
      if(i<(VSTRING_LEN-1)) ++i;
      col += VCharWidth;
   }
}

int show_digital_clock(int row, int col)
{
int time_exit;
COORD time_row, time_col;
int width, height;
int top_line;
#define TIME_CHARS 8

   // show the big digital clock if we can find a place for it
 
   time_exit = 0;     // assume there is room for the sat info and clock
   if(full_circle) {
      if(plot_watch || plot_signals || plot_azel) return 1;
      if(plot_lla && zoom_lla) return 1;
      VCHAR_SCALE = ((SCREEN_WIDTH/8) / 10);
      time_col = ((SCREEN_WIDTH-((8*VCHAR_SCALE)*8)) / TEXT_WIDTH) / 2;
      time_row = ((SCREEN_HEIGHT/TEXT_HEIGHT)/2) - ((VCHAR_SCALE*16)/(2*TEXT_HEIGHT));
time_row += (VCHAR_SCALE*16/2)/TEXT_HEIGHT;
   }
   else if(all_adevs) {
      if(small_font == 2) VCHAR_SCALE = 5;  // DOS 8x8 font
      else                VCHAR_SCALE = 6;

      if(WIDE_SCREEN) {  // center digital clock over right hand adev tables
         width = VCharWidth;
      }
      else {   // center digital clock in the upper right hand corner space
         width = ((SCREEN_WIDTH/TEXT_WIDTH)-col)*TEXT_WIDTH;
         width -= (TIME_CHARS * VCharWidth);
         width /= 2;
      }
      time_col = col + (width/TEXT_WIDTH);
      time_row = row;
   }
   else if(0 && (TEXT_HEIGHT >= 16) && (SCREEN_WIDTH >= 1900) && (plot_lla == 0) && ebolt) { 
      time_col = (AZEL_COL + AZEL_SIZE + TEXT_WIDTH-1) / TEXT_WIDTH;
      time_col += 4;
      time_row = 2;
      if(SCREEN_WIDTH >= 1900) VCHAR_SCALE = 7;
      else VCHAR_SCALE = 5;
   }
   else {  // clock goes in sat info area
      top_line = (row+1+temp_sats);
      width = (INFO_COL - TIME_CHARS) * TEXT_WIDTH;
      height = (MOUSE_ROW-top_line-1);
      #ifdef DOS
         height &= (~1);   // DOS clock must be on even row boundaries
      #endif
      height *= TEXT_HEIGHT;

      VCHAR_SCALE = 1;

      width = (width / (VCharWidth*TIME_CHARS));
      height = (height / VCharHeight);
      if(width > height) VCHAR_SCALE = height;
      else               VCHAR_SCALE = width;

      if(VCHAR_SCALE < 4) {  // no room for time under the sat info
         time_exit = 1;      // so dont draw the sat info table
         top_line = row;
         VCHAR_SCALE = 6;
      }

      time_col = (INFO_COL - 0 - ((TIME_CHARS*VCharWidth)/TEXT_WIDTH)) / 2;
      time_row = (MOUSE_ROW - top_line - (VCharHeight/TEXT_HEIGHT)) / 2;
      time_row += top_line+1;
   }

   if(full_circle) {
      reset_vstring();
      if(show_euro_dates) {
         sprintf(out, "%02d.%02d.%02d", pri_day, pri_month, pri_year%100);
      }
      else {
         sprintf(out, "%02d/%02d/%02d", pri_month, pri_day, pri_year%100);
      }
      vchar_string(time_row, time_col, time_color, out);
      time_row -= (VCHAR_SCALE*16*3/2)/TEXT_HEIGHT;
      reset_vstring();
      show_title();
   }

   sprintf(out, "%02d:%02d:%02d", clock_12?(pri_hours%12):pri_hours, pri_minutes, pri_seconds);
   vchar_string(time_row, time_col, time_color, out);
   digital_clock_shown = 1;

   return time_exit;
}
#endif  // DIGITAL_CLOCK


#ifdef ANALOG_CLOCK
//
// stuff for drawing an analog clock (watch)
//
char *roman[] = {
   " ",
   "I",     "II",    "III",
   "IIII",  "V",     "VI", 
   "VII",   "VIII",  "IX",
   "X",     "XI",    "XII",
   "XIII",  "XIV",   "XV",   
   "XVI",   "XVII",  "XVIII",
   "IXX",   "XX",    "XXI",  
   "XXII",  "XXIII", "*"
};
char *arabic[] = {
  " ",
  "1",    "2",   "3",
  "4",    "5",   "6",
  "7",    "8",   "9",
  "10",   "11",  "12",
  "13",   "14",  "15",
  "16",   "17",  "18",
  "19",   "20",  "21",
  "22",   "23",  "0" 
};
char *stars[] = {
   " ",
   "*",   "*",    "*",
   "*",   "*",    "*",
   "*",   "*",    "*",
   "*",   "*",    "*",
   "*",   "*",    "*",
   "*",   "*",    "*",
   "*",   "*",    "*",
   "*",   "*",    "!"
};

#define NUM_FACES 3
char **face[] = {  // select the watch face style
   roman,
   arabic,
   stars
};


#define ACLOCK_R     (ACLOCK_SIZE/2-TEXT_WIDTH)
#define AA           ((float)ACLOCK_R/8.0F)
#define ALARM_WIDTH  2   // how many degrees wide the alarm time marker is
#define WATCH_HOURS  (12*((watch_face/NUM_FACES)+1))  //12
#define WATCH_STEP   (360/WATCH_HOURS)
#define WATCH_MULT   (WATCH_HOURS/12)
#define hand_angle(x) ((int) (270.0F+((x)*6.0F)))

int ACLOCK_SIZE;
int aclock_x,aclock_y;

void position_watch()
{
   // calculate where we can draw the watch and how big to make it
   ACLOCK_SIZE = AZEL_SIZE;
   aclock_x = (AZEL_COL+ACLOCK_SIZE/2);
   aclock_y = (AZEL_ROW+ACLOCK_SIZE/2);

   if(full_circle) {
      aclock_x = SCREEN_WIDTH/2;
      aclock_y = SCREEN_HEIGHT/2;
   }
   else if(all_adevs) {
      if(WIDE_SCREEN) {
         if(plot_lla) {
            goto watch_it;
         }
         else {
            ACLOCK_SIZE = (SCREEN_WIDTH-WATCH_COL);
            if(ACLOCK_SIZE > (PLOT_ROW-TEXT_HEIGHT*4)) {
               ACLOCK_SIZE = PLOT_ROW-TEXT_HEIGHT*4;
            }
            if(ACLOCK_SIZE > 320) ACLOCK_SIZE = 320;
            aclock_x = (SCREEN_WIDTH-ACLOCK_SIZE/2-TEXT_WIDTH*2);   // and azel goes in the plot area
            aclock_y = (0+ACLOCK_SIZE/2+TEXT_HEIGHT*2);
         }
      }
   }
   else if((plot_azel || plot_signals) && plot_lla && WIDE_SCREEN) {
      goto watch_it;
   }
   else if((plot_azel || plot_signals) && (plot_lla == 0) && (SCREEN_HEIGHT >= 600)) {  // both azel and watch want to be on the sceen
      watch_it:
      ACLOCK_SIZE = WATCH_SIZE;               // watch goes in the adev area
      aclock_x = (WATCH_COL+ACLOCK_SIZE/2);   // and azel goes in the plot area
      aclock_y = (WATCH_ROW+ACLOCK_SIZE/2);
   }
}


void erase_watch()
{
int right;

   right = aclock_x+ACLOCK_SIZE/2-1;
   if(right >= SCREEN_WIDTH) right = SCREEN_WIDTH-1;

   #ifdef WIN_VFX
      VFX_rectangle_fill(stage, aclock_x-ACLOCK_SIZE/2,aclock_y-ACLOCK_SIZE/2, 
                                right,aclock_y+ACLOCK_SIZE/2-1, 
                                LD_DRAW, RGB_TRIPLET(0,0,0));
      VFX_io_done = 1;
   #endif

   #ifdef DOS
      dos_erase(aclock_x-ACLOCK_SIZE/2,aclock_y-ACLOCK_SIZE/2, 
                right,aclock_y+ACLOCK_SIZE/2-1);
   #endif
}

void draw_watch_face()
{
int x, y;
int hh;
int lasth;
char **wf;
float hr;
u08 bottom_blocked, top_blocked;
int i;

   wf = face[watch_face%NUM_FACES];
   graphics_coords = 1;

   // find the best place to put the watch brand name
   bottom_blocked = top_blocked = 0;
   hh = (pri_hours / WATCH_MULT) % 12;
   hh *= 100;
   hh += pri_minutes;
   if((pri_minutes >= 20) && (pri_minutes <= 40)) bottom_blocked |= 1;
   if((hh >= 400) && (hh <= 800))                 bottom_blocked |= 2;
   if((pri_minutes >= 10) && (pri_minutes <= 50)) top_blocked |= 0;
   else                                           top_blocked |= 5;
   if((hh >= 200) && (hh <= 1000))                top_blocked |= 0;
   else                                           top_blocked |= 6;

   if     (top_blocked == 0)    y = aclock_y-ACLOCK_SIZE/4;
   else if(bottom_blocked == 0) y = aclock_y+ACLOCK_SIZE/4;
   else if(top_blocked & 1)     y = aclock_y-ACLOCK_SIZE/4; 
   else                         y = aclock_y+ACLOCK_SIZE/4; 

   out[0] = 0;
   if(clock_name[0]) strcpy(out, clock_name);
   else              strcpy(out, days[day_of_week(pri_day, pri_month, pri_year)]);
   vidstr(y+TEXT_HEIGHT*0,aclock_x-strlen(out)*TEXT_WIDTH/2, time_color, out);

   out[0] = 0;
   if(clock_name2[0]) strcpy(out, clock_name2);
   else               strcpy(out, fmt_date());
   i = strlen(out);
   while(i--) {
      if(out[i] == ' ') out[i] = 0;
      else break;
   }
   vidstr(y+TEXT_HEIGHT*1,aclock_x-strlen(out)*TEXT_WIDTH/2, time_color, out);

   #ifdef GREET_STUFF
      if(moons[pri_day] && (clock_name[0] != ' ')) {
         if(y < aclock_y) vidstr(y+TEXT_HEIGHT*2,aclock_x-strlen(moons[pri_day])*TEXT_WIDTH/2, time_color, moons[pri_day]);
         else             vidstr(y-TEXT_HEIGHT*1,aclock_x-strlen(moons[pri_day])*TEXT_WIDTH/2, time_color, moons[pri_day]);
      }
   #endif


   for(lasth=1; lasth<=WATCH_HOURS; lasth++) {  // draw the watch numerals
      hr = (float) lasth;
      hr *= (60.0F/(float)WATCH_HOURS);
      if(lasth == 24) x = 0;
      else if(lasth == (6*WATCH_MULT)) x = 0;
      else x = (int) (ACLOCK_R * cos360(hand_angle(hr))); 
      if(x > 0) x -= strlen(wf[lasth])*TEXT_WIDTH;
      else if(x == 0) x -= (strlen(wf[lasth])*TEXT_WIDTH/2-(TEXT_WIDTH/2));
      x += aclock_x;
      x -= TEXT_WIDTH/2;

      y = (int) (ACLOCK_R * sin360(hand_angle(hr))); 
      if(y > 0) y -= TEXT_HEIGHT;
      else if(y < 0) y += TEXT_HEIGHT;
      y += aclock_y;
      y -= TEXT_HEIGHT/2;


      // tweak text positions for more natural goodness
      hh = lasth;
      if((hh >= (2*WATCH_MULT)) && (hh <= (4*WATCH_MULT))) {
         x -= TEXT_WIDTH/2;
         if(hh == (3*WATCH_MULT)) x -= TEXT_WIDTH/2;
      }
      else if((hh >= (8*WATCH_MULT)) && (hh <= (10*WATCH_MULT))) {
         x += TEXT_WIDTH;  // *3/4;
         if(hh == (9*WATCH_MULT)) x += TEXT_WIDTH/2;
      }

      if(WATCH_HOURS == 24) {
         if     (hh == 1)  {                     x += TEXT_WIDTH/2;   }
         else if(hh == 7)  { y += TEXT_HEIGHT;                        }
         else if(hh == 3)  { y += TEXT_HEIGHT/2; x += TEXT_WIDTH/2;   }
         else if(hh == 9)  { y += TEXT_HEIGHT/2; x -= TEXT_WIDTH/2;   }
         else if(hh == 19) { y -= TEXT_HEIGHT;   x += TEXT_WIDTH/2;   }
         else if(hh == 21) { y -= TEXT_HEIGHT/2; x += TEXT_WIDTH;     }
         else if(hh == 13) { 
            if((watch_face%NUM_FACES) == 0) {  // special tweak for roman numerals
               y -= TEXT_HEIGHT/4; x -= TEXT_WIDTH*3/2;
            }
            else {
               x -= TEXT_WIDTH*3/4;
            }
         }
      }

      if(SCREEN_WIDTH <= 800) {  // tweaks for small screens
         if(hh == (1*WATCH_MULT)) {           // 1:00
            x += TEXT_WIDTH;
            if(PLOT_HEIGHT < 160) y += TEXT_HEIGHT/4;
         }
         else if(hh == (11*WATCH_MULT)) {     // 11:00
            if(PLOT_HEIGHT < 160) y += TEXT_HEIGHT/4;
         }
         else if(hh == (5*WATCH_MULT)) {      // 5:00
            x += TEXT_WIDTH;
            if(PLOT_HEIGHT < 160) y -= TEXT_HEIGHT/4;
         }
         else if(hh == (7*WATCH_MULT)) {      // 7:00
            if(PLOT_HEIGHT < 160) {
               x += TEXT_WIDTH/2;
               y -= TEXT_HEIGHT/4;
            }
         }
         else if(hh == (2*WATCH_MULT)) {
            y -= TEXT_HEIGHT/2;
         }
         else if(hh == (10*WATCH_MULT)) {
            #ifdef DOS
               x += TEXT_WIDTH;
            #else
               y -= TEXT_HEIGHT/2;
            #endif
         }

         if(WATCH_HOURS == 24) {
            if     (hh == 1)  { x += TEXT_WIDTH;                       }
            else if(hh == 3)  { x += TEXT_WIDTH/2; y -= TEXT_HEIGHT/2; }
            else if(hh == 4)  { x += TEXT_WIDTH/2;                     }
            else if(hh == 5)  { x += TEXT_WIDTH/2; y -= TEXT_HEIGHT/2; }
            else if(hh == 6)  { x += TEXT_WIDTH/2;                     }
            else if(hh == 8)  { x += TEXT_WIDTH/2; y += TEXT_HEIGHT/2; }
            else if(hh == 9)  { x += TEXT_WIDTH/2;                     }
            else if(hh == 11) { x += TEXT_WIDTH/2;                     }
//          else if(hh == 13) { x -= TEXT_WIDTH/1; y -= TEXT_HEIGHT/2; }
            else if(hh == 14) { x += TEXT_WIDTH/4;                     }
            else if(hh == 16) { x -= TEXT_WIDTH/4; y += TEXT_HEIGHT/4; }
            else if(hh == 17) {                    y += TEXT_HEIGHT/4; }
            else if(hh == 22) { x += TEXT_WIDTH*0; y -= TEXT_HEIGHT/4; }
            else if(hh == 23) { x += TEXT_WIDTH/2; y -= TEXT_HEIGHT/4; }
         }
      }

      vidstr(y,x, time_color, wf[lasth]);
   }

   graphics_coords = 0;
}

void draw_watch_outline()
{
int x, y;
int x0, y0;
int theta;
int alarm_theta;
int alarm_ticks;
int color;
int stem_size;
#define THETA_STEP 1

   // draw the watch outline
   alarm_ticks = 0;
   if(alarm_date || alarm_time) {  // highlight the alarm clock time
      alarm_theta = ((alarm_hh%WATCH_HOURS)*WATCH_STEP*2)+alarm_mm;
      alarm_theta /= 2;
      alarm_theta -= 90;
      alarm_theta -= ALARM_WIDTH;
      if(alarm_theta < 0) alarm_theta += 360;
      alarm_theta /= THETA_STEP;      // round to clockface drawing increment
      alarm_theta *= THETA_STEP;
      if(alarm_theta < ALARM_WIDTH) { // alarm spans 0/360 degrees
         alarm_ticks = ALARM_WIDTH;
      }
   }

   x0 = (int) (ACLOCK_R * cos360(0));
   y0 = (int) (ACLOCK_R * sin360(0));

   for(theta=THETA_STEP; theta<=360; theta+=THETA_STEP) { 
      x = (int) (ACLOCK_R * cos360(theta));
      y = (int) (ACLOCK_R * sin360(theta));

      color = time_color;

      if((alarm_date || alarm_time) && (theta == alarm_theta)) {
         alarm_ticks = ALARM_WIDTH*2;
      }
      if(alarm_ticks) {
         color = ALARM_COLOR;
         --alarm_ticks;
         if(THETA_STEP > 1) alarm_ticks = 0;
      }

      line(aclock_x+x0,aclock_y+y0, aclock_x+x,aclock_y+y, color);
      x0 = x;
      y0 = y;
   }

   stem_size = ACLOCK_SIZE / 50;
   if(stem_size < 3) stem_size = 3;
   line(aclock_x-stem_size, aclock_y-ACLOCK_R-stem_size, aclock_x+stem_size, aclock_y-ACLOCK_R-stem_size, time_color);
   line(aclock_x-stem_size, aclock_y-ACLOCK_R-stem_size, aclock_x-stem_size, aclock_y-ACLOCK_R, time_color);
   line(aclock_x+stem_size, aclock_y-ACLOCK_R-stem_size, aclock_x+stem_size, aclock_y-ACLOCK_R, time_color);
}

void draw_watch_hands(void) 
{
int thickness;
int x, y;
float hr;
int a;

   // draw new hands
   thickness = 1;
   a = hand_angle((float)pri_seconds);
   x = (int) ((ACLOCK_R-AA) * cos360(a));
   y = (int) ((ACLOCK_R-AA) * sin360(a));  
   thick_line(aclock_x,aclock_y,  aclock_x+x,aclock_y+y, time_color, thickness);

   if(ACLOCK_R > 100) thickness = 3;
   else               thickness = 2;

   hr = (float) pri_minutes;
// hr += ((float) pri_seconds) / 60.0F;
   a = hand_angle(hr);
   x = (int) ((ACLOCK_R-AA) * cos360(a));  
   y = (int) ((ACLOCK_R-AA) * sin360(a));  
   thick_line(aclock_x,aclock_y,  aclock_x+x,aclock_y+y, time_color, thickness);

   hr = (float) pri_hours;
   hr += ((float)pri_minutes/60.0F);
   hr *= (60.0F/(float)WATCH_HOURS);  // convert hour angle to minute angle
   a = hand_angle(hr);
   x = (int) ((ACLOCK_R-AA*3) * cos360(a)); 
   y = (int) ((ACLOCK_R-AA*3) * sin360(a)); 
   thick_line(aclock_x,aclock_y,  aclock_x+x,aclock_y+y, time_color, thickness);
}

void draw_watch(void)
{
   if(plot_watch == 0) return;
   if(text_mode) return;
   if(all_adevs && plot_lla && (WIDE_SCREEN == 0)) return;
   if(shared_plot) {
      if(plot_signals && full_circle) return;
      if(plot_lla && (SCREEN_WIDTH < 800)) return;
   }
   else {  // see if no room for both watch and lla
      if(plot_signals) return;
      if(plot_lla && (WIDE_SCREEN == 0)) return;
      if(SCREEN_WIDTH < 800) return; 
   }

   position_watch();
   if(first_key && (aclock_y > PLOT_ROW)) return;  // watch would be in active help screen

   erase_watch();
   draw_watch_face();
   draw_watch_outline();
   draw_watch_hands();
}
#endif // analog_clock


#ifdef AZEL_STUFF
//
//
// AZIMUTH/ELEVATION plots
//
//

void erase_azel() 
{
   #ifdef WIN_VFX
      VFX_io_done = 1;
      if((full_circle == 0) && ((AZEL_ROW+AZEL_SIZE) >= PLOT_ROW)) {  // az/el map is in the normal plot window
         VFX_rectangle_fill(stage, PLOT_COL+PLOT_WIDTH,AZEL_ROW, SCREEN_WIDTH-1,AZEL_ROW+AZEL_SIZE-1, LD_DRAW, RGB_TRIPLET(0,0,0));
      }
      else {
         VFX_rectangle_fill(stage, AZEL_COL,AZEL_ROW, AZEL_COL+AZEL_SIZE-1,AZEL_ROW+AZEL_SIZE-1, LD_DRAW, RGB_TRIPLET(0,0,0));
      }
   #endif

   #ifdef DOS
      dos_erase(AZEL_COL,AZEL_ROW, AZEL_COL+AZEL_SIZE-1,AZEL_ROW+AZEL_SIZE-1);
   #endif

   azel_grid_color = GREY;
}


void check_azel_changes()
{
int i;
float az, el;

   // this routine redraws the az/el map if anything has moved
   if(plot_azel == 0) return;
   if(text_mode) return;
   if(first_key) return;

   this_track = this_used = 0L;
   this_q1 = this_q2 = this_q3 = this_q4 = 0;

   for(i=1; i<=32; i++) {
      if(sat[i].level_msg == 0x00) continue;

      el = sat[i].elevation;
      if((el < 0.0F) || (el > 90.0F)) continue;
      az = sat[i].azimuth;
      if((az < 0.0F) || (az >= 360.0F)) continue;
      if((az == 0.0F) && (el == 0.0F)) continue;

      this_used |= (1L << (i-1));  //  sat is currently being looked at
      if(sat[i].tracking > 0) {    //  we are actively tracking this sat
         this_track |= (1L << (i-1));
      }

      if     (az < 90.0F)  this_q1 |= (1L << (i-1));
      else if(az < 180.0F) this_q2 |= (1L << (i-1));
      else if(az < 270.0F) this_q3 |= (1L << (i-1));
      else                 this_q4 |= (1L << (i-1));
   }


   if((this_track != last_track) || (this_used != last_used)) {  // sats being tracked has changed
      draw_plot(1);      // plot area shared with az/el map
      last_minute = pri_minutes;
   }
   else if((this_q1 != last_q1) || (this_q2 != last_q2) || (this_q3 != last_q3) || (this_q4 != last_q4)) {  // a sat has shifted to a new quadrant
      draw_plot(1);      // plot area shared with az/el map
      last_minute = pri_minutes;
   }
   else if((pri_minutes != last_minute) && (pri_seconds == AZEL_UPDATE_SECOND)) {  // periodic update
      draw_azel_plot();  // writes onto old plot without erasing
      refresh_page();
      last_minute = pri_minutes;
   }

   last_track = this_track;
   last_used = this_used;
   last_q1 = this_q1;
   last_q2 = this_q2;
   last_q3 = this_q3;
   last_q4 = this_q4;
}


void tick_radial(int max_el, int az)
{
int x0,y0;
int x1,y1;
int x2,y2;
int el;
float len;
float r;

   x0 = AZEL_X;
   y0 = AZEL_Y;
   r = (float) AZEL_RADIUS;
   if(plot_signals == 3) {
      x0 -= AZEL_RADIUS;
      y0 += AZEL_RADIUS;
      r *= 2.0F;
   }

   for(el=0; el<max_el; el+=5) {
      len = ((float) r) * ((float)(max_el-el)/(float) max_el);
      x1 = (int) (len * cos360(az-2));
      y1 = (int) (len * sin360(az-2));

      x2 = (int) (len * cos360(az+2));
      y2 = (int) (len * sin360(az+2));

      line(x0+x1,y0+y1, x0+x2,y0+y2, azel_grid_color);
   }
}

void draw_azel_grid()
{
int az, el;
int x0, y0;
float x1, y1;
float x2, y2;
float elx;
float len;
float r;
int max_el, el_grid;

   x0 = AZEL_X;
   y0 = AZEL_Y;
   len = (float) AZEL_RADIUS;
   if(plot_signals == 3) {
      x0 -= AZEL_RADIUS;
      y0 += AZEL_RADIUS;
      len *= 2.0F;
   }

   x1 = cos360(0) * len;
   y1 = sin360(0) * len;

   for(az=AZ_INCR; az<=360; az+=AZ_INCR) {
      x2 = cos360(az) * len;
      y2 = sin360(az) * len;

      if((plot_signals == 3) && ((az-AZ_INCR) > 90) && ((az-AZ_INCR) < 360)) {
         goto skip_it;
      }

      if((plot_signals == 0) || (plot_signals >= 4)) {
         r = 1.03F;  // extend radials out a smidge
         max_el = 90;
         el_grid = 30;
      }
      else {
         r = 1.0F;
         max_el = 100;
         el_grid = 25;
      }

      if(((az-AZ_INCR)%AZ_GRID) == 0) {  // draw azimuth vectors
         line(x0+0,y0-0, 
              x0+(int)(x1*r),y0-(int)(y1*r), azel_grid_color);
         if(plot_signals) {
            if(plot_signals >= 4) tick_radial(90,  (az-AZ_INCR)+270);
            else                  tick_radial(100, (az-AZ_INCR)+270);
         }
      }

      // tick mark the 0 degree elevation circle
      line(x0+(int)(x1*0.99F),y0-(int)(y1*0.99F),  
           x0+(int)(x1*1.03F),y0-(int)(y1*1.03F), azel_grid_color); 

      // dot the satalite elevation mask angle circle
      if(plot_el_mask && (plot_signals == 0) || (plot_signals > 3)) { 
         elx = ((90.0F-el_mask) / 90.0F);
         dot(x0+(int)(x1*elx),y0-(int)(y1*elx), azel_grid_color); 
      }

      // draw the elevation circles
      for(el=0; el<max_el; el+=el_grid) {  
         elx = r = ((((float) max_el) - (float) el) / (float) max_el);

         if(plot_el_mask && (((float) el) == el_mask) && (el_mask != 0.0F)) {
            continue;  
         }
         if((plot_signals == 3) && ((az-AZ_INCR) == 90)) continue;
         line(x0+(int)(x1*elx),y0-(int)(y1*elx), 
              x0+(int)(x2*elx),y0-(int)(y2*elx), azel_grid_color);
      }

      skip_it:
      x1 = x2;  y1 = y2;
   }
}


#ifdef SAT_TRAILS

#ifdef DOS
   #define TRAIL_INTERVAL 15  // minutes between trail reference points (must divide into 60)
#else
   #define TRAIL_INTERVAL 5   // minutes between trail reference points (must divide into 60)
#endif

#define TRAIL_POINTS   ((6*60)/TRAIL_INTERVAL)  // number of points for 6 hours of data (the max a sat can be above the horizon)
#define TRAIL_MARKER   30                       // dot trail every 30 minutes (must divide into 60)

int trail_count;    // how many reference points that we have accumulated

struct SAT_TRAIL {  // the satellite location history (newest to oldest)
   s16 az[TRAIL_POINTS+1];  // (az<<6) | (lower 6 bits are minutes)
   s08 el[TRAIL_POINTS+1];
} sat_trail[32+1];  // one member for each sat


void update_sat_trails()
{
int prn;
int i;
int az,el;

   if(pri_seconds != TRAIL_UPDATE_SECOND) return;      // keeps too many things from happening at the same time
   if((pri_minutes % TRAIL_INTERVAL) != 0) return;

   for(prn=1; prn<=32; prn++) {       // for each satellite
      az = (int) (sat[prn].azimuth+0.5F);      // get current position
      el = (int) (sat[prn].elevation+0.5F);
      if((az < 0) || (az > 360)) az = el = 0;  // validate it
      else if((el < 0) || (el > 90)) az = el = 0;

      // duplicate or zero entry means sat is no longer tracked
      if(((az == 0) && (el == 0)) || (((sat_trail[prn].az[0]>>6) == az) && (sat_trail[prn].el[0] == el))) {  
         sat[prn].azimuth = 0.0F;
         sat[prn].elevation = 0.0F;
         for(i=0; i<TRAIL_POINTS; i++) {  // clear the sat's position list
            sat_trail[prn].az[i] = 0;
            sat_trail[prn].el[i] = 0;
         }
      }
      else {   // sat is being tracked
         for(i=trail_count; i>0; i--) {  // shift the old positions down one slot
            sat_trail[prn].az[i] = sat_trail[prn].az[i-1];
            sat_trail[prn].el[i] = sat_trail[prn].el[i-1];
         }

         // insert current position at top of the list
         sat_trail[prn].az[0] = (s16) ((az<<6) | pri_minutes);
         sat_trail[prn].el[0] = (s08) el;
      }
   }

   ++trail_count;
   if(trail_count >= TRAIL_POINTS) {  // the trail list is now full
      trail_count = TRAIL_POINTS-1;
   }
}

void plot_sat_trail(int prn, float az,float el,  int color, int dots)
{
int x1,y1;
int x2,y2;
int i;
int row;
u08 m;

   if(map_vectors == 0) return;  // trails disabled
   m = 0;
   row = 0;

   // the current location of the sat
   x1 = AZEL_X + (int) (el * cos360(az) * AZEL_RADIUS);
   y1 = AZEL_Y + (int) (el * sin360(az) * AZEL_RADIUS);

   for(i=0; i<trail_count; i++) { // check the old locations of the sat
      el = (float) sat_trail[prn].el[i];
      az = (float) (sat_trail[prn].az[i]>>6);
      if((az == 0.0F) && (el == 0.0F)) break;  // end of valid locations

      el = (el - 90.0F) / 90.0F;
      az += 90.0F;

      x2 = AZEL_X + (int) (el * cos360(az) * AZEL_RADIUS);
      y2 = AZEL_Y + (int) (el * sin360(az) * AZEL_RADIUS);

      if(dots) {  // mark the trail with time markers
         if(((sat_trail[prn].az[i] & 0x3F) % TRAIL_MARKER) == 0) {    // it's time to mark the trail
            #ifdef DOS   // is too stupid to draw circles
               if((sat_trail[prn].az[i] & 0x3F) != 0) {  // it's not the hour... hollow marker dot
                  line(x2-2,y2-2, x2+2,y2-2, color);
                  line(x2-2,y2+2, x2+2,y2+2, color);
                  line(x2-2,y2-2, x2-2,y2+2, color);
                  line(x2+2,y2-2, x2+2,y2+2, color);
                  for(row=y2-1; row<y2+1; row++) {
                     line(x2-1,row, x2+1,row, 0);
                  }
               }
               else {  // it's the hour,  solid marker dot
                  for(row=y2-2; row<y2+2; row++) {
                     line(x2-2,row, x2+2,row, color);
                  }
               }
            #endif
            #ifdef WIN_VFX
               #ifdef DOS_MOUSE
                  m = mouse_hit(x2-2,y2-2, x2+2,y2+2);
               #endif
               VFX_ellipse_fill(stage, x2,y2,  2, 2, palette[color]);
               if((sat_trail[prn].az[i] & 0x3F) != 0) {  // it's not the hour... dot the dot
                  VFX_ellipse_fill(stage, x2,y2,  1, 1, palette[0]);  // hollow out the marker dot
               }
               #ifdef DOS_MOUSE
                  if(m) update_mouse();
               #endif
            #endif
         }
      }
      else {   // draw the trail line
         line(x1,y1, x2,y2, color); 
      }

      x1 = x2;
      y1 = y2;
   }
}
#endif

void draw_azel_sats()
{
float az,el;
int x, y;
int marker_size;
int row;
int i, j;
u08 m;

   m = 0;
   j = 0;
   row = 0;
   
   for(i=1; i<=32; i++) { // now draw the sat vectors
      if(sat[i].level_msg == 0x00) continue;
      // filter out potentially bogus data
      el = sat[i].elevation;
      if((el < 0.0F) || (el > 90.0F)) continue;
      az = sat[i].azimuth;
      if((az < 0.0F) || (az >= 360.0F)) continue;
      if((az == 0.0F) && (el == 0.0F)) continue;

      el = (el - 90.0F) / 90.0F;
      az += 90.0F;

      x = AZEL_X + (int) (el * cos360(az) * AZEL_RADIUS);
      y = AZEL_Y + (int) (el * sin360(az) * AZEL_RADIUS);

      // the size of the sat marker is based upon the signal strength
      marker_size = (int) sat[i].sig_level;
      if(marker_size < 0) marker_size = 0 - marker_size;
      if(amu_mode == 0) {
          marker_size -= 30;  // 30 dBc is our minimum size
      }
      if(marker_size > 14)     marker_size = 14;   // and 44 dBc is our max size
      else if(marker_size < 2) marker_size = 2;
      if(SCREEN_WIDTH <= 800)  marker_size /= 2;   // scale markers to smaller screens

      #ifdef SAT_TRAILS
         // we draw the line first,  then the time marker dots
         // since the dots can be hollow and erase part of the line
         plot_sat_trail(i, az,el, sat_colors[j], 0);   // plot the satellite's path line 
         plot_sat_trail(i, az,el, sat_colors[j], 1);   // dot the satellite's path with time markers
      #endif

      if(sat[i].tracking > 0) {  //  mark sats that are actively tracked with a filled box
         #ifdef WIN_VFX
            #ifdef DOS_MOUSE
               m = mouse_hit(x-marker_size,y-marker_size, x+marker_size,y+marker_size);
            #endif
            VFX_ellipse_fill(stage, x,y,  marker_size, marker_size, palette[sat_colors[j]]);
            #ifdef DOS_MOUSE
               if(m) update_mouse();
            #endif
         #endif
         #ifdef DOS
            for(row=y-marker_size; row<y+marker_size; row++) {
               line(x-marker_size,row, x+marker_size,row, sat_colors[j]);
            }
         #endif
      }
      else {  // mark inactive sats with a hollow box
         #ifdef WIN_VFX
            #ifdef DOS_MOUSE
               m = mouse_hit(x-marker_size,y-marker_size, x+marker_size,y+marker_size);
            #endif
            VFX_ellipse_draw(stage, x,y,  marker_size, marker_size, palette[sat_colors[j]]);
            if(marker_size > 1) VFX_ellipse_fill(stage, x,y,  marker_size-1, marker_size-1, palette[0]);
            #ifdef DOS_MOUSE
               if(m) update_mouse();
            #endif
         #endif
         #ifdef DOS
            // draw the box outline
            line(x-marker_size,y-marker_size, x+marker_size,y-marker_size, sat_colors[j]);
            line(x-marker_size,y+marker_size, x+marker_size,y+marker_size, sat_colors[j]);
            line(x-marker_size,y-marker_size, x-marker_size,y+marker_size, sat_colors[j]);
            line(x+marker_size,y-marker_size, x+marker_size,y+marker_size, sat_colors[j]);
            if(marker_size > 1) { // hollow out the box
               for(row=y-marker_size+1; row<y+marker_size; row++) {
                  line(x-marker_size+1,row, x+marker_size-1,row, 0);
               }
            }
         #endif
      }
      ++j;
   }
}

void draw_azel_prns()
{
int i, j;
char el_dir;
float az,el;
int q1_row,q2_row,q3_row,q4_row;
int q3_q4_col, q1_q2_col;
char *s;

   // label the round maps
   q4_row = q1_row = ((AZEL_Y - AZEL_SIZE/2) / TEXT_HEIGHT) + 1;
   q3_row = q2_row = ((AZEL_Y + AZEL_SIZE/2) / TEXT_HEIGHT) - 1;
   q3_q4_col = (AZEL_X-AZEL_SIZE/2)/TEXT_WIDTH+2;
   q1_q2_col = (AZEL_X+AZEL_SIZE/2)/TEXT_WIDTH-2;
   if(SCREEN_WIDTH <= 800) {
      if(all_adevs || shared_plot) {  // plot area shared with az/el map
         if(SCREEN_HEIGHT < 600) {
            ++q4_row;  ++q1_row;
            ++q3_row;  ++q2_row;
         }
      }
      else {
         ++q3_row; ++q2_row;
      }
      --q4_row; --q1_row;
   }
   else if(all_adevs || shared_plot) {  // az/el map shared with plot area
      q1_q2_col -= 2;
      q3_q4_col += 1;
   }
   else {
      q1_q2_col -= 1;
   }

   if(0 & (SCREEN_HEIGHT < 600)) {
      --q2_row;
      --q3_row;
   }

   if(q1_q2_col < 0) q1_q2_col = 0;
   if(q3_q4_col < 0) q3_q4_col = 0;
   if(q1_row < 0) q1_row = 0;
   if(q2_row < 0) q2_row = 0;
   if(q3_row < 0) q3_row = 0;
   if(q4_row < 0) q4_row = 0;

   if(full_circle) show_title();
   s = "";
   no_x_margin = no_y_margin = 1;

   #ifdef SIG_LEVELS
      if(full_circle || ((SCREEN_WIDTH >= 1024) && (AZEL_ROW < PLOT_ROW))) {
         label_circles(q2_row+3);
      }

      if(plot_signals >= 4) {
         if(full_circle || ((SCREEN_WIDTH >= 1024) && (AZEL_ROW < PLOT_ROW))) {
            q1_q2_col-=2;
            q3_q4_col-=2;
            if(full_circle && (SCREEN_WIDTH == 800)) {
               ++q1_row;  ++q4_row;
            }
            vidstr(q4_row+0, q3_q4_col, LOW_SIGNAL,      "<30 dBc");
            vidstr(q4_row+1, q3_q4_col, level_color[1],  ">30 dBc");
            vidstr(q4_row+2, q3_q4_col, level_color[2],  ">32 dBc");

            vidstr(q1_row+0, q1_q2_col, level_color[3],  ">34 dBc");
            vidstr(q1_row+1, q1_q2_col, level_color[4],  ">36 dBc");
            vidstr(q1_row+2, q1_q2_col, level_color[5],  ">38 dBc");

            vidstr(q3_row-2, q3_q4_col, level_color[6],  ">40 dBc");
            vidstr(q3_row-1, q3_q4_col, level_color[7],  ">42 dBc");
            vidstr(q3_row-0, q3_q4_col, level_color[8],  ">44 dBc");

            vidstr(q2_row-2, q1_q2_col, level_color[9],  ">46 dBc");
            vidstr(q2_row-1, q1_q2_col, level_color[10], ">48 dBc");
            vidstr(q2_row-0, q1_q2_col, level_color[11], ">50 dBc");
         }
         else {
            vidstr(q4_row+0, q3_q4_col, LOW_SIGNAL,      "<30");
            vidstr(q4_row+1, q3_q4_col, level_color[1],  ">30");
            vidstr(q4_row+2, q3_q4_col, level_color[2],  ">32");

            vidstr(q1_row+0, q1_q2_col, level_color[3],  ">34");
            vidstr(q1_row+1, q1_q2_col, level_color[4],  ">36");
            vidstr(q1_row+2, q1_q2_col, level_color[5],  ">38");

            vidstr(q3_row-2, q3_q4_col, level_color[6],  ">40");
            vidstr(q3_row-1, q3_q4_col, level_color[7],  ">42");
            vidstr(q3_row-0, q3_q4_col, level_color[8],  ">44");

            vidstr(q2_row-2, q1_q2_col, level_color[9],  ">46");
            vidstr(q2_row-1, q1_q2_col, level_color[10], ">48");
            vidstr(q2_row-0, q1_q2_col, level_color[11], ">50");
         }
         no_x_margin = no_y_margin = 0;
         return;
      }
      else if(plot_signals) {
         no_x_margin = no_y_margin = 0;
         return;
      }
   #endif

   j = 0;
   for(i=1; i<=32; i++) {  // first draw the sat PRNs in use
      if(sat[i].level_msg == 0x00) continue;

      el = sat[i].elevation;
      if((el < 0.0F) || (el > 90.0F)) continue;
      az = sat[i].azimuth;
      if((az < 0.0F) || (az >= 360.0F)) continue;
      if((az == 0.0F) && (el == 0.0F)) continue;

      if(sat[i].tracking > 0) {  //  we are actively tracking at least one satellite
         if(azel_grid_color == AZEL_ALERT) azel_grid_color = AZEL_COLOR;  
      } 
      el_dir = sat[i].el_dir;
      if(el_dir == 0) el_dir = ' ';

      if(SCREEN_WIDTH > 800) sprintf(out, "%02d%c ", i,el_dir);
      else                   sprintf(out, "%02d ", i);

      if(az < 90.0F) {  // figure out what quadrant they are in
         vidstr(q1_row, q1_q2_col, sat_colors[j], out);
         ++q1_row;
      }
      else if(az < 180.0F) {
         vidstr(q2_row, q1_q2_col, sat_colors[j], out);
         --q2_row;
      }
      else if(az < 270.0F) {
         vidstr(q3_row, q3_q4_col, sat_colors[j], out);
         --q3_row;
      }
      else {
         vidstr(q4_row, q3_q4_col, sat_colors[j], out);
         ++q4_row;
      }
      ++j;
   }

   no_x_margin = no_y_margin = 0;
   return;
}


#ifdef SIG_LEVELS

float amu_2_dbc[] = {  // indexed with amu*10.0
   20.0F, 20.4F, 20.8F, 21.2F, 21.6F, 22.0F, 22.4F, 22.8F, 23.2F, 23.6F, 
   24.0F, 24.4F, 24.8F, 25.2F, 25.6F, 26.0F, 26.4F, 26.8F, 27.2F, 27.6F, 
   28.0F, 28.4F, 28.8F, 29.2F, 29.6F, 30.0F, 30.3F, 30.6F, 30.9F, 31.2F, 
   31.6F, 31.9F, 32.2F, 32.5F, 32.8F, 33.1F, 33.4F, 33.7F, 34.0F, 34.3F, 
   34.6F, 34.8F, 35.0F, 35.2F, 35.4F, 35.6F, 35.8F, 36.0F, 36.2F, 36.4F, 
   36.6F, 36.8F, 37.0F, 37.2F, 37.4F, 37.6F, 37.8F, 38.0F, 38.2F, 38.4F, 
   38.6F, 38.8F, 39.0F, 39.2F, 39.4F, 39.6F, 39.8F, 39.9F, 40.0F, 40.1F, 
   40.2F, 40.3F, 40.4F, 40.5F, 40.6F, 40.7F, 40.8F, 40.9F, 41.0F, 41.1F, 
   41.2F, 41.3F, 41.4F, 41.5F, 41.6F, 41.7F, 41.8F, 41.9F, 42.0F, 42.1F, 
   42.2F, 42.3F, 42.4F, 42.5F, 42.6F, 42.7F, 42.8F, 42.9F, 43.0F, 43.1F, 
   43.2F, 43.3F, 43.4F, 43.5F, 43.6F, 43.7F, 43.7F, 43.8F, 43.9F, 44.0F, 
   44.1F, 44.2F, 44.3F, 44.4F, 44.5F, 44.6F, 44.7F, 44.8F, 44.9F, 45.0F, 
   45.1F, 45.2F, 45.3F, 45.4F, 45.4F, 45.5F, 45.5F, 45.6F, 45.6F, 45.7F, 
   45.7F, 45.8F, 45.8F, 45.9F, 46.0F, 46.0F, 46.1F, 46.1F, 46.2F, 46.2F, 
   46.3F, 46.3F, 46.4F, 46.5F, 46.6F, 46.7F, 46.8F, 46.8F, 46.9F, 46.9F, 
   47.0F, 47.0F, 47.1F, 47.1F, 47.2F, 47.2F, 47.3F, 47.3F, 47.4F, 47.4F, 
   47.5F, 47.5F, 47.6F, 47.6F, 47.7F, 47.7F, 47.8F, 47.8F, 47.9F, 47.9F, 
   48.0F, 48.0F, 48.1F, 48.1F, 48.2F, 48.2F, 48.3F, 48.3F, 48.4F, 48.4F, 
   48.5F, 48.5F, 48.6F, 48.6F, 48.7F, 48.7F, 48.8F, 48.8F, 48.9F, 48.9F, 
   49.0F, 49.0F, 49.0F, 49.0F, 49.1F, 49.1F, 49.1F, 49.2F, 49.2F, 49.2F, 
   49.3F, 49.3F, 49.3F, 49.4F, 49.4F, 49.4F, 49.5F, 49.5F, 49.5F, 49.6F, 
   49.6F, 49.6F, 49.7F, 49.7F, 49.7F, 49.8F, 49.8F, 49.8F, 49.9F, 49.9F, 
   49.9F, 50.0F, 50.0F, 50.0F, 50.1F, 50.1F, 50.1F, 50.2F, 50.2F, 50.2F, 
   50.3F, 50.3F, 50.3F, 50.4F, 50.4F, 50.4F, 50.5F, 50.5F, 50.5F, 50.6F, 
   50.6F, 50.6F, 50.7F, 50.7F, 50.7F, 50.8F, 50.8F, 50.8F, 50.9F, 50.9F, 
   50.9F, 51.0F, 51.0F, 51.0F, 51.1F, 51.1F
};

float amu_to_dbc(float sig_level)
{
   if     (sig_level < 0.0F)   return 0.0F;
   else if(sig_level >= 25.1F) return 51.0F + ((sig_level - 25.1F) / 3.0F);
   else                        return amu_2_dbc[(int) (sig_level*10.0F)];
}

float dbc_to_amu(float sig_level)
{
int i;

   if(sig_level <= 0.0F)  return 0.0F;
   if(sig_level >= 51.0F) return 25.1F + ((sig_level - 51.0F) * 3.0F);

   for(i=0; i<256; i++) {
      if(amu_2_dbc[i] > sig_level) return ((float) i) / 10.0F;
   }
   return 0.0F;
}

void label_circles(int row)
{
int col;
int x, y;
char *s;

   if     (plot_signals == 1) s = "Relative strength vs azimuth";
   else if(plot_signals == 2) s = "1/EL weighted strength vs azimuth";
   else if(plot_signals == 3) s = "Relative strength vs elevation";
   else if(plot_signals == 4) s = "Signal strength vs az/el";
   else if(plot_signals == 5) s = "Raw signal level data";
   else                       s = "Satellite positions";
   col = (AZEL_X/TEXT_WIDTH) - (strlen(s)/2);
   if(full_circle) ++row;
   vidstr(row,col, WHITE, s);

   graphics_coords = 1;   // these labels need fine positioning

   if(plot_signals == 3) {
      x = AZEL_X;
      y = AZEL_Y;  // AZEL_RADIUS;
      if(full_circle) y += TEXT_HEIGHT;
      y += AZEL_RADIUS+TEXT_HEIGHT/2;
//    sprintf(out, "Signal%c", RIGHT_ARROW);
//    vidstr(y+AZEL_RADIUS+TEXT_HEIGHT/2,x-strlen(out)*TEXT_WIDTH/2, WHITE, out);
      vidstr(y, x-AZEL_RADIUS*1-2*TEXT_WIDTH/2, WHITE, "0%");
      vidstr(y, x-AZEL_RADIUS/2-3*TEXT_WIDTH/2, WHITE, "25%");
      vidstr(y, x-AZEL_RADIUS*0-3*TEXT_WIDTH/2, WHITE, "50%");
      vidstr(y, x+AZEL_RADIUS/2-3*TEXT_WIDTH/2, WHITE, "75%");
      vidstr(y, x+AZEL_RADIUS*1-4*TEXT_WIDTH/2, WHITE, "100%");

      x = AZEL_X+AZEL_RADIUS+TEXT_WIDTH*2;
      y = AZEL_Y+AZEL_RADIUS-TEXT_HEIGHT/2;
      if(full_circle) x += TEXT_WIDTH;
      sprintf(out, "0%c", DEGREES);
      vidstr(y,x, WHITE, out);

      x = AZEL_X-AZEL_RADIUS+TEXT_WIDTH*3  + (int) ((float)(AZEL_RADIUS*2)*cos360(30));
      y = AZEL_Y+AZEL_RADIUS-TEXT_HEIGHT*2 - (int) ((float)(AZEL_RADIUS*2)*sin360(30));
      if(full_circle == 0) y += TEXT_HEIGHT;
      if(full_circle == 0) x -= TEXT_WIDTH;
      sprintf(out, "30%c", DEGREES);
      vidstr(y,x, WHITE, out);

      x = AZEL_X-AZEL_RADIUS+TEXT_WIDTH*1  + (int) ((float)(AZEL_RADIUS*2)*cos360(60));
      y = AZEL_Y+AZEL_RADIUS-TEXT_HEIGHT*3 - (int) ((float)(AZEL_RADIUS*2)*sin360(60));
      if(full_circle == 0) y += TEXT_HEIGHT;
      sprintf(out, "60%c", DEGREES);
      vidstr(y,x, WHITE, out);

      x = AZEL_X-AZEL_RADIUS-(TEXT_WIDTH*3/2);
      y = AZEL_Y-AZEL_RADIUS-TEXT_HEIGHT;
      if(full_circle) y -= TEXT_HEIGHT;
      sprintf(out, "90%c", DEGREES);
      vidstr(y,x, WHITE, out);
   }
   else if(plot_signals) {
      x = AZEL_X-TEXT_WIDTH/2;
      y = AZEL_Y;  // AZEL_RADIUS;
      vidstr(y-AZEL_RADIUS-TEXT_HEIGHT-TEXT_HEIGHT/2,x, WHITE, "N");
      vidstr(y+AZEL_RADIUS+TEXT_HEIGHT/2,x,             WHITE, "S");

      x = AZEL_X;
      y = AZEL_Y - TEXT_HEIGHT/2;
      vidstr(y,x-AZEL_RADIUS-TEXT_WIDTH*2-TEXT_WIDTH/2, WHITE, "W");

      x = x+AZEL_RADIUS+TEXT_WIDTH*1+TEXT_WIDTH/2;
      if(plot_signals >= 4) {
         vidstr(y,x, WHITE, "E");
      }
      else {
         if((SCREEN_WIDTH == 1280) && (full_circle == 0)) ;
         else x += TEXT_WIDTH;
         vidstr(y,x, WHITE, "E");

         x = AZEL_X;
         y = AZEL_Y+TEXT_HEIGHT/4;  // AZEL_RADIUS;
         vidstr(y, x+AZEL_RADIUS*1/4-3*TEXT_WIDTH/2, WHITE, "25%");
         vidstr(y, x+AZEL_RADIUS*2/4-3*TEXT_WIDTH/2, WHITE, "50%");
         vidstr(y, x+AZEL_RADIUS*3/4-3*TEXT_WIDTH/2, WHITE, "75%");
         vidstr(y, x+AZEL_RADIUS*4/4-4*TEXT_WIDTH/2, WHITE, "100%");

         vidstr(y, x-AZEL_RADIUS*4/4-3*TEXT_WIDTH/2, WHITE, "100%");
         vidstr(y, x-AZEL_RADIUS*3/4-2*TEXT_WIDTH/2, WHITE, "75%");
         vidstr(y, x-AZEL_RADIUS*2/4-2*TEXT_WIDTH/2, WHITE, "50%");
         vidstr(y, x-AZEL_RADIUS*1/4-2*TEXT_WIDTH/2, WHITE, "25%");
      }
   }

   graphics_coords = 0;
}

void clear_signals()
{
int x, y;

   // clear out the signal level data structures
   for(x=0; x<=360; x++) {
      db_az_sum[x] = 0.0F;
      db_weighted_az_sum[x] = 0.0F;
      db_az_count[x] = 0.0F;
      max_el[x] = 0;
      min_el[x] = 90;

      for(y=0; y<=90; y++) {
         db_3d_sum[x][y] = 0.0F;
         db_3d_count[x][y] = (-1.0F);
      }
   }

   for(y=0; y<=90; y++) {
      db_el_sum[y]= 0.0F;
      db_el_count[y] = 0.0F;
   }

   max_sig_level = 0.0F;
   for(x=0; x<=32; x++) {
      max_sat_db[x] = 0.0F;
   }

   reading_signals = 0;  // re-enable live data recording
   signal_length = 0L;
}

void log_signal(float azf, float elf, float sig_level, int amu_flag)
{
int az, ell;

   // record sat signal strength data in the various arrays
   az  = (int) (azf+0.50F);
   ell = (int) (elf+0.50F);
   if((az < 0) || (az > 360)) return;
   if((ell < 0) || (ell > 90)) return;

   if(sig_level < 1.00F) {  // don't let zero signal levels spoil the average
      if(db_3d_count[az][ell] > 0.0) return; // only log one of them
   }

   if(amu_flag) {  // convert AMU values to dBc values
      sig_level = amu_to_dbc(sig_level);
   }

   db_az_sum[az] += sig_level;
   db_weighted_az_sum[az] += (sig_level * ((90.0F-(float)ell)/90.0F));
   db_az_count[az] += 1.0F;

   db_el_sum[ell] += sig_level;
   db_el_count[ell] += 1.0F;

   if(ell > max_el[az]) max_el[az] = ell;
   if(ell < min_el[az]) min_el[az] = ell;

   if(0) {  // track peak signal
      if(sig_level > db_3d_sum[az][ell]) db_3d_sum[az][ell] = sig_level;
      db_3d_count[az][ell] = 1.0F;
   }
   else {   // track average signal
      if(db_3d_count[az][ell] < 1.0F) {
         db_3d_sum[az][ell] = sig_level;
         db_3d_count[az][ell] = 1.0F;
      }
      else {
         db_3d_sum[az][ell] += sig_level;
         db_3d_count[az][ell] += 1.0F;
      }
   }
   ++signal_length;
}

void dump_signals(char *fn)
{
FILE *f;
int az, el;

   // write signal strength data to a log file

   f = fopen(fn, "w");
   if(f == 0) return;

   for(az=0; az<=360; az++) {
      for(el=0; el<=90; el++) {
         if(db_3d_count[az][el] > 0.0F) {
            fprintf(f, "%03d %02d %5.2f\n", az, el, db_3d_sum[az][el]/db_3d_count[az][el]);
         }
      }
   }

   fclose(f);
}


void read_signals(char *s)
{
FILE *sig_file;
float az, el, sig;

   // stop recording live signal strength data and load in signal strength 
   // data from a log file

   sig_file = fopen(s, "r");
   if(sig_file == 0) return;
   clear_signals();       // flush the old data
   
   reading_signals = 1;   // signal we are reading data from a log file
   plot_signals = 4;      // show the pretty color map

   while(fgets(out, sizeof out, sig_file) != NULL) {
      if((out[0] == '*') || (out[0] == ';') && (out[0] != '#') || (out[0] == '/')) {
         continue;  // comment line
      }
      sscanf(out, "%f %f %f", &az, &el, &sig);
      log_signal(az, el, sig, 0);
   }

   fclose(sig_file);
}


void plot_raw_signals(int el)
{
int az;
int color;
int thick;
int x, y;
float elf;

   // draw the raw signal strength data seen at an elevation angle
   thick = (AZEL_RADIUS/90)+1;

   for(az=0; az<=360; az++) {
      if(db_3d_count[az][el] <= 0.0F) continue;

      elf = db_3d_sum[az][el] / db_3d_count[az][el];
      elf -= MIN_SIGNAL;
      color = (int) (elf / LEVEL_STEP);
      if     (color > LEVEL_COLORS) color = LEVEL_COLORS;
      else if(color <= BLACK)       color = LOW_SIGNAL;
      else                          color = level_color[color];

      elf = ((float)(el - 90)) / 90.0F;

      x = AZEL_X + (int) (elf * cos360(az+90) * AZEL_RADIUS) - thick/2;
      y = AZEL_Y + (int) (elf * sin360(az+90) * AZEL_RADIUS) - thick/2;

      thick_line(x,y, x,y, color, thick);
   }
}

#define HIGH_LATS 55.0

int next_az_point(int start_az, int el)
{
   if(start_az >= 360) return 360;
   while(++start_az < 360) {
      if(db_3d_count[start_az][el] > 0.0F) break;
      if((lat < 0.0) || (DABS(lat*RAD_TO_DEG) > HIGH_LATS)) {
         if(start_az == 180) break;
      }
   }
   return start_az;
}

int min_az_clip[360+1];
int max_az_clip[360+1];

void calc_az_clip()
{
int az, el;

   // find the min and max elevation angle that has signals at each az point
   for(az=0; az<=360; az++) {
      min_az_clip[az] = 90;
      max_az_clip[az] = 0;
   }

   for(az=360; az>0; az--) {  // find starting point
      if(min_el[az] <= max_el[az]) break;
   }
   if(min_el[az] > max_el[az]) return;  // no data to plot

   el = max_el[az];
   for(az=0; az<=360; az++) {
      if(min_el[az] <= max_el[az]) el = max_el[az];
      max_az_clip[az] = el;
   }

   // now find the min elevation bounds
   for(az=360; az>0; az--) {
      if(min_el[az] <= max_el[az]) break;
   }

   el = min_el[az];
   for(az=0; az<=360; az++) {
      if(min_el[az] <= max_el[az]) el = min_el[az];
      min_az_clip[az] = el;
   }
}

void db_arc(int start, int end, int el, int arc_color)
{
int x1, y1;
int x2, y2;
float elf;
int az;
int color;
int thickness;

   if(el) thickness = (AZEL_SIZE+99) / 100;
   else   thickness = 1;

   elf = ((float)(90 - el)) / 90.0F;
   az = start + 90;
   x1 = AZEL_X - (int) (elf * cos360(az) * AZEL_RADIUS) - (thickness/2)-1;
   y1 = AZEL_Y - (int) (elf * sin360(az) * AZEL_RADIUS) - (thickness/2)-0;

   while(start <= end) {
      az = start + 90;
      x2 = AZEL_X - (int) (elf * cos360(az) * AZEL_RADIUS) - (thickness/2)-1; 
      y2 = AZEL_Y - (int) (elf * sin360(az) * AZEL_RADIUS) - (thickness/2)-0; 

      if     (el < min_az_clip[start]) color = BLACK;
      else if(el > max_az_clip[start]) color = BLACK;
      else                             color = arc_color;

      thick_line(x1,y1, x2,y2, color, thickness);

      x1 = x2;
      y1 = y2;
      ++start;
   }
}

int start_color;

void plot_3d_signals(int el)
{
int color;
float elf;
int start_az, center_az, end_az;
int saz, eaz;
int MAX_ARC;  // maximum pixel exapansion in degrees azimuth
int last_max;

   MAX_ARC = last_max = 25;
   start_az = 0;
   while(start_az < 360) {
      center_az = next_az_point(start_az, el);
      end_az = next_az_point(center_az, el);

      // satellite sky coverage has holes towards the poles
      // we better define them by shrinking the max size of the arcs
      // when they are where the holes might be
      last_max = MAX_ARC;
      MAX_ARC = 25;
      if((lat < 0.0) || ((lat*RAD_TO_DEG) >= HIGH_LATS)) {
         if     (start_az == 180)  MAX_ARC = 1;
         else if(center_az == 180) MAX_ARC = 1;
         else if(end_az == 180)    MAX_ARC = 1;
      }
      if((lat >= 0.0) || ((lat*RAD_TO_DEG) <= (-HIGH_LATS))) {
         if     (start_az == 0) MAX_ARC = 1;
         else if(end_az == 360) MAX_ARC = 1;
      }
      end_az = (center_az + end_az) / 2;

      if(db_3d_count[center_az][el] > 0.0F) {
         elf = db_3d_sum[center_az][el] / db_3d_count[center_az][el];
         elf -= MIN_SIGNAL;
         color = (int) (elf / LEVEL_STEP);
         if     (color > LEVEL_COLORS) color = LEVEL_COLORS;
         else if(color <= BLACK)       color = LOW_SIGNAL;
         else                          color = level_color[color];
      }
      else {
         color = BLACK;
      }

      if((start_az == 0) && (color != BLACK)) start_color = color;

      if((center_az - start_az) > MAX_ARC)     saz = center_az - MAX_ARC;
      else if((lat < 0.0F) && (last_max == 1)) saz = center_az - 1; 
      else                                     saz = start_az;
      if((end_az - center_az) > MAX_ARC)       eaz = center_az + MAX_ARC;
      else if((lat < 0.0F) && (last_max == 1)) eaz = center_az + 1;
      else                                     eaz = end_az;

      if(color != BLACK) {
         db_arc(saz, eaz, el, color);
      }
      else if(end_az >= 360) {
         if(start_color != BLACK) db_arc(saz, eaz, el, start_color);
      }

      start_az = end_az;
   }
}


void draw_3d_signals()
{
int el;

   // draw signal level map as a function of az/el
   start_color = BLACK;
   if(plot_signals == 4) {  // find min and max elevations seen at each az angle
      calc_az_clip();
   }

   for(el=0; el<90; el++) {
      if(plot_signals >= 5) {  // just show raw data points
         plot_raw_signals(el);
      }
      else {                   // show fully expanded color data plot
         plot_3d_signals(el);
      }
   }

   azel_grid_color = WHITE;  // AZEL_COLOR;
   draw_azel_grid();
}

void draw_az_levels()
{
int first_az;
int az;
int x0, y0;
int x1, y1;
int x2, y2;
float r;
float max_db;
float max_weighted_db;
int color;

   // show signal level vesus azimuth
   first_az = (-1);
   max_db = max_weighted_db = 0.0F;

   for(x0=0; x0<360; x0++) {  // find max db values
      if(db_az_sum[x0] && db_az_count[x0]) {
         r = db_az_sum[x0] / db_az_count[x0];
         if(r > max_db) {
            max_db = r;
            if(first_az < 0) first_az = x0;
         }
      }
      if(db_weighted_az_sum[x0] && db_az_count[x0]) {
         r = db_weighted_az_sum[x0] / db_az_count[x0];
         if(r > max_weighted_db) {
            max_weighted_db = r;
         }
      }
   }
   if(first_az < 0) return;
   if(max_db == 0.0F) return;
   if(max_weighted_db == 0.0F) return;

   db_az_count[360] = db_az_count[0];
   db_az_sum[360] = db_az_sum[0];

   if(plot_signals == 1) r = (db_az_sum[first_az] / db_az_count[first_az]) / max_db;
   else                  r = (db_weighted_az_sum[first_az] / db_az_count[first_az]) / max_weighted_db;
   if(r > 1.0F) r = 1.0F;
   x0 = x1 = AZEL_X - (int) (r * cos360(0+90) * AZEL_RADIUS);
   y0 = y1 = AZEL_Y - (int) (r * sin360(0+90) * AZEL_RADIUS);

   for(az=0; az<=360; az+=1) {
      if(db_az_count[az] && db_az_sum[az]) {
         if(plot_signals == 1) r = (db_az_sum[az] / db_az_count[az]) / max_db;
         else                  r = (db_weighted_az_sum[az] / db_az_count[az]) / max_weighted_db;
         if(r >= 1.0F) r = 1.0F;
         color = GREEN;
      }
      else color = BLUE;

      x2 = AZEL_X - (int) (r * cos360(az+90) * AZEL_RADIUS);
      y2 = AZEL_Y - (int) (r * sin360(az+90) * AZEL_RADIUS);

      line(x1,y1, x2,y2, color);
      x1 = x2;
      y1 = y2;
   }

   line(x0,y0, x1,y1, LEVEL_COLOR);
}

void draw_el_levels()
{
int first_el;
int el;
int x1, y1;
int x2, y2;
float max_db;
float len;
float r;
int color;

   // show signal level versus elevation
   for(first_el=0; first_el<=90; first_el++) {
      if(db_el_count[first_el] != 0.0F) break;
   }
   if(first_el > 89) return;   // no data recorded

   max_db = 0.0F;
   for(el=0; el<=90; el++) {  // find max db value
      if(db_el_count[el]) {
         r = db_el_sum[el] / db_el_count[el];
         if(r > max_db) max_db = r;
      }
   }
   if(max_db == 0.0F) return;

   r = (db_el_sum[first_el] / db_el_count[first_el]) / max_db;
   if(r > 1.0F) r = 1.0F;

   len = (float) (AZEL_RADIUS*2);
   x1 = AZEL_X-AZEL_RADIUS + (int) (r * cos360(0+90) * len);
   y1 = AZEL_Y+AZEL_RADIUS - (int) (r * sin360(0+90) * len);

   for(el=0; el<=90; el++) {
      if(db_el_count[el] && db_el_sum[el]) {
         r = (db_el_sum[el] / db_el_count[el]) / max_db;
         if(r > 1.0F) r = 1.0F;
         color = GREEN;
      }
      else color = BLUE;

      x2 = AZEL_X-AZEL_RADIUS + (int) (r * cos360(el) * len);
      y2 = AZEL_Y+AZEL_RADIUS - (int) (r * sin360(el) * len);

      if(el != 0) line(x1,y1, x2,y2, color);
      x1 = x2;
      y1 = y2;
   }

   el = good_el_level();
   x1 = AZEL_X-AZEL_RADIUS + (int) (0.99F * cos360(el) * len);
   y1 = AZEL_Y+AZEL_RADIUS - (int) (0.99F * sin360(el) * len);
   x2 = AZEL_X-AZEL_RADIUS + (int) (1.03F * cos360(el) * len);
   y2 = AZEL_Y+AZEL_RADIUS - (int) (1.03F * sin360(el) * len);
   line(x1,y1, x2,y2, GREEN);
}

#define GOOD_EL_LEVEL  30      // if no good signal found, use this elevation
#define EL_THRESHOLD   ((res_t == 2)? 0.750F:0.875F)  // "good" is this percent of max signal 

int good_el_level()
{
int el;
float max_db;
float r;

   // find lowest EL level that has good signal
   max_db = 0.0F;
   for(el=0; el<=90; el++) {  // find max db value
      if(db_el_count[el]) {
         r = db_el_sum[el] / db_el_count[el];
         if(r > max_db) max_db = r;
      }
   }
   if(max_db == 0.0F) return GOOD_EL_LEVEL;

   for(el=0; el<=90; el++) {
      if(db_el_count[el] && db_el_sum[el]) {
         r = (db_el_sum[el] / db_el_count[el]) / max_db;
         if(r > EL_THRESHOLD) {
//sprintf(plot_title, "Good el %d -> %f", el, r);
            return el;
         }
      }
   }

   return GOOD_EL_LEVEL;
}

#endif // SIG_LEVELS


void draw_azel_plot()
{
   // draw satellite position map
   if((plot_azel == 0) && (plot_signals == 0)) return;
   if(text_mode) return;
   if(first_key) return;
   if((SCREEN_WIDTH < 800) && (plot_lla || plot_watch)) {
      return; // no room for both azel and lla or watch
   }

   if(plot_watch && (plot_signals == 0)) {  // both azel and watch want to be on the screen
      if(all_adevs) {
         if(WIDE_SCREEN == 0) return;
      }
      else if(WIDE_SCREEN && (shared_plot == 0) && plot_lla) return;  // the watch wins
      else if(WIDE_SCREEN && plot_lla) ;
      else if(WIDE_SCREEN && (shared_plot == 0) && (plot_lla == 0)) {
         AZEL_ROW = LLA_ROW;
         AZEL_COL = LLA_COL;
         AZEL_SIZE = LLA_SIZE;
      }
      else if((shared_plot == 0) || all_adevs || plot_lla || (SCREEN_HEIGHT < 600)) return;  // the watch wins
   }
   else if(all_adevs && WIDE_SCREEN && (plot_lla == 0)) {
   }

   azel_grid_color = AZEL_ALERT;
   if(osc_control_on && (discipline_mode == 6)) ;
   else if(discipline_mode != 0) azel_grid_color = YELLOW;

   erase_azel();       // erase the old az/el plot

   azel_grid_color = AZEL_COLOR;
   if(plot_azel || plot_signals) {
      draw_azel_prns();   // label the azel plot
   }

   #ifdef SIG_LEVELS
      if(plot_signals >= 4) {
         draw_3d_signals();
         return;
      }
   #endif

   draw_azel_grid();      // now draw the az/el grid

   if(plot_azel && (plot_signals == 0)) {
      draw_azel_sats();   // fill in the satellites
   }

   #ifdef SIG_LEVELS
      if((plot_signals == 1) || (plot_signals == 2)) {
         draw_az_levels(); // signal level vs azimuth
      }
      else if(plot_signals == 3) {
         draw_el_levels(); // signal level vs elevation
      }
   #endif
}

#endif  // AZEL_STUFF

//
//
//   Precision survey stuff
//
//
#ifdef PRECISE_STUFF

u08 debug_lla = 1;
#define SURVEY_COLOR        BLUE

#define LAT_REF (precise_lat*RAD_TO_DEG)
#define LON_REF (precise_lon*RAD_TO_DEG)
#define ALT_REF precise_alt
#define COS_FACTOR(x) cos((x)/RAD_TO_DEG)    //!!!!! verify usages

long start_tow;
long minute_tow;
long hour_tow;

double interp;

double xlat, xlon, xalt;
double lat_sum, lon_sum, alt_sum;
double best_lat, best_lon, best_alt;
double best_count;

double lat_bins[SURVEY_BIN_COUNT+1];
double lon_bins[SURVEY_BIN_COUNT+1];
double alt_bins[SURVEY_BIN_COUNT+1];
int second_lats, second_lons, second_alts;

double lat_min_bins[SURVEY_BIN_COUNT+1];
double lon_min_bins[SURVEY_BIN_COUNT+1];
double alt_min_bins[SURVEY_BIN_COUNT+1];
int minute_lats, minute_lons, minute_alts;

double alt_hr_bins[SURVEY_BIN_COUNT+1];
int hour_alts;

#define LAT_THRESH (ANGLE_SCALE/RAD_TO_DEG)  //radian per foot
#define LON_THRESH (ANGLE_SCALE/RAD_TO_DEG)
#define ALT_THRESH (1.0)


#define LAT_SEC_TURN 0.457374
#define LON_SEC_TURN 0.469305
#define ALT_SEC_TURN 0.461097

#define LAT_MIN_TURN 0.371318
#define LON_MIN_TURN 0.414684
#define ALT_MIN_TURN 0.405716

#define LAT_HR_TURN  0.469675
#define LON_HR_TURN  0.439141
#define ALT_HR_TURN  0.416180


double float_error(double val)
{
float x;

   // calculate the error between the single and double precision value
   x = (float) val;
   val = val - x;
   if(val < 0.0) val = 0.0 - val;
   return val;
}

void open_lla_file()
{
// if(lla_file == 0) lla_file = fopen("LLA.LLA", "a");
   if(lla_file == 0) lla_file = fopen("LLA.LLA", "w");
   if(lla_file == 0) return;
   fprintf(lla_file, "#TITLE: LLA log: %02d %s %04d  %02d:%02d:%02d - receiver mode %d\n", 
      day, months[month], year, hours,minutes,seconds, rcvr_mode);
   fprintf(lla_file, "#LLA: %.8lf %.8lf %.3lf\n", precise_lat*RAD_TO_DEG, precise_lon*RAD_TO_DEG, precise_alt);
}

void close_lla_file()
{
   if(lla_file == 0) return;
   fclose(lla_file);
   lla_file = 0;
}


void start_3d_fixes(int mode)
{
   #ifdef BUFFER_LLA
      clear_lla_points();
   #endif
   open_lla_file();
   set_rcvr_config(mode);  // 0=auto 2D/3D  or  4=3D only mode
   request_sat_list();
   plot_lla = 1;
   plot_azel = 1;
   if(WIDE_SCREEN == 0) {
      shared_plot = 1; 
      all_adevs = 0;
   }
   precision_samples = 0L;
}

void start_precision_survey()
{
   stop_self_survey();  // abort standard survey
   if(do_survey <= 0) do_survey = 48;
   else if(do_survey > SURVEY_BIN_COUNT) do_survey = 48;
   set_rcvr_config(4);  // put receiver into 3D mode

   if(log_file) fprintf(log_file, "# Precision %d hour survey started.\n", do_survey);

   open_lla_file();

   precision_survey = 1;   // config screen to show survey map
   show_fixes = 0;
   plot_lla = 1;
   plot_azel = 1;
   if(WIDE_SCREEN == 0) {
      shared_plot = 1; 
      all_adevs = 0;
   }
   config_screen();

   start_tow = tow;
   minute_tow = start_tow + 60L;
   hour_tow = start_tow + 60L*60L;

   lat_sum = lon_sum = alt_sum = 0.0;
   precision_samples = 0;
   survey_minutes = 0L;

   second_lats = second_lons = second_alts = 0;
   minute_lats = minute_lons = minute_alts = 0;
   hour_lats = hour_lons = hour_alts = 0;

   best_lat = best_lon = best_alt = 0.0;
   best_count = 0.0;

   plot_lla_axes();
}

void stop_precision_survey()
{
   if(precision_survey == 0) return;
   precision_survey = 0;

   if(log_file) fprintf(log_file, "# Precision survey stopped.\n");

   set_rcvr_config(7);     // overdetermined clock mode
   if(lla_file) { //!!!! kludge - add a extra entry so external processor program works correctly
      fprintf(lla_file, "# time filler kludge\n");
      fprintf(lla_file, "%-6ld %d %13.8lf  %13.8lf  %8.3lf\n", 
      tow+60L, 0, lat*RAD_TO_DEG, lon*RAD_TO_DEG, alt);
   }
}

void precise_check()
{
double d_lat, d_lon, d_alt;

   if(surveying) return;
   if(check_precise_posn == 0) return;
   if(check_delay) {  // ignore first few survey points - they could be bogus filter remnants
      --check_delay;
      return;
   }

   d_lat = (lat-precise_lat)*RAD_TO_DEG/ANGLE_SCALE;
   d_lon = (lon-precise_lon)*RAD_TO_DEG/ANGLE_SCALE*cos_factor;
   d_alt = (alt-precise_alt);
// if((DABS(lat-precise_lat) < LAT_THRESH) && (DABS(lon-precise_lon) < LON_THRESH) && (DABS(alt-precise_alt) < ALT_THRESH)) {

   // see if surveyed position error is small enough
   if((sqrt(d_lat*d_lat + d_lon*d_lon) <= 1.0) && (DABS(d_alt) < ALT_THRESH)) {
      save_segment(7);          // yes,  save the position in EEPROM
      check_precise_posn = 0;   // we are done
      log_saved_posn(-1);
      if(SCREEN_WIDTH > 800) lla_header("Precise survey complete  ", WHITE);
      else                   lla_header("Survey complete  ", WHITE);
      close_lla_file();
      survey_done = 1;
   }
   else { // try single point survey again
      start_self_survey(0x00);
   }
}


void save_precise_posn(int force_save)
{
   // see if single precision TSIP message position will be close enough 
   // !!!! resolution_SMT does not update lla values during survey, can't do precise save
   if(force_save || (res_t == 2) || (1 && (float_error(precise_lat) < LAT_THRESH) && (float_error(precise_lon) < LON_THRESH))) {
      stop_self_survey();  // abort standard survey
      check_precise_posn = 0;
      set_lla((float) precise_lat, (float) precise_lon, (float) precise_alt);
      save_segment(7);     // save the position in EEPROM
      log_saved_posn(force_save);
      if(SCREEN_WIDTH > 800) lla_header("Precise survey complete  ", WHITE);
      else                   lla_header("Survey complete  ", WHITE);
      close_lla_file();
      survey_done = 1;
   }
   else {  // single/double precision difference is too big, do single point surveys until we get close
      set_survey_params(1, 0, 1L);  // config for single fix survey, don't save position
      start_self_survey(0x00);      // start the survey

      check_precise_posn = 1;       // we are looking for a fix very close to where we are
      check_delay = 4;
      plot_lla = 1;                 // prepare to plot the search attempts
      plot_azel = 1;
      if(WIDE_SCREEN == 0) {
         shared_plot = 1; 
         all_adevs = 0;
      }
      config_screen();
      redraw_screen();
      if(log_file) {
         fprintf(log_file, "# Starting save precise position surveys\n");
      }
      if(SCREEN_WIDTH > 800) lla_header("Saving precise position  ", YELLOW);
      else                   lla_header("Saving position    ", YELLOW);
   }
}

void abort_precise_survey()
{
float flat, flon, falt;

   if(check_precise_posn) {  // precise position found
      flat = (float) precise_lat;
      flon = (float) precise_lon;
      falt = (float) precise_alt;

      lat = (double) flat;
      lon = (double) flon;
      alt = (double) falt;
      save_precise_posn(2);  // save it using low res TSIP message
   }
   else if(precision_survey) {  // precise survey is incomplete
      if(precision_samples) {   // so save the average position
         precise_lat = lat_sum / (double)precision_samples;
         precise_lon = lon_sum / (double)precision_samples;
         precise_alt = alt_sum / (double)precision_samples;

         flat = (float) precise_lat;
         flon = (float) precise_lon;
         falt = (float) precise_alt;

         lat = (double) flat;
         lon = (double) flon;
         alt = (double) falt;
         save_precise_posn(3);
      }
   }
}

void plot_lla_point(int color)
{
double x,y;
int xi, yi;

   if(all_adevs && (WIDE_SCREEN == 0)) return;
   if(text_mode || (first_key && (SCREEN_HEIGHT < 600))) return;
   if(full_circle && (zoom_lla == 0)) return;
   if((SCREEN_HEIGHT < 600) && (shared_plot == 0)) return;

   y = (precise_lat - lat) * RAD_TO_DEG / ANGLE_SCALE;
   x = (lon - precise_lon) * RAD_TO_DEG / ANGLE_SCALE*cos_factor;
   if(y > LLA_SPAN) y = LLA_SPAN;
   if(y < -LLA_SPAN) y = -LLA_SPAN;
   if(x > LLA_SPAN) x = LLA_SPAN;
   if(x < -LLA_SPAN) x = -LLA_SPAN;

   yi = (int) (y * (double)lla_width / 2.0 / LLA_SPAN);
   xi = (int) (x * (double)lla_width / 2.0 / LLA_SPAN);
   dot(LLA_X+xi, LLA_Y+yi, color);

#ifdef BUFFER_LLA
   // save lla point in the buffer so we can redraw the screen later
   xi += (lla_width/2);
   yi += (lla_width/2);
   if((zoom_lla == 0) && (xi >= 0) && (xi < MAX_LLA_SIZE) && (yi >= 0) && (yi < MAX_LLA_SIZE)) {
      lla_data[xi][yi] = (u08) color;
   }
#endif
}

void lla_header(char *s, int color)
{
int xx;
int yy;

   // label the lat/lon/alt data block
   if(text_mode) {
      yy = MOUSE_ROW-1;
      xx = MOUSE_COL;
   }
   else {
      xx = LLA_COL/TEXT_WIDTH+1;
      yy = (LLA_ROW+LLA_SIZE)/TEXT_HEIGHT+0;
      if(SCREEN_WIDTH > 800) xx += 5;
      else                   xx -= 2;
   }
   vidstr(yy, xx, color, s);
}

#ifdef BUFFER_LLA

void redraw_lla_points()
{
int x, y;

   // redraw splatter plot from the saved data points
   if(zoom_lla) return;

   for(x=0; x<lla_width; x++) {
      for(y=0; y<lla_width; y++) {
         if(lla_data[x][y]) {
            dot(LLA_X+x-(lla_width/2),LLA_Y+y-(lla_width/2), lla_data[x][y]);
         }
      }
   }
}

void clear_lla_points()
{
int x, y;

   // clear splatter plot data buffer
   if(zoom_lla) return;

   for(x=0; x<=MAX_LLA_SIZE; x++) {
      for(y=0; y<=MAX_LLA_SIZE; y++) {
         lla_data[x][y] = 0;
      }
   }
}
#endif

void plot_lla_axes()
{
int x,y;
int xx,yy;
int color;

   if(plot_lla == 0) return;
   if(all_adevs && (WIDE_SCREEN == 0)) return;
   if(text_mode || (first_key && (SCREEN_HEIGHT < 600))) return;
   if(full_circle && (zoom_lla == 0)) return;
   if((SCREEN_HEIGHT < 600) && (shared_plot == 0)) return;

   erase_lla();

   if(text_mode || (SCREEN_WIDTH > 800)) {
      if(check_precise_posn) sprintf(out, "Saving precise position  ");
      else if(show_fixes) {
         if     (rcvr_mode == 4)     sprintf(out, "3D Fixes (%d %s/div from):     ", ((int)LLA_SPAN)*2/LLA_DIVISIONS, angle_units);
         else if(rcvr_mode == 3)     sprintf(out, "2D Fixes (%d %s/div from):     ", ((int)LLA_SPAN)*2/LLA_DIVISIONS, angle_units);
         else if(rcvr_mode == 0)     sprintf(out, "2D/3D Fixes (%d %s/div from):  ", ((int)LLA_SPAN)*2/LLA_DIVISIONS, angle_units);
         else if(configed_mode == 5) sprintf(out, "Mode %d Fixes (%d %s/div from):", configed_mode, ((int)LLA_SPAN)*2/LLA_DIVISIONS, angle_units);
         else                        sprintf(out, "Mode %d Fixes (%d %s/div from):", rcvr_mode, ((int)LLA_SPAN)*2/LLA_DIVISIONS, angle_units);
      }
      else if(reading_lla)           sprintf(out, "Read LLA (%d %s/div from):     ", ((int)LLA_SPAN)*2/LLA_DIVISIONS, angle_units);
      else                           sprintf(out, "Surveying (%d %s/div from):    ", ((int)LLA_SPAN)*2/LLA_DIVISIONS, angle_units);
   }
   else {
      if(check_precise_posn) sprintf(out, "Saving position    ");
      else                   sprintf(out, "%d %s/div from: ", ((int)LLA_SPAN)*2/LLA_DIVISIONS, angle_units);
   }
   lla_header(out, YELLOW);
   if(text_mode) return;  // plot area is in use for help/warning message

   x = LLA_X - LLA_SIZE/2 + LLA_MARGIN;
   y = LLA_Y - LLA_SIZE/2 + LLA_MARGIN;

   for(yy=y; yy<=y+lla_step*LLA_DIVISIONS; yy+=lla_step) {  // horizontals
     color = GREY;
     if(yy == y) color = WHITE;
     if(yy == y+lla_step*LLA_DIVISIONS) color = WHITE;
     line(x,yy, x+lla_step*LLA_DIVISIONS,yy, color);
   }

   for(xx=x; xx<=x+lla_step*LLA_DIVISIONS; xx+=lla_step) {  // verticals
     color = GREY;
     if(xx == x) color = WHITE;
     if(xx == x+lla_step*LLA_DIVISIONS) color = WHITE;
     line(xx,y+1,    xx,y+lla_step*LLA_DIVISIONS-1, color);
   }

   // draw the center cross
   xx = LLA_X;
   yy = LLA_Y;
   dot(xx+0, yy-0, WHITE);
   dot(xx+0, yy-1, WHITE);
   dot(xx+0, yy-2, WHITE);
   dot(xx+0, yy-3, WHITE);
   dot(xx+0, yy-4, WHITE);
   dot(xx+0, yy-5, WHITE);
   dot(xx+0, yy+0, WHITE);
   dot(xx+0, yy+1, WHITE);
   dot(xx+0, yy+2, WHITE);
   dot(xx+0, yy+3, WHITE);
   dot(xx+0, yy+4, WHITE);
   dot(xx+0, yy+5, WHITE);

   dot(xx+1, yy+0, WHITE);
   dot(xx+2, yy+0, WHITE);
   dot(xx+3, yy+0, WHITE);
   dot(xx+4, yy+0, WHITE);
   dot(xx+5, yy+0, WHITE);
   dot(xx-1, yy+0, WHITE);
   dot(xx-2, yy+0, WHITE);
   dot(xx-3, yy+0, WHITE);
   dot(xx-4, yy+0, WHITE);
   dot(xx-5, yy+0, WHITE);

   xx = LLA_COL/TEXT_WIDTH+1;
   if(SCREEN_WIDTH > 800) xx += 5;
   else                   xx -= 2;
   format_lla(precise_lat,precise_lon,precise_alt, (LLA_ROW+LLA_SIZE)/TEXT_HEIGHT+1, xx);

   #ifdef BUFFER_LLA
      redraw_lla_points();
   #endif
}


void calc_precise_lla()
{
u08 have_lla;

   // calculate simple average of all surveyed points
   have_lla = 0;
   if(precision_samples) {
      xlat = lat_sum / (double)precision_samples;
      xlon = lon_sum / (double)precision_samples;
      xalt = alt_sum / (double)precision_samples;
      precise_lat = xlat;
      precise_lon = xlon;
      precise_alt = xalt;

      xlat *= RAD_TO_DEG;
      xlon *= RAD_TO_DEG;
      if(lla_file) {
         fprintf(lla_file, "# Simple average of %.0lf points:\n", (double)precision_samples);
         fprintf(lla_file, "#   lat=%14.8lf  %.8lf\n",  xlat, (xlat-LAT_REF)/ANGLE_SCALE);
         fprintf(lla_file, "#   lon=%14.8lf  %.8lf\n",  xlon, (xlon-LON_REF)/ANGLE_SCALE*COS_FACTOR(xlat));
         fprintf(lla_file, "#   alt=%14.8lf  %.8lf\n",  xalt, (xalt-ALT_REF)*3.28);
         fprintf(lla_file, "#\n");
      }
      have_lla |= 0x01;
   }


   // calculate average of all 24 hour median intervals
   if(best_count && best_lat && best_lon && best_alt) {
      xlat = best_lat / best_count;
      xlon = best_lon / best_count;
      xalt = best_alt / best_count;
      precise_lat = xlat;
      precise_lon = xlon;
      precise_alt = xalt;

      xlat *= RAD_TO_DEG;
      xlon *= RAD_TO_DEG;
      if(lla_file) {
         fprintf(lla_file, "# REF: %.8lf  %.8lf  %.8lf  cos=%.8lf\n", LAT_REF, LON_REF, ALT_REF, COS_FACTOR(xlat));
         fprintf(lla_file, "# Average of %.0lf 24 hour medians:\n", best_count);
         fprintf(lla_file, "#   lat=%14.8lf  %.8lf\n",  xlat, (xlat-LAT_REF)/ANGLE_SCALE);
         fprintf(lla_file, "#   lon=%14.8lf  %.8lf\n",  xlon, (xlon-LON_REF)/ANGLE_SCALE*COS_FACTOR(xlat));
         fprintf(lla_file, "#   alt=%14.8lf  %.8lf\n",  xalt, (xalt-ALT_REF)*3.28);
      }
      have_lla |= 0x02;
   }

   if(have_lla) {            // we have a good idea where we are
      save_precise_posn(0);  // save it in the receiver (directly or via repeated single point surveys)
   }
}                             

void analyze_hours()
{
int i;
int j;
int k;
int interval;

   if(hour_lats == 0) return;

   // copy hourly medians to working array
   for(i=0; i<hour_lats; i++) {
      lat_bins[i] = lat_hr_bins[i];
      lon_bins[i] = lon_hr_bins[i];
      alt_bins[i] = alt_hr_bins[i];
   }

   minute_lats = minute_lons = minute_alts = 0;

   k = hour_lats;
   if(k > 24) k = 24;   // the size of the intervals we are analyzing (should be 24 hours)
   j = hour_lats - 24;
   if(j < 1) j = 1;     // the number of overlapping 24 hour intervals we have

   if(lla_file) fprintf(lla_file, "#\n# Processing %d %d hour intervals:\n", j, k);

   for(interval=0; interval<j; interval++) {
      if(lla_file) fprintf(lla_file, "#\n# Interval %d:\n", interval+1);

      hour_lats = hour_lons = hour_alts = 0;
      for(i=0; i<k; i++) {    // sort the interval's data into the median arrays
         hour_lats = add_to_bin(lat_bins[interval+i], lat_hr_bins, hour_lats);
         hour_lons = add_to_bin(lon_bins[interval+i], lon_hr_bins, hour_lons);
         hour_alts = add_to_bin(alt_bins[interval+i], alt_hr_bins, hour_alts);
      }

      if(debug_lla) {
         for(i=0; i<hour_lats; i++) {
            xlat = lat_hr_bins[i];
            xlon = lon_hr_bins[i];
            xalt = alt_hr_bins[i];

            xlat *= RAD_TO_DEG;
            xlon *= RAD_TO_DEG;
            if(lla_file) {
               fprintf(lla_file, "# Hour %d: lat=%14.8lf  %.8lf\n",  i, xlat, (xlat-LAT_REF)/ANGLE_SCALE);
               fprintf(lla_file, "# Hour %d: lon=%14.8lf  %.8lf\n",  i, xlon, (xlon-LON_REF)/ANGLE_SCALE*COS_FACTOR(xlat));
               fprintf(lla_file, "# Hour %d: alt=%14.8lf  %.8lf\n",  i, xalt, (xalt-ALT_REF)*3.28);
               fprintf(lla_file, "#\n");
            }
         }
      }

      // calculate the weighted median of the 24 hour lat/lon/alt data
      interp = (double) hour_lats * LAT_HR_TURN;
      i = (int) interp;
      if(i < (hour_lats-1)) {
         xlat = (lat_hr_bins[i+1]-lat_hr_bins[i]) * (interp-(double)i);
         xlat += lat_hr_bins[i];
         minute_lats = add_to_bin(xlat, lat_min_bins, minute_lats);
         best_lat += xlat;
         if(lla_file) fprintf(lla_file, "# BEST %d: lat=%14.8lf  %.8lf\n",  i, xlat*RAD_TO_DEG, (xlat*RAD_TO_DEG-LAT_REF)/ANGLE_SCALE);
      }

      interp = (double) hour_lons * LON_HR_TURN;
      i = (int) interp;
      if(i < (hour_lons-1)) {
         xlon = (lon_hr_bins[i+1]-lon_hr_bins[i]) * (interp-(double)i);
         xlon += lon_hr_bins[i];
         minute_lons = add_to_bin(xlon, lon_min_bins, minute_lons);
         best_lon += xlon;
         if(lla_file) fprintf(lla_file, "# BEST %d: lon=%14.8lf  %.8lf\n",  i, xlon*RAD_TO_DEG, (xlon*RAD_TO_DEG-LON_REF)/ANGLE_SCALE*COS_FACTOR(xlat*RAD_TO_DEG));
      }

      interp = (double) hour_alts * ALT_HR_TURN;
      i = (int) interp;
      if(i < (hour_alts-1)) {
         xalt = (alt_hr_bins[i+1]-alt_hr_bins[i]) * (interp-(double)i);
         xalt += alt_hr_bins[i];
         minute_alts = add_to_bin(xalt, alt_min_bins, minute_alts);
         best_alt += xalt;
         if(lla_file) fprintf(lla_file, "# BEST %d: alt=%14.8lf  %.8lf\n",  i, xalt,  (xalt-ALT_REF)*3.28);
      }
      if(lla_file) fprintf(lla_file, "#\n");

      ++best_count;
   }
}


int analyze_minutes()
{
int i;
int fault;

   fault = 0;

   interp = (double) minute_lats * LAT_MIN_TURN;
   i = (int) interp;
   if(i >= (minute_lats-1)) ++fault;
   else {
      xlat = (lat_min_bins[i+1]-lat_min_bins[i]) * (interp-(double)i);
      xlat += lat_min_bins[i];
   }

   interp = (double) minute_lons * LON_MIN_TURN;
   i = (int) interp;
   if(i >= (minute_lons-1)) ++fault;
   else {
      xlon = (lon_min_bins[i+1]-lon_min_bins[i]) * (interp-(double)i);
      xlon += lon_min_bins[i];
   }

   interp = (double) minute_alts * ALT_MIN_TURN;
   i = (int) interp;
   if(i >= (minute_alts-1)) ++fault;
   else {
      xalt = (alt_min_bins[i+1]-alt_min_bins[i]) * (interp-(double)i);
      xalt += alt_min_bins[i];
   }

   minute_lats = minute_lons = minute_alts = 0;
   while(hour_tow <= this_tow) hour_tow += 60L*60L;

   if((fault == 0) && (hour_lats < SURVEY_BIN_COUNT)) {
      lat_hr_bins[hour_lats++] = xlat;
      lon_hr_bins[hour_lons++] = xlon;
      alt_hr_bins[hour_alts++] = xalt;
      if(hour_lats >= PRECISE_SURVEY_HOURS) return 1;  // precise survey is complete

      if(erase_every_hour) {  // clear screen to prepare for the next hour of data
         if(precision_samples) {  // calculate new center point of plot based upon sample average
            precise_lat = lat_sum / precision_samples;
            precise_lon = lon_sum / precision_samples;
            precise_alt = alt_sum / precision_samples;
         }
         else {   // or based upon last hour's median point
            precise_lat = xlat;
            precise_lon = xlon;
            precise_alt = xalt;
         }

         redraw_screen();
      }

      sprintf(out, "hr  %2d: %.8lf %.8lf %.3lf     ", 
         hour_lats, xlat*RAD_TO_DEG, xlon*RAD_TO_DEG,xalt);
      vidstr(MOUSE_ROW+2, MOUSE_COL, SURVEY_COLOR, out);
   }

   return 0;
}


int analyze_seconds()
{
int i;
int fault;

   fault = 0;

   interp = (double) second_lats * LAT_SEC_TURN;
   i = (int) interp;
   if((i+1) >= second_lats) ++fault;
   else {
      xlat = (lat_bins[i+1]-lat_bins[i]) * (interp-(double)i);
      xlat += lat_bins[i];
   }

   interp = (double) second_lons * LON_SEC_TURN;
   i = (int) interp;
   if((i+1) >= second_lats) ++fault;
   else {
      xlon = (lon_bins[i+1]-lon_bins[i]) * (interp-(double)i);
      xlon += lon_bins[i];
   }

   interp = (double) second_alts * ALT_SEC_TURN;
   i = (int) interp;
   if((i+1) >= second_lats) ++fault;
   else {
      xalt = (alt_bins[i+1]-alt_bins[i]) * (interp-(double)i);
      xalt += alt_bins[i];
   }

   if((fault == 0) && (second_lats >= 60)) {  // we have a full minute of data
      minute_lats = add_to_bin(xlat, lat_min_bins, minute_lats);
      minute_lons = add_to_bin(xlon, lon_min_bins, minute_lons);
      minute_alts = add_to_bin(xalt, alt_min_bins, minute_alts);
sprintf(out, "min %2d: %.8lf %.8lf %.3lf     ", minute_lats, xlat*RAD_TO_DEG, xlon*RAD_TO_DEG, xalt);
vidstr(MOUSE_ROW+1, MOUSE_COL, SURVEY_COLOR, out);
   }

   // prepare for the next minute
   second_lats = second_lons = second_alts = 0;
   while(minute_tow <= this_tow) minute_tow += 60L;

   if(this_tow >= hour_tow) {  // we have 60 minutes of data
      return analyze_minutes();
   }

   return 0;
}

int add_survey_point()
{
   if(gps_status != 0) return 0;

   lat_sum += lat;
   lon_sum += lon;
   alt_sum += alt;
   ++precision_samples;
//if(precision_samples > do_survey) return 1;  //!!! debug statement

   second_lats = add_to_bin(lat, lat_bins, second_lats);
   second_lons = add_to_bin(lon, lon_bins, second_lons);
   second_alts = add_to_bin(alt, alt_bins, second_alts);
sprintf(out, "sec %2d: %.8lf %.8lf %.3lf     ", second_lats, lat*RAD_TO_DEG, lon*RAD_TO_DEG, alt);
vidstr(MOUSE_ROW+0, MOUSE_COL, SURVEY_COLOR, out);

   while(this_tow < start_tow) this_tow += (7L*24L*60L*60L);
   if(this_tow >= minute_tow) {  // we have 60 seconds of data
      ++survey_minutes;
      return analyze_seconds();
   }

   return 0;
}

void update_precise_survey()
{
int color;

   if(precision_survey || check_precise_posn || show_fixes) {  // plot the points
      if((gps_status == 0) && plot_lla) {  // plot data
         color = precision_samples / 3600L;
         color %= 14;
         plot_lla_point(color+1);
      }
   }

   if(precision_survey) {
      if(add_survey_point()) {     // precision survey has completed
         stop_precision_survey();  // stop doing the precison survey
         analyze_hours();          // analyze the hourly data
         calc_precise_lla();       // figure up where we are
      }
   }
   else {
      if(show_fixes) ++precision_samples;
   }
}
#endif  // PRECISE_STUFF


//
//
//   Allan deviation stuff
//
//
#ifdef ADEV_STUFF

//
// Get next tau to calculate. Some tools calculate Allan deviation
// for only one point per decade; others two or three. Below are
// several unique alternatives that produce cleaner-looking plots.
//

long next_tau(long tau, int bins)
{
long pow10;
long n;

    switch (bins) {
       case 0 :    // all tau (not practical)
           return tau + 1;

       case 1 :    // one per decade
           return tau * 10;

       case 2 :    // one per octave
           return tau * 2;

       case 3 :    // 3 dB
       case 4 :    // 1-2-4 decade
       case 5 :    // 1-2-5 decade
       case 10 :   // ten per decade
           pow10 = 1;
           while(tau >= 10) {
               pow10 *= 10;
               tau /= 10;
           }

           if(bins == 3) {
               return ((tau == 3) ? 10 : 3) * pow10;
           }
           if((bins == 4) && (tau == 4)) {
               return 10 * pow10;
           }
           if((bins == 5) && (tau == 2)) {
               return 5 * pow10;
           }
           if(bins == 10) {
               return (tau + 1) * pow10;
           }
           return tau * 2 * pow10;

       case 29 :    // 29 nice round numbers per decade
           pow10 = 1;
           while(tau > 100) {
               pow10 *= 10;
               tau /= 10;
           }

           if(tau < 22) {
               return (tau + 1) * pow10;
           } 
           else if(tau < 40) {
               return (tau + 2) * pow10;
           } 
           else if(tau < 60) {
               return (tau + 5) * pow10;
           } 
           else {
               return (tau + 10) * pow10;
           }

       default :   // logarithmically evenly spaced divisions
           n = (long) (log10((double) tau) * (double)bins + 0.5) + 1;
           n = (long) pow(10.0, (double)n / (double)bins);
           return (n > tau) ? n : tau + 1;
    }
}

void reset_incr_bins(struct BIN *bins)
{
int b;
S32 m;
struct BIN *B;

   m = 1L;
   for(b=0; b<n_bins; b++) {  // flush adev bin data
      B = &bins[b];

      B->m     = m;
      B->n     = 0;
      B->sum   = 0.0;
      B->value = 0.0;
      B->tau   = ((double) m) * adev_period;
      B->accum = 0.0;
      B->i     = 0;
      B->j     = 0;
      B->init  = 0;

      m = next_tau(m, bin_scale);
   }
}

void reset_adev_bins()
{
   adev_q_overflow = 0.0;

   // reset the incremental adev bins
   reset_incr_bins(&pps_adev_bins[0]);
   reset_incr_bins(&osc_adev_bins[0]);
   reset_incr_bins(&pps_hdev_bins[0]);
   reset_incr_bins(&osc_hdev_bins[0]);
   reset_incr_bins(&pps_mdev_bins[0]);
   reset_incr_bins(&osc_mdev_bins[0]);

   // This code works for 1-2-5 adev decades.  Should be generalized to
   // work with any bin density.
   if     (adev_q_size <= 10L)       max_adev_rows = 2;
   else if(adev_q_size <= 20L)       max_adev_rows = 3;
   else if(adev_q_size <= 40L)       max_adev_rows = 4;
   else if(adev_q_size <= 100L)      max_adev_rows = 5;
   else if(adev_q_size <= 200L)      max_adev_rows = 6;
   else if(adev_q_size <= 400L)      max_adev_rows = 7;
   else if(adev_q_size <= 1000L)     max_adev_rows = 8;
   else if(adev_q_size <= 2000L)     max_adev_rows = 9; 
   else if(adev_q_size <= 4000L)     max_adev_rows = 10;
   else if(adev_q_size <= 10000L)    max_adev_rows = 11;
   else if(adev_q_size <= 20000L)    max_adev_rows = 12;
   else if(adev_q_size <= 40000L)    max_adev_rows = 13;
   else if(adev_q_size <= 100000L)   max_adev_rows = 14;
   else if(adev_q_size <= 200000L)   max_adev_rows = 15;
   else if(adev_q_size <= 400000L)   max_adev_rows = 16;
   else if(adev_q_size <= 1000000L)  max_adev_rows = 17;
   else if(adev_q_size <= 2000000L)  max_adev_rows = 18;
   else if(adev_q_size <= 4000000L)  max_adev_rows = 19;
   else if(adev_q_size <= 10000000L) max_adev_rows = 20;
   else if(adev_q_size <= 20000000L) max_adev_rows = 21;
   else if(adev_q_size <= 40000000L) max_adev_rows = 22;
   else                              max_adev_rows = 23;
}

float adev_decade(float val)
{
float decade;

    for(decade=1.0E-0F; decade>=1.0E-19F; decade/=10.0F) {
       if(val > decade) break;
    }
    return decade*10.0F;
}


float scale_adev(float val)
{
   // convert adev value into an decade_exponent.mantissa value
   // Warning: due to subtle rounding/truncation issues,  change
   //          anything in this routine at your own risk...
   if(val > 1.0e-0F) {
      val = (float) (0.0F);
   }
   else if(val > 1.0e-1F) {
      val = (float) ((-1.0F) + (val/1.0e-0F));
   }
   else if(val > 1.0e-2F) {
      val = (float) ((-2.0F) + (val/1.0e-1F));
   }
   else if(val > 1.0e-3F) {
      val = (float) ((-3.0F) + (val/1.0e-2F));
   }
   else if(val > 1.0e-4F) {
      val = (float) ((-4.0F) + (val/1.0e-3F));
   }
   else if(val > 1.0e-5F) {
      val = (float) ((-5.0F) + (val/1.0e-4F));
   }
   else if(val > 1.0e-6F) {
      val = (float) ((-6.0F) + (val/1.0e-5F));
   }
   else if(val > 1.0e-7F) {
      val = (float) ((-7.0F) + (val/1.0e-6F));
   }
   else if(val > 1.0e-8F) {
      val = (float) ((-8.0F) + (val/1.0e-7F));
   }
   else if(val > 1.0e-9F) {
      val = (float) ((-9.0F) + (val/1.0e-8F));
   }
   else if(val > 1.0e-10F) {
      val = (float) ((-10.0F) + (val/1.0e-9F));
   }
   else if(val > 1.0e-11F) {
      val = (float) ((-11.0F) + (val/1.0e-10F));
   }
   else if(val > 1.0e-12F) {
      val = (float) ((-12.0F) + (val/1.0e-11F));
   }
   else if(val > 1.0e-13F) {
      val = (float) ((-13.0F) + (val/1.0e-12F));
   }
   else if(val > 1.0e-14F) {
      val = (float) ((-14.0F) + (val/1.0e-13F));
   }
   else if(val > 1.0e-15F) {
      val = (float) ((-15.0F) + (val/1.0e-14F));
   }
   else if(val > 1.0e-16F) {
      val = (float) ((-16.0F) + (val/1.0e-15F));
   }
   else if(val > 1.0e-17F) {
      val = (float) ((-17.0F) + (val/1.0e-16F));
   }
   else if(val > 1.0e-18F) {
      val = (float) ((-18.0F) + (val/1.0e-17F));
   }
   else if(val > 1.0e-19F) {
      val = (float) ((-19.0F) + (val/1.0e-18F));
   }
   else val = (float) (-20.0F);

   return val;
}


void adev_mouse()
{
u08 old_disable_kbd;

   // keep mouse lively during long periods of thinking
   if((++adev_mouse_time & 0x3FFF) != 0x0000) return;
   update_pwm();
   if(mouse_shown == 0) return;

   old_disable_kbd = disable_kbd;
   if(kbd_flag) disable_kbd = 2; // (so that do_kbd() won't do anything when it's called by WM_CHAR during get_pending_gps())

   show_mouse_info();

   disable_kbd = old_disable_kbd;
}

void do_incr_adevs()
{
   // update each of the adev bins with the latest queue data
   incr_adev(PPS_ADEV, &pps_adev_bins[0]);
   incr_adev(OSC_ADEV, &osc_adev_bins[0]);

   incr_hdev(PPS_HDEV, &pps_hdev_bins[0]);
   incr_hdev(OSC_HDEV, &osc_hdev_bins[0]);

   incr_mdev(PPS_MDEV, &pps_mdev_bins[0]);
   incr_mdev(OSC_MDEV, &osc_mdev_bins[0]);
}

void reload_adev_info()
{
   // recalculate all the adevs from scratch
   reset_adev_bins();  // reset the bins
   do_incr_adevs();    // recalculate all the adevs from the queued data
}

void incr_overflow()
{
int b;

   // tweek bin data counts when the adev queue is full
   adev_q_overflow += 1.0;

   for(b=0; b<n_bins; b++) {  
      pps_adev_bins[b].n--;
      pps_adev_bins[b].j--;
      osc_adev_bins[b].n--;
      osc_adev_bins[b].j--;

      pps_hdev_bins[b].n--;
      pps_hdev_bins[b].j--;
      osc_hdev_bins[b].n--;
      osc_hdev_bins[b].j--;

      pps_mdev_bins[b].n--;
      pps_mdev_bins[b].j--;
      osc_mdev_bins[b].n--;
      osc_mdev_bins[b].j--;
   }
}

void add_adev_point(double osc_offset, double pps_offset)
{
struct ADEV_Q q;

   if((subtract_base_value == 1) && (adev_q_count == 0)) {
      pps_base_value = pps_offset;
      osc_base_value = osc_offset;
   }

   q.pps = (OFS_SIZE) (pps_offset - pps_base_value);   // place data into adev queue
   q.osc = (OFS_SIZE) (osc_offset - osc_base_value);
   put_adev_q(adev_q_in, q);

   if(++adev_q_in >= adev_q_size) {  // queue has wrapped
      adev_q_in = 0;
   }

   if(adev_q_in == adev_q_out) {  // queue is full
      ++adev_q_out;               // drop oldest entry from the queue
      incr_overflow();            // tweek counts in the adev bins

      // Once the adev queue fills up,  the adev results begin to get stale
      // because the incremental adevs are based upon all the points seen in
      // the past.  To keep the adev numbers fresh, we preiodically reset
      // the incremental adev bins.  This causes the adevs to be
      // recalculated from just the values stored in the queue.
      if(keep_adevs_fresh && (adev_q_overflow > adev_q_size)) {
         reset_adev_bins();              
      }
   }
   else ++adev_q_count;   // keep count of number of entries in the adev queue
   if(adev_q_out >= adev_q_size) adev_q_out = 0;

   // incrementally update the adev bin values with the new data point
   do_incr_adevs();
}


double get_adev_point(u08 id, long i)
{
struct ADEV_Q q;
double val;
#define INTERVAL ((OFS_SIZE) 0.0)  //1.0

   i = adev_q_out + i;
   if(i >= adev_q_size) i -= adev_q_size;

   q = get_adev_q(i);

   if(id & 0x01) {   // return PPS value
      val = INTERVAL + (1.0e-9 * ((double)q.pps+pps_base_value));
   }
   else {            // return OSC value
      val = INTERVAL + ((100.0 * 1.0e-9) * ((double)q.osc+osc_base_value));
   }
   return val;
}

//
// For given sample interval (tau) compute the Allan deviation.
// Nole: all Allan deviations are of the overlapping type.
//

void incr_adev(u08 id, struct BIN *bins)
{
S32 b;
S32 t1,t2;
struct BIN *B;
double v;
int vis_bins;

   vis_bins = 0;

   for(b=0; b<n_bins; b++) {
      B = &bins[b];
      if(B->n < 0) break;

      t1 = B->m;
      t2 = t1 + t1;

//    if((B->n+t2) >= adev_q_count) break;
      if((B->n+t2) > adev_q_count) break;

      while((B->n+t2) < adev_q_count) {
         v =  get_adev_point(id, B->n+t2);
         v -= get_adev_point(id, B->n+t1) * 2.0;
         v += get_adev_point(id, B->n);

         B->sum += (v * v);
         B->n++;
         adev_mouse();
      }

      if(B->n >= min_points_per_bin) {
         B->value = sqrt(B->sum / (2.0 * ((double) B->n + adev_q_overflow))) / B->tau;
         ++vis_bins;
      }
   }

   if(vis_bins > max_adev_rows) max_adev_rows = vis_bins;
}

void incr_hdev(u08 id, struct BIN *bins)
{
S32 b;
S32 t1,t2,t3;
struct BIN *B;
double v;
int vis_bins;

   vis_bins = 0;

   for(b=0; b<n_bins; b++) {
      B = &bins[b];
      if(B->n < 0) break;

      t1 = B->m;
      t2 = t1 + t1;
      t3 = t1 + t1 + t1;

//    if((B->n+t3) >= adev_q_count) break;
      if((B->n+t3) > adev_q_count) break;

      while((B->n+t3) < adev_q_count) {
         v =  get_adev_point(id, B->n+t3);
         v -= get_adev_point(id, B->n+t2) * 3.0;
         v += get_adev_point(id, B->n+t1) * 3.0;
         v -= get_adev_point(id, B->n);

         B->sum += (v * v);
         B->n++;
         adev_mouse();
      }

      if(B->n >= min_points_per_bin) {
         B->value = sqrt(B->sum / (6.0 * ((double) B->n + adev_q_overflow))) / B->tau;
         ++vis_bins;
      }
   }

   if(vis_bins > max_adev_rows) max_adev_rows = vis_bins;
}

void incr_mdev(u08 id, struct BIN *bins)
{
S32 b;
S32 t1,t2,t3;
struct BIN *B;
double divisor;
int vis_bins;
double tdev;

   vis_bins = 0;

   for(b=0; b<n_bins; b++) {
      B = &bins[b];
      if(B->n < 0) break;
      if(B->j < 0) break;
      if(B->i < 0) break;

      t1 = B->m;
      t2 = t1 + t1;
      t3 = t1 + t1 + t1;

//    if((B->j+t3) >= adev_q_count) break;
      if((B->j+t3) > adev_q_count) break;

      while(((B->i+t2) < adev_q_count) && (B->i < t1)) {
         B->accum += get_adev_point(id, B->i+t2);
         B->accum -= get_adev_point(id, B->i+t1) * 2.0;
         B->accum += get_adev_point(id, B->i);
         B->i++;
         adev_mouse();
      }

      if(B->init == 0) {
         B->sum += (B->accum * B->accum);
         B->n++;
         B->init = 1;
      }

      while((B->j+t3) < adev_q_count) {
         B->accum += get_adev_point(id, B->j+t3);
         B->accum -= get_adev_point(id, B->j+t2) * 3.0;
         B->accum += get_adev_point(id, B->j+t1) * 3.0;
         B->accum -= get_adev_point(id, B->j);

         B->sum += (B->accum * B->accum);
         B->j++;
         B->n++;
         adev_mouse();
      }

      if(B->n >= min_points_per_bin) {
         divisor = (double) B->m * B->tau;
         if(divisor != 0.0) {
            B->value = sqrt(B->sum / (2.0 * ((double) B->n + adev_q_overflow))) / divisor;
            tdev = B->value * B->tau / SQRT3;
         }
         ++vis_bins;
      }
   }

   if(vis_bins > max_adev_rows) max_adev_rows = vis_bins;
}

int fetch_adev_info(u08 dev_id, struct ADEV_INFO *bins)
{
double adev;
double tau;
long on;
long q_count;
struct BIN *table;

    if     (dev_id == PPS_ADEV) table = &pps_adev_bins[0];
    else if(dev_id == OSC_ADEV) table = &osc_adev_bins[0];
    else if(dev_id == PPS_HDEV) table = &pps_hdev_bins[0];
    else if(dev_id == OSC_HDEV) table = &osc_hdev_bins[0];
    else if(dev_id == PPS_MDEV) table = &pps_mdev_bins[0];
    else if(dev_id == OSC_MDEV) table = &osc_mdev_bins[0];
    else if(dev_id == PPS_TDEV) table = &pps_mdev_bins[0];
    else if(dev_id == OSC_TDEV) table = &osc_mdev_bins[0];
    else {
        sprintf(out, "Bad dev_id in calc_adevs: %d\n", dev_id);
        error_exit(92, out);
    }

    bins->adev_min = 1.0e29F;
    bins->adev_max = (-1.0e29F);
    bins->bin_count = 0;
    bins->adev_type = dev_id;
    q_count = adev_q_count;

    // convert data from the incremental adev bins into the
    // old style adev tables (so we don't have to mess with changing
    // all that old well-debugged display code).
    while(bins->bin_count < ADEVS) {
        on = table[bins->bin_count].n;
        if(on < min_points_per_bin) break;

        tau = (double) table[bins->bin_count].tau;

        adev = table[bins->bin_count].value;
        if((dev_id == PPS_TDEV) || (dev_id == OSC_TDEV)) {
           adev = adev * tau / SQRT3;
        }

        bins->adev_on[bins->bin_count] = on;
        bins->adev_taus[bins->bin_count] = (float) tau;
        bins->adev_bins[bins->bin_count] = (float) adev;

        bins->bin_count += 1;
        if(adev > bins->adev_max) bins->adev_max = (float) adev;
        if(adev < bins->adev_min) bins->adev_min = (float) adev;
    }

    if(1 && global_adev_max) {  // give all adev plots the same top decade
       bins->adev_max = (float) global_adev_max;
    }

    bins->adev_max = adev_decade(bins->adev_max);

    // !!! force tidy ADEV graph scale factors
    if((PLOT_HEIGHT/VERT_MAJOR) <= 6) {  // three decades (/VS - makes for a cramped plot) 
       bins->adev_min = bins->adev_max * 1.0e-6F;
    }
    else if((PLOT_HEIGHT/VERT_MAJOR) <= 8) {  // four decades
       bins->adev_min = bins->adev_max * 1.0e-4F;
    }
    else if((PLOT_HEIGHT/VERT_MAJOR) <= 10) {  // five decades (/VM)
       bins->adev_min = bins->adev_max * 1.0e-5F;
    }
    else if((PLOT_HEIGHT/VERT_MAJOR) <= 12) {  // six decades
       bins->adev_min = bins->adev_max * 1.0e-6F;
    }
    else if((PLOT_HEIGHT/VERT_MAJOR) <= 14) {  // seven decades
       bins->adev_min = bins->adev_max * 1.0e-7F;
    }
    else if((PLOT_HEIGHT/VERT_MAJOR) <= 16) {  // eight decades (/VL)
       bins->adev_min = bins->adev_max * 1.0e-8F;
    }
    else if((PLOT_HEIGHT/VERT_MAJOR) <= 18) {  // nine decades
       bins->adev_min = bins->adev_max * 1.0e-9F;
    }
    else { // ten decades?
       bins->adev_min = bins->adev_max * 1.0e-10F;
    }

    if((PLOT_HEIGHT/VERT_MAJOR) & 0x01) {  // odd number of decade bins are off-centered
       bins->adev_max /= 2.0F;
       bins->adev_min *= 5.0F;
    }

    return bins->bin_count;
}


void reload_adev_queue()
{
long i;
long counter;
long val;
struct PLOT_Q q;
int dump_size;

   reset_queues(0x01);   // clear the adev queue

   pause_data = 1;
   counter = 0;

   dump_size = 'p';
   if(dump_size == 'p') i = plot_q_col0;  // dumping the plot area's data
   else                 i = plot_q_out;   // dumping the full queue

   while(i != plot_q_in) {
      q = get_plot_q(i);

      pps_offset = q.data[PPS] / (OFS_SIZE) queue_interval;
      osc_offset = q.data[OSC] / (OFS_SIZE) queue_interval;
      add_adev_point(osc_offset, pps_offset);

      if((++counter & 0xFFF) == 0x0000) {   // keep serial data from overruning
         get_pending_gps();  //!!!! possible recursion
      }
      if((counter % 1000L) == 1L) {
         sprintf(out, "Line %ld", counter-1L);
         vidstr(PLOT_TEXT_ROW+4+2, PLOT_TEXT_COL, PROMPT_COLOR, out);
         refresh_page();
      }

      if(dump_size == 'p') {
         val = view_interval * (long) PLOT_WIDTH;
         val /= (long) plot_mag;
         if(counter >= val) break;
      }
      if(++i >= plot_q_size) i = 0;
   }

   force_adev_redraw();
   redraw_screen();
}

//
//   Allan deviation table output and plotting stuff
//

void show_adev_table(struct ADEV_INFO *bins, int adev_row, int adev_col, u08 color)
{
int i;
char *d;
char *t;
int adev_bottom_row;

   if(text_mode) {
      if(first_key) return;
      if(all_adevs == 0) return;     // no room on screen to do this
   }
   if(full_circle) return;
   if(bins->bin_count <= 0) return;              // nothing to show
   if(adev_row >= ((PLOT_TEXT_ROW) - 1)) return; // table starts in plot area
   if(SCREEN_WIDTH < ADEV_AZEL_THRESH) {  // no room for both a map and adev tables
      if(all_adevs == 0) {
         if(shared_plot) {
            if(plot_watch && (plot_azel || plot_signals)) return;
         }
         else {  // adev table area may be in use by something else
            if(plot_azel)    return;
            if(plot_signals) return;
            if(plot_watch)   return;
         }
         if(precision_survey) return;
         if(check_precise_posn) return;
         if(show_fixes) return;
         if(survey_done) return;
      }
   }

   if((bins->adev_bins[0] == 0.0) && (bins->adev_bins[1] == 0.0)) return;

   adev_bottom_row  = PLOT_TEXT_ROW - 1;
   adev_bottom_row -= (TEXT_Y_MARGIN+TEXT_HEIGHT-1)/TEXT_HEIGHT;
   if(extra_plots) adev_bottom_row -= 2;

   if(bins->adev_type & 0x01) d = "PPS";
   else                       d = "OSC";

   if     (bins->adev_type == OSC_ADEV) t = "ADEV";
   else if(bins->adev_type == PPS_ADEV) t = "ADEV";
   else if(bins->adev_type == OSC_MDEV) t = "MDEV";
   else if(bins->adev_type == PPS_MDEV) t = "MDEV";
   else if(bins->adev_type == OSC_HDEV) t = "HDEV";
   else if(bins->adev_type == PPS_HDEV) t = "HDEV";
   else if(bins->adev_type == OSC_TDEV) t = "TDEV";
   else if(bins->adev_type == PPS_TDEV) t = "TDEV";
   else t="????";

   blank_underscore = 1; // '_' char is used for formatting columns,  convert to blank

   if((SCREEN_WIDTH < 800) && (all_adevs == 0)) {  // no room for ADEV tables
   }
   else if(all_adevs || (SCREEN_WIDTH < 1024)) {  // 800x600 - short version of tables
      sprintf(out, "%s %s: %lu pts", d, t, adev_q_count+(long) adev_q_overflow);
      vidstr(adev_row, adev_col, color, out);
      for(i=0; i<bins->bin_count; i++) {
         ++adev_row;
         if(adev_row >= adev_bottom_row) {
            blank_underscore = 0;
            return;
         }
         #ifdef VARIABLE_FONT
            if(all_adevs && (SCREEN_WIDTH >= 1024) && (text_mode == 0)) {
               print_using = "000000_t 0.0000e-000 (n=0)";
               // note: leading space needed for print_using to format correctly
               if(adev_period < 1.0F) sprintf(out, " %.3f_t %.2le (n=%ld)   ", bins->adev_taus[i], bins->adev_bins[i], bins->adev_on[i]);
               else sprintf(out, " %ld_t %.2le (n=%ld)   ", (long) bins->adev_taus[i], bins->adev_bins[i], bins->adev_on[i]);
            }
            else {
               print_using = "000000_t 0.0000e-000";
               // note: leading space needed for print_using to format correctly
               if(adev_period < 1.0F) sprintf(out, " %.3f_t %.2le   ", bins->adev_taus[i], bins->adev_bins[i]);
               else sprintf(out, " %ld_t %.2le   ", (long) bins->adev_taus[i], bins->adev_bins[i]);
            }
            vidstr(adev_row, adev_col, color, out);
            print_using = 0;
         #endif
         #ifdef FIXED_FONT
            if(all_adevs && (SCREEN_WIDTH >= 1024) && (text_mode == 0)) {
               if(adev_period < 1.0F) sprintf(out, "%.3f t %.4le (n=%ld)", bins->adev_taus[i], bins->adev_bins[i], bins->adev_on[i]);
               else sprintf(out, "%6ld t %.4le (n=%ld)", (long) bins->adev_taus[i], bins->adev_bins[i], bins->adev_on[i]);
            }
            else {
               if(1 || (SCREEN_WIDTH < 800)) {
                  if(adev_period < 1.0F) sprintf(out, "%.3ft %.3le", bins->adev_taus[i], bins->adev_bins[i]);
                  else sprintf(out, "%6ldt %.3le", (long) bins->adev_taus[i], bins->adev_bins[i]);
               }
               else {
                  if(adev_period < 1.0F) sprintf(out, "%.3f t %.3le", bins->adev_taus[i], bins->adev_bins[i]);
                  else sprintf(out, "%6ld t %.3le", (long) bins->adev_taus[i], bins->adev_bins[i]);
               }
            }
            vidstr(adev_row, adev_col, color, out);
         #endif
      }

      while(i++ < max_adev_rows) {  // erase the rest of the table area
         ++adev_row;
         if(adev_row >= adev_bottom_row) {
            blank_underscore = 0;
            return;
         }

         if(small_font == 1) vidstr(adev_row, adev_col, color, &blanks[TEXT_COLS-32]);
         else                vidstr(adev_row, adev_col, color, &blanks[TEXT_COLS-20]);
      }
   }
   else {
      sprintf(out, "%s %s over %lu points", d, t, adev_q_count+(long) adev_q_overflow);
      vidstr(adev_row, adev_col+7, color, out);
      for(i=0; i<bins->bin_count; i++) {  // draw the adev table entries
         ++adev_row;
         if(adev_row >= adev_bottom_row) {
            blank_underscore = 0;
            return;
         }

         if(small_font == 1) vidstr(adev_row, adev_col+4, color, &blanks[TEXT_COLS-64]);
         else                vidstr(adev_row, adev_col+4, color, &blanks[TEXT_COLS-36]);

         #ifdef VARIABLE_FONT
            print_using = "00000000_tau  0.0000e-000 (n=0)";
            // note: leading space needed for print_using to format correctly
            if(adev_period < 1.0F) sprintf(out, " %.3f_tau_  %.4le (n=%ld)  ", bins->adev_taus[i], bins->adev_bins[i], bins->adev_on[i]);
            else sprintf(out, " %ld_tau_  %.4le (n=%ld)  ", (long) bins->adev_taus[i], bins->adev_bins[i], bins->adev_on[i]);
            vidstr(adev_row, adev_col+4, color, out);
            print_using = 0;
         #endif
         #ifdef FIXED_FONT
            if(adev_period < 1.0F) sprintf(out, "%.4f tau  %.3le (n=%ld)", bins->adev_taus[i], bins->adev_bins[i], bins->adev_on[i]);
            else sprintf(out, "%8ld tau  %.4le (n=%ld)", (long) bins->adev_taus[i], bins->adev_bins[i], bins->adev_on[i]);
            vidstr(adev_row, adev_col+4, color, out);
         #endif
      }

      while(i++ < max_adev_rows) {  // erase the rest of the table area
         ++adev_row;
         if(adev_row >= adev_bottom_row) {
            blank_underscore = 0;
            return;
         }
         if(small_font == 1) vidstr(adev_row, adev_col+4, color, &blanks[TEXT_COLS-64]);
         else                vidstr(adev_row, adev_col+4, color, &blanks[TEXT_COLS-36]);
      }
   }

   blank_underscore = 0;
   return;
}


void plot_adev_curve(struct ADEV_INFO *bins, u08 color)
{
int i;
int x1, x2;
float y1, y2;
float max_scale;
float min_scale;
float adev_range;
int vert_scale;
int vert_ofs;

   if(text_mode) return;  // no room on screen to do this
   if(full_circle) return;
   if(first_key) return;
   if(plot_adev_data == 0) return;

   max_scale = scale_adev(bins->adev_max);
   min_scale = scale_adev(bins->adev_min);
   adev_range = max_scale - min_scale;
   if(adev_range == 0.0) return;
   adev_range = (float) ABS(adev_range);

   // adev plots are scaled to major vertical divisions
   vert_scale = (PLOT_HEIGHT / (VERT_MAJOR*2)) * (VERT_MAJOR*2);  // two major divisions per decade
   vert_ofs = (PLOT_HEIGHT - vert_scale) / 2;
   if(((PLOT_HEIGHT / (VERT_MAJOR*2)) & 0x01) == 0x00) {  // align adevs to cyan dotted lines
      vert_ofs += VERT_MAJOR;
   }

   for(i=1; i<bins->bin_count; i++) {
      y1 = (0.0F - scale_adev(bins->adev_bins[i-1]));
      y1 += max_scale;
      y1 /= adev_range;
      y1 *= vert_scale;
      y1 += (PLOT_ROW + vert_ofs);
      if(y1 < PLOT_ROW) y1 = (float) PLOT_ROW;
      if(y1 >= (PLOT_ROW+PLOT_HEIGHT)) y1 = (float) (PLOT_ROW+PLOT_HEIGHT);

      y2 = (0.0F - scale_adev(bins->adev_bins[i]));
      y2 += max_scale;
      y2 /= adev_range;
      y2 *= vert_scale;
      y2 += (PLOT_ROW+vert_ofs);
      if(y2 < PLOT_ROW) y2 = (float) PLOT_ROW;
      if(y2 >= (PLOT_ROW+PLOT_HEIGHT)) y2 = (float) (PLOT_ROW+PLOT_HEIGHT);

      x1 = (i-1) * HORIZ_MAJOR;   // make sure line fits in the plot
      x2 = i * HORIZ_MAJOR;
      if(x2 > PLOT_WIDTH) x2 = PLOT_WIDTH;

      if(x1 < PLOT_WIDTH) {
         line(PLOT_COL+x1, (int) y1, PLOT_COL+x2, (int) y2, color);
      }
   }
}

void scan_bins(struct BIN *bin)
{
int i;

   // find the min and max adev values in the specified adev bins
   for(i=0; i<ADEVS; i++) {
      if(bin[i].n < min_points_per_bin) break;
      if(bin[i].value == 0.0) continue;
      if(bin[i].value > global_adev_max) global_adev_max = bin[i].value;
      if(bin[i].value < global_adev_min) global_adev_min = bin[i].value;
   }
}

void find_global_max()
{
   // find the min and max adev values in all the adev bins
   global_adev_max = (-1.0E29);
   global_adev_min = (1.0E29);

   scan_bins(&pps_adev_bins[0]);
   scan_bins(&osc_adev_bins[0]);

   scan_bins(&pps_hdev_bins[0]);
   scan_bins(&osc_hdev_bins[0]);

   scan_bins(&pps_mdev_bins[0]);
   scan_bins(&osc_mdev_bins[0]);
}

void show_all_adevs()
{
struct ADEV_INFO bins;
COORD row,col;

   // show all adev tables on the screen at the same time
   find_global_max();

   if(TEXT_HEIGHT <= 8) row = ALL_ROW+5;
   else if((TEXT_HEIGHT <= 16) && (PLOT_ROW >= 576))      row = ALL_ROW+5;
   else if((TEXT_HEIGHT <= 14) && big_plot)               row = ALL_ROW+5;
   else if((TEXT_HEIGHT <= 12) && (SCREEN_WIDTH >= 1280)) row = ALL_ROW+5;
   else if((TEXT_HEIGHT <= 14) && (SCREEN_WIDTH >= 1024)) row = ALL_ROW+3;
   else {
      row = ALL_ROW;
//    if(osc_params || (osc_discipline == 0)) ++row;
      if(osc_params) ++row;
   }
   all_adev_row = row;
   col = 0;
   fetch_adev_info(OSC_ADEV+all_adevs-1, &bins);
   if(all_adevs == 1) {  // showing OSC adevs
      show_adev_table(&bins, row, col, OSC_ADEV_COLOR);
      if(mixed_adevs != 2) plot_adev_curve(&bins, OSC_ADEV_COLOR);
   }
   else {   // showing PPS adevs
      show_adev_table(&bins, row, col, PPS_ADEV_COLOR);
      if(mixed_adevs != 2) plot_adev_curve(&bins, PPS_ADEV_COLOR);
   }

   if(SCREEN_WIDTH < 800)       col = (TEXT_COLS*1)/4;
   else if(SCREEN_WIDTH < 1024) col += 25;
   else                         col += 32;
   fetch_adev_info(OSC_HDEV+all_adevs-1, &bins);
   show_adev_table(&bins, row, col, GREEN);
   if(mixed_adevs != 2) plot_adev_curve(&bins, GREEN);

   if(SCREEN_WIDTH >= 1280) col = FILTER_COL+20;
   else                     col = (TEXT_COLS*2)/4;
   fetch_adev_info(OSC_MDEV+all_adevs-1, &bins);
   show_adev_table(&bins, row, col, MAGENTA);
   if(mixed_adevs != 2) plot_adev_curve(&bins, MAGENTA);

   if(SCREEN_WIDTH >= 1280) col += 32;
   else                     col = (TEXT_COLS*3)/4;
   fetch_adev_info(OSC_TDEV+all_adevs-1, &bins);
   show_adev_table(&bins, row, col, YELLOW);
   if(mixed_adevs != 2) plot_adev_curve(&bins, YELLOW);
}

void show_adev_info()
{
   // draw the adev tables and graphs
   if(all_adevs) {
      if(mixed_adevs == 2) {  // show 4 adev tables, PPS and OSC curves
         show_all_adevs();
         plot_adev_curve(&pps_bins, PPS_ADEV_COLOR);
         plot_adev_curve(&osc_bins, OSC_ADEV_COLOR);
      }
      else {  // show all 4 adev tables and matching curves
         show_all_adevs();
      }
   }
   else {  // show OSC and PPS tables and curves
      find_global_max();
      show_adev_table(&pps_bins, ADEV_ROW+0, ADEV_COL, PPS_ADEV_COLOR);
      plot_adev_curve(&pps_bins, PPS_ADEV_COLOR);

      show_adev_table(&osc_bins, ADEV_ROW+max_adev_rows+1, ADEV_COL, OSC_ADEV_COLOR);
      plot_adev_curve(&osc_bins, OSC_ADEV_COLOR);
   }
}

void update_adev_display(int type)
{
int bin_count;
u08 new_adev_info;
u08 need_new_plot;

   // redraw the adev displays if it is time to do so
   if(adev_period <= 0.0F) return;

   new_adev_info = 0;
   need_new_plot = 0;

   if(++pps_adev_time >= ADEV_DISPLAY_RATE) {
      pps_adev_time = 0;
      if(all_adevs) new_adev_info = 1;

      bin_count = fetch_adev_info(type|0x01, &pps_bins);

      if(bin_count > last_bin_count) {  // force redraw whenever a new bin fills
         need_new_plot |= 0x01;
         last_bin_count = bin_count;
      }
      new_adev_info |= 0x01;
   }

   if(++osc_adev_time >= ADEV_DISPLAY_RATE) {
      osc_adev_time = 0;
      bin_count = fetch_adev_info(type&(~1), &osc_bins);

      if(bin_count > last_bin_count) {  // force redraw whenever a new bin fills
         need_new_plot |= 0x02;
         last_bin_count = bin_count;
      }
      new_adev_info |= 0x02;
   }

   if(need_new_plot) {
      draw_plot(0);
   }

   if(new_adev_info) {
      show_adev_info();
   }
}

void force_adev_redraw()
{
    // force a redraw of the the adev tables and graphs
    have_time = 0;
//  adev_time = 0;
    pps_adev_time = ADEV_DISPLAY_RATE;
    osc_adev_time = ADEV_DISPLAY_RATE;
    update_adev_display(ATYPE);
}


void write_log_adevs(struct ADEV_INFO *bins)
{
int i;
char *d;
char *t;

   // write an adev table to the log file
   if(log_file == 0) return;

   if(bins->adev_type & 0x01) d = "PPS";
   else                       d = "OSC";

   if     (bins->adev_type == OSC_ADEV) t = "ADEV";
   else if(bins->adev_type == PPS_ADEV) t = "ADEV";
   else if(bins->adev_type == OSC_MDEV) t = "MDEV";
   else if(bins->adev_type == PPS_MDEV) t = "MDEV";
   else if(bins->adev_type == OSC_HDEV) t = "HDEV";
   else if(bins->adev_type == PPS_HDEV) t = "HDEV";
   else if(bins->adev_type == OSC_TDEV) t = "TDEV";
   else if(bins->adev_type == PPS_TDEV) t = "TDEV";

   fprintf(log_file, "#\n");
   fprintf(log_file, "#  %s %s over %lu points - sample period=%.1f secs\n", 
      d, t, adev_q_count+(long) adev_q_overflow, adev_period);

   for(i=0; i<bins->bin_count; i++) {
      fprintf(log_file, "# %10.3f tau  %.4le (n=%ld)\n", 
      bins->adev_taus[i], bins->adev_bins[i], bins->adev_on[i]);
   }
}

void log_adevs()
{
struct ADEV_INFO bins;

   // write all adev tables to the log file
   if(log_file && (adev_period > 0.0F)) {
      if(log_stream && (kol > 0)) fprintf(log_file, "\n");

      fetch_adev_info(PPS_ADEV, &bins);
      write_log_adevs(&bins);  

      fetch_adev_info(PPS_HDEV, &bins);
      write_log_adevs(&bins);  

      fetch_adev_info(PPS_MDEV, &bins);
      write_log_adevs(&bins);  

      fetch_adev_info(PPS_TDEV, &bins);
      write_log_adevs(&bins);  


      fetch_adev_info(OSC_ADEV, &bins);
      write_log_adevs(&bins);

      fetch_adev_info(OSC_HDEV, &bins);
      write_log_adevs(&bins);

      fetch_adev_info(OSC_MDEV, &bins);
      write_log_adevs(&bins);

      fetch_adev_info(OSC_TDEV, &bins);
      write_log_adevs(&bins);
   }
}

#endif // ADEV_STUFF


#ifdef GIF_FILES

//
// .GIF stuff from GraphApp
//
#pragma pack(1)  // Do NOT allow compiler to reorder structs!

typedef unsigned char      byte;
typedef unsigned long      Char;

typedef struct Colour      Color;
typedef struct Colour      Colour;

struct Colour {
 byte  alpha;    /* transparency, 0=opaque, 255=transparent */
 byte  red;      /* intensity, 0=black, 255=bright red */
 byte  green;    /* intensity, 0=black, 255=bright green */
 byte  blue;     /* intensity, 0=black, 255=bright blue */
};

typedef struct {
    int      length;
    Colour * colours;
} GifPalette;

typedef struct {
    int          width, height;
    int          has_cmap, color_res, sorted, cmap_depth;
    int          bgcolour, aspect;
    GifPalette * cmap;
} GifScreen;

typedef struct {
    int              left, top, width, height;
    int              has_cmap, interlace, sorted, reserved, cmap_depth;
    GifPalette *     cmap;
} GifPicture;

typedef struct {
    int             byte_count;
    unsigned char * bytes;
} GifData;

typedef struct {
    int        marker;
    int        data_count;
    GifData ** data;
} GifExtension;

typedef struct {
    int            intro;
    GifPicture *   pic;
    GifExtension * ext;
} GifBlock;

typedef struct {
    char        header[8];
    GifScreen * screen;
    int         block_count;
    GifBlock ** blocks;
} Gif;

/*
 *  Gif internal definitions:
 */

#define LZ_MAX_CODE     4095    /* Largest 12 bit code */
#define LZ_BITS         12

#define FLUSH_OUTPUT    4096    /* Impossible code = flush */
#define FIRST_CODE      4097    /* Impossible code = first */
#define NO_SUCH_CODE    4098    /* Impossible code = empty */

#define HT_KEY_MASK     0x1FFF  /* 13 bit key mask */

#define IMAGE_LOADING   0       /* file_state = processing */
#define IMAGE_SAVING    0       /* file_state = processing */
#define IMAGE_COMPLETE  1       /* finished reading or writing */

typedef struct {
    FILE *file;
    int depth,
        clear_code, eof_code,
        running_code, running_bits,
        max_code_plus_one,
        prev_code, current_code,
        stack_ptr,
        shift_state;
    unsigned long shift_data;
    unsigned long pixel_count;
    int           file_state, position, bufsize;
    unsigned char buf[256];
} GifEncoder;

//****************************************************************************
//
// GIF routines from GraphApp
//
//****************************************************************************

#define rgb(r,g,b)  app_new_rgb((r),(g),(b))

int  write_byte(FILE *file, int ch);
void write_gif_byte(FILE *file, GifEncoder *encoder, int ch);
void write_gif_int(FILE *file, int output);
void write_gif_palette(FILE *file, GifPalette *cmap);
void write_gif_header(FILE *file, Gif *gif);
void write_gif_code(FILE *file, GifEncoder *encoder, int code);
void init_gif_encoder(FILE *file, GifEncoder *encoder, int depth);
void write_gif_line(FILE *file, GifEncoder *encoder, int line, int length);
void flush_gif_encoder(FILE *file, GifEncoder *encoder);
void write_gif_picture(FILE *file, GifPicture *pic);
void write_gif(FILE *file, Gif *gif);
int  write_gif_file(char *filename, Gif *gif);

//
//  Gif data structures  (no longer dynamically allocated/freed)
//
GifEncoder  encoder_data;
GifPalette  palette_data;
GifScreen   screen_data;
GifPicture  pic_data;
GifBlock    block_data;
Gif         gif_data;

/*
 *  Hash table:
 */

/*
 *  The 32 bits contain two parts: the key & code:
 *  The code is 12 bits since the algorithm is limited to 12 bits
 *  The key is a 12 bit prefix code + 8 bit new char = 20 bits.
 */
#define HT_GET_KEY(x)   ((x) >> 12)
#define HT_PUT_KEY(x)   ((x) << 12)
#define HT_GET_CODE(x)  ((x) & 0x0FFF)
#define HT_PUT_CODE(x)  ((x) & 0x0FFF)

#ifdef EMS_HASH
//  Warning:  EMS hash table is known to not work and be hideously slow
unsigned long get_hash_table(long i)
{
   #ifdef EMS_MEMORY
      u16 ems_page, ems_ofs;

      if(hash_pid != 0xDEAD) {
         ems_page = i / HASHES_PER_PAGE;
         ems_ofs  = i % HASHES_PER_PAGE;
         if(ems_page != current_hash_page) {
            reg.wd.ax = 0x4400 | hash_frame;
            reg.wd.bx = ems_page;
            reg.wd.dx = hash_pid;

            int86x(EMS_INT, &reg, &reg, &seg);
            if(reg.h.ah) {     /* map the page in to its place failed */
               sprintf(out, "*** EMS ERROR %02X: map %04X:%04X to page %02X.\n", reg.h.ah, ems_page, 0, 0);
               error_exit(90, out);
            }
            current_hash_page = ems_page;
         }
         return *(&hash_table[ems_ofs]);
      }
   #endif

   return hash_table[i];
}

void put_hash_table(long i, long hash_val)
{
   #ifdef EMS_MEMORY
      u16 ems_page, ems_ofs;

      if(hash_pid != 0xDEAD) {
         ems_page = i / HASHES_PER_PAGE;
         ems_ofs  = i % HASHES_PER_PAGE;
         if(ems_page != current_hash_page) {
            reg.wd.ax = 0x4400 | hash_frame;
            reg.wd.bx = ems_page;
            reg.wd.dx = hash_pid;

            int86x(EMS_INT, &reg, &reg, &seg);
            if(reg.h.ah) {     /* map the page in to its place failed */
               sprintf(out, "*** EMS ERROR %02X: map %04X:%04X to page %02X.\n", reg.h.ah, ems_page, 0, 0);
               error_exit(91, out);
            }
            current_hash_page = ems_page;
         }
         *(&hash_table[ems_ofs]) = hash_val;
         return;
      }
   #endif

   hash_table[i] = hash_val;
   return;
}
#else
   #define get_hash_table(i)   hash_table[i]
   #define put_hash_table(i,j) hash_table[i]=(j)
#endif

/*
 *  Generate a hash key from the given unique key.
 *  The given key is assumed to be 20 bits as follows:
 *    lower 8 bits are the new postfix character,
 *    the upper 12 bits are the prefix code.
 */
static int gif_hash_key(unsigned long key)
{
   return ((key >> 12) ^ key) & HT_KEY_MASK;
}

/*
 *  Reset the hash_table to an empty state.
 */
static void clear_gif_hash_table()
{
int i;

   for(i=0; i<HT_SIZE; i++) {
      put_hash_table(i, 0xFFFFFFFFL);
   }
}

/*
 *  Insert a new item into the hash_table.
 *  The data is assumed to be new.
 */
static void add_gif_hash_entry(unsigned long key, int code)
{
int hkey;

   hkey = gif_hash_key(key);
   while(HT_GET_KEY(get_hash_table(hkey)) != 0xFFFFFL) {
      hkey = (hkey + 1) & HT_KEY_MASK;
   }
   put_hash_table(hkey, HT_PUT_KEY(key) | HT_PUT_CODE(code));
}

/*
 *  Determine if given key exists in hash_table and if so
 *  returns its code, otherwise returns -1.
 */
static int lookup_gif_hash(unsigned long key)
{
int hkey;
unsigned long htkey;

   hkey = gif_hash_key(key);
   while((htkey = HT_GET_KEY(get_hash_table(hkey))) != 0xFFFFFL) {
      if(key == htkey) return HT_GET_CODE(get_hash_table(hkey));
      hkey = (hkey + 1) & HT_KEY_MASK;
   }
   return (-1);
}

/*
 *   Initialise the encoder, given a GifPalette depth.
 */
void init_gif_encoder(FILE *file, GifEncoder *encoder, int depth)
{
int lzw_min;

   lzw_min = depth = (depth < 2 ? 2 : depth);
   encoder->file_state   = IMAGE_SAVING;
   encoder->position     = 0;
   encoder->bufsize      = 0;
   encoder->buf[0]       = 0;
   encoder->depth        = depth;
   encoder->clear_code   = (1 << depth);
   encoder->eof_code     = encoder->clear_code + 1;
   encoder->running_code = encoder->eof_code + 1;
   encoder->running_bits = depth + 1;
   encoder->max_code_plus_one = 1 << encoder->running_bits;
   encoder->current_code = FIRST_CODE;
   encoder->shift_state  = 0;
   encoder->shift_data   = 0;

   /* Write the LZW minimum code size: */
   write_byte(file, lzw_min);

   /* Clear hash table, output Clear code: */
   clear_gif_hash_table();
   write_gif_code(file, encoder, encoder->clear_code);
}

void flush_gif_encoder(FILE *file, GifEncoder *encoder)
{
   write_gif_code(file, encoder, encoder->current_code);
   write_gif_code(file, encoder, encoder->eof_code);
   write_gif_code(file, encoder, FLUSH_OUTPUT);
}

/*
 *  Write a Gif code word to the output file.
 *
 *  This function packages code words up into whole bytes
 *  before writing them. It uses the encoder to store
 *  codes until enough can be packaged into a whole byte.
 */
void write_gif_code(FILE *file, GifEncoder *encoder, int code)
{
   if(code == FLUSH_OUTPUT) {
      /* write all remaining data */
      while(encoder->shift_state > 0) {
         write_gif_byte(file, encoder, encoder->shift_data & 0xff);
         encoder->shift_data >>= 8;
         encoder->shift_state -= 8;
      }
      encoder->shift_state = 0;
      write_gif_byte(file, encoder, FLUSH_OUTPUT);
   }
   else {
      encoder->shift_data |= ((long) code) << encoder->shift_state;
      encoder->shift_state += encoder->running_bits;

      while(encoder->shift_state >= 8) { /* write full bytes */
         write_gif_byte(file, encoder, encoder->shift_data & 0xff);
         encoder->shift_data >>= 8;
         encoder->shift_state -= 8;
      }
   }

   /* If code can't fit into running_bits bits, raise its size.
    * Note that codes above 4095 are for signalling. */
   if((encoder->running_code >= encoder->max_code_plus_one) && (code < 4096)) {
      encoder->max_code_plus_one = (1 << ++encoder->running_bits);
   }
}


//
//
//   Basic item I/O routines
//
//
int write_byte(FILE *file, int ch)
{
   return putc(ch, file);
}

int write_stream(FILE *file, unsigned char buffer[], int length)
{
   return fwrite(buffer, 1, length, file);
}

void write_gif_int(FILE *file, int output)
{
   putc((output & 0xff), file);
   putc((((unsigned int) output) >> 8) & 0xff, file);
}


/*
 *  Write a byte to a Gif file.
 *
 *  This function is aware of Gif block structure and buffers
 *  chars until 255 can be written, writing the size byte first.
 *  If FLUSH_OUTPUT is the char to be written, the buffer is
 *  written and an empty block appended.
 */
void write_gif_byte(FILE *file, GifEncoder *encoder, int ch)
{
unsigned char *buf;

   buf = encoder->buf;

   if(encoder->file_state == IMAGE_COMPLETE) return;

   if(ch == FLUSH_OUTPUT) {
      if(encoder->bufsize) {
         write_byte(file, encoder->bufsize);
         write_stream(file, buf, encoder->bufsize);
         encoder->bufsize = 0;
      }
      /* write an empty block to mark end of data */
      write_byte(file, 0);
      encoder->file_state = IMAGE_COMPLETE;
   }
   else {
      if(encoder->bufsize == 255) {
         /* write this buffer to the file */
         write_byte(file, encoder->bufsize);
         write_stream(file, buf, encoder->bufsize);
         encoder->bufsize = 0;
      }
      buf[encoder->bufsize++] = ch;
   }
}

/*
 *  Write one scanline of pixels out to the Gif file,
 *  compressing that line using LZW into a series of codes.
 */
void write_gif_line(FILE *file, GifEncoder *encoder, int line, int length)
{
int i, current_code, new_code;
unsigned long new_key;
unsigned char pixval;

    i = 0;

    if(encoder->current_code == FIRST_CODE) current_code = get_pixel(i++, line);
    else current_code = encoder->current_code;

    while(i<length) {
       pixval = get_pixel(i++, line); /* Fetch next pixel from screen */

       /* Form a new unique key to search hash table for the code
        * Combines current_code as prefix string with pixval as
        * postfix char */
       new_key = (((unsigned long) current_code) << 8) + pixval;
       if((new_code = lookup_gif_hash(new_key)) >= 0) {
           /* This key is already there, or the string is old,
            * so simply take new code as current_code */
           current_code = new_code;
       }
       else {
           /* Put it in hash table, output the prefix code,
            * and make current_code equal to pixval */
           write_gif_code(file, encoder, current_code);
           current_code = pixval;

           /* If the hash_table if full, send a clear first
            * then clear the hash table: */
           if(encoder->running_code >= LZ_MAX_CODE) {
              write_gif_code(file, encoder, encoder->clear_code);
              encoder->running_code = encoder->eof_code + 1;
              encoder->running_bits = encoder->depth + 1;
              encoder->max_code_plus_one = 1 << encoder->running_bits;
              clear_gif_hash_table();
           }
           else {
              /* Put this unique key with its relative code in hash table */
              add_gif_hash_entry(new_key, encoder->running_code++);
           }
       }
    }

    /* Preserve the current state of the compression algorithm: */
    encoder->current_code = current_code;
}

/*
 *  GifPicture:
 */

void write_gif_header(FILE *file, Gif *gif)
{
unsigned char info;
GifScreen *screen;
GifPalette *cmap;
int i;
Colour c;

   fprintf(file, "%s", gif->header);  // header info

   // image description
   screen = gif->screen;
   cmap = screen->cmap;
   write_gif_int(file, screen->width);
   write_gif_int(file, screen->height);

   info = 0;
   info = info | (screen->has_cmap ? 0x80 : 0x00);
   info = info | ((screen->color_res - 1) << 4);
// info = info | (screen->sorted ? 0x08 : 0x00);
   if(screen->cmap_depth > 0) {
      info = info | ((screen->cmap_depth) - 1);
   }
   write_byte(file, info);

   write_byte(file, screen->bgcolour);
   write_byte(file, screen->aspect);

   if(screen->has_cmap) {   // palette data
      for(i=0; i<cmap->length; i++) {
         c = cmap->colours[i];
         write_byte(file, c.red);
         write_byte(file, c.green);
         write_byte(file, c.blue);
      }
   }
}

void write_gif_picture(FILE *file, GifPicture *pic)
{
unsigned char info;
GifEncoder *encoder;
int row;

   write_byte(file, 0x2C);   // intro code

   write_gif_int(file, pic->left);
   write_gif_int(file, pic->top);
   write_gif_int(file, pic->width);
   write_gif_int(file, pic->height);

   info = 0;
// info = info | (pic->has_cmap    ? 0x80 : 0x00);
// info = info | (pic->interlace   ? 0x40 : 0x00);
// info = info | (pic->sorted      ? 0x20 : 0x00);
   info = info | ((pic->reserved << 3) & 0x18);
   if(pic->has_cmap) info = info | (pic->cmap_depth - 1);

   write_byte(file, info);

   encoder = &encoder_data;
   init_gif_encoder(file, encoder, pic->cmap_depth);

   row = 0;
   while(row < pic->height) {
     update_pwm();
     write_gif_line(file, encoder, row, pic->width);
     row += 1;
   }

   flush_gif_encoder(file, encoder);

   write_byte(file, 0x3B);   // end code
}

Colour app_new_rgb(int r, int g, int b)
{
Colour c;

   c.alpha  = 0;
   c.red    = r;
   c.green  = g;
   c.blue   = b;

   return c;
}

//****************************************************************************
//
// Write contents of screen to .GIF file, returning 1 if OK or 0 on error 
//
//****************************************************************************


int dump_gif_file(int invert, FILE *file)
{
S32 i;
Gif *gif;
GifPalette *cmap;
GifPicture *pic;
#define BPP 4   // 4 bits per pixel = 16 colors
u08 ctable[(1 << BPP) * sizeof(Colour)];

    // setup GIF header
    gif = &gif_data;
    strcpy(gif->header, "GIF87a");
    gif->blocks      = NULL;
    gif->block_count = 0;
    gif->screen      = &screen_data;

    gif->screen->width      = SCREEN_WIDTH;
    gif->screen->height     = SCREEN_HEIGHT;
    gif->screen->has_cmap   = 1;
    gif->screen->color_res  = BPP;
    gif->screen->cmap_depth = BPP;
   
    // setup palette color map
    screen_data.cmap = &palette_data;
    cmap = gif->screen->cmap;
    cmap->length = (1 << BPP);
    cmap->colours = (Colour *) &ctable[0];

    for(i=0; i<(1 << BPP); i++) {  // clear all the palette table
       cmap->colours[i] = rgb(0, 0, 0);
    }
    for(i=0; i<16; i++) {   // define the colors that we use
       cmap->colours[i] = rgb(bmp_pal[i*4+2], bmp_pal[i*4+1], bmp_pal[i*4+0]);
       if(invert) {
          if     (i == BLACK)  cmap->colours[i] = rgb(bmp_pal[WHITE*4+2], bmp_pal[WHITE*4+1], bmp_pal[WHITE*4+0]);
          else if(i == WHITE)  cmap->colours[i] = rgb(bmp_pal[BLACK*4+2], bmp_pal[BLACK*4+1], bmp_pal[BLACK*4+0]);
          else if(i == YELLOW) cmap->colours[i] = rgb(bmp_pal[BMP_YELLOW*4+2], bmp_pal[BMP_YELLOW*4+1], bmp_pal[BMP_YELLOW*4+0]);
       }
    }

    // picture description
    pic = &pic_data;
    pic->cmap       = &palette_data;
    pic->width      = SCREEN_WIDTH;
    pic->height     = SCREEN_HEIGHT;
    pic->interlace  = 0; 
    pic->has_cmap   = 0;
    pic->cmap_depth = BPP;

    write_gif_header(file, gif);
    write_gif_picture(file, pic);

    return 1;
}
#endif  // GIF_FILES

//
//
//   Automatic oscillator parameter configuration routine
//
//


#define STABLE_TIME    5     // how many secs to stabilize after changing DAC before averaging
#define GAIN_AVG_TIME 60     // how many secs to average osc reading over
                             // must be less than SURVEY_BIN_COUNT

#define HIGH_STATE   100     // state when measuring with high dac setting
#define WAIT_STATE  1000     // state when waiting for dac to settle
#define LOW_STATE  10000     // state when measuring with low dac setting

#define OSC_TUNE_GAIN 10.0F
#define DAC_STEP      0.005F // dac voltage step (+ and -)

void calc_osc_gain()
{
int old_read_only;
   // dac_dac is the current gain measurement state
   if(dac_dac == 0) return;  // we are not measuring

   if(dac_dac == 1) {        // initialize dac gain test
      if(res_t) goto tune_res_t;
      #ifdef OSC_CONTROL
         disable_osc_control();
      #endif
      #ifdef PRECISE_STUFF
         stop_precision_survey();
      #endif

      old_read_only = read_only;
      read_only = 1;
      set_el_mask(0.0F);
      read_only = old_read_only;

      hour_lats = 0;         // setup to determine median osc offset values
      hour_lons = 0;
      gain_voltage = dac_voltage; // initial (current) DAC voltage
      set_discipline_mode(4);     // disable disciplining
      set_dac_voltage(gain_voltage+(DAC_STEP*OSC_TUNE_GAIN));  // bump the osc voltage up
      ++dac_dac;
      sprintf(plot_title, "Determining OSC gain: initializing");
      title_type = OTHER;
   }
   else if(dac_dac <= STABLE_TIME) {    // wait a little while for things to settle
      ++dac_dac;
      sprintf(plot_title, "Determining OSC gain: stabilizing high");
      title_type = OTHER;
   }
   else if(dac_dac == (STABLE_TIME+1)) {    // prepare to start taking data
      dac_dac = HIGH_STATE;
   }
   else if(dac_dac < (HIGH_STATE+1+GAIN_AVG_TIME)) {   // find median osc offset value with dac set high
      hour_lats = add_to_bin((double)osc_offset, lat_hr_bins, hour_lats);
      sprintf(plot_title, "Determining OSC gain: high osc(%d)=%f", hour_lats, (float) lat_hr_bins[hour_lats/2]);
      title_type = OTHER;
      ++dac_dac;
   }
   else if(dac_dac == (HIGH_STATE+1+GAIN_AVG_TIME)) {  // prepare to measure osc with dac at low value
      sprintf(plot_title, "Determining OSC gain: stabilizing low");
      title_type = OTHER;
      set_dac_voltage(gain_voltage-(DAC_STEP*OSC_TUNE_GAIN));
      dac_dac = WAIT_STATE;
   }
   else if(dac_dac <= (WAIT_STATE+STABLE_TIME)) {  // wait for things to settle down some
      ++dac_dac;
   }
   else if(dac_dac == (WAIT_STATE+1+STABLE_TIME)) {  // prepare to start measuring osc with dac voltage low
      dac_dac = LOW_STATE;
   }
   else if(dac_dac < (LOW_STATE+GAIN_AVG_TIME+1)) {   // calculate the median osc value with dac set low
      ++dac_dac;
      hour_lons = add_to_bin((double)osc_offset, lon_hr_bins, hour_lons);
      sprintf(plot_title, "Determining OSC gain: low(%d)=%f", hour_lons, (float) lon_hr_bins[hour_lons/2]);
      title_type = OTHER;
   }
   else {  // we are done,  calc osc params and store into eeprom
      user_osc_gain  = (float) lon_hr_bins[hour_lons/2];
      user_osc_gain -= (float) lat_hr_bins[hour_lats/2];
      user_osc_gain /= OSC_TUNE_GAIN;
      user_initial_voltage = gain_voltage;
      user_time_constant   = 500.0F;
      user_damping_factor  = 1.0F;    //// !!!! 0.707F;  0.800F
      user_min_volts = min_volts;
      user_max_volts = max_volts;
      user_jam_sync = jam_sync;
      user_max_freq_offset = max_freq_offset;

//    sprintf(plot_title, "OSC gain: %f Hz/V", user_osc_gain);
sprintf(plot_title, "gain:%.3f iv=%.3f min=%.3f  max=%.3f  tc=%.3f  damp=%.3f", 
user_osc_gain, user_initial_voltage, user_min_volts, user_max_volts, user_time_constant, user_damping_factor);
title_type = OTHER;

      set_discipline_params(1);       // save params into eeprom
      set_dac_voltage(gain_voltage);  // restore dac voltage
      set_discipline_mode(5);         // re-enable disciplining
      set_discipline_mode(0);         // jam sync the pps back in line
      request_all_dis_params();       // display the newest settings

      tune_res_t:
//    set_el_level();                 // set elevation mask level 
//    !!!! res-t does not like back to back config commands, set both params with one operation
      if(res_t == 2) set_el_amu(good_el_level(), 30.0F);  // set signal level mask level in dBc
      else           set_el_amu(good_el_level(), 1.0F);   // set signal level mask level in AMU

      redraw_screen();
      dac_dac = 0;                    // we are done dac-ing around
   }

   show_title();
}



#ifdef TEMP_CONTROL   // stuff
//
// Active tempeature stabilization of the unit temperature:
// An easy way to do this is to use choose a control temperature above
// maximum ambient temperature and below the self heating temperature
// of the unit in a closed box.  Use the apply_cool() function to turn 
// on a fan and the apply_heat() function to turn it off.

// Heat control via the parallel port uses the upper four bits to
// enable/disable the heat controller.  A value of 9 in the upper 4
// bits says to enable the controller.  A value of 6 says to disable it.
//
// The lower 4 bits are used to select the control state:
//    9=apply heat  A=apply cool  6=hold temperature
//
// These values were chosen so that both polarities of control signal
// are available.  Also the enable/disable patterns were chosen so that
// they are unlikely to occur due to the BIOS initializing the parallel
// port (which ususally leaves a AA/55/00/FF in the port register at
// power up).

// Heat control via the serial port uses the DTR line for fan speed control
// and the RTS line for heat controller enable/disable.

#define HEAT 0     // serial port DTR line values
#define COOL 1
#define ENABLE  0  // serial port RTS line values
#define DISABLE 1

u08 a_heating;     // the current heating/cooling/holding state (only one should be true)
u08 a_cooling;
u08 a_holding;

float this_temperature; // the current temperature reading
float last_temp;        // the previous temperature reading
float delta;            // the temperature error
float last_delta;       // the previous temperature error
float spike_temp;       // the temperature reading before a sensor misread spike

double last_msec, this_msec;           // millisecond tick clock values
double heat_off_time, cool_off_time;   // when to update the pwm output
float  heat_time, cool_time;           // how many milliseconds to heat or cool for

// stuff used to analyze the raw controller response (in bang-bang mode)
double heat_on_tick, cool_on_tick, hold_on_tick;
double cycle_time, avg_cycle_time, ct_samples;
double this_cycle, last_cycle;
double fall_cycle, fall_time;
float heat_sum, heat_ticks;
float cool_sum, cool_ticks;

float heat_rate, cool_rate;            // heating and cooling rate (deg/sec)
float rate_sum, rate_count;

long low_ok, high_ok;
float HEAT_OVERSHOOT;
float COOL_UNDERSHOOT;



unsigned init_lpt()   /* init LPT port,  return base I/O address */
{
   if(lpt_port == 1) return 0x378;
   if(lpt_port == 2) return 0x278;
   if(lpt_port == 3) return 0x3BC;
   return 0;
}


// Warren's filter info
float PID_out;          // new filter output
float last_PID_out;     // previous filter output
float PID_error;        // new temperature error
float last_PID_error;   // previous temperature error
float last_PID_display; // previous PID filter value (for debug display)

float integral_step;    // integrator update factor
float integrator;       // the current integrator value

// internally used PID filter constants (derived from user friendly ones)
float k1;      // the proportional gain
float k2;      // the derivitive time constant
float k3;      // the filter time constant (scaled)
float k4;      // the integrator time constant
// float k5;   // note that the k5 pid param is FILTER_OFFSET
float k6;      // load distubance test tuning param
float k7;      // loop gain tuning param
float k8;      // integrator reset
#define loop_gain        0.0F // k7
#define integrator_reset k8

#define PID_UPDATE_RATE   1.0F              // how fast the PID filter is updated in seconds
#define PWM_CYCLE_TIME    (1002.5F/2.0F)    // milliseconds
#define MAX_PID           0.99F
#define MAX_INTEGRAL      2.0               // max integrator update step
#define SCALE_FACTOR      (1.0F+loop_gain)  // !!!! ////

#define TUNE_CYCLES   7       // end autotune after this many waveform cycles
int tune_cycles;
double tune_amp[TUNE_CYCLES+1];
int    amp_ptr;
double tune_cyc[TUNE_CYCLES+1];
int    cyc_ptr;
int bang_settle_time;         // how long to wait for PID filter to settle before starting analysis
int BANG_INITS;               // number of cycles to wait before starting distubance measurments

void calc_k_factors()
{
   // This routine converts the user friendly input PID constants into the
   // "k" values used by the PID filter algorithm.  It is called whenever
   // a PID filter constant is changed by the user.
   //
   // P_GAIN        -> k1   (PID gain)
   // D_TC          -> k2   (derivative time constant)
   // FILTER_TC     -> k3   (filter time constant)
   // I_TC          -> k4   (integrator time constant)
   // FILTER_OFFSET         (temperature setpoint offset)

   if(I_TC) k4 = (1.0F/I_TC) / PID_UPDATE_RATE * P_GAIN;
   else     k4 = 0.0F;

   if(FILTER_TC > 0.0F) {
      k3 = 1.0F - (1.0F/((FILTER_TC*PID_UPDATE_RATE)+1.0F));
      k2 = (-1.0F * D_TC * P_GAIN) / (FILTER_TC*PID_UPDATE_RATE+1.0F) * (PID_UPDATE_RATE/SCALE_FACTOR);
   }
   else {
      k3 = FILTER_TC;
      k2 = (-1.0F * D_TC * P_GAIN) * (PID_UPDATE_RATE/SCALE_FACTOR);
   }

   k1 = ((P_GAIN/SCALE_FACTOR) * (1.0F-k3)) - k2;

   if(pid_debug || bang_bang) {  // set a marker each time a PID value changes
      if(++test_marker > 9) test_marker = 1;
      mark_q_entry[test_marker] = plot_q_in;
   }
}

void set_default_pid(int pid_num)
{
   // standard filter constants
   FILTER_OFFSET = 0.0F;
   FILTER_TC = 2.0F;
   k6 = 0.0F;
   k7 = 0.0F;
   k8 = 1.0F;
   integrator = 0.0F;

   if(pid_num == 0) {        // Warren's tortise slow default values
      P_GAIN = 1.50F; 
      D_TC = 45.0F;
      I_TC = 180.0F;
   }
   else if(pid_num == 1) {   // Mark's medium fast hare values
      P_GAIN = 3.5F;  // 5.0F;
      D_TC = 25.0F;   // 32.0F; 
      I_TC = 100.0F;  // 120.0F 
   }
   else if(pid_num == 2) {   // Mark's fast hare values
      P_GAIN = 10.0F;  
      D_TC = 28.0;     
      I_TC = 112.0F;   
   }
   else if(pid_num == 3) {   // Mark's fast hare-on-meth values
      P_GAIN = 24.0F;  
      D_TC = 12.0;     
      I_TC = 48.0F;   
   }

   calc_k_factors();
}

void show_pid_values()
{
   if(dac_dac) return;

   sprintf(plot_title, "kP(%.2f %c %.4f) kD(%.2f %c %.4f) kF(%.2f %c %.4f) kI(%.1f %c %.3f)  kO=%.3f kL=%.3f kS=%.3f kR=%.3f k9=%.3f", 
      P_GAIN,RIGHT_ARROW,k1, 
      D_TC,RIGHT_ARROW,k2, 
      FILTER_TC,RIGHT_ARROW,k3, 
      I_TC,RIGHT_ARROW,k4, 
      FILTER_OFFSET, k6, k7, k8, KL_TUNE_STEP);
   title_type = OTHER;
}

int calc_autotune()
{
double val;

   if(HEAT_OVERSHOOT == 0.0F) return 1;
   if(COOL_UNDERSHOOT == 0.0F) return 2;
   if(ct_samples == 0.0) return 3;
   if(amp_ptr < 2) return 4;
   if(cyc_ptr < 2) return 5;

   val  = tune_amp[amp_ptr/2];
   if(amp_ptr > 2) {
      val += tune_amp[amp_ptr/2-1];
      val += tune_amp[amp_ptr/2+1];
      val /= 3.0;
   }
   P_GAIN = 0.625F * (KL_TUNE_STEP+KL_TUNE_STEP) / (float) val;

   val  = tune_cyc[cyc_ptr/2];
   if(cyc_ptr > 2) {
      val += tune_cyc[cyc_ptr/2-1];
      val += tune_cyc[cyc_ptr/2+1];
      val /= 3.0;
   }
   I_TC   = 0.75F * (float) val;
   D_TC   = 0.1875F * (float) val; 

   k6 = 0.0F;
// integrator = 0.0F;

   calc_k_factors();
   clear_pid_display();
   show_pid_values();

   bang_bang = 0;    // pid autotune analysis done
   return 0;
}


void pid_filter()
{
float pwm_width;

   #ifdef DEBUG_TEMP_CONTROL
      if(pid_debug || bang_bang) {
         show_pid_values();
      }
   #endif

   last_PID_error = PID_error;

   // PID_error is the curent temperature error (desired_temp - current_temp)
   // Negative values indicate the temperature is too high and needs to be 
   // lowered.  Positive values indicate the temperature is too low and needs
   // to be raised.
   //
   // FILTER_OFFSET is a temperature offset constant: negative values raise 
   // the temperature curve,  positive values lower it
   PID_error = (desired_temp + FILTER_OFFSET) - this_temperature;

   // integral_step is the clipped temperature error
   integral_step = PID_error; 
   if(integral_step > MAX_INTEGRAL) integral_step = MAX_INTEGRAL;  
   else if(integral_step < (-MAX_INTEGRAL)) integral_step = (-MAX_INTEGRAL);

   if((PID_out >= MAX_PID) && (integral_step > 0.0)) integral_step *= (-integrator_reset);
   else if((PID_out <= (-MAX_PID)) && (integral_step < 0.0)) integral_step *= (-integrator_reset);

   integrator += (integral_step * k4);
   if((integrator+k6) > 1.0F) integrator = (1.0F-k6);    // integrator value is maxed out
   else if((integrator+k6) < (-1.0F)) integrator = (-1.0F-k6);

   PID_out = (k1*PID_error) + (k2*last_PID_error) + (k3*last_PID_out);
   last_PID_out = PID_out;
   if(PID_out > 0.0F) PID_out -= k7;   // autotune debug - manual step size
   else               PID_out += k7;
   PID_out += (integrator + k6);

   // clamp the filter response
   if(PID_out > MAX_PID) PID_out = MAX_PID;
   else if(PID_out < (-MAX_PID)) PID_out = (-MAX_PID);

   // convert filter output to PWM duty cycle times
   pwm_width = (-PID_out) * PWM_CYCLE_TIME;
   cool_time = PWM_CYCLE_TIME + pwm_width;
   heat_time = PWM_CYCLE_TIME - pwm_width;

   #ifdef DEBUG_TEMP_CONTROL
      if(pid_debug || bang_bang) {
         sprintf(dbg,  "pwm=%.2f  heat:%.1f ms  cool:%.1f ms   PID(%.5f)  err(%.5f)  int=%.5f",
            pwm_width, heat_time,cool_time,  
            PID_out,  PID_error*(-1.0F),  integrator);
         last_PID_display =  PID_out;
         debug_text = &dbg[0];
      }
   #endif
}

void analyze_bang_bang()
{
char *s;
double ct;
double val;

   ct = 0.0;
   if(bang_bang == 0) {  // we are not banging around
      return;   
   }
   else if(bang_bang == 1) {    // waiting for a temp zero crossing (temp near setpoint)
      if(((PID_error > 0.0) && (last_PID_error < 0.0)) ||
         ((PID_error < 0.0) && (last_PID_error > 0.0))) { 
         bang_bang = 2;         // now wait for pid to settle
         bang_settle_time = (int) (I_TC * 5.0F);  // wait for 5 integrator time constants
      }
      s = "ZEROING";
      goto tuned_up;
   }
   else if(bang_bang == 2) {  // waiting for pid filter to settle
      if(bang_settle_time > 0) --bang_settle_time;
      else bang_bang = 3;     // start analyzing pid disturbance cycles
      ct_samples = bang_settle_time;
      s = "SETTLING";
      goto tuned_up;
   }
   else if(bang_bang == 3) {  // first bang time,  initialize variables
      s = "INITIALIZING";
      BANG_INITS = 2;
      tune_cycles = TUNE_CYCLES;
      goto start_banging;
   }
   else if(bang_bang == 4) { // quick bang test
      s = "QUICK TEST";
      BANG_INITS = 0;
      tune_cycles = 1;

      start_banging:
      low_ok = high_ok = 0;
      heat_rate = cool_rate = 0.0F;
      heat_sum = cool_sum = 0.0F;
      heat_ticks = cool_ticks = 0.0F;
      cycle_time = avg_cycle_time = fall_time = ct_samples = 0.0;
      this_cycle = last_cycle = fall_cycle = GetMsecs();
      cyc_ptr = amp_ptr = 0;
      tune_cyc[0] = tune_amp[0] = 0.0;

      OLD_P_GAIN = P_GAIN;
      P_GAIN = 0.0F;
      if(PID_error > 0.0F) k6 = KL_TUNE_STEP;
      else                 k6 = (-KL_TUNE_STEP);
      calc_k_factors();

      bang_bang = 5;
      goto tuned_up;
   }


   // PID filter should now be near a steady state
   // Start disturbance testing
   last_delta = delta;
   delta = this_temperature - desired_temp;

   if(delta > 0.0F) {
      heat_sum += delta;     // calculate average peak values
      heat_ticks += 1.0F;
   }
   else if(delta < 0.0F) {
      cool_sum += delta;
      cool_ticks += 1.0F;
   }

   if(PID_error > 0.0F) k6 = KL_TUNE_STEP;
   else                 k6 = (-KL_TUNE_STEP);

   if((low_ok > BANG_INITS) && (high_ok > BANG_INITS)) {  // we are past the disturbance startup wobblies
      s = "TUNING";
   }
   else {
      s = "DISTURBING";
   }

   if((last_PID_error > 0.0F) && (PID_error <= 0.0F)) {  // rising zero crossing
      this_cycle = GetMsecs();
      cycle_time = this_cycle - last_cycle;
      cycle_time /= 1000.0;    // waveform cycle time in seconds
      fall_time = this_cycle - fall_cycle;
      fall_time /= 1000.0;

      if((low_ok > BANG_INITS) && (high_ok > BANG_INITS)) { // it's not the first rising crossing
         if(cycle_time > 10.0) {  // crossing is probably not noise
            avg_cycle_time += cycle_time;
            ct_samples += 1.0;
            if(cyc_ptr <= tune_cycles) cyc_ptr = add_to_bin(cycle_time,  tune_cyc,  cyc_ptr);

            if(heat_ticks) HEAT_OVERSHOOT = (heat_sum / heat_ticks) * 1.414F;
            if(cool_ticks) COOL_UNDERSHOOT = (cool_sum / cool_ticks) * 1.414F;
            val = HEAT_OVERSHOOT - COOL_UNDERSHOOT;
            if(amp_ptr <= tune_cycles) amp_ptr = add_to_bin(val,  tune_amp,  amp_ptr);
            heat_sum = cool_sum = 0.0F;
            heat_ticks = cool_ticks = 0.0F;

            if(cyc_ptr >= tune_cycles) {
               calc_autotune();
               s = "DONE";
               last_cycle = this_cycle;
               goto tuned_up;
            }
         }
      }
      else { // first rising zero crossing
         heat_sum = cool_sum = 0.0F;
         heat_ticks = cool_ticks = 0.0F;
         avg_cycle_time = ct_samples = 0.0;
      }

      last_cycle = this_cycle;
      rate_sum = delta;
      rate_count = 1.0F;

      ++low_ok;
   }
   else if((last_PID_error < 0.0F) && (PID_error >= 0.0F)) {  // falling zero crossing
      fall_cycle = GetMsecs();
      rate_sum = delta;     //!!!! should we filter test the cycle time here?
      rate_count = 1.0F;
      ++high_ok;
   }
   else {  // calculate the heating and cooling rates near the zero crossing
      rate_sum += delta;
      if(++rate_count == 5.0) {
         rate_sum /= rate_count;
         if(delta > 0.0) heat_rate = rate_sum;
         else            cool_rate = rate_sum;
      }
   }

   tuned_up:
   #ifdef DEBUG_TEMP_CONTROL
      ct = cycle_time;
      sprintf(dbg2, "%s: cycle(%.0f)=%.3f+%.3f=%.3f sec   us(%.0f)=%.4f os(%.0f)=%.4f  [%.3f,%.4f]", 
             s, ct_samples, ct-fall_time, fall_time, ct,  
             cool_ticks, COOL_UNDERSHOOT,  heat_ticks, HEAT_OVERSHOOT,
             tune_cyc[cyc_ptr/2], tune_amp[amp_ptr/2]
      );
      debug_text2 = &dbg2[0];
   #endif
}


#define SPIKE_FILTER_TIME 45  // seconds

void filter_spikes()
{
// This routine helps filter out false temperature sensor reading spikes.
// It sends the last valid temperature reading to the PID until the reading  
// either increases in value,  falls below the starting value, or times out.

//temperature += KL_TUNE_STEP;    // simulate a temperature spike
//if(KL_TUNE_STEP > 0.0F) KL_TUNE_STEP -= 0.01F;
//else KL_TUNE_STEP = 0.0F;

   this_temperature = temperature;
   if(spike_mode == 0) {
      spike_delay = 0;
      return;  // not filtering spikes
   }

   if(spike_threshold && last_temp) { // filter out temperature spikes
      if(spike_delay > 0) {  // hold temperature while spike settles out
         if(this_temperature < spike_temp) {  // temp has fallen below where the spike started
            spike_delay = 0; 
         }
         else if((this_temperature > last_temp_val) && (spike_mode < 2)) {  // temp is now rising
            spike_delay = 0; 
         }
         else {  // countdown max spike filter time
            --spike_delay;
            this_temperature = spike_temp;
         }
      }
      else if((this_temperature - last_temp) > spike_threshold) {  // temp spike seen
         if(undo_fw_temp_filter) spike_delay = 3;                  // de-filtered temps cause one second spikes
         else                    spike_delay = SPIKE_FILTER_TIME;  // filtered temps cause 30 second spikes
         spike_temp = last_temp;
         this_temperature = spike_temp;
      }
   }

   if(spike_mode > 1) temperature = this_temperature;
}

void control_temp()
{
   if(temp_control_on == 0) return;

   if((test_heat > 0.0F) || (test_cool > 0.0F)) {  // manual pwm control is in effect
      heat_time = test_heat;
      cool_time = test_cool;

      #ifdef DEBUG_TEMP_CONTROL
         if(pid_debug || bang_bang) {
            sprintf(dbg,  "heat:%.1f ms  cool:%.1f ms", heat_time, cool_time);
            debug_text = &dbg[0];
         }
      #endif
   }
   else {   // we are doing the PID filter stuff
      pid_filter();          // update the PID and fan speed
      analyze_bang_bang();   // do autotune analysis
   }

   last_temp = this_temperature;
}

void apply_heat(void)
{
   if(lpt_addr) {  // using parallel port to control temp
      lpt_val = (lpt_val & 0xF0) | 0x09;
      outp(lpt_addr, lpt_val);
   }
   else {          // using serial port DTR line to control temp
      SetDtrLine(HEAT);
      SetRtsLine(ENABLE);
   }

   if(a_heating == 0) {
      heat_on_tick = GetMsecs(); 
   }
   a_heating = 1;
   a_cooling = 0;
   a_holding = 0;
   temp_dir = UP_ARROW;
}

void apply_cool() 
{
   if(lpt_addr) {  // using parallel port to control temp
      lpt_val = (lpt_val & 0xF0) | 0x0A;
      outp(lpt_addr, lpt_val);
   }
   else {
      SetDtrLine(COOL);
      SetRtsLine(ENABLE);
   }

   if(a_cooling == 0) {
      cool_on_tick = GetMsecs(); 
   }
   a_cooling = 1;
   a_heating = 0;
   a_holding = 0;
   temp_dir = DOWN_ARROW;
}

void hold_temp()
{
   if(lpt_addr) {  // we are using the parallel port to control temp
      lpt_val = (lpt_val & 0xF0) | 0x06;
      outp(lpt_addr, lpt_val);
   }
   else {          // we are using the serial port DTR/RTS lines to control temp
      SetDtrLine(COOL);
      SetRtsLine(DISABLE);
   }

   if(a_holding == 0) {
      hold_on_tick = GetMsecs(); 
   }
   a_holding = 1;
   a_heating = 0;
   a_cooling = 0;
   temp_dir = '=';
}

void enable_temp_control()
{
   if(lpt_port) {  // get lpt port address from bios area
      lpt_addr = init_lpt();
   }
   else lpt_addr = 0;

   if(lpt_addr) {  // turn on parallel port temp control unit
      lpt_val = (lpt_val & 0x0F) | 0x90;
      outp(lpt_addr, lpt_val);
   }
   else {   // turn on serial port temp control unit
      SetRtsLine(ENABLE);
   }

   low_ok = high_ok = 0;
   HEAT_OVERSHOOT = COOL_UNDERSHOOT = 0.0F;

   temp_control_on = 1;
   hold_temp();
}

void disable_temp_control()
{
   if(temp_control_on == 0) return;

   hold_temp();
   if(lpt_addr) {  // using parallel port to control temp
      lpt_val = (lpt_val & 0x0F) | 0x60;
      outp(lpt_addr, lpt_val);
   }
   else {   // using parallel port to control temp
     SetRtsLine(DISABLE);
   }

   temp_control_on = 0;
}


void update_pwm(void)
{
   // This routine is called whenever the system is idle or waiting in a loop.
   // It checks to see if it is time to change the temperature contoller PWM
   // output.
   if(temp_control_on == 0) return;

   this_msec = GetMsecs();
   if(this_msec < last_msec) {  // millisecond clock has wrapped
      cool_off_time = heat_off_time = 0.0F;
   }
   last_msec = this_msec;

   if(a_cooling) {   // we are currently cooling
      if(this_msec > cool_off_time) {  // it's time to stop cooling
         if(heat_time) {               // so turn on the heat
            apply_heat();
            heat_off_time = this_msec + (double) heat_time;  // set when to stop heating
         }
      }
   }
   if(a_heating) {  // we are currently heating
      if(this_msec > heat_off_time) {   // it's time to stop heating
         if(cool_time) {                // so turn on the cooler
            apply_cool();
            cool_off_time = this_msec + (double) cool_time;  // set when to stop cooling
         }
      }
   }

   // failsafe code to make sure controller does not lock up!!!!
   if((cool_time == 0.0) && (heat_time >= (PWM_CYCLE_TIME*2.0)) && (a_heating == 0)) {
      apply_heat();
      heat_off_time = this_msec + (double) heat_time;  // set when to stop heating
   }
   if((heat_time == 0.0) && (cool_time >= (PWM_CYCLE_TIME*2.0)) && (a_cooling == 0)) {
      apply_cool();
      cool_off_time = this_msec + (double) cool_time;  // set when to stop heating
   }
   if((a_heating == 0) && (a_cooling == 0) && (a_holding == 0)) {
      apply_cool();
      cool_off_time = this_msec + 100.0;
   }
   if(a_holding) {  // we are currently holding !!!!
      apply_cool();
      heat_off_time = this_msec + 100.0;  // set when to stop heating
   }
}
#endif  // TEMP_CONTROL


#ifdef OSC_CONTROL

// Warren's filter info
double osc_PID_out;          // new filter output
double last_osc_PID_out;     // previous filter output
double osc_PID_error;        // new temperature error
double last_osc_PID_error;   // previous temperature error
double last_osc_PID_display; // previous PID filter value (for debug display)

double osc_integral_step;    // integrator update factor
double osc_integrator;       // the current integrator value

// internally used PID filter constants (derived from user friendly ones)
double osc_k1;      // the proportional gain
double osc_k2;      // the derivitive time constant
double osc_k3;      // the filter time constant (scaled)
double osc_k4;      // the integrator time constant
double osc_k5;   // note that the k5 pid param is FILTER_OFFSET
double osc_k6;      // load distubance test tuning param
double osc_k7;      // loop gain tuning param
double osc_k8;      // integrator reset
#define osc_loop_gain        0.0 // osc_k7
#define osc_integrator_reset osc_k8

#define OSC_PID_UPDATE_RATE   1.0              // how fast the PID filter is updated in seconds
#define OSC_MAX_PID           100.0  // 0.99F
#define OSC_MAX_INTEGRAL      10.0    //2.0               // max integrator update step
#define OSC_SCALE_FACTOR      (1.0+osc_loop_gain)  // !!!! ////
#define MAX_PID_HZ            0.1    // saturated pid moves freq this many Hz


void calc_osc_k_factors()
{
double i_tc;
double f_tc;

   // This routine converts the user friendly input PID constants into the
   // "k" values used by the PID filter algorithm.  It is called whenever
   // a PID filter constant is changed by the user.
   //
   // OSC_P_GAIN        -> osc_k1   (PID gain)
   // OSC_D_TC          -> osc_k2   (derivative time constant)
   // OSC_FILTER_TC     -> osc_k3   (filter time constant)
   // OSC_I_TC          -> osc_k4   (integrator time constant)
   // OSC_FILTER_OFFSET             (osc setpoint offset)

   if(osc_rampup && (osc_rampup < OSC_I_TC)) {
      i_tc = OSC_I_TC      * (osc_rampup / OSC_I_TC);
      f_tc = OSC_FILTER_TC * (osc_rampup / OSC_I_TC);
      osc_rampup += 1.0;
   }
   else {
      i_tc = OSC_I_TC;
      f_tc = OSC_FILTER_TC;
   }

   if(i_tc) osc_k4 = (1.0/i_tc) / OSC_PID_UPDATE_RATE * OSC_P_GAIN;
   else     osc_k4 = 0.0;

   if(f_tc > 0.0) {
      osc_k3 = 1.0 - (1.0/((f_tc*OSC_PID_UPDATE_RATE)+1.0));
      osc_k2 = (-1.0 * OSC_D_TC * OSC_P_GAIN) / (f_tc*OSC_PID_UPDATE_RATE+1.0) * (OSC_PID_UPDATE_RATE/OSC_SCALE_FACTOR);
   }
   else {
      osc_k3 = OSC_FILTER_TC;
      osc_k2 = (-1.0 * OSC_D_TC * OSC_P_GAIN) * (OSC_PID_UPDATE_RATE/OSC_SCALE_FACTOR);
   }

   osc_k1 = ((OSC_P_GAIN/OSC_SCALE_FACTOR) * (1.0-osc_k3)) - osc_k2;

   if(osc_pid_debug && (osc_rampup == 0.0)) {  // set a marker each time a PID value changes
      if(++test_marker > 9) test_marker = 1;
      mark_q_entry[test_marker] = plot_q_in;
   }

   show_osc_pid_values();
}

void reset_osc_pid(int kbd_cmd)
{
   disable_osc_control();

   osc_PID_out = 0.0;          // new filter output
   last_osc_PID_out = 0.0;     // previous filter output
   osc_PID_error = 0.0;        // new temperature error
   last_osc_PID_error = 0.0;   // previous temperature error
   last_osc_PID_display = 0.0; // previous PID filter value (for debug display)
   osc_integral_step = 0.0;    // integrator update factor
   osc_integrator = 0.0;       // integral value

   post_q_count = 0;
   new_postfilter();

// if(kbd_cmd) enable_osc_control(); 
}

void set_default_osc_pid(int pid_num)
{
   // standard filter constants
   OSC_FILTER_OFFSET = 0.0;
   OSC_FILTER_TC = 2.0;
   osc_k6 = 0.0;
   osc_k7 = 0.0;
   osc_k8 = 0.0;
   osc_integrator = 0.0;

   if(pid_num == 0) {        // slower PPS recovery
      OSC_P_GAIN = 0.1;     
      OSC_D_TC = 0.0;         
      OSC_FILTER_TC = 100.0; 
      OSC_I_TC = 100.0;      
      OSC_FILTER_OFFSET = 50.0;
   }
   else if(pid_num == 1) {   // faster, noiser PPS recovery
      OSC_P_GAIN = 0.1;     
      OSC_D_TC = 0.0;         
      OSC_FILTER_TC = 100.0; 
      OSC_I_TC = 100.0;      
      OSC_FILTER_OFFSET = 100.0;
   }
   else if(pid_num == 2) {   // John's fancy pants oscillator
      OSC_P_GAIN = 0.03;    
      OSC_D_TC = 0.0;         
      OSC_FILTER_TC = 100.0; 
      OSC_I_TC = 500.0;      
      OSC_FILTER_OFFSET = 40.0;
      osc_postfilter = 100;
      new_postfilter();
   }
   else if(pid_num == 3) {   // TBD
   }

   calc_osc_k_factors();
}

void show_osc_pid_values()
{
   if(dac_dac) return;

   sprintf(plot_title, "bP(%.2f %c %.4f) bD(%.2f %c %.4f) bF(%.2f %c %.4f) bI(%.1f %c %.3f)  bO=%.3f bL=%.3f bS=%.3f bR=%.3f b9=%-3d", 
      OSC_P_GAIN,RIGHT_ARROW,osc_k1, 
      OSC_D_TC,RIGHT_ARROW,osc_k2, 
      OSC_FILTER_TC,RIGHT_ARROW,osc_k3, 
      OSC_I_TC,RIGHT_ARROW,osc_k4, 
      OSC_FILTER_OFFSET, osc_k6, osc_k7, osc_k8, osc_postfilter);
   title_type = OTHER;
}

double osc_pps_q[PRE_Q_SIZE+1];
double osc_osc_q[PRE_Q_SIZE+1];

double osc_post_q[POST_Q_SIZE+1];
double post_q_sum;

void new_postfilter()
{
double post_val;
int i;

   if(1 || (osc_postfilter < post_q_count)) {  // changing filter size,  rebuild filter queue
      if(post_q_count) post_val = post_q_sum / (double) post_q_count;
      else             post_val = osc_PID_out;
      post_q_sum = 0.0;
      for(i=0; i<osc_postfilter; i++) {
         osc_post_q[i] = post_val;
         post_q_sum += post_val;
      }
      post_q_count = osc_postfilter;
      post_q_in = 0;
   }
}

void osc_pid_filter()
{
double dac_ctrl;
double pps_tweak;
double osc_PID_val;
int i;

   if(0 && osc_prefilter) {
      osc_osc_q[opq_in] = osc_offset;
      osc_pps_q[opq_in] = pps_offset;
      if(++opq_in >= osc_prefilter) opq_in = 0;
      if(++opq_count > osc_prefilter) opq_count = osc_prefilter;

      pps_bin_count = 0;
      avg_pps = avg_osc = 0.0;
      for(i=0; i<opq_count; i++) {  // !!! we should calc this more efficiently by tracking what is in the queue
         avg_osc += osc_osc_q[i];
         avg_pps += osc_pps_q[i];
         ++pps_bin_count;
      }
      avg_osc /= (double) pps_bin_count;
      avg_pps /= (double) pps_bin_count;
   }
   else {
      avg_osc = osc_offset;
      avg_pps = pps_offset;
   }

   if(osc_rampup && (osc_rampup < OSC_I_TC)) {
      calc_osc_k_factors();
   }

   #ifdef DEBUG_OSC_CONTROL
      if(osc_pid_debug) {
         show_osc_pid_values();
      }
   #endif

   last_osc_PID_error = osc_PID_error;

   // osc_PID_error is the curent oscillator error 
   // Negative values indicate the osc is too high and needs to be 
   // lowered.  Positive values indicate the osc too high and needs
   // to be raised.
   //
   // OSC_FILTER_OFFSET is a offset constant: negative values raise 
   // the osc curve,  positive values lower it
// pps_tweak = ((float) pps_offset) * OSC_FILTER_OFFSET * (OSC_P_GAIN/1000.0F);
// osc_PID_error = ((float) osc_offset) + pps_tweak;  // OSC_FILTER_OFFSET;
   pps_tweak = avg_pps * OSC_FILTER_OFFSET * (OSC_P_GAIN/1000.0);
   osc_PID_error = avg_osc + pps_tweak;  // OSC_FILTER_OFFSET;

   // osc_integral_step is the clipped osc error
   osc_integral_step = osc_PID_error; 
   if(osc_integral_step > OSC_MAX_INTEGRAL) osc_integral_step = OSC_MAX_INTEGRAL;  
   else if(osc_integral_step < (-OSC_MAX_INTEGRAL)) osc_integral_step = (-OSC_MAX_INTEGRAL);

   if((osc_PID_out >= OSC_MAX_PID) && (osc_integral_step > 0.0)) osc_integral_step *= (-osc_integrator_reset);
   else if((osc_PID_out <= (-OSC_MAX_PID)) && (osc_integral_step < 0.0)) osc_integral_step *= (-osc_integrator_reset);

   osc_integrator += (osc_integral_step * osc_k4);
   if((osc_integrator+osc_k6) > 1.0F) osc_integrator = (1.0F-osc_k6);    // integrator value is maxed out
   else if((osc_integrator+osc_k6) < (-1.0F)) osc_integrator = (-1.0F-osc_k6);

   osc_PID_out = (osc_k1*osc_PID_error) + (osc_k2*last_osc_PID_error) + (osc_k3*last_osc_PID_out);
   last_osc_PID_out = osc_PID_out;
   if(osc_PID_out > 0.0F) osc_PID_out -= osc_k7;   // autotune debug - manual step size
   else                   osc_PID_out += osc_k7;
   osc_PID_out += (osc_integrator + osc_k6);

   // clamp the filter response
   if(osc_PID_out > OSC_MAX_PID) osc_PID_out = OSC_MAX_PID;
   else if(osc_PID_out < (-OSC_MAX_PID)) osc_PID_out = (-OSC_MAX_PID);

   if(osc_postfilter) {
      if(post_q_count >= osc_postfilter) post_q_sum -= osc_post_q[post_q_in];
      post_q_sum += osc_PID_out;
      osc_post_q[post_q_in] = osc_PID_out;
      if(++post_q_in >= osc_postfilter)   post_q_in = 0;
      if(++post_q_count > osc_postfilter) post_q_count = osc_postfilter;
      osc_PID_val = post_q_sum / (double) post_q_count;
   }
   else {
      osc_PID_val = osc_PID_out;
   }

   // convert filter output to DAC volatge
   dac_ctrl = (osc_PID_val*MAX_PID_HZ) / (double) osc_gain;
   dac_ctrl += (double) osc_pid_initial_voltage;
   set_dac_voltage((float) dac_ctrl);

   #ifdef DEBUG_OSC_CONTROL
      if(osc_pid_debug) {
         sprintf(dbg,  " PID(%.5f)  post(%.5f,%d)  last_out(%.5f)  err(%.5f)  last_err(%.5f)  int=%.5f  ramp=%.1f",
            osc_PID_out, osc_PID_val,post_q_count,  last_osc_PID_out, osc_PID_error*(-1.0F),  last_osc_PID_error, osc_integrator, osc_rampup);
         last_osc_PID_display =  osc_PID_out;
         debug_text = &dbg[0];
      }
   #endif
}

void enable_osc_control()
{
   osc_discipline = 0;
   set_discipline_mode(4);
   set_dac_voltage(dac_voltage);
   osc_pid_initial_voltage = dac_voltage;

   osc_control_on = 1;
   osc_rampup = 1.0;
}

void disable_osc_control()
{
   if(osc_control_on == 0) return;

   osc_discipline = 1;
   set_discipline_mode(5);

   osc_control_on = 0;
}


void request_pps(void);
int trick_tick;
#define MAX_TRICK 1000.0

void control_osc()
{
double true_pps;

   // calculate integral of osc signal
   if(trick_scale) {
      if(trick_tick) --trick_tick;
      else {
         true_pps = (pps_offset + trick_value);
         trick_value = (true_pps * trick_scale);

         if(trick_value > MAX_TRICK) trick_value = MAX_TRICK;
         else if(trick_value <= (-MAX_TRICK)) trick_value = (-MAX_TRICK);

         set_pps(user_pps_enable, pps_polarity,  trick_value*1.0E-9,  300.0, 0);
         request_pps();
         sprintf(plot_title,  "Cable trick:  scale=%f  ofs=%f", trick_scale, trick_value);
         title_type = OTHER;
         trick_tick = 5;
      }
   }

   if(osc_control_on) {  // we are controlling the oscillator disciplining
      osc_pid_filter();
   }

   if(dac_dac) {         // we are doing a osc param autotune
      calc_osc_gain();
   }
}

#endif  // OSC_CONTROL

void load_china_data()
{
FILE *f, *o;
s16 nyears;
s16 index;
int i;
u32 val;
u32 jd;
long yy;
double x,y;

   f = fopen("chinese.dat", "rb");
   if(f == 0) return;
   o = fopen("ccc", "w");

   fread(&nyears, 2, 1, f);
   fread(&index, 2, 1, f);
   fprintf(o, "ny=%d  ndx=%d\n", (int) nyears, (int) index);

   val = 0;
   for(i=0; i<nyears; i++) {
      yy = index+i;

      fread(&val, 3, 1, f);
      jd = val >> 13;

      fprintf(o, "   { 0x%04lX, %2ld, ", val&0x1FFFL, jd%14L);
      jd = (yy*365L) + (yy/4L) + 757862L + jd/14L;
      fprintf(o, "%.1f },  // %d %d \n", ((float) jd)-0.5F, yy+60, yy-4635+1998);
   }

   for(i=2009; i<=2100; i++) {
      pri_year = i;
      calc_seasons();
      x = chinese_ny();
      y = china_data[i-2009].jd;
      fprintf(o, "%d %f %f %f:  ", i, x, y, x-y);
      gregorian(x);
      fprintf(o, "ms:%d/%d  ", g_day,g_month);
      gregorian(y);
      fprintf(o, "pp:%d/%d\n", g_day,g_month);
   }

   fclose(o);
   fclose(f);
}

#ifdef GEN_CHINA_DATA
void load_hk_data()
{
FILE *f, *o;
int yy,mm,dd;
char c;
int cday;
int last_cday;
int last_cmonth;
int leap;
int mask;
double jd, last_jd;
int months;

   f = fopen("hk_data.txt", "r");
   if(f == 0) return;
   o = fopen("hhh", "w");

   last_cday = 0;
   last_cmonth = 1;
   months = 0;
   mask = 0x0001;
   jd = last_jd = 0.0;
   leap = 0;
   while(fgets(out, sizeof out, f) != NULL) {
      strupr(out);
      sscanf(out, "%d%c%d%c%d %d", &yy,&c,&mm,&c,&dd, &cday);
      if(strstr(out, "LUNAR")) {
         if(last_cday) {
            if(last_cmonth == cday) leap = cday;
            last_cmonth = cday;
            if(last_cday == 30) months |= mask;
            mask <<= 1;
         }

         if(strstr(out, "1ST LUNAR")) {
            jd = jdate(yy,mm,dd);
            fprintf(o, "   { 0x%04X, %2d, %.1lf },  // %d %d\n", months, leap, last_jd, yy-1+4706-2009, yy-1);
            mask = 0x0001;
            months = 0x0000;
            last_jd = jd;
            leap = 0;
         }
      }
      last_cday = cday;
   }

   fclose(o);
   fclose(f);
}
#endif // GEN_CHINA_DATA

//
// Lady Heather's TSIP bridge
//
// 5-Dec-2009 John Miles, KE5FX
// john@miles.io
//
// Server exit codes:
//
//    0: Normal (shut down at local console)
//    1: Error
//    2: New server.ex1 version detected 
//       (i.e., serve.bat should copy server.ex1 to server.exe and restart)
//

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mswsock.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include <limits.h>
#include <conio.h>
#include <time.h>
#include <assert.h>
#include <malloc.h>

#include "typedefs.h"
#include "timeutil.cpp"
#include "ipconn.cpp"

#define SERVER_VERSION             "1.00"

//
// Configuration equates
//

#define DEFAULT_LOGFILE_NAME       "server.log"
#define N_CLIENTS                   8          // Max # of clients supported
#define DEFAULT_PORT                45000      // Default server port for connection requests
#define KEEPALIVE_INTERVAL_MS       10000      // Client is disconnected if no traffic sent for 10 seconds
#define EXIT_CHECK_INTERVAL_MS      250        // Check for exit request 4x/second
#define MAX_TSIP_BYTES              32768      // Size of TSIP xmit/recv buffers
#define SCREEN_LOG_THRESHOLD        IP_NOTICE  // Show all logged messages >= IP_NOTICE on screen

//
// Serial port globals
//

HANDLE hSerial = INVALID_HANDLE_VALUE;
DCB dcb = {0};

//
// Data from Thunderbolt via RS232
//

U8 hardware_message[MAX_TSIP_BYTES];
S32 hardware_message_bytes = 0;

// 
// Misc globals
//

USTIMER global_timer;

//
// Connection and server data structures
//
// These are subclassed to maintain LH-specific connection data
// and to write diagnostic messages to our logfile
//

struct LH_CONN : public IPCONN
{
   U8  client_message[MAX_TSIP_BYTES];
   S32 client_message_bytes;

   virtual void on_connect(void)
      {
      client_message_bytes = 0; 
      }

   virtual void message_sink(IPMSGLVL level,   
                             C8      *text)
      {
      if (level >= SCREEN_LOG_THRESHOLD)
         {
         printf("%s", global_timer.log_printf("%s\n", text));
         }
      else
         {
         global_timer.log_printf("%s\n", text);
         }
      }

};

class LH_SERVER : public IPSERVER <LH_CONN, N_CLIENTS>
{
   virtual BOOL on_request(SOCKET requestor, sockaddr_in *from_addr)
      {
      message_sink(IP_NOTICE,"");
      return TRUE;
      }

   virtual void message_sink(IPMSGLVL level,   
                             C8      *text)
      {
      if (level >= SCREEN_LOG_THRESHOLD)
         {
         printf("%s", global_timer.log_printf("%s\n", text));
         }
      else
         {
         global_timer.log_printf("%s\n", text);
         }
      }
}
SRVR;

void _____________________________________________________Utility_functions_________________________________(void) 
{}

void SetDtrLine(U8 on)
{   
   if(hSerial != INVALID_HANDLE_VALUE) {
      EscapeCommFunction(hSerial, on ? SETDTR : CLRDTR);
   }
}

void SetRtsLine(U8 on)
{   
   if(hSerial != INVALID_HANDLE_VALUE) {
      EscapeCommFunction(hSerial, on ? SETRTS : CLRRTS);
   }
}

void _____________________________________________________Main_loop___________________________________________(void) 
{}

void shutdown(void)
{
   //
   // Drop serial connections to receiver hardware
   //

   if (hSerial != INVALID_HANDLE_VALUE)
      {
      SetDtrLine(0);

      CloseHandle(hSerial);
      hSerial = INVALID_HANDLE_VALUE;
      }

   //
   // Kill the server
   // 

   SRVR.shutdown();
}

//****************************************************************************
//
// Main app function
//
//****************************************************************************

int main(int argc, char **argv)
{
   S32 i;

   setbuf(stdout,NULL);

   SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

   if ((argc < 2) || (strchr(argv[1],'?') != NULL))
      {
      printf("Usage: server <COM_port> [/port=#] [/log=filename]\n\nExample for Thunderbolt at COM1:\n\n\tserver COM1 /port=40000 /log=logfile.txt\n\n(Default port # is 45000, default logfile is \""DEFAULT_LOGFILE_NAME"\")\n");
      exit(1);
      }

   S32 port = DEFAULT_PORT;
   C8 logfile_name[MAX_PATH] = DEFAULT_LOGFILE_NAME;

   if (argc >= 3)
      {
      for (S32 i=2; i < argc; i++)
         {
         if (!_strnicmp(argv[i],"/port",5)) 
            {
            port = atoi(&argv[i][6]);
            }
         else if (!_strnicmp(argv[i],"/log",4))
            {
            strcpy(logfile_name, &argv[i][5]);
            }
         }
      }

   atexit(shutdown);

   printf("_______________________________________________________________________________\n\n");
   printf("Lady Heather's Control Server V" SERVER_VERSION" of "__DATE__"            John Miles, KE5FX\n");
   printf("Press ESC to terminate, Scroll Lock to view traffic           john@miles.io\n");
   printf("_______________________________________________________________________________\n\n");

   //
   // Initialize COM port
   //
   // kd5tfd hack to handle comm ports > 9
   // see http://support.microsoft.com/default.aspx?scid=kb;%5BLN%5D;115831 
   // for the reasons for the bizarre comm port syntax
   //

   char com_name[64];
   sprintf(com_name, "\\\\.\\%s", argv[1]);

   hSerial = CreateFile(com_name,
                        GENERIC_READ | GENERIC_WRITE, 
                        0,
                        0,
                        OPEN_EXISTING, 
                        FILE_ATTRIBUTE_NORMAL, 
                        0);

   if(hSerial == INVALID_HANDLE_VALUE) {
      printf("ERROR: Can't open port %s\n", com_name);
      exit(1);
   }

   dcb.DCBlength = sizeof(dcb);
   if(!GetCommState(hSerial, &dcb)) {
      printf("ERROR: Can't get comm state\n");
      exit(1);
   }

   dcb.BaudRate        = 9600;
   dcb.ByteSize        = 8;
   dcb.Parity          = NOPARITY;
   dcb.StopBits        = ONESTOPBIT;
   dcb.fBinary         = TRUE;
   dcb.fOutxCtsFlow    = FALSE;
   dcb.fOutxDsrFlow    = FALSE;
   dcb.fDtrControl     = DTR_CONTROL_ENABLE;    // zserial.cpp clears this 
   dcb.fDsrSensitivity = FALSE;
   dcb.fOutX           = FALSE;
   dcb.fInX            = FALSE;
   dcb.fErrorChar      = FALSE;
   dcb.fNull           = FALSE;
   dcb.fRtsControl     = RTS_CONTROL_ENABLE;    // zserial.cpp clears this
   dcb.fAbortOnError   = FALSE;

   // zserial.cpp sets XonLim=2048, XoffLim=512, StopBits=stopBits-1, XonChar=0x11, XoffChar=0x13

   if(!SetCommState(hSerial, &dcb)) {
      printf("ERROR: Can't get comm state\n");
      exit(1);
   }

   // zserial.cpp does SetupComm(port,8192,100) and then sets DTR and RTS
   // zserial.cpp does SetCommMask(port, EV_RXCHAR | EV_CTS | EV_DSR | EV_RLSD | EV_ERR | EV_RING)
   // zserial.cpp then clears RTS

   // set com port timeouts so we return immediately if no serial port
   // character is available

   // (zserial.cpp does WaitCommEvent for an EV_RXCHAR event)

   COMMTIMEOUTS cto = { 0, 0, 0, 0, 0 };
   cto.ReadIntervalTimeout = MAXDWORD;
   cto.ReadTotalTimeoutConstant = 0;
   cto.ReadTotalTimeoutMultiplier = 0;

   if(!SetCommTimeouts(hSerial, &cto)) {
      printf("ERROR: Can't set comm timeouts\n");
      exit(1);
   }

   SetDtrLine(1);

   //
   // Write header to log file
   //

   FILE *debugfile = fopen(logfile_name,"a+t");

   if (debugfile != NULL)
      {
      C8 text[512] = { 0 };

      fprintf(debugfile,
              "\n-------------------------------------------------------------------------------\n"
              "HEATHER server logfile generated by server version " SERVER_VERSION "\n"
              "Start time: %s\n"
              "-------------------------------------------------------------------------------\n\n"
              ,global_timer.timestamp(text, sizeof(text)));

      fclose(debugfile);
      }

   global_timer.set_log_filename(logfile_name);

   //
   // Init server object
   //

   SRVR.startup(port);

   SRVR.set_keepalive_ms(KEEPALIVE_INTERVAL_MS);

   //
   // Execute foreground loop
   //

   S32 last_exit_check_time = timeGetTime();

   for (;;)
      {
      S32 activity = 0;

      S32 cur_time = timeGetTime();

      if ((cur_time - last_exit_check_time) >= EXIT_CHECK_INTERVAL_MS)
         {
         last_exit_check_time = cur_time;

         S32 ch = 0;

         if (_kbhit())
            {
            ch = tolower(_getch());
            }

         if (ch == 27)
            {
            global_timer.log_printf("Manual shutdown requested (ESC)\n");
            break;
            }

         FILE *new_version = fopen("server.ex1","rb");
         
         if (new_version != NULL)
            {
            fclose(new_version);
            Sleep(2000);

            global_timer.log_printf("Server terminated due to release of new version\n");
            exit(2);
            }
         }

      // --------------------------------------
      // Release disconnected client objects and
      // check for new client connection requests
      //
      // Exit if the SRVR object failed for any reason
      // --------------------------------------

      if (!SRVR.status())
         {
         exit(1);
         }

      SRVR.update();

      // ---------------------------------------------------------------
      // Check for incoming client packets and send them to the hardware
      // ---------------------------------------------------------------

      for (i=0; i < N_CLIENTS; i++)
         {
         LH_CONN *C = SRVR.clients[i];

         if (C == NULL)
            {
            continue;
            }

         //
         // Keep reading until no more data is available from each client, before
         // moving on to the next one
         //

         for (;;)
            {
            U8 ch = 0;

            C->receive_poll();        

            if (C->read_block((C8 *) &ch, 1) == 0)
               {
               break;
               }

            activity++;

            //
            // Add byte to client message buffer, and transmit the buffer
            // to the hardware if we receive an ETX (0x03) preceded by 
            // an odd number of DLE (0x10) bytes
            //
            // The client breaks command packets up at ETX boundaries, but on the server side
            // we need to wait for a valid <DLE><ETX> sequence before sending the packet to
            // the hardware, in case other clients are also sending traffic that would
            // interrupt the command
            //
            // Note that this means that the C->client_message[] buffer must be large 
            // enough to contain any possible command packet, since we can't reliably flush 
            // it in the middle of a command
            //

            C->client_message[C->client_message_bytes++] = ch;

            BOOL do_flush = FALSE;

            if (C->client_message_bytes >= sizeof(C->client_message))
               {
               global_timer.log_printf("WARNING: Client %d command buffer full; flushing contents\n", i);
               do_flush = TRUE;
               }
            else
               {
               if (ch == 3)
                  {
                  S32 n_preceding_DLEs = 0;

                  for (S32 n=C->client_message_bytes-2; n >= 0; n--)    // (don't check the ETX we just wrote at -1)
                     {
                     if (C->client_message[n] != 0x10)
                        {
                        break;
                        }

                     ++n_preceding_DLEs;
                     }

                  do_flush = ((n_preceding_DLEs & 1) != 0);
                  }
               }

            if (do_flush)
               {
               if (GetKeyState(VK_SCROLL) & 0x1)
                  {
                  printf("\nFrom client %d: ",i);

                  for (S32 j=0; j < C->client_message_bytes; j++)
                     {
                     printf("%.02X ",C->client_message[j]);
                     }

                  printf("\n");
                  }

               DWORD written = 0;

               S32 flag = WriteFile(hSerial, 
                             (C8 *) C->client_message, 
                                    C->client_message_bytes, 
                                   &written, 
                                    NULL);

               if ((flag == 0) || (written != C->client_message_bytes)) 
                  {
                  global_timer.log_printf("Slot %d: WriteFile() error, flag=%d, written=%d, tried=%d\n", i, flag, written, C->client_message_bytes);
                  break;
                  }

               C->client_message_bytes = 0;
               }
            }
         }

      // ---------------------------------------------------------------
      // Check for traffic from hardware and send it to all the clients
      // ---------------------------------------------------------------

      for (;;)
         {
         DWORD bc = 0;
         U8 ch = 0;
         ReadFile(hSerial, &ch, 1, &bc, NULL);

         if (bc != 1)
            {
            break;
            }

         //
         // Add byte to hardware message buffer, and transmit the buffer
         // to all clients if it fills up, or if we receive an ETX (03)
         //

         activity++;

         hardware_message[hardware_message_bytes++] = ch;

         if ((hardware_message_bytes >= sizeof(hardware_message)) 
              ||
             (ch == 3))
            {
            for (i=0; i < N_CLIENTS; i++)
               {
               LH_CONN *C = SRVR.clients[i];

               if (C == NULL)
                  {
                  continue;
                  }

               if (GetKeyState(VK_SCROLL) & 0x1)
                  {
                  printf("\nTo client %d: ",i);

                  for (S32 j=0; j < hardware_message_bytes; j++)
                     {
                     printf("%.02X ",hardware_message[j]);
                     }

                  printf("\n");
                  }

               C->send_block(&hardware_message,
                              hardware_message_bytes);
               }

            hardware_message_bytes = 0;
            }
         }

      // ---------------------------------------------------------------
      // Give up some time for other processes, depending on how
      // busy the server is
      // ---------------------------------------------------------------

      if (activity)
         {
         Sleep(0);
         }
      else
         {
         Sleep(20);
         }
      }

   return 0;
}

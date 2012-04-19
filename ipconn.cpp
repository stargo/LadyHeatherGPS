// *****************************************************************************
//
// IPCONN: Lightweight wrapper for TCP/IP sockets (non-blocking by default)
//
// IPSERVER: Template that implements a simple TCP/IP server, managing 
// an array of IPCONN-derived connection instances on behalf of the app
//
// jmiles@pop.net 12-Mar-10
//
// *****************************************************************************

#pragma once                           

#ifndef _WINSOCK2API_
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <mswsock.h>
#endif

//
// Configuration
//

#ifndef IPCONN_DEFAULT_RECV_BUF_BYTES          // Circular buffer used to reduce overhead of calling recv() for small block sizes
  #define IPCONN_DEFAULT_RECV_BUF_BYTES 8192   // (Reducing this value too far can stall low-bandwidth connections) 
#endif                                         
                                        
#ifndef IPCONN_MAX_SEND_ATTEMPTS
  #define IPCONN_MAX_SEND_ATTEMPTS 300         // # of times send() will be tried if it returns WSAEWOULDBLOCK
#endif

#ifndef IPCONN_SEND_RETRY_DELAY_MS
  #define IPCONN_SEND_RETRY_DELAY_MS 10        // Wait 10 ms before retrying after WSAEWOUDLBLOCK
#endif

#ifndef IPCONN_SO_RECVBUF_BYTES
  #define IPCONN_SO_RECVBUF_BYTES 32768        // TCP send/receive windows (Win32 defaults are 8K each)
#endif

#ifndef IPCONN_SO_XMITBUF_BYTES
  #define IPCONN_SO_XMITBUF_BYTES 32768
#endif

#ifndef IPCONN_NAGLE
  #define IPCONN_NAGLE 0                       // Nagle disabled by default
#endif

enum IPMSGLVL
{
   IP_DEBUG = 0,                        // Debugging traffic
   IP_VERBOSE,                          // Low-level notice
   IP_DISCON,                           // Reason for disconnection (connection was lost, possibly intentionally dropped)
   IP_NOTICE,                           // Standard notice
   IP_WARNING,                          // Warning (does not imply loss of connection or failure of operation)
   IP_ERROR                             // Error (implies failure of operation, and/or abnormal connection loss)
};

enum IPDREASON
{
   IPDR_REPLACED,                       // Server is booting this connection to make room for a new one
   IPDR_DISCONNECTED,                   // Server is deleting this connection because it's no longer valid
   IPDR_TIMED_OUT,                      // Server is deleting this connection due to keepalive expiration
   IPDR_TERMINATING                     // Server is cleaning up all remaining connections at termination time
};

enum IPSRVRSTATUS
{
   IPSS_FAILURE,                        // Server's constructor didn't run successfully for some reason
   IPSS_INITIALIZED,                    // Server's constructor finished; public vars are valid (but possibly not meaningful)
   IPSS_LISTENING                       // Server is actively maintaining connections
};

void ___________________________________________________________IPCONN____________________________________________________(void) 
{}

class IPCONN
{
   SOCKET IP_socket;                    // TCP socket used to communicate with server
   BOOL   active;                       // TRUE if socket has a valid connection
   BOOL   local;                        // TRUE if host is on our machine or class-C subnet
   BOOL   blocking_mode;                // TRUE if socket has been set to block

   sockaddr_in server_addr;             // Various other buffers and vars 
   C8          local_hostname[1024];          
   C8          remote_hostname[1024];
   S32         xmit_byte_count;
   S32         recv_byte_count;
   C8         *rcv_buf;
   S32         rcv_head;
   S32         rcv_tail;
   S32         rcv_bufsize;
   S64         connection_time;
   S64         last_recv_time;
   BOOL        ws_started;
   DWORD       last_error_code;
   S32         user_int;
   void       *user_void;

public:

   USTIMER timer;                        // Public, for ease of access to routines such as log.printf()

   //
   // Generate dotted-quad address string based on IPv4 AF_INET socket designator
   //

   C8 *address_string(sockaddr_in *address, 
                      C8          *text, 
                      S32          text_array_size)
      {
      assert(text != NULL);

      memset(text, 0, text_array_size);
      S32 n = text_array_size - 1;

      _snprintf(text, n, "%s:%d",
         inet_ntoa(address->sin_addr),
             ntohs(address->sin_port));

      return text;
      }

   //                                                                          
   // Reverse-DNS utility routine to identify remote hosts
   //
   // This routine can take a long time, so it's better to use
   // a worker thread with getnameinfo(), as IPSERVER does
   //

   BOOL hostname_string(sockaddr_in *address, 
                        C8          *text, 
                        S32          text_array_size,
                        S32          timeout_ms)
      {
      assert (text != NULL);

      if (timeout_ms == 0)
         {
         address_string(address, text, text_array_size);
         return TRUE;
         }

      //
      // Handle returned by async version won't work with WaitForSingleObject(), but
      // this hack appears to work
      //

      C8 buffer[MAXGETHOSTSTRUCT];
      memset(buffer, 0, sizeof(buffer));
   
      volatile struct hostent *HE = (struct hostent *) buffer;
      HE->h_name = "(Unresolved)";
   
      HANDLE request = WSAAsyncGetHostByAddr(NULL,       // HWND
                                             WM_USER,
                                             (C8 *) &address->sin_addr, 
                                             sizeof(*address),
                                             PF_INET,
                                             buffer,
                                             sizeof(buffer));
      if (request == 0)
         {
         return FALSE;
         }
   
      S32 start = timeGetTime();
      S32 end   = 0;
   
      for (;;)
         {
         Sleep(10);
   
         end = timeGetTime();
   
         if ((end - start) >= timeout_ms)
            {
            break;
            }

         if ((HE->h_name != NULL) && (HE->h_name[0] != '('))
            {
            Sleep(100);    // the pointer write should be atomic, but wait a bit just to be safe(r)
            break;
            }
         }

      if (WSACancelAsyncRequest(request) == SOCKET_ERROR)
         {
         DWORD err = WSAGetLastError();

         if ((err != WSAEINVAL) && (err != WSAEALREADY))
            {
            WSASetLastError(err);
            return FALSE;
            }
         }
   
      memset(text, 0, text_array_size);

      if ((HE != NULL) && (text_array_size > 16))
         {
         strncpy(text, HE->h_name, text_array_size-16);
#if 0
         ::sprintf(&dest[strlen(dest)]," (%d ms)", end-start);
#endif
         }

      S32 len = strlen(text);

      if (len < 3)      // (unavailable or degenerate reverse-DNS name)
         {
         address_string(address, text, text_array_size);
         }

      return TRUE;
      }

   //
   // Constructor
   //

   IPCONN()
      {
      IP_socket = INVALID_SOCKET;
      active = FALSE;
      local = FALSE;
      blocking_mode = FALSE;

      memset(&server_addr,       0, sizeof(server_addr));
      memset(local_hostname,     0, sizeof(local_hostname));
      memset(remote_hostname,    0, sizeof(remote_hostname));

      xmit_byte_count = 0;
      recv_byte_count = 0;

      rcv_buf     = NULL;
      rcv_head    = 0;
      rcv_tail    = 0;
      rcv_bufsize = 0;

      connection_time = 0;
      last_recv_time  = 0;
      ws_started      = FALSE;
      last_error_code = 0;
      user_int        = 0;
      user_void       = NULL;
      }

   //
   // Destructor releases connection object and Winsock reference
   // 

   virtual ~IPCONN()
      {
      disconnect();
      }

   virtual void disconnect(void)
      {
      active = FALSE;

      if (IP_socket != INVALID_SOCKET)
         {
         closesocket(IP_socket);
         IP_socket = INVALID_SOCKET;
         }

      if (rcv_buf != NULL)
         {
         free(rcv_buf);
         rcv_buf = NULL;
         }

      if (ws_started)
         {
         WSACleanup();
         ws_started = FALSE;
         }
      }

   //
   // Establish connection
   //
   // Should be called right after construction to activate the connection object.  
   // Subsequent calls will have no effect.  (The connection may have been dropped
   // locally or at the server end, or it may have failed for other reasons -- 
   // call status() frequently to check for loss of connectivity.)
   //
   // Separate from constructor because the caller might want to subclass some virtual methods,
   // e.g. on_connect()
   //

   virtual void connect(C8  *IP_address,                              
                        S32  default_port,         // (Used if the IP_address doesn't specify a :port substring)
                        S32  receive_buffer_size = IPCONN_DEFAULT_RECV_BUF_BYTES)
      {
      C8 msg[256];

      if (ws_started || active)
         {
         return;
         }

      //
      // Request Winsock services and get our own hostname
      //

      WSADATA wsadata;
     
      WORD wRequestVer = MAKEWORD(2,2);
     
      if (WSAStartup(wRequestVer, &wsadata)) 
         {
         message_printf(IP_ERROR, "Couldn't open WinSock");
         return;
         }

      ws_started = TRUE;

      if (gethostname(local_hostname, sizeof(local_hostname)-1) == SOCKET_ERROR) 
         {
         message_printf(IP_ERROR, "Local hostname unobtainable");
         return;
         }
     
      LPHOSTENT lphp;
      struct in_addr inaddrIP;
     
      lphp = gethostbyname(local_hostname);
      inaddrIP = *(struct in_addr *) (lphp->h_addr);
     
      C8 ipnum[512];
      strcpy(ipnum,inet_ntoa(inaddrIP));
     
      message_printf(IP_VERBOSE, "Using Windows Sockets V%d.%d (%s)",
         HIBYTE(wsadata.wVersion),
         LOBYTE(wsadata.wVersion),
         wsadata.szDescription);

      message_printf(IP_NOTICE, "Initializing host %s, address %s",
         local_hostname,
         ipnum);
     
      //
      // Allocate socket
      //

      IP_socket = ::socket(PF_INET, SOCK_STREAM, 0);
     
      if (IP_socket == INVALID_SOCKET) 
         {
         message_printf(IP_ERROR, "Error: invalid socket (%s)", error_text(msg,sizeof(msg)));
         return;
         }

      //
      // Determine remote address
      //

      C8  server_address_name[1024];
      S32 server_port_num = default_port;
     
      memset(server_address_name, 0, sizeof(server_address_name));
      strncpy(server_address_name, IP_address, sizeof(server_address_name)-1);
     
      C8 *colon = strrchr(server_address_name,':');
     
      if (colon != NULL) 
         {
         *colon = 0;
         server_port_num = atoi(&colon[1]);
         }

      colon = strchr(server_address_name,':');

      if (colon != NULL)      // handle cases with multiple port numbers appended by batch files, etc.
         {
         *colon = 0;
         }
     
      memset(&server_addr,0,sizeof(server_addr));
     
      server_addr.sin_family = PF_INET;
      server_addr.sin_port   = htons((U16) server_port_num);
     
      //
      // Resolve it
      //

      DWORD li = inet_addr(server_address_name);
     
      if (li != INADDR_NONE) 
         {
         //
         // Numeric IP was parsed successfully
         //

         server_addr.sin_addr.s_addr = li;
         }
      else 
         {
         //
         // Otherwise, try DNS
         //

         HOSTENT *hostinfo = gethostbyname(server_address_name);
     
         if (hostinfo == NULL) 
            {
            message_printf(IP_ERROR, "Host '%s' not found (code %d)", server_address_name, WSAGetLastError());
            return;
            }
     
         memcpy(&server_addr.sin_addr, hostinfo->h_addr, hostinfo->h_length);
         }
     
      message_printf(IP_NOTICE, "Attempting to connect to server %s:%d . . .",
              inet_ntoa(server_addr.sin_addr),
              ntohs    (server_addr.sin_port));
     
      S32 result = ::connect(IP_socket,
               (sockaddr *) &server_addr,
                             sizeof(server_addr));
     
      if (result) 
         {
         message_printf(IP_ERROR, "Connection failed: %s", error_text(msg,sizeof(msg))); 
         return;
         }
     
      //
      // Configure Nagle algorithm and blocking
      //

      DWORD dwVal = !IPCONN_NAGLE;
     
      setsockopt(IP_socket,    
                 IPPROTO_TCP,
                 TCP_NODELAY,
         (C8 *) &dwVal,
                 sizeof(dwVal));
     
      set_blocking_mode(FALSE);
     
      //
      // Set send/receive window size
      //

      S32 send_size = IPCONN_SO_XMITBUF_BYTES;
      S32 recv_size = IPCONN_SO_RECVBUF_BYTES;

      setsockopt(IP_socket,
                 SOL_SOCKET,
                 SO_SNDBUF,
         (C8 *) &send_size,
                 sizeof(send_size));

      setsockopt(IP_socket,
                 SOL_SOCKET,
                 SO_RCVBUF,
         (C8 *) &recv_size,
                 sizeof(recv_size));

      //
      // Distinguish local connections from remote ones, where possible
      //

      local = FALSE;

      if (!_strnicmp(IP_address,"localhost",9))
         {
         local = TRUE;
         }
      else
         {
         U32 remote_IP = ntohl(server_addr.sin_addr.S_un.S_addr);
         U32 local_IP  = ntohl(inaddrIP.S_un.S_addr);

         local = (remote_IP & 0xffffff00) == (local_IP & 0xffffff00);

         if (remote_IP == ((127 << 24) | (0 << 16) | (0 << 8) | 1))
            {
            local = TRUE;   
            }
         }

      //
      // Get remote hostname (doesn't use reverse DNS due to time required, and
      // because an app-initiated connection probably knows what it's connecting to...)
      //

      address_string(&server_addr, remote_hostname, sizeof(remote_hostname));

      //
      // Allocate receive buffer and mark connection as active
      //

      rcv_bufsize = receive_buffer_size;

      rcv_buf = (C8 *) malloc(rcv_bufsize);

      if (rcv_buf == NULL)
         {
         message_printf(IP_ERROR, "Out of memory");
         return;
         }

      last_recv_time  = timer.us();
      connection_time = last_recv_time;

      message_printf(IP_NOTICE, "Connected to server %s:%d",
              inet_ntoa(server_addr.sin_addr),
              ntohs    (server_addr.sin_port));

      active = TRUE;
      on_connect();
      }

   //
   // Called by server after a successful accept() operation that returns
   // a socket.  Not normally used by apps
   //

   virtual void server_accept(sockaddr_in *who, 
                              SOCKET       requestor,
                              S32          receive_buffer_size = IPCONN_DEFAULT_RECV_BUF_BYTES)
      {
      if (active)
         {
         return;
         }

      if (gethostname(local_hostname, sizeof(local_hostname)-1) == SOCKET_ERROR) 
         {
         message_printf(IP_ERROR, "Local hostname unobtainable");
         return;
         }
     
      LPHOSTENT lphp;
      struct in_addr inaddrIP;
     
      lphp = gethostbyname(local_hostname);
      inaddrIP = *(struct in_addr *) (lphp->h_addr);
     
      C8 ipnum[512];
      strcpy(ipnum,inet_ntoa(inaddrIP));
     
      message_printf(IP_VERBOSE, "Server \"%s\", address %s",
         local_hostname,
         ipnum);

      //
      // Configure Nagle algorithm and set non-blocking mode
      //

      IP_socket = requestor;

      DWORD dwVal = !IPCONN_NAGLE;
     
      setsockopt(IP_socket,    
                 IPPROTO_TCP,
                 TCP_NODELAY,
         (C8 *) &dwVal,
                 sizeof(dwVal));
     
      dwVal = 1;

      ioctlsocket(IP_socket,   
                  FIONBIO,
                  &dwVal);
     
      //
      // Set send/receive window size
      //

      S32 send_size = IPCONN_SO_XMITBUF_BYTES;
      S32 recv_size = IPCONN_SO_RECVBUF_BYTES;

      setsockopt(IP_socket,
                 SOL_SOCKET,
                 SO_SNDBUF,
         (C8 *) &send_size,
                 sizeof(send_size));

      setsockopt(IP_socket,
                 SOL_SOCKET,
                 SO_RCVBUF,
         (C8 *) &recv_size,
                 sizeof(recv_size));

      //
      // Distinguish local connections from remote ones, where possible
      //

      local = FALSE;

      U32 remote_IP = ntohl(who->sin_addr.S_un.S_addr);
      U32 local_IP  = ntohl(inaddrIP.S_un.S_addr);

      local = (remote_IP & 0xffffff00) == (local_IP & 0xffffff00);

      if (remote_IP == ((127 << 24) | (0 << 16) | (0 << 8) | 1))
         {
         local = TRUE;   
         }

      //
      // Get remote hostname (doesn't use reverse DNS due to time required,
      // but the server might update this name field later)
      //

      server_addr = *who;
      address_string(&server_addr, remote_hostname, sizeof(remote_hostname));

      //
      // Allocate receive buffer and mark connection as active
      //

      rcv_bufsize = receive_buffer_size;

      rcv_buf = (C8 *) malloc(rcv_bufsize);

      if (rcv_buf == NULL)
         {
         message_printf(IP_ERROR, "Out of memory");
         return;
         }

      last_recv_time  = timer.us();
      connection_time = last_recv_time;

      message_printf(IP_VERBOSE, "Incoming connection from %s:%d",
              inet_ntoa(who->sin_addr),
              ntohs    (who->sin_port));

      active = TRUE;
      on_connect();
      }

   //
   // Copy OS-specific error text to buffer and return it
   //

   virtual C8 *error_text(C8 *text, S32 text_array_size, DWORD last_error = 0)
      {
      LPVOID lpMsgBuf = "Hosed";       // Default message for Win9x (FormatMessage() handles Winsock messages on NT only)

      last_error_code = (last_error == 0) ? GetLastError() : last_error;

      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                    NULL,
                    last_error_code,
                    0,
          (LPTSTR) &lpMsgBuf, 
                    0, 
                    NULL);

      memset(text, 0, text_array_size);
      strncpy(text, (C8 *) lpMsgBuf, text_array_size-1);

      if (strcmp((C8 *) lpMsgBuf,"Hosed"))
         {
         LocalFree(lpMsgBuf);
         }

      return text;
      }

   //
   // Progress monitor function for lengthy operations
   // Connection is dropped if this function returns FALSE
   //

   virtual bool monitor(S32 bytes_so_far, S32 bytes_total)
      {
      return TRUE;
      }

   //
   // Error/status message sink
   //

   virtual void message_sink(IPMSGLVL level,   
                             C8      *text)
      {
      ::printf("%s\n",text);
      }

   virtual void message_printf(IPMSGLVL level,
                               C8 *fmt,
                               ...)
      {
      C8 buffer[4096];

      va_list ap;

      va_start(ap, 
               fmt);

      memset(buffer, 0, sizeof(buffer));

      _vsnprintf(buffer,
                 sizeof(buffer)-1,
                 fmt,
                 ap);

      va_end(ap);

      //
      // Remove trailing whitespace
      //

      C8 *end = &buffer[strlen(buffer)-1];

      while (end > buffer)
         {
         if (!isspace((U8) *end)) 
            {
            break;
            }

         *end = 0;
         end--;
         }

      message_sink(level,
                   buffer);
      }

   //
   // Get/set user words
   //

   virtual S32 user_word(void)
      {
      return user_int;
      }

   virtual S32 set_user_word(S32 val)
      {
      S32 prev = user_int;
      user_int = val;
      return prev;
      }

   virtual void *user_ptr(void)
      {
      return user_void;
      }

   virtual void *set_user_ptr(void *val)
      {
      void *prev = user_void;
      user_void = val;
      return prev;
      }

   //
   // Set/clear blocking mode
   //
   // High-throughput TCP apps are better off with blocking sockets, as OS blocking is 
   // somewhat more efficient than sleeping on incomplete read calls at the application level.
   // Timeout management is somewhat more complicated, though...
   //

   virtual void set_blocking_mode(BOOL mode)
      {
      if (IP_socket == INVALID_SOCKET)
         {
         return;
         }

      blocking_mode = mode;

      DWORD dwVal = blocking_mode ? 0:1;

      ioctlsocket(IP_socket,   
                  FIONBIO,
                 &dwVal);
      }

   //
   // Return TRUE if connection is valid, or FALSE if any routines have detected closure
   // or other fatal conditions
   //
   // The app (or server) should delete the IPCONN object once its status has gone FALSE, 
   // but it's OK to check only once per frame, or otherwise when the app is in an appropriate
   // context.  There is no need to check status() before every IPCONN call; the other 
   // methods are designed to behave harmlessly if the connection is invalid.  E.g., 
   // read_block() will return 0 bytes, send_block() will return FALSE, etc.  
   //

   virtual BOOL status(void)
      {
      return active;
      }

   //
   // Return last error code reported by OS
   //

   virtual S32 last_error(void)
      {
      return last_error_code;
      }

   //
   // Transport-specific info functions
   //

   virtual void host_info(C8          **local_name  = NULL, 
                          C8          **remote_name = NULL, 
                          sockaddr_in **remote_addr = NULL)
      {
      if (local_name  != NULL) *local_name  =  local_hostname;
      if (remote_name != NULL) *remote_name =  remote_hostname;
      if (remote_addr != NULL) *remote_addr = &server_addr;
      }

   virtual void update_remote_hostname(C8 *new_remote_hostname)
      {
      memset(remote_hostname, 0, sizeof(remote_hostname));
      strncpy(remote_hostname, new_remote_hostname, sizeof(remote_hostname)-1);
      }

   //
   // Connection statistics
   //

   virtual S64 statistics(S32 *bytes_received = NULL,
                          S32 *bytes_sent     = NULL,
                          S32 *xmit_bit_rate  = NULL,
                          S32 *recv_bit_rate  = NULL)
      {
      if (bytes_received != NULL) *bytes_received = recv_byte_count;
      if (bytes_sent     != NULL) *bytes_sent     = xmit_byte_count;

      S64 age = timer.us() - connection_time;
      if (age < 1) age = 1;

      if (xmit_bit_rate != NULL)
         {
         *xmit_bit_rate = (S32) (((S64) 8000000 * (S64) xmit_byte_count) / age);
         }

      if (recv_bit_rate != NULL)
         {
         *recv_bit_rate = (S32) (((S64) 8000000 * (S64) recv_byte_count) / age);
         }

      return age;
      }

   //
   // Connection age (in string form)
   //

   virtual C8 *age_string(C8  *text,  
                          S32  text_array_size)     
      {
      assert(text != NULL);
      memset(text, 0, text_array_size);

      S64 age = timer.us() - connection_time;
      if (age < 1) age = 1;

      return timer.duration_string(age, text, text_array_size);
      }

   //
   // Return TRUE if this is a server-client connection that should never be 
   // disconnected to make room for another one
   //

   virtual BOOL is_protected(void)
      {
      return FALSE;
      }

   //
   // Return TRUE if the target of this connection is local to the host or its class-C block
   // 

   virtual BOOL is_local(void)
      {
      return local;
      } 

   //
   // Time in milliseconds since traffic was received
   //

   virtual S64 us_since_recv(void)
      {
      return timer.us() - last_recv_time;
      }

   //
   // Accessor for underlying OS socket 
   //

   virtual SOCKET socket(void)
      {
      return IP_socket;
      }

   //
   // Called just before successful return from connect() or server_accept()
   // 

   virtual void on_connect(void)
      {
      }

   //
   // Called periodically during lengthy operations to give some time back to
   // the application
   //
   // Currently this only happens when send_block() is backlogged
   //

   virtual void on_lengthy_operation(void)
      {
      }

   //
   // Called when an IPCONN object is about to be deleted by the IPSERVER that
   // owns it
   //

   virtual void on_server_discard(IPDREASON reason)
      {
      }

   //
   // Transmit a block of data
   //
   // We retry blocked send() calls for (IPCONN_MAX_SEND_ATTEMPTS-1)*10 milliseconds
   // before giving up, in case the receiver falls behind or an ACK is lost
   //
   // Returns FALSE if the operation failed for any reason
   //

   virtual BOOL send_block(void *block, S32 n_bytes)
      {
      C8 msg[256];

      if ((!active) || (block == NULL) || (n_bytes == 0))
         {
         return FALSE;    
         }

      S32 nonblocking_retries = 0;

      for (;;)
         {
         S32 result = send(IP_socket,
                    (C8 *) block,
                           n_bytes,
                           0);

         if (result != SOCKET_ERROR)
            {
            xmit_byte_count += n_bytes;

            block   = ((C8 *) block) + result;
            n_bytes -= result;

            if (n_bytes <= 0)
               {
               break;            // All bytes have been sent
               }
            }
         else
            {
            S32 WSA_error = WSAGetLastError();

            if (WSA_error != WSAEWOULDBLOCK)
               {
               if ((WSA_error == 0) || (WSA_error == WSAECONNRESET) || (WSA_error == WSAECONNABORTED))
                  message_printf(IP_DISCON, "send() disconnected: %s", error_text(msg,sizeof(msg), WSA_error)); 
               else
                  message_printf(IP_ERROR, "send() failed: %s", WSA_error, error_text(msg,sizeof(msg), WSA_error)); 

               active = FALSE;
               return FALSE;
               }
            }

         if (++nonblocking_retries == IPCONN_MAX_SEND_ATTEMPTS)
            {
            break;
            }

         on_lengthy_operation();
         Sleep(IPCONN_SEND_RETRY_DELAY_MS);
         }

      if (nonblocking_retries == IPCONN_MAX_SEND_ATTEMPTS)
         {
         message_printf(IP_ERROR, "send() failed with WSAEWOULDBLOCK, %d retries exhausted", 
            IPCONN_MAX_SEND_ATTEMPTS);

         active = FALSE;
         return FALSE;
         }

      if (nonblocking_retries > 0)
         {
         message_printf(IP_VERBOSE, "send() returned WSAEWOULDBLOCK (succeeded after %d of %d retries)",
            nonblocking_retries, IPCONN_MAX_SEND_ATTEMPTS); 
         }

      return TRUE;
      }

   //
   // printf() wrapper for send_block()
   //

   virtual BOOL send_printf(C8 *fmt,
                            ...)
      {
      if (!active)
         {
         return FALSE;
         }

      C8 buffer[4096];

      va_list ap;

      va_start(ap, 
               fmt);

      memset(buffer, 0, sizeof(buffer));

      _vsnprintf(buffer,
                 sizeof(buffer)-1,
                 fmt,
                 ap);

      va_end(ap);

      return send_block(buffer, strlen(buffer));
      }

   //
   // Fetch as much data from socket as will fit in our receive buffer, which can store 
   // up to rcv_bufsize-1 bytes (1 byte is lost for housekeeping) 
   //
   // Return total amount of data available in the receive buffer
   //

   virtual S32 receive_poll(S32 *fetched_this_call = NULL)
      {
      C8 msg[256];

      if (fetched_this_call != NULL)
         {
         *fetched_this_call  = 0;
         }

      if (!active) 
         {
         return 0;
         }

      //
      // rcv_head -> where the next recv() byte will be stored
      // rcv_tail -> the next byte to be returned to the caller
      // 
      // Buffer is completely empty when rcv_head == rcv_tail
      // Buffer is completely full when rcv_head+1 == rcv_tail 
      // 

      C8 *seg[2] = { NULL, NULL };
      S32 len[2] = { 0,    0    };

      if (rcv_head < rcv_tail)
         {
         seg[0] = &rcv_buf[rcv_head];
         len[0] = (rcv_tail - rcv_head)-1;     // head can't reach tail, because that's our empty condition
         }
      else
         {
         if (rcv_tail == 0)                    // likewise, we can't wrap the head pointer as long as tail==0
            {
            seg[0] = &rcv_buf[rcv_head];
            len[0] = (rcv_bufsize - rcv_head)-1; 
            }
         else
            {
            seg[0] = &rcv_buf[rcv_head];
            len[0] =  rcv_bufsize - rcv_head;  

            seg[1] = rcv_buf;
            len[1] = rcv_tail-1;               // total len can't reach bufsize in the head==tail case
            }
         }

      //
      // Try to read enough data to fill the open segment(s)
      //

      S32 bytes_fetched = 0;

      for (S32 s=0; s < 2; s++)
         {
         if (len[s] == 0)
            {
            continue;
            }

         S32 result = recv(IP_socket,
                    (C8 *) seg[s],
                           len[s],
                           0);

         DWORD WSA_error = WSAGetLastError();

         if (result == 0)                 // No error, but connection has been closed
            {
            if ((WSA_error == 0) || (WSA_error == WSAECONNRESET) || (WSA_error == WSAECONNABORTED))
               message_printf(IP_DISCON, "recv() disconnected: %s", error_text(msg,sizeof(msg), WSA_error)); 
            else
               message_printf(IP_ERROR, "recv() error: %s", error_text(msg,sizeof(msg), WSA_error)); 
               
            active = FALSE;
            return 0;
            }

         if (result != SOCKET_ERROR)      // No error, operation succeeded
            {
            recv_byte_count += result;
            last_recv_time = timer.us();
            }
         else
            {
            result = 0;                   // Either no data was available, or an unrecoverable connection error has occurred

            if (WSA_error != WSAEWOULDBLOCK)
               {
               if ((WSA_error == 0) || (WSA_error == WSAECONNRESET) || (WSA_error == WSAECONNABORTED))
                  message_printf(IP_DISCON, "recv() disconnected: %s", error_text(msg,sizeof(msg), WSA_error)); 
               else
                  message_printf(IP_ERROR, "recv() failed: %s", error_text(msg,sizeof(msg), WSA_error)); 

               active = FALSE;
               return 0;
               }
            }

         bytes_fetched += result;
         rcv_head += result;

         if (rcv_head >= rcv_bufsize)
            {
            rcv_head -= rcv_bufsize;
            }

         if (result < len[s])   
            {
            break;
            }
         }

      if (fetched_this_call  != NULL)
         {
         *fetched_this_call = bytes_fetched;
         }

      if (rcv_head >= rcv_tail)
         return rcv_head - rcv_tail;
      else
         return rcv_head + rcv_bufsize - rcv_tail;
      }

   //
   // Copy incoming data out of the receive buffer, returning the
   // amount actually copied.  
   //
   // Together with receive_poll(), this is basically a wrapper around
   // the recv() call, intended to minimize ring transitions for apps 
   // that read a few bytes at a time.  It could be further optimized 
   // for small transfers by not using memcpy().
   //
   // Up to rcv_bufsize-1 bytes may be returned
   //

   virtual S32 read_block(void *dest, S32 n_bytes)
      {
      if ((!active) || (n_bytes == 0))
         {
         return 0;                                       // disconnected clients return 0 bytes
         }

      if (rcv_head == rcv_tail)
         {
         receive_poll();                                 // if no data at all available, try to fetch some first
         }                                               // (clients would normally want to call receive_poll() themselves
                                                         // to free up Winsock's buffer space)
      C8 *d = (C8 *) dest;
   
      if (rcv_head >= rcv_tail)                  
         {
         n_bytes = min(rcv_head - rcv_tail, n_bytes);

         if (d != NULL)
            {
            memcpy(d, &rcv_buf[rcv_tail], n_bytes);      // all data requested is in one segment 
            }
         }
      else
         {
         n_bytes = min(rcv_head + rcv_bufsize - rcv_tail, n_bytes);
         S32 n = min(n_bytes, rcv_bufsize - rcv_tail);

         if (d != NULL)
            {
            memcpy(d,  &rcv_buf[rcv_tail], n);           // copy at least one byte from first segment 
            memcpy(d+n, rcv_buf,           n_bytes-n);   // copy remainder from second segment
            }
         }

      rcv_tail += n_bytes;

      if (rcv_tail >= rcv_bufsize)
         {
         rcv_tail -= rcv_bufsize;
         }

      return n_bytes;
      }

   //
   // Basic text send/receive functionality, useful for issuing
   // queries and retrieving the resulting data in text form
   //

   bool read_text(FILE   *outfile,
                  C8     *command_string = NULL,    // NULL = do not transmit a command first
                  S32     timeout_msec   = 3000,    // 0 = no timeout
                  S32     dropout_msec   = 3000,
                  S32     repeat_cnt     = 0,       // 0 = indefinite
                  S32     EOS            = 10,
                  S32     max_line_chars = 512)
      {
      if (!active)
         {
         return FALSE;
         }

      S64 timeout_usec = (S64) timeout_msec * 1000LL;

      if (command_string != NULL)
         {
         message_printf(IP_NOTICE, "Transmitting \"%s\" . . .\n", command_string);
         send_printf("%s\r\n", command_string);
         }
   
      C8 *incoming_line = (C8 *) alloca(max_line_chars);

      if (incoming_line == NULL)
         {
         message_printf(IP_ERROR, "No room for %d-character reply buffer", max_line_chars);
         return FALSE;
         }

      S32 len = 0;
      S32 lines_received = 0;
      S32 bytes_received = 0;
   
      S64 start_time = timer.us();
      S64 last_line_time = start_time;
   
      bool done = FALSE;
   
      while (!done)
         {
         if ((timeout_usec > 0) && ((timer.us() - last_line_time) >= timeout_usec))
            {
            break;
            }
   
         if (!active)
            {
            return FALSE;
            }
   
         S32 avail = receive_poll();
   
         if (avail == 0)
            {
            Sleep(10);
            continue;
            }

         if (dropout_msec > 0)
            {
            timeout_usec = (S64) dropout_msec * 1000LL;
            }
         
         bytes_received += avail;

         for (S32 i=0; i < avail; i++)
            {
            if ((timeout_usec > 0) && ((timer.us() - last_line_time) >= timeout_usec))
               {
               done = 1;
               break;
               }

            C8 ch = 0;
            assert(read_block(&ch, 1));
   
            incoming_line[len++] = ch;
   
            if ((ch == EOS) || (len == max_line_chars-1))
               {
               incoming_line[len] = 0;
               fprintf(outfile,"%s",incoming_line);
               message_printf(IP_VERBOSE,"%s", incoming_line);
               len = 0;
   
               last_line_time = timer.us();
   
               if (!monitor(bytes_received, -1))
                  {
                  disconnect();
                  return FALSE;
                  }

               if ((++lines_received >= repeat_cnt) && (repeat_cnt != 0))
                  {
                  done = 1;
                  break;
                  }
               }
            }
         }
   
      if (lines_received > 0)
         {
         DOUBLE Hz = ((DOUBLE) lines_received * 1E6) / ((DOUBLE) (timer.us() - start_time));
         message_printf(IP_NOTICE,"%d line(s) received (average rate = %.1lf lines/sec)",lines_received, Hz);
         }

      return TRUE;
      }

};

void ___________________________________________________________IPSERVER___________________________________________(void) 
{}

//
// In response to incoming connection activity, IPSERVER::update() creates and deletes 
// objects of class T, where T should be either IPCONN or a class that inherits
// from it.  The application has free access to the IPSERVER's array of 
// connection objects (clients[]), but it must not alter the contents
// of this array or try to retain any indexes, pointers, or other information
// related to the client array between calls to IPSERVER::update()!
//

enum IPRDNSSTATE
{
   IPRDNS_READY,              // RDNS thread has not yet begun running; thread handle is not valid
   IPRDNS_IN_USE,             // RDNS thread is in progress, waiting on getnameinfo()
   IPRDNS_EXIT_OK,            // RDNS thread has exited, and FQDN field contains a valid domain name
   IPRDNS_EXIT_ERR            // RDNS thread has exited, but FQDN field contains error information
};

struct IPRDNSSTRUC
{
   sockaddr_in address;          // Copy of remote address structure for the connection
   HANDLE      hThread;          // Worker thread assigned to execute getnameinfo()
   C8          FQDN[NI_MAXHOST]; // Fully-qualified domain name written by getnameinfo() thread
   IPRDNSSTATE thread_state;     // Status of request thread
   S32         latency_mS;       // Time used by getnameinfo()
   void       *orig_client;      // Original client that initiated this request
};                               

template <class T, S32 max_clients> class IPSERVER
{
   //
   // Private data and methods 
   //

   SOCKET      monitor_socket;    
   S32         active;
   DWORD       last_error_code;
   S32         user_int; 
   void       *user_void;
   BOOL        ws_started;
   BOOL        initialized;
   S64         keepalive_time_us;
   IPRDNSSTRUC RDNS [max_clients];       // RDNS request structure for each client slot

   virtual BOOL init(void)
      {
      monitor_socket    = INVALID_SOCKET;
      active            = FALSE;
      last_error_code   = 0;
      ws_started        = FALSE;
      initialized       = FALSE;
      keepalive_time_us = 0;
      memset(hostname, 0, sizeof(hostname));
      memset(ipnum,    0, sizeof(ipnum));

      port = 0;
      memset(clients,  0, sizeof(clients));

      for (S32 i=0; i < max_clients; i++)
         {
         RDNS[i].thread_state = IPRDNS_READY;
         RDNS[i].hThread      = 0;
         }

      //
      // Init Winsock
      //

      WORD wRequestVer = MAKEWORD(2,2);

      memset(&wsadata, 0, sizeof(wsadata));
      if (WSAStartup(wRequestVer, &wsadata))
         {
         return FALSE;
         }

      ws_started = TRUE;

      //
      // Get host's IP address with gethostname(), followed by gethostbyname()
      // if necessary
      //

      if (gethostname(hostname, sizeof(hostname)-1) == SOCKET_ERROR)
         {
         strcpy(hostname,"(IP unknown)");
         strcpy(ipnum, hostname);
         }
      else
         {
         in_addr server_addr;
         memset(&server_addr,0,sizeof(server_addr));

         server_addr.S_un.S_addr = inet_addr(hostname);

         if (server_addr.S_un.S_addr == INADDR_NONE)
            {
            //
            // Currently we just use the first entry from the address list, 
            // which isn't ideal...
            //

            LPHOSTENT lphp = gethostbyname(hostname);
            server_addr = *(struct in_addr *) (lphp->h_addr_list[0]);
            }

         strcpy(ipnum,inet_ntoa(server_addr));
         }

      initialized = TRUE;
      return TRUE;
      }

   //
   // Find an available client slot, booting an existing client if necessary to
   // make room
   //
   // Returns -1 if the array is full of protected clients
   //

   virtual S32 obtain_slot(sockaddr_in *request_addr)
      {
      // 
      // First try to find an empty slot
      //
   
      S32 slot;
   
      for (slot=0; slot < max_clients; slot++)
         {
         if (clients[slot] == NULL)
            {
            break;
            }
         }
   
      if (slot < max_clients)
         {
         return slot;
         }
   
      //
      // If all slots are occupied, find the oldest connection and delete it
      // Try to boot remote clients before local ones
      //
   
      S64 oldest_local_client_time  =  0;
      S32 oldest_local_client       = -1;
      S64 oldest_remote_client_time =  0;
      S32 oldest_remote_client      = -1;
   
      for (slot=0; slot < max_clients; slot++)
         {
         T *C = clients[slot];
   
         if (C->is_protected())
            {
            continue;
            }

         S64 age = C->statistics();

         if (C->is_local())
            {
            if (age > oldest_local_client_time)
               {
               oldest_local_client_time = age;
               oldest_local_client      = slot;
               }
            }
         else
            {
            if (age > oldest_remote_client_time)
               {
               oldest_remote_client_time = age;
               oldest_remote_client      = slot;
               }
            }
         }
   
      slot = oldest_remote_client;
   
      if (slot == -1)
         {
         slot = oldest_local_client;
         }

      if (slot == -1)
         {
         return -1;        // all existing connections are protected 
         }

      clients[slot]->on_server_discard(IPDR_REPLACED);

      message_printf(IP_NOTICE, "Slot %d: Booted to make room for new connection", slot);

      delete clients[slot];
      clients[slot] = NULL;

      return slot;
      }

   //
   // Thread procedure to execute reverse-DNS request on each incoming
   // connection
   //

   static UINT WINAPI RDNS_thread_proc(LPVOID pParam)
      {
//    SetThreadName("RDNS thread");
      IPRDNSSTRUC *R = (IPRDNSSTRUC *) pParam;

      S32 start_time = timeGetTime();
                                            
      S32 result = getnameinfo((sockaddr *) &R->address,
                                sizeof(R->address),
                                R->FQDN,
                                sizeof(R->FQDN),
                                NULL,
                                0,
                                0);

      R->latency_mS = timeGetTime() - start_time;

      if (result == 0)
         {
         R->thread_state = IPRDNS_EXIT_OK;
         }
      else
         {
         _snprintf(R->FQDN,sizeof(R->FQDN)-1,"unresolvable, error code %d", WSAGetLastError());
         R->thread_state = IPRDNS_EXIT_ERR;
         }

      return TRUE;
      }

   virtual void RDNS_close_request(IPRDNSSTRUC *R)
      {
      WaitForSingleObject(R->hThread, INFINITE);     
      CloseHandle(R->hThread);
      R->hThread = 0;
      R->thread_state = IPRDNS_READY;
      }

public:
   //
   // Public data members
   //

   T *clients  [max_clients];                // Client connections, accessible to app between calls to IPSERVER::update()

   C8  hostname[512];                        // Server hostname
   C8  ipnum   [512];                        // Dotted-quad copy of our IP address
   S32 port;                                 // Server listens on this port

   WSADATA wsadata;                          // Winsock attributes

   //
   // Construction
   //

   IPSERVER()
      {
      user_int  = 0;
      user_void = NULL;

      init();
      }                 

   virtual ~IPSERVER()
      {
      shutdown();
      }

   //
   // Enable server operation
   //

   virtual void startup(S32 listen_port)
      {
      C8 msg[256];

      if (!initialized)
         {
         if (!init())
            {
            return;
            }
         }

      if (active)
         {
         return;
         }

      port = listen_port;

      message_printf(IP_VERBOSE, "Using Windows Sockets V%d.%d (%s)",
         HIBYTE(wsadata.wVersion),
         LOBYTE(wsadata.wVersion),
         wsadata.szDescription);

      message_printf(IP_NOTICE, "Initializing host %s, address %s:%d",
         hostname,
         ipnum,
         port);

      //
      // Create monitor socket and mark it non-blocking
      //

      monitor_socket = socket(PF_INET, SOCK_STREAM, 0);

      if (monitor_socket == INVALID_SOCKET)
         {
         message_printf(IP_ERROR,"Bad monitor socket");
         return;
         }

      DWORD dwVal = 1;

      ioctlsocket(monitor_socket,
                  FIONBIO,
                 &dwVal);

      dwVal = !IPCONN_NAGLE;

      setsockopt(monitor_socket,
                 IPPROTO_TCP,
                 TCP_NODELAY,
         (C8 *) &dwVal,
                 sizeof(dwVal));

      //
      // Assign socket's IP address and port
      //
      // INADDR_ANY allows us to work with any interface that receives
      // traffic destined for our port.  The default binding address for 
      // transmission is the 0th interface as discovered above
      //

      sockaddr_in monitor_addr;

      memset(&monitor_addr,0,sizeof(monitor_addr));

      monitor_addr.sin_family      = AF_INET;
      monitor_addr.sin_addr.s_addr = htonl(INADDR_ANY);
      monitor_addr.sin_port        = htons((S16) port);

      S32 result = bind(monitor_socket,
          (sockaddr *) &monitor_addr,
                        sizeof(monitor_addr));

      if (result)
         {
         message_printf(IP_ERROR, "bind() failed: %s", error_text(msg,sizeof(msg))); 
         return;
         }

      //
      // Set up to listen for connection requests from clients
      //

      memset(clients, 0, sizeof(clients));

      result = listen(monitor_socket,
                      5);              // connection backlog = 5 (max allowed for non-server versions of Windows)

      if (result)
         {
         message_printf(IP_ERROR, "listen() failed: %s", error_text(msg,sizeof(msg))); 
         return;
         }

      active = TRUE;
      }

   //
   // Shut down
   //
   // Multiple startup/shutdown cycles are permitted (unlike IPCONNs, which
   // can be connected only once during their lifetimes)
   //

   virtual void shutdown(void)
      {
      for (S32 slot=0; slot < max_clients; slot++)
         {
         //
         // Ensure all RDNS threads have exited cleanly
         //

         IPRDNSSTRUC *R = &RDNS[slot];

         if (R->hThread != 0)
            {
            message_printf(IP_NOTICE, "Slot %d: Waiting for RDNS thread to finish . . .", slot);
            RDNS_close_request(R);
            }

         //
         // Boot each client
         //

         T *C = clients[slot];

         if (C != NULL)
            {
            C->on_server_discard(IPDR_DISCONNECTED);

            message_printf(IP_NOTICE, "Slot %d: Released at server shutdown", slot);

            delete C;
            clients[slot] = NULL;
            }
         }

      if (monitor_socket != INVALID_SOCKET)
         {
         closesocket(monitor_socket);
         monitor_socket = INVALID_SOCKET;
         }

      if (ws_started)
         {
         WSACleanup();
         ws_started = FALSE;
         }

      active = FALSE;
      initialized = FALSE;
      }

   //
   // Give the app a chance to reject connection attempts, do
   // handshaking, etc.
   //

   virtual BOOL on_request(SOCKET requestor, sockaddr_in *from_addr)
      {
      return TRUE;
      }

   //
   // Copy OS-specific error text to buffer and return it
   //

   virtual C8 *error_text(C8 *text, S32 text_array_size, DWORD last_error = 0)
      {
      LPVOID lpMsgBuf = "Hosed";       // Default message for Win9x (FormatMessage() handles Winsock messages on NT only)

      last_error_code = (last_error == 0) ? GetLastError() : last_error;

      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                    NULL,
                    last_error_code,
                    0,
          (LPTSTR) &lpMsgBuf, 
                    0, 
                    NULL);

      memset(text, 0, text_array_size);
      strncpy(text, (C8 *) lpMsgBuf, text_array_size-1);

      if (strcmp((C8 *) lpMsgBuf,"Hosed"))
         {
         LocalFree(lpMsgBuf);
         }

      return text;
      }

   //
   // Set keepalive timeout property
   // 0 = keepalive checks disabled
   //

   virtual S32 set_keepalive_ms(S32 ms)
      {
      S64 prev = keepalive_time_us;
      keepalive_time_us = (S64) ms * 1000;

      return (S32) (prev / 1000);
      }

   //
   // Error/status message sink
   //

   virtual void message_sink(IPMSGLVL level,   
                             C8      *text)
      {
      ::printf("%s\n",text);
      }

   virtual void message_printf(IPMSGLVL level,
                               C8 *fmt,
                               ...)
      {
      C8 buffer[4096];

      va_list ap;

      va_start(ap, 
               fmt);

      memset(buffer, 0, sizeof(buffer));

      _vsnprintf(buffer,
                 sizeof(buffer)-1,
                 fmt,
                 ap);

      va_end(ap);

      //
      // Remove trailing whitespace
      //

      C8 *end = &buffer[strlen(buffer)-1];

      while (end > buffer)
         {
         if (!isspace((C8) *end)) 
            {
            break;
            }

         *end = 0;
         end--;
         }

      message_sink(level,
                   buffer);
      }

   //
   // Get/set user values
   //
   // User values survive shutdown/startup calls
   // New connections inherit the server's user values
   //

   virtual S32 user_word(void)
      {
      return user_int;
      }

   virtual S32 set_user_word(S32 val)
      {
      S32 prev = user_int;
      user_int = val;
      return prev;
      }

   virtual void *user_ptr(void)
      {
      return user_void;
      }

   virtual void *set_user_ptr(void *val)
      {
      void *prev = user_void;
      user_void = val;
      return prev;
      }

   //
   // Return server status based on whether or not the constructor
   // ran successfully, and whether or not it's actively listening for 
   // connections
   // 

   virtual IPSRVRSTATUS status(void)
      {
      if (active)      return IPSS_LISTENING;
      if (initialized) return IPSS_INITIALIZED;

      return IPSS_FAILURE;    
      }

   //
   // Return last error code reported by OS
   //

   virtual S32 last_error(void)
      {
      return last_error_code;
      }

   //
   // Clean up inactive connections and poll for new ones
   //
   // Optionally swallow any incoming data on managed connections, e.g. if 
   // clients send nothing but keepalive traffic
   // 
   // Returns # of newly-accepted connections, or -1 if none
   // 
   // WARNING: This function alters the contents of the clients[] array -- don't retain any
   // state that will be corrupted if entries are created or deleted!
   //

   virtual S32 update(BOOL consume_received_data = FALSE)
      {
      C8 msg[256];

      if (!active)
         {
         return -1;
         }

      S32 slot;

      for (slot=0; slot < max_clients; slot++)
         {
         //
         // Consume keepalive traffic
         //

         T *C = clients[slot];

         if ((C != NULL) && consume_received_data)
            {
            C->read_block(NULL, C->receive_poll());
            }

         //
         // Check for completed RDNS request
         //

         IPRDNSSTRUC *R = &RDNS[slot];

         if ((R->thread_state != IPRDNS_EXIT_OK) && (R->thread_state != IPRDNS_EXIT_ERR))
            {
            continue;
            }

         if (C == R->orig_client)
            {  
            message_printf(IP_NOTICE, "Slot %d: Client is %s (%d ms)",
               slot, R->FQDN, R->latency_mS);

            C->update_remote_hostname(R->FQDN);
            }
         else
            {
            message_printf(IP_NOTICE, "Slot %d: Previous client was %s (%d ms)",
               slot, R->FQDN, R->latency_mS);
            }

         RDNS_close_request(R);
         }

      //
      // Clean up any unused or timed-out connections
      //

      T *C;
   
      for (slot=0; slot < max_clients; slot++)
         {
         C8 msgbuf[64] = { 0 };

         C = clients[slot];
         if (C == NULL) continue;

         S32 xmit_rate = 0, recv_rate = 0;
         C->statistics(NULL, NULL, &xmit_rate, &recv_rate);

         if (!C->status())
            {
            C->on_server_discard(IPDR_DISCONNECTED);

            message_printf(IP_NOTICE, "Slot %d: Disconnected (%s, R=%d bps, X=%d bps)", 
               slot, C->age_string(msgbuf, sizeof(msgbuf)), recv_rate, xmit_rate);

            delete C;
            clients[slot] = NULL;
            }
         else
            {
            if (keepalive_time_us > 0)
               {
               S64 t = C->us_since_recv();

               if (t > keepalive_time_us)
                  {
                  C->on_server_discard(IPDR_TIMED_OUT);

                  message_printf(IP_NOTICE, "Slot %d: Timed out (%d ms, limit=%d ms) (%s, R=%d,X=%d bps)", 
                     slot, (S32) (t / 1000), (S32) (keepalive_time_us / 1000), 
                     C->age_string(msgbuf, sizeof(msgbuf)), recv_rate, xmit_rate);

                  delete C;
                  clients[slot] = NULL;
                  }
               }
            }
         }
   
      //
      // Poll for new connection requests
      //

      S32 new_connections;

      for (new_connections=0; new_connections < max_clients; new_connections++)
         {
         SOCKET      requestor;
         sockaddr_in request_addr;
         S32         request_addr_size;

         memset(&request_addr, 0, sizeof(request_addr));
         request_addr_size = sizeof(request_addr);

         requestor = accept(monitor_socket,
              (sockaddr *) &request_addr,
                           &request_addr_size);

         if (requestor == INVALID_SOCKET)
            {
            S32 WSA_error = WSAGetLastError();

            if (WSA_error != WSAEWOULDBLOCK)
               {
               message_printf(IP_ERROR, "accept() failed: %s", error_text(msg,sizeof(msg), WSA_error)); 
               active = FALSE;
               }

            break;
            }

         //
         // Connection request received -- find an available client slot, booting
         // an existing client if necessary to make room
         //

         if (!on_request(requestor, &request_addr))
            {
            closesocket(requestor);
            continue;
            }

         S32 slot = obtain_slot(&request_addr);

         if (slot == -1)
            {
            break;
            }

         //
         // Accept the new connection
         // 

         T *C = new T;

         C->set_user_word(user_word());
         C->set_user_ptr(user_ptr());

         C->server_accept(&request_addr, requestor, IPCONN_DEFAULT_RECV_BUF_BYTES);

         if (!C->status())
            {
            delete C;
            continue;
            }

         //
         // Log it
         //

         clients[slot] = C;

         C8 *hostname = NULL;
         C->host_info(NULL, &hostname, NULL);

         if (C->is_local())
            {
            message_printf(IP_NOTICE, "Slot %d: Local client %s accepted",
                           slot,
                           hostname);
            }
         else
            {
            message_printf(IP_NOTICE, "Slot %d: Remote client %s accepted",
                           slot,
                           hostname);
            }

         //
         // Start thread to perform asynchronous reverse-DNS lookup, so we can
         // log the client's fully-qualified domain name
         //
         // (This functionality is cosmetic; if the slot's RDNS worker thread is 
         // already running, leave it alone)
         // 

         IPRDNSSTRUC *R = &RDNS[slot];

         if (R->thread_state == IPRDNS_READY)
            {
            R->address      = request_addr;
            R->FQDN[0]      = 0;
            R->thread_state = IPRDNS_IN_USE;
            R->orig_client  = C;

            unsigned int stupId;
            R->hThread = (HANDLE) _beginthreadex(NULL,                   
                                                 0,                              
                                                 RDNS_thread_proc,
                                                 R,
                                                 0,                              
                                                &stupId);
            }
         }

      return new_connections;
      }
};


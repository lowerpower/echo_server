/*!						
-----------------------------------------------------------------------------
echo.c

Simple single threaded echo server


-----------------------------------------------------------------------------
*/
#include "echo_server.h"
#include "config.h"
#include "arch.h"
#include "yselect.h"
#include "daemonize.h"
//#include "file_config.h"
#include "debug.h"


#if defined(WIN32)
#include "wingetopt.h"
#endif

#define BUFFER_SIZE 1024

#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

int global_flag=0;
int go=0;


//
// network_init()-
// If anything needs to be initialized before using the network, put it here, mostly this
//  is for windows.
//
int network_init(void)
{
#if defined(WIN32) || defined(WINCE)

    WSADATA w;                              /* Used to open Windows connection */
    /* Open windows connection */
    if (WSAStartup(0x0101, &w) != 0)
    {
        fprintf(stderr, "Could not open Windows connection.\n");
        printf("**** Could not initialize Winsock.\n");
        exit(0);
    }

#endif
return(0);
}


int
init_server(ECHO *echo)
{
    int ret=-1;
    struct sockaddr_in  client;             /* Information about the client */

    /* gety a tcp socket */
    echo->listen_soc = socket(AF_INET, SOCK_STREAM, 0);
    // 
    if (echo->listen_soc == INVALID_SOCKET)
    {
        DEBUG1("Could not create socket.\n");
        echo->listen_soc=0;
        return(-1);
    }
    memset((void *)&client, '\0', sizeof(struct sockaddr_in));
    client.sin_family       = AF_INET;                                  // host byte order
    client.sin_port         = htons(echo->listen_port);                 // listen port
    client.sin_addr.s_addr  = echo->Bind_IP.ip32;                       // client.sin_addr.s_addr
    ret=bind(echo->listen_soc, (struct sockaddr *)&client, sizeof(struct sockaddr_in));
    if(ret==-1)
    {
        DEBUG1("fail to bind\n");
        closesocket(echo->listen_soc);
        echo->listen_soc=0;
        return(-1);
    }
    //
    // If your needing very high load, increase the proxy_listen_backlog value, this will 
    // let more connections stack up and may help with a loaded server.  The default
    // value of 10 should be more than enough for most servers.
    //
    ret=listen(echo->listen_soc,ECHO_LISTEN_BACKLOG);
    if(-1==ret)
    {
        closesocket(echo->listen_soc);
        echo->listen_soc=0;
        return(-1);
    }
    // Proxy Listener started, lets create the client info

    DEBUG1("listener started\n");

    // Add the listen socket to the select map
    Y_Set_Select_rx(echo->listen_soc);

    // Non Block
    set_sock_nonblock(echo->listen_soc);

    return(1);
}


#if defined(WIN32)
BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
    switch(CEvent)
    {
    case CTRL_C_EVENT:
        yprintf("CTRL+C received!\n");
        break;
    case CTRL_BREAK_EVENT:
        yprintf("CTRL+BREAK received!\n");
        break;
    case CTRL_CLOSE_EVENT:
        yprintf("program is being closed received!\n");
        break;
    case CTRL_SHUTDOWN_EVENT:
        yprintf("machine is being shutdown!\n");
        break;
    case CTRL_LOGOFF_EVENT:
        return FALSE;
    }
    go=0;

    return TRUE;
}
#else
void
termination_handler (int signum)
{

    go=0;

    if((SIGFPE==signum) || (SIGSEGV==signum) || (11==signum))
    {
        yprintf("Echo Server Terminated from Signal %d\n",signum);
        if(global_flag&GF_DAEMON) syslog(LOG_ERR,"Echo Server Terminated from Signal 11\n");

        exit(11);
    }
}
#endif

void
startup_banner()
{
    //------------------------------------------------------------------
    // Print Banner
    //------------------------------------------------------------------
    printf("Echo Server Server built " __DATE__ " at " __TIME__ "\n");
    printf("   Version " VERSION " - (c)2018 Remot3.it\n");
    fflush(stdout);
}


void usage(int argc, char **argv)
{
  startup_banner();

  printf("usage: %s [-h] [-v(erbose)] [-d][pid file] [-f config_file] [-l listen_tcp_port] [-u user_2_run_as] [-c control_port] \n",argv[0]);
  printf("\t -h this output.\n");
  printf("\t -v console debug output.\n");
  printf("\t -d runs the program as a daemon with optional pid file.\n");
  //printf("\t -f specify a config file.\n");
  printf("\t -l Listen port (defaults to 53)\n");
  printf("\t -c control port (defaults to 1032)\n");
  exit(2);
}



int main() {
  struct sockaddr_in bind_addr;
  struct sockaddr peer_addr;
  int optval = 1;
  int tcp_socket,client_fd;
  int err;
  unsigned int addr_len = sizeof(struct sockaddr);
  char buf[BUFFER_SIZE];

 
  //
  // Banner
  startup_banner();

  // Startup Network
  network_init();
  Y_Init_Select();

  //------------------------------------------------------------------
  // Initialize error handling and signals
  //------------------------------------------------------------------
#if defined(WIN32) 
if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler,TRUE)==FALSE)
{
  // unable to install handler... 
  // display message to the user
  yprintf("Error - Unable to install control handler!\n");
  exit(0);
}
#else
#if !defined(WINCE)
  //  SetConsoleCtrlHandle(termination_handler,TRUE);

  if (signal (SIGINT, termination_handler) == SIG_IGN)
      signal (SIGINT, SIG_IGN);
  if (signal (SIGTERM, termination_handler) == SIG_IGN)
      signal (SIGTERM, SIG_IGN);
  if (signal (SIGILL , termination_handler) == SIG_IGN)
      signal (SIGILL , SIG_IGN);
  if (signal (SIGFPE , termination_handler) == SIG_IGN)
      signal (SIGFPE , SIG_IGN);
  //if (signal (SIGSEGV , termination_handler) == SIG_IGN)
  //  signal (SIGSEGV , SIG_IGN);
#if defined(LINUX) || defined(MACOSX) || defined(IOS)
  if (signal (SIGXCPU , termination_handler) == SIG_IGN)
      signal (SIGXCPU , SIG_IGN);
  if (signal (SIGXFSZ , termination_handler) == SIG_IGN)
      signal (SIGXFSZ , SIG_IGN);
#endif
#endif
#endif

    //
    // Override with command line, after config file is loaded
    //
    while ((c = getopt(argc, argv, "p:c:u:t:l:a:d:vh")) != EOF)
    {
    	switch (c)
        {
            case 0:
                break;
            case 'd':
                // Startup as daemon with pid file
                strncpy(echo.pidfile,optarg,MAX_PATH-1);
                printf("Starting up as daemon with pidfile %s\n",echo.pidfile);
                global_flag|=GF_DAEMON;
                break;
            case 'v':
                echo.verbose=1;
                break;
            case 'h':
                usage (argc,argv);
                break;
            default:
                usage (argc,argv);
                break;
   		}
	}

    // 
    // Socket must be bound before Daemonize because we may drop privilages here
    //






#if !defined(WIN32)
    //
    // Should Daemonize here,  
    //
    if(global_flag&GF_DAEMON)
    {
        // Daemonize this
        daemonize(echo.pidfile,0,0,0,0,0,0);
        // Setup logging
        openlog("chat_server",LOG_PID|LOG_CONS,LOG_USER);
        syslog(LOG_INFO,Echo Server built "__DATE__ " at " __TIME__ "\n");
        syslog(LOG_INFO,"   Version " VERSION " - (c)2018 Remot3.it\n");
        syslog(LOG_INFO,"Starting up as daemon\n");
    }
#endif





  memset(&bind_addr, 0, sizeof(struct sockaddr_in));
  memset(&peer_addr, 0, sizeof(struct sockaddr));
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons(53);

  if (inet_pton(AF_INET, "0.0.0.0", &(bind_addr.sin_addr)) != 1) {
    perror("inet_pton");
    exit(1);
  }

  tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  err = bind(tcp_socket, (const struct sockaddr *)&bind_addr, sizeof(struct sockaddr));

  if (err != 0) {
    perror("bind failed");
    exit(1);
  }

  err = listen(tcp_socket, 256);
  if (err != 0) {
    perror("listen failed");
    exit(1);
  }

    printf("echo started up on port %d\n",htons(bind_addr.sin_port)); 

  // This code only accepts one socket at a time
  while (1) {
	memset((void *)&peer_addr, '\0', sizeof(struct sockaddr_in));
  	client_fd=accept(tcp_socket, &peer_addr, &addr_len);
	
	if(client_fd<0) 
	{
		continue;
	}

	while(1)
	{
      int read = recv(client_fd, buf, BUFFER_SIZE, 0);

      if (!read) break; // done reading
      if (read < 0) on_error("Client read failed\n");

      err = send(client_fd, buf, read, 0);
      if (err < 0) on_error("Client write failed\n");		
	}
  }


}



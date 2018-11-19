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
    client.sin_addr.s_addr  = echo->bind_IP.ip32;                       // client.sin_addr.s_addr
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

// close and cleanup a socket
int
cleanup_socket(ECHO *echo, ECHO_CONNECTION *ec)
{
	int ret = 0;
	int extracted = 0;
	ECHO_CONNECTION *tec;

	tec = echo->connections;
	// first extract the connection from the list
	if ((tec) && (ec))
	{
		if (tec == ec)
		{
			// easy, at the head of the list
			echo->connections = ec->next;
			extracted = 1;
		}
		else
		{
			// lets search through the list to remove the connection
			while (tec->next)
			{
				if (tec->next == ec)
				{
					// found, remove
					tec->next = ec->next;
					extracted = 1;
					break;
				}
				tec = tec->next;
			}
		}
		if (extracted)
		{
			// clean up
			Y_Del_Select_rx(ec->soc);
			closesocket(ec->soc);
			free(ec);
			ret = 1;
			echo->connection_count--;
			if (echo->connection_count < 0)
				printf("Error: connection count <0\n");
		}
		else
		{
			printf("cleanup_socket: nothing to remove, do nothing.\n");
		}
	}
	else
	{
		printf("cleanup_socket: input is invalid, do nothing.\n");
	}
	return(ret);
}

int main(int argc, char **argv) {
  struct sockaddr_in bind_addr;
  struct sockaddr peer_addr;
  int c,optval = 1;
  int err;
  char buf[BUFFER_SIZE];
  ECHO echo;

 
  //
  // Banner
  startup_banner();
  //
  // Startup Network
  network_init();
  Y_Init_Select();												// were going to use the select engine
  //
  // Set defaults
  memset(&echo, 0, sizeof(ECHO));
  echo.listen_port	= ECHO_DEFALT_LISTEN_PORT;					// echo service defaults to port 7 (must be root) or make over 1024
  echo.bind_IP.ip32 = 0;


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
                if(0!=optarg)
                {
                    strncpy(echo.pidfile,optarg,MAX_PATH-1);
                    printf("Starting up as daemon with pidfile %s\n",echo.pidfile);
                }
                else
                {
                    printf("Starting up as daemon with no pidfile.\n");
                    echo.pidfile[0]=0;
                }
                global_flag|=GF_DAEMON;
            
                break;
            case 'p':
                // Override Port
                echo.listen_port=atoi(optarg);
            case 'v':
                echo.verbose++;
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
        syslog(LOG_INFO,"Echo Server built "__DATE__ " at " __TIME__ "\n");
        syslog(LOG_INFO,"   Version " VERSION " - (c)2018 mycal.net\n");
        syslog(LOG_INFO,"Starting up as daemon\n");
    }
#endif


  memset(&bind_addr, 0, sizeof(struct sockaddr_in));
  memset(&peer_addr, 0, sizeof(struct sockaddr));
  bind_addr.sin_family = AF_INET;
  //bind_addr.sin_port = htons(53);
  //
  // Set bind address
  //
  bind_addr.sin_port = htons(echo.listen_port);
  bind_addr.sin_addr.s_addr = echo.bind_IP.ip32;
  //
  // reuse the address and bind the socket to the echo port
  //
  echo.listen_soc = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(echo.listen_soc, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
  err = bind(echo.listen_soc, (const struct sockaddr *)&bind_addr, sizeof(struct sockaddr));

  if (err != 0) {
    perror("bind failed");
    exit(1);
  }
  //
  // Listen on the port for TCP sockets
  //
  err = listen(echo.listen_soc, ECHO_LISTEN_BACKLOG);
  if (err != 0) {
    perror("listen failed");
    exit(1);
  }

  // add the listen socket to select for rx
  Y_Set_Select_rx(echo.listen_soc);

  printf("echo started up on port %d\n",htons(bind_addr.sin_port)); 

  // This code only accepts one socket at a time
  while (1)
  {
	  int sc;
	  int process_count;
	  ECHO_CONNECTION *ec;

	  // wait for event or timeout (500ms)
	  sc = Y_Select(500);

	  process_count = 0;
	  // if sc > 0 we have some action on a socket
	  if (sc > 0)
	  {
		  // check the listen socket
		  while (Y_Is_Select(echo.listen_soc))
		  {
			  // should we accept this socket?
			  if (ECHO_MAX_CONNECTIONS <= echo.connection_count)
			  {
				  // we cannot accept, do nothing for now this will increase the backlog, threadswitch to not hammer CPU 
				  // may need  usleep(10) or not select on listen socket until the backlog clears.
				  threadswitch();
				  if (echo.verbose) printf("accept: Over maximum concurrent connections of %d\n", ECHO_MAX_CONNECTIONS);
				  // exit listen_soc select
				  break;
			  }
			  // Try to accept here, create a new echo connection structure
			  ec = (ECHO_CONNECTION *)malloc(sizeof(ECHO_CONNECTION));
			  // of we have a structure,lets accept the socket
			  if (ec)
			  {
				  struct sockaddr_in  peer;
				  socklen_t 		  peer_len = sizeof(struct sockaddr);

				  // we are going to use ec lets clear it first
				  memset(ec, 0, sizeof(ECHO_CONNECTION));
				  // accept the socket
				  ec->soc = accept(echo.listen_soc, (struct sockaddr *)&peer, &peer_len);
				  // check if accept was sucessful
				  if (ec->soc >= 0)
				  {
					  echo.connection_count++;
					  if (echo.verbose) printf("accept: socket %d, at %d concurrent connections\n",ec->soc, echo.connection_count);
					  // set the peer endpoint in case we want to list it
					  ec->peer_ip.ip32 = peer.sin_addr.s_addr;
					  ec->peer_port = htons(peer.sin_port);
					  //
					  if (echo.verbose) printf("accept: socket %d from %s:%d, at %d concurrent connections\n", ec->soc, inet_ntoa(peer.sin_addr),
															ec->peer_port, echo.connection_count);
					  // add it to select for read (maybe write later)
					  Y_Set_Select_rx(ec->soc);
					  // add it to the list
					  ec->next = echo.connections;
					  echo.connections = ec;
					  process_count++;
					  if (process_count >= sc)
						  break;
				  }
				  else
				  {
					  // no accept free
					  free(ec);
					  break;
				  }
			  }
			  else
			  {
				  // malloc failed
				  printf("malloc failed cannot accept a connection\n");
				  break;
			  }
			  // see if there are more to accept
		  }
		  //
		  // process the rest of the sockets
		  //
		  ec = echo.connections;
		  while (ec)
		  {
			  ECHO_CONNECTION *current_connection=ec;
			  
			  // setup the next connection, we will use current_connection for all operations below
			  ec = ec->next;
			  // may save small amount of CPU on large lists if we bail after processing a number of connections
			  // that select said we had to process
			  if (process_count >= sc)
				  break;
			  // check if select
			  if (Y_Is_Select(current_connection->soc))
			  {
				  // try to read
				  int read = recv(current_connection->soc, buf, BUFFER_SIZE, 0);
				  process_count++;
				  if (0 == read)
				  {
					  if (echo.verbose) printf("socket %d from %s:%d, closed buy peer.\n", current_connection->soc,inet_ntoa(*(struct in_addr *)&current_connection->peer_ip.ip32),
							current_connection->peer_port);
					  cleanup_socket(&echo, current_connection);
					  if (echo.verbose) printf("at concurrent socket count of %d\n", echo.connection_count);
				  }else
				  if (read < 0) {
					  if (echo.verbose) printf("socket %d from %s:%d, read error closed\n", current_connection->soc, inet_ntoa(*(struct in_addr *)&current_connection->peer_ip.ip32),
						  current_connection->peer_port);
					  cleanup_socket(&echo, current_connection);
					  if (echo.verbose) printf("at concurrent socket count of %d\n", echo.connection_count);
				  }
				  else
				  {
					  int err;
					  err = send(current_connection->soc, buf, read, 0);
					  if (err < 0) {
						  // we should really check for wouldblock and queue the packet and wait on select for writable
						  // but for now we just kill the socket
						  if (echo.verbose) printf("socket %d from %s:%d, write error closed\n", current_connection->soc, inet_ntoa(*(struct in_addr *)&current_connection->peer_ip.ip32),
							  current_connection->peer_port);
						  cleanup_socket(&echo, current_connection);
						  if (echo.verbose) printf("at concurrent socket count of %d\n", echo.connection_count);
					  }
				  }
			  }
		  }
	  }
	  else
	  {
		  // timeout, any timout processing here
	  }
	  // any background processing here
  }
}



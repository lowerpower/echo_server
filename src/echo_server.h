#ifndef __Y_ECHO_SERVER_H__
#define __Y_ECHO_SERVER_H__
//---------------------------------------------------------------------------
// echo_server.h - TCP Echo Server                                          -
//---------------------------------------------------------------------------
// Version                                                                  -
//      0.1 Original Version May 1, 2018                                    -
//                                                                          -
// (c)2018 remot3.it                                                        -
//---------------------------------------------------------------------------

#include "config.h"
#include "mytypes.h"


#define ECHO_LISTEN_BACKLOG       10
#define ECHO_DEFALT_LISTEN_PORT   7 



//
// GF flags, global flags
//
#define GF_GO           0x01                /* go */
#define GF_DAEMON       0x02                /* we are a daemon */
#define GF_QUIET        0x04                /* no output */


// Custom File config for each product here
typedef struct echo_config_
{
    IPADDR      Bind_IP;
    U16         listen_port;
    SOCKET      listen_soc;
    //
    int         verbose;
    int         log_level;
	int			echo_on;
	int			accept_on;
	int			connection_count;
    //

    // Stats
    //char        stats_file[MAX_PATH];
    //U32         stats_interval;
    //U32         stats_file_timestamp;
    // Stat Values
	long		connection_requests;
	long		connection_accepts;
    long        requests;
	long		replies;
    long        accept_err;
    long        tx_err;

    char        pidfile[MAX_PATH];
}ECHO;

#endif





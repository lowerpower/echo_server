/*!						
-----------------------------------------------------------------------------
echo.c

Simple single threaded echo server


-----------------------------------------------------------------------------
*/
#include "config.h"


#define BUFFER_SIZE 1024

#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

int main() {
  struct sockaddr_in bind_addr;
  struct sockaddr peer_addr;
  int optval = 1;
  int tcp_socket,client_fd;
  int err;
  unsigned int addr_len = sizeof(struct sockaddr);
  char buf[BUFFER_SIZE];

  
    

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



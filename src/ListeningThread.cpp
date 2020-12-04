//
// Created by Will Gulian on 11/25/20.
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "capnp/message.h"

#include "ListeningThread.h"

#define PORT 4000
#define BUFSIZE 2048

void ListeningThread::run() {
  printf("Starting listening thread...\n");

  int fd;
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("cannot create socket");
    return;
  }

  u_int yes = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
  {
    perror("Reusing ADDR failed");
    exit(1);
  }

  in_addr_t source_address = inet_addr("239.15.55.200");

  struct sockaddr_in myaddr;

  memset((char *) &myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = source_address;
  myaddr.sin_port = htons(PORT);

  if (bind(fd, (struct sockaddr *) &myaddr, sizeof(myaddr)) < 0) {
    perror("bind failed");
    return;
  }

  struct ip_mreq mreq;
  mreq.imr_multiaddr.s_addr = source_address;
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);

  if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
  {
    perror("setsockopt");
    exit(1);
  }

  struct sockaddr_in remaddr;     /* remote address */
  socklen_t addrlen = sizeof(remaddr);
  int recvlen;
  uint8_t buf[BUFSIZE];     /* receive buffer */

  while (!should_close) {
    printf("waiting on port %d\n", PORT);
    recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
    printf("received %d bytes\n", recvlen);

    trace_data->parse(buf, recvlen);

    if (recvlen > 0) {
      buf[recvlen] = 0;
      printf("received message: \"%s\"\n", buf);
    }
  }

}

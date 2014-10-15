/****************************************************************************
 * examples/hello/hello_main.c
 *
 *   Copyright (C) 2008, 2011-2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <debug.h>
#include <apps/netutils/netlib.h>


/****************************************************************************
 * Definitions
 ****************************************************************************/

#define PORTNO     5471

#define ASCIISIZE  (0x7f - 0x20)
#define SENDSIZE   (ASCIISIZE+1)

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/


static inline int check_buffer(unsigned char *buf)
{
  int ret = 1;
  int offset;
  int ch;
  int j;

  offset = buf[0];
  for (ch = 0x20, j = offset + 1; ch < 0x7f; ch++, j++)
    {
      if (j >= SENDSIZE)
        {
          j = 1;
        }
      if (buf[j] != ch)
        {
          printf("server: Buffer content error for offset=%d, index=%d\n", offset, j);
          ret = 0;
        }
    }
  return ret;
}

void recv_server(void)
{
  struct sockaddr_in server;
  struct sockaddr_in client;
  in_addr_t tmpaddr;
  unsigned char inbuf[1024];
  int sockfd;
  int nbytes;
  int optval;
  int offset;
  socklen_t addrlen;

  /* Create a new UDP socket */

  sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    {
      printf("server: socket failure\n");
      return;
    }

  /* Set socket to reuse address */

  optval = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&optval, sizeof(int)) < 0)
    {
      printf("server: setsockopt SO_REUSEADDR failure\n");
      return;
    }

  /* Bind the socket to a local address */

  server.sin_family      = AF_INET;
  server.sin_port        = HTONS(PORTNO);
  server.sin_addr.s_addr = HTONL(INADDR_ANY);

  if (bind(sockfd, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) < 0)
    {
      printf("server: bind failure\n");
      return;
    }

  /* Then receive up to 256 packets of data */

  for (offset = 0; offset < 256; offset++)
    {
      printf("server: %d. Receiving up 1024 bytes in port: %d\n", offset, PORTNO);
      addrlen = sizeof(struct sockaddr_in);
      nbytes = recvfrom(sockfd, inbuf, 1024, 0,
                        (struct sockaddr*)&client, &addrlen);

      tmpaddr = ntohl(client.sin_addr.s_addr);
      printf("server: %d. Received %d bytes from %d.%d.%d.%d:%d\n",
             offset, nbytes,
             tmpaddr >> 24, (tmpaddr >> 16) & 0xff,
             (tmpaddr >> 8) & 0xff, tmpaddr & 0xff,
             ntohs(client.sin_port));
      
      /* Print the content received  */
      printf("Content (hex):\n");
      int i;
      for (i = 0; i<nbytes; i++){
      	printf(" %2x", inbuf[i]);
      }
      printf("\n");
      printf("Content:\n");
      printf("%s\n", inbuf);

      if (nbytes < 0)
        {
          printf("server: %d. recv failed\n", offset);
          close(sockfd);
          return;
        }

      if (offset < inbuf[0])
        {
          printf("server: %d. %d packets lost, resetting offset\n", offset, inbuf[0] - offset);
          offset = inbuf[0];
        }
      else if (offset > inbuf[0])
        {
          printf("server: %d. Bad offset in buffer: %d\n", offset, inbuf[0]);
          close(sockfd);
          return;
        }

    /*    
      if (!check_buffer(inbuf))
        {
          printf("server: %d. Bad buffer contents\n", offset);
          close(sockfd);
          return;
        }
	*/

    }
  close(sockfd);
}

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int udpsimple_main(int argc, char *argv[])
#endif
{

  struct in_addr addr;

  /* Set up our host address */

  addr.s_addr = HTONL(CONFIG_UDP_IPADDR);
  netlib_sethostaddr("eth0", &addr);

  /* Set up the default router address */

  addr.s_addr = HTONL(CONFIG_UDP_DRIPADDR);
  netlib_setdraddr("eth0", &addr);

  /* Setup the subnet mask */

  addr.s_addr = HTONL(CONFIG_UDP_NETMASK);
  netlib_setnetmask("eth0", &addr);

  recv_server();

  return 0;
}

#include <stdio.h>
#include <sys/time.h>
#include <stddef.h>
#include <netinet/in.h>
#include "split_router.h"
#include "helper_methods.h"

// Did something happen within the last n seconds?
int recent(struct timeval t, int limit) {
  struct timeval timenow;
  gettimeofday(&timenow, NULL);
  timenow.tv_sec = timenow.tv_sec - limit;
  return(t.tv_sec > timenow.tv_sec | (t.tv_sec == timenow.tv_sec & t.tv_usec > timenow.tv_usec));
}

// Calculate an IP checksum
unsigned short in_cksum(unsigned short *addr, int len)
{
  int nleft = len;
  int sum = 0;
  unsigned short *w = addr;
  unsigned short answer = 0;

  while (nleft > 1) {
    sum += *w++;
    nleft -= 2;
  }

  if (nleft == 1) {
    *(unsigned char *) (&answer) = *(unsigned char *) w;
    sum += answer;
  }

  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);
  answer = ~sum;
  return (answer);
}

// Find the peer for a given route
int find_route(in_addr_t address) {
  int i, j;
  unsigned int route, mask, computed;
  for(i=0;i<remote_site_count;i++) {
    for(j=0;j<remote_sites[i].route_count;j++) {
      route = ntohl(remote_sites[i].routes[j].address);
      mask = (uint32_t)-1 << (32-remote_sites[i].routes[j].netmask);
      computed = ntohl(address) & mask;
      if(route == computed) {
        return i;
      }
    }
  }
  return -1;
}

// Identify an incoming host
int find_host(in_addr_t address) {
  int i, j;
  for(i=0;i<remote_site_count;i++) {
    for(j=0;j<remote_sites[i].host_count;j++) {
      if (address  == remote_sites[i].remote_hosts[j].remote_address) {
        return(i * 0x100 + j);
      }
    }
  }
  return -1;
}


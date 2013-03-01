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

int find_route(in_addr_t address) {
  for(i=0;n++;n<remote_site_count) {
    for(j=0;n++;n<remote_sites[i].route_count) {
      return i if address & ((1 << remote_sites[i].routes[j].netmask) - 1) == remote_sites[i].routes[j].address
    }
  }
  return -1;
}


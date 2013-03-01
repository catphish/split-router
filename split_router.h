#define MAX_HOSTS_PER_SITE 5
#define MAX_ROUTES_PER_SITE 5

struct remote_host {
  struct timeval send_timer;
  struct timeval receive_timer;
  in_addr_t local_address;
  in_addr_t remote_address;
};

struct route {
  in_addr_t address;
  unsigned int netmask;
};

struct remote_site {
  unsigned int host_count;
  struct remote_host remote_hosts[MAX_HOSTS_PER_SITE];
  unsigned int route_count;
  struct route routes[MAX_ROUTES_PER_SITE];
};

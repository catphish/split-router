// Did something happen within the last n seconds?
int recent(struct timeval t, int limit);
// Calculate an IP checksum
unsigned short in_cksum(unsigned short *addr, int len);

int find_route(in_addr_t address);
int find_host(in_addr_t address);


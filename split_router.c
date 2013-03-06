#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <linux/if.h>
#include <linux/if_tun.h>

#include <netinet/ip.h>
#include <netinet/in.h>

#include "split_router.h"
#include "helper_methods.h"

// Send out pings if any line is idle
void* ping(void* p) {
  // sin scruct for sending
	struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;

  // Some handy loop cunters
  unsigned int i, j;

  // Set up an empty packet
  char send_buffer[36];
  struct ip *ip = (struct ip*)send_buffer;
  ip->ip_hl = 0x5;
  ip->ip_v = 0x4;
  ip->ip_tos = 0x0;
  ip->ip_len = htons(36);
  ip->ip_id = 0x0;
  ip->ip_off = 0x0;
  ip->ip_ttl = 64;
  ip->ip_p = 253;
  ip->ip_sum = 0x0;
  while(1) {
    usleep(100000);
    for(i=0;i<remote_site_count;i++) {
      for(j=0;j<remote_sites[i].host_count;j++) {
        if(!recent(remote_sites[i].remote_hosts[j].send_timer, 1)) {
          gettimeofday(&(remote_sites[i].remote_hosts[j].send_timer), NULL);
          ip->ip_src.s_addr = remote_sites[i].remote_hosts[j].local_address;
          ip->ip_dst.s_addr = remote_sites[i].remote_hosts[j].remote_address;
          ip->ip_sum = in_cksum((unsigned short *)ip, sizeof(struct ip));
          sin.sin_addr.s_addr = ip->ip_dst.s_addr;
          if (sendto(raw_socket, send_buffer, 36, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr)) < 0)  {
            perror("sendto");
            return(NULL);
          }
        }
      }
    }
  }
}

int main() {
  remote_site_count = 0;
  local_address_count = 0;
  
  // Useful loop counters
  int i, j, local, local_count;
  
  // Read the config
  FILE *file = fopen ("split_router.conf", "r");
  if ( file != NULL ) {
    remote_site_count = 0;
    char line[128];
    char value[128];
    while (fgets (line, sizeof line, file) != NULL) {
      if(sscanf(line, "[%[^]]]", value)) {
        if(!strcmp(value, "local")) {
          local = 1;
        } else {
          printf("%s\n", value);
          local = 0;
          remote_site_count++;
          remote_sites[remote_site_count-1].host_count = 0;
          remote_sites[remote_site_count-1].route_count = 0;
          strncpy(remote_sites[remote_site_count-1].name, line, 31);
          remote_sites[remote_site_count-1].name[31] = 0;
        }
      } else if (sscanf(line, "address %[0-9.]", value)) {
        if(local) {
          local_addresses[local_address_count] = inet_addr(value);
          local_address_count++;
        } else {
          remote_sites[remote_site_count-1].host_count++;
          remote_sites[remote_site_count-1].remote_hosts[remote_sites[remote_site_count-1].host_count-1].remote_address = inet_addr(value);
          remote_sites[remote_site_count-1].remote_hosts[remote_sites[remote_site_count-1].host_count-1].local_address = local_addresses[remote_sites[remote_site_count-1].host_count-1];
          printf("Address: %s\n", value);
        }
      } else if (sscanf(line, "route %[0-9.]/%i", value, &j)) {
        if(!local) {
          remote_sites[remote_site_count-1].route_count++;
          remote_sites[remote_site_count-1].routes[remote_sites[remote_site_count-1].route_count-1].address = inet_addr(value);
          remote_sites[remote_site_count-1].routes[remote_sites[remote_site_count-1].route_count-1].netmask = j;
          printf("Route: %s\n", value);
        }
      }
    }
    fclose (file);
  } else {
    perror ("split_router.conf");
  }

  // Set up the ping thread
  pthread_t ping_thread;
  pthread_create(&ping_thread, NULL, ping, NULL);

  // Receive and transmit buffers
  char receive_buffer[1500];
  char send_buffer[1500];

  // An IP pointer we can use to populate or modify packets
  struct ip *ip;

  // Set up the raw IP socket
  static int on = 1;
  if ((raw_socket = socket(AF_INET, SOCK_RAW, 253)) < 0) {
    perror("raw socket");
    return(1);
  }
  if (setsockopt(raw_socket, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
    perror("setsockopt");
    return(1);
  }

  // Set up the TUN interface
  int tun_fd;
  if( (tun_fd = open("/dev/net/tun", O_RDWR)) < 0 ) {
    printf("Error\n");
    return(1);
  }
  struct ifreq ifr; memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN|IFF_NO_PI;
  if( (ioctl(tun_fd, TUNSETIFF, (void *) &ifr)) < 0 ){
    printf("Error\n");
    return(1);
  }

  // Create scructure to poll both interfaces
  struct pollfd fds[2];
  fds[0].fd = tun_fd;
  fds[0].events = POLLIN;
  fds[1].fd = raw_socket;
  fds[1].events = POLLIN;

  // Some variables used in the main loop
  int received_packet_size;
  int header_size;
  int full_data_size;
  int chunk_size;
  int offset;
  uint16_t id = 0;
  int conn_up[MAX_ROUTES_PER_SITE];
  int up_count;
  int remote_site_id;
  
	struct sockaddr_in sin;
  int len;

  // Main loop, wait for data on either socket
  while(poll(fds, 2, -1)) {
    if(fds[0].revents) {
      // Receive a packet from the TUN interface
      received_packet_size = read(tun_fd, receive_buffer, 1500);
      ip = (struct ip*)receive_buffer;

      // Consult routing table for destination
      remote_site_id = find_route(ip->ip_dst.s_addr);

      up_count = 0;
      printf("%i\n", remote_site_id);
      for(j=0;j<remote_sites[remote_site_id].host_count;j++) {
        //if(j==2) { conn_up[j] = 1; } else { conn_up[j] = 0; }
        conn_up[j] = recent(remote_sites[i].remote_hosts[j].receive_timer, 2);
        up_count = up_count + conn_up[j];
      }
      if(up_count > 0) {
        // Increment packet ID
        id++;
        // Calculate the data sizes
        ip = (struct ip*)receive_buffer;
        header_size = ip->ip_hl * 4;
        full_data_size = received_packet_size - header_size;
        chunk_size = (full_data_size / (8 * remote_sites[remote_site_id].host_count)) * 8;
        offset = 0;

        for(j=0;j<remote_sites[remote_site_id].host_count;j++) {
          // If this is the last chunk it might be a bit bigger
          if((full_data_size - offset) < (chunk_size * 2)) {
            chunk_size = full_data_size - offset;
          }

          // Set up a first packet to be transmitted
          ip = (struct ip*)send_buffer;
          ip->ip_hl = 0x5;  // Fixed header length
          ip->ip_v = 0x4;   // Version 4
          ip->ip_tos = 0x0; // Unused
          // Length is made of our header + original header + one chunk of the data
          ip->ip_len = htons(20 + header_size + chunk_size);
          ip->ip_id = 0x0;  // The packet ID doesn't really matter on outer packets
          ip->ip_off = 0x0; // No fragmentation on outer packets
          ip->ip_ttl = 64;  // Sensible TTL
          ip->ip_p = 253;   // IPIP encapsulation
          ip->ip_sum = 0x0; // NULL checksum, this will be calclated later
          // Choose a remote host
          if(conn_up[j]) {
            ip->ip_src.s_addr = remote_sites[remote_site_id].remote_hosts[j].local_address;
            ip->ip_dst.s_addr = remote_sites[remote_site_id].remote_hosts[j].remote_address;
            gettimeofday(&(remote_sites[remote_site_id].remote_hosts[j].send_timer), NULL);
          } else {
            // Send this out over a random link
            i = rand() % remote_sites[remote_site_id].host_count;
            while(!conn_up[i]) {
              i = rand() % remote_sites[remote_site_id].host_count;
            }
            ip->ip_src.s_addr = remote_sites[remote_site_id].remote_hosts[i].local_address;
            ip->ip_dst.s_addr = remote_sites[remote_site_id].remote_hosts[i].remote_address;
            gettimeofday(&(remote_sites[remote_site_id].remote_hosts[i].send_timer), NULL);
          }
          ip->ip_sum = in_cksum((unsigned short *)ip, sizeof(struct ip));

          // Copy the original header into place
          memcpy(send_buffer + 20, receive_buffer, header_size);

          // Copy some data into place
          memcpy(send_buffer + 20 + header_size, receive_buffer + 20 + offset, chunk_size);

          // Modify the duplicate packet to mark as fragmented

          // Apply a packet ID
          ip = (struct ip*)send_buffer + 1;
          ip->ip_id = htons(id);
          // Encode the offset and "more fragments" flag unless this is the last chunk
          ip->ip_off = htons((offset / 8) | (0x2000 * !(chunk_size == full_data_size - offset)));
          ip->ip_len = htons(header_size + chunk_size); // Packet length
          // Calculate checksum
          ip->ip_sum = 0x0;
          ip->ip_sum = in_cksum((unsigned short *)ip, sizeof(struct ip));

          // Transmit the packet
          ip = (struct ip*)send_buffer;
          memset(&sin, 0, sizeof(sin));
          sin.sin_family = AF_INET;
          sin.sin_addr.s_addr = ip->ip_dst.s_addr;
          if (sendto(raw_socket, send_buffer, 20 + header_size + chunk_size, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr)) < 0)  {
            perror("sendto");
            return(1);
          }
        }
      } else {
        printf("No active peer to send packet.\n");
      }
    }

    if(fds[1].revents) {
      // Receive an encapsulated packet
      received_packet_size = recvfrom(raw_socket, receive_buffer, 1500, 0, (struct sockaddr *)&sin, &len);
      // If it's too small to be an IPIP packet, it's a ping
      if(received_packet_size >= 40) {
        printf("Received encapsulated data\n");
        // If it contains data, pass to the OS
      //  write(tun_fd, receive_buffer + 20, received_packet_size - 20);
      } else {
        printf("Received ping\n");
      }
      // See where the packet came from and update the appropriate receive timer
      ip = (struct ip*)receive_buffer;
      i = find_host(ip->ip_dst.s_addr);
      if(i != -1) {
        j = i & 0xff;
        i = i >> 16;
        gettimeofday(&(remote_sites[i].remote_hosts[j].receive_timer), NULL);
      }
    }
  }

  return 0;
}


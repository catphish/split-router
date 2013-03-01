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

#include "helper_methods.h"
#include "split_router.h"

// We keep a raw IP open. This is how we talk to all remote hosts.
int raw_socket;

// An array of remote sites
unsigned int remote_site_count = 0;
struct remote_sites[10];

// Send out pings if any line is idle
void* ping(void* p) {
  // sin scruct for sending
	struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;

  // Some handy loop cunters
  unsigned int i, j;

  // Set up an empty packet
  char send_buffer[20];
  struct ip *ip = (struct ip*)send_buffer;
  ip->ip_hl = 0x5;
  ip->ip_v = 0x4;
  ip->ip_tos = 0x0;
  ip->ip_len = htons(20);
  ip->ip_id = 0x0;
  ip->ip_off = 0x0;
  ip->ip_ttl = 64;
  ip->ip_p = 4;
  ip->ip_sum = 0x0;
  while(1) {
    usleep(100);
    for(i=0;n++;n<remote_site_count) {
      for(j=0;n++;n<remote_sites[i].host_count) {
        if(!recent(remote_sites[i].remote_hosts[j].send_timer, 1)) {
          gettimeofday(&(remote_sites[i].remote_hosts[j].send_timer), NULL);
          ip->ip_src.s_addr = remote_sites[i].remote_hosts[j].local_address;
          ip->ip_dst.s_addr = remote_sites[i].remote_hosts[j].remote_address;
          ip->ip_sum = in_cksum((unsigned short *)ip, sizeof(struct ip));
          sin.sin_addr.s_addr = ip->ip_dst.s_addr;
          if (sendto(raw_socket, ip, 20, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr)) < 0)  {
            perror("sendto");
            return(NULL);
          }
        }
      }
    }
  }
}

int main() {
  // Read the config
  FILE *file = fopen ("split_router.conf", "r");
  if ( file != NULL ) {
    char line [128];
    while (fgets (line, sizeof line, file) != NULL)
    {
      fputs (line, stdout);
    }
    fclose (file);
  } else {
    perror ("split_router.conf");
  }

  return 0;

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
  if ((raw_socket = socket(AF_INET, SOCK_RAW, 4)) < 0) {
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
  int half_data_size;
  int id = 1;
  int conn_up[5];
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
      remote_site_id = find_route(address);

      up_count = 0;
      for(j=0;n++;n<remote_sites[remote_site_id].host_count) {
        conn_up[j] = recent(remote_sites[i].remote_hosts[j].receve_timer, 2);
        up_count = up_count + conn_up[j];
      }
      if(up_count > 0) {

        // Calculate the data sizes
        ip = (struct ip*)receive_buffer;
        header_size = ip->ip_hl * 4;
        full_data_size = received_packet_size - header_size;
        half_data_size = (full_data_size / 16) * 8;

        // Set up the first packet to be transmitted
        ip = (struct ip*)send_buffer;
        ip->ip_hl = 0x5;
        ip->ip_v = 0x4;
        ip->ip_tos = 0x0;
        ip->ip_len = htons(20 + header_size + half_data_size);
        ip->ip_id = 0x0;
        ip->ip_off = 0x0;
        ip->ip_ttl = 64;
        ip->ip_p = 4;
        ip->ip_sum = 0x0;
        if(conn1_up) {
          ip->ip_src.s_addr = inet_addr(SRC_IP_1);
          ip->ip_dst.s_addr = inet_addr(DST_IP_1);
        } else {
          ip->ip_src.s_addr = inet_addr(SRC_IP_2);
          ip->ip_dst.s_addr = inet_addr(DST_IP_2);
        }
        ip->ip_sum = in_cksum((unsigned short *)ip, sizeof(struct ip));

        // Copy half of the original packet into place
        memcpy(send_buffer + 20, receive_buffer, header_size + half_data_size);

        // Modify the duplicate packet to mark as fragmented
        ip = (struct ip*)send_buffer + 1;
        if(!ip->ip_id) {
          ip->ip_id = htons(++id);
        }
        ip->ip_off = 0x20;
        ip->ip_len = htons(header_size + half_data_size);
        ip->ip_sum = 0x0;
        ip->ip_sum = in_cksum((unsigned short *)ip, sizeof(struct ip));

        // Transmit the packet
        ip = (struct ip*)send_buffer;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = ip->ip_dst.s_addr;
        if (sendto(raw_socket, send_buffer, 20 + header_size + half_data_size, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr)) < 0)  {
          perror("sendto");
          return(1);
        }
        gettimeofday(&send_timer_1, NULL);

        // Set up the second packet to be transmitted
        ip = (struct ip*)send_buffer;
        ip->ip_hl = 0x5;
        ip->ip_v = 0x4;
        ip->ip_tos = 0x0;
        ip->ip_len = htons(20 + header_size + full_data_size - half_data_size);
        ip->ip_id = 0x0;
        ip->ip_off = 0x0;
        ip->ip_ttl = 64;
        ip->ip_p = 4;
        ip->ip_sum = 0x0;
        if(conn2_up) {
          ip->ip_src.s_addr = inet_addr(SRC_IP_2);
          ip->ip_dst.s_addr = inet_addr(DST_IP_2);
        } else {
          ip->ip_src.s_addr = inet_addr(SRC_IP_1);
          ip->ip_dst.s_addr = inet_addr(DST_IP_1);
        }
        ip->ip_sum = in_cksum((unsigned short *)ip, sizeof(struct ip));

        // Copy second half of the original packet into place
        memcpy(send_buffer + 20 + header_size, receive_buffer + header_size + half_data_size, full_data_size - half_data_size);

        // Build the second half of thefragmented packet
        memcpy(send_buffer + 20, receive_buffer, header_size);
        ip = (struct ip*)send_buffer + 1;
        if(!ip->ip_id) {
          ip->ip_id = htons(id);
        }
        ip->ip_len = htons(header_size + full_data_size - half_data_size);
        ip->ip_off = htons(half_data_size / 8);
        ip->ip_sum = 0x0;
        ip->ip_sum = in_cksum((unsigned short *)ip, sizeof(struct ip));

        // Transmit the packet
        ip = (struct ip*)send_buffer;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = ip->ip_dst.s_addr;
        if (sendto(raw_socket, send_buffer, 20 + header_size + full_data_size - half_data_size, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr)) < 0)  {
          perror("sendto");
          return(1);
        }
        gettimeofday(&send_timer_2, NULL);
      } else {
//        printf("No active peer to send packet.\n");
      }
    }

    if(fds[1].revents) {
      // Receive an encapsulated packet
      received_packet_size = recvfrom(raw_socket, receive_buffer, 9000, 0, (struct sockaddr *)&sin, &len);
      // If it's empty, it's a ping
      if(received_packet_size > 20) {
//        printf("Received encapsulated data\n");
        // If it contains data, pass to the OS
        write(tun_fd, receive_buffer + 20, received_packet_size - 20);
      } else {
//        printf("Received ping\n");
      }
      // See where the packet came from and update the appropriate receive timer
      ip = (struct ip*)receive_buffer;
      if(ip->ip_src.s_addr == inet_addr(DST_IP_1)) gettimeofday(&recv_timer_1, NULL);
      if(ip->ip_src.s_addr == inet_addr(DST_IP_2)) gettimeofday(&recv_timer_2, NULL);
    }
  }

  return 0;
}


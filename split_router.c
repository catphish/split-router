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

#include "config.h"

// LINK 1
struct timeval send_timer_1;
struct timeval recv_timer_1;

// LINK 2
struct timeval send_timer_2;
struct timeval recv_timer_2;

int raw_socket;

int recent(struct timeval t, int limit) {
  struct timeval timenow;
  gettimeofday(&timenow, NULL);
  timenow.tv_sec = timenow.tv_sec - limit;
  return(t.tv_sec > timenow.tv_sec | (t.tv_sec == timenow.tv_sec & t.tv_usec > timenow.tv_usec));
}

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

void* ping(void* p) {
  // sin scruct for sending
	struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  
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
    if(!recent(send_timer_1, 1)) {
      gettimeofday(&send_timer_1, NULL);
      ip->ip_src.s_addr = inet_addr(SRC_IP_1);
      ip->ip_dst.s_addr = inet_addr(DST_IP_1);
      ip->ip_sum = in_cksum((unsigned short *)ip, sizeof(struct ip));
      sin.sin_addr.s_addr = ip->ip_dst.s_addr;
      if (sendto(raw_socket, ip, 20, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr)) < 0)  {
        perror("sendto");
        return(NULL);
      }
//      printf("Ping 1\n");
    }
    
    if(!recent(send_timer_2, 1)) {
      gettimeofday(&send_timer_2, NULL);
      ip->ip_src.s_addr = inet_addr(SRC_IP_1);
      ip->ip_dst.s_addr = inet_addr(DST_IP_1);
      ip->ip_sum = in_cksum((unsigned short *)ip, sizeof(struct ip));
      sin.sin_addr.s_addr = ip->ip_dst.s_addr;
      if (sendto(raw_socket, ip, 20, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr)) < 0)  {
        perror("sendto");
        return(NULL);
      }
//      printf("Ping 2\n");
    }
  }
}

int main() {
  // Initialize timers
  memset(&send_timer_1, 0, sizeof(struct timeval));
  memset(&recv_timer_1, 0, sizeof(struct timeval));
  memset(&send_timer_2, 0, sizeof(struct timeval));
  memset(&recv_timer_2, 0, sizeof(struct timeval));
  
  // Set up the ping thread
  pthread_t ping_thread;
  pthread_create(&ping_thread, NULL, ping, NULL);
  
  // Receive and transmit buffers
  char receive_buffer[1504];
  char send_buffer[770];
  
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
  int conn1_up = 0;
  int conn2_up = 0;
  
	struct sockaddr_in sin;
  int len;
  
  // Main loop, wait for data on either socket
  while(poll(fds, 2, -1)) {
    if(fds[0].revents) {
      // Receive a packet from the TUN interface
//      printf("Received TUN data\n");
      
      received_packet_size = read(tun_fd, receive_buffer, 1500);
      
      conn1_up = recent(recv_timer_1, 1);
      conn2_up = recent(recv_timer_2, 1);
      if(conn1_up || conn2_up) {
        
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
      received_packet_size = recvfrom(raw_socket, receive_buffer, 1500, 0, (struct sockaddr *)&sin, &len);
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


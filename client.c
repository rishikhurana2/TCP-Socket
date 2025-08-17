#include "helpers.h"


int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: client <hostname> <port> \n");
    exit(1);
  }

  /* Create sockets */
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  // use IPv4  use UDP
  // Error if socket could not be created
  if (sockfd < 0)
    return errno;

  // Set socket for nonblocking
  int flags = fcntl(sockfd, F_GETFL);
  flags |= O_NONBLOCK;
  fcntl(sockfd, F_SETFL, flags);

  // Setup stdin for nonblocking
  flags = fcntl(STDIN_FILENO, F_GETFL);
  flags |= O_NONBLOCK;
  fcntl(STDIN_FILENO, F_SETFL, flags);

  /* Construct server address */
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET; // use IPv4
  // Only supports localhost as a hostname, but that's all we'll test on
  char *addr = strcmp(argv[1], "localhost") == 0 ? "127.0.0.1" : argv[1];
  server_addr.sin_addr.s_addr = inet_addr(addr);
  // Set sending port
  int PORT = atoi(argv[2]);
  server_addr.sin_port = htons(PORT); // Big endian
  socklen_t s = sizeof(struct sockaddr_in);

  uint8_t buffer[MSS];
  packet sending_buffer[20];
  packet receiving_buffer[20];
  int curr_sending_buffer_index = 0;
  int curr_receiving_buffer_index = 0;
  int bytes_recvd;
  uint32_t client_seq_num = rand_uint();
  uint32_t server_seq_num;
  int send_ack = 0;
  int retransmission_timer = 0;
  uint32_t last_ack_num = 0;
  int num_dup_acks = 0;

  packet send_pkt;
  packet rec_pkt;
  packet ack_pkt;

  // Initiate Three way handshake
   int connected = 0;
   while (1) {
    // RECEIVING BYTES
    memset(&rec_pkt, 0, sizeof(rec_pkt));
    bytes_recvd = recvfrom(sockfd, &rec_pkt, sizeof(rec_pkt), 0, (struct sockaddr*) &server_addr, &s);

    if (bytes_recvd > 0) {
      print_diag(&rec_pkt, RECV);
      connected = 1;
      int syn = rec_pkt.flags & 1;
      int ack = (rec_pkt.flags >> 1) & 1;
      if (syn == 1 && ack == 1) {
        server_seq_num = ntohl(rec_pkt.seq);
      }
    }

    // SENDING BYTES
    if (!connected) {
      memset(&send_pkt, 0, sizeof(send_pkt));
      send_pkt.flags = 1;
      send_pkt.seq = htonl(client_seq_num);
      print_diag(&send_pkt, SEND);
      sendto(sockfd, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
      sleep(1);
    } else {
//      int bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer));
      memset(&send_pkt, 0, sizeof(send_pkt));
      send_pkt.flags = 2;
      send_pkt.ack = htonl(server_seq_num + 1);
      send_pkt.seq = htonl(client_seq_num + 1);
      client_seq_num = client_seq_num + 1;
//      send_pkt.length = bytes_read;
      // for (int i = 0; i < bytes_read; i++) {
      //   send_pkt.payload[i] = buffer[i];
      // }
      print_diag(&send_pkt, SEND);
      sendto(sockfd, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
      break;
    }

  }

  // memset(&send_pkt, 0, sizeof(send_pkt));
  // send_pkt.flags = 1;
  // send_pkt.seq = htonl(client_seq_num);
  // sendto(sockfd, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));

  // // Receive the server response
  // memset(&rec_pkt, 0, sizeof(rec_pkt));
  // // wait for the server response
  // while(1) {
  //   bytes_recvd = recvfrom(sockfd, &rec_pkt, sizeof(rec_pkt), 0, (struct sockaddr*) &server_addr, &s);
  //   if (bytes_recvd > 0) { // once we have received bytes, we can stop the first handshake
  //     break;
  //   }
  // }
  // server_seq_num = ntohl(rec_pkt.seq);

  // // Response to the server (three-way)
  // int bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer));
  // memset(&send_pkt, 0, sizeof(send_pkt));
  // send_pkt.flags = 2;
  // send_pkt.ack = htonl(server_seq_num + 1);
  // send_pkt.seq = htonl(client_seq_num + 1);
  // client_seq_num = client_seq_num + 1;
  // // send_pkt.length = bytes_read;
  // // for (int i = 0; i < bytes_read; i++) {
  // //   send_pkt.payload[i] = buffer[i];
  // // }
  // sendto(sockfd, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));


  uint32_t next_expected_packet = server_seq_num + 1;
  client_seq_num = client_seq_num + 1;
  clock_t start_time = clock();

  // Client Sequence Number in client_seq_num
  // Server Sequence Number in server_seq_num
  sleep(1);

  int i = 0;
  while (1) {

    // RECEIVING DATA
    // Receive from socket
    memset(&rec_pkt, 0, sizeof(rec_pkt));
    bytes_recvd = recvfrom(sockfd, &rec_pkt, sizeof(rec_pkt), 0,
                               (struct sockaddr *)&server_addr, &s);
    // Data available to write
    if (bytes_recvd > 0 && !(rec_pkt.flags & 1)) {
      print_diag(&rec_pkt, RECV);
      int ack_flag = (rec_pkt.flags >> 1) & 1;

      if (ack_flag == 1) {
        uint32_t ack = ntohl(rec_pkt.ack);

        if (ack == last_ack_num && ntohs(rec_pkt.length) == 0) {
          num_dup_acks = num_dup_acks + 1;
        } else {
          num_dup_acks = 0;
        }
        last_ack_num = ack;
        // Perform linear scan on the packets in the sending_buffer
        // Any packet with an SEQ less than the packet's ack is assumed to be acknowledged -- remove these packets from the sending buffer
        for (int i = 0; i < 20; i++) {
          if (sending_buffer[i].seq < ack && sending_buffer[i].seq > 0) {        
            remove_element(sending_buffer, i, 20);
            i = i - 1;
            curr_sending_buffer_index = curr_sending_buffer_index - 1;            
          }
        }
      }

      if (ntohs(rec_pkt.length) > 0) {
        // Place the packet in a receiving buffer
        if (curr_receiving_buffer_index < 20 && ntohl(rec_pkt.seq) >= next_expected_packet) {   
          receiving_buffer[curr_receiving_buffer_index] = rec_pkt;
          curr_receiving_buffer_index = curr_receiving_buffer_index + 1;
        }
        // Do a linear scan of all the packets in the receiving buffer starting with the NEXT seq number you exepct and write contents to STDOUT

        fprintf(stderr, "CURR BUFF:");
        for (int i = 0; i < 20; i++) {
            uint32_t curr_seq = ntohl(receiving_buffer[i].seq);
            fprintf(stderr, "%u ", curr_seq);
            if (curr_seq == next_expected_packet) {
              write(STDOUT_FILENO, receiving_buffer[i].payload, ntohs(receiving_buffer[i].length));
              next_expected_packet = curr_seq + ntohs(receiving_buffer[i].length);
              remove_element(receiving_buffer, i, 20);
              i = i - 1;
              curr_receiving_buffer_index = curr_receiving_buffer_index - 1;
            }
        }

        // Finally, send an acknowledgement packet with the ack number being the next sequence number expected
        send_ack = 1;
      }
    }


    // SENDING DATA

        //RETRANSMISSION

    // Retransmit data after 1 second of no acks
    clock_t diff = clock() - start_time;
    int msec = diff * 1000/CLOCKS_PER_SEC;

    if (msec >= 1000 || num_dup_acks >= 2) {

      fprintf(stderr, "RETRANSMITTING: ");
      uint32_t min_seq = UINT32_MAX;
      int mindex = 0;
      for (int i = 0; i < 20; i++) {
        if (sending_buffer[i].seq < min_seq && sending_buffer[i].seq > 0) {
          min_seq = sending_buffer[i].seq;
          mindex = i;
        }
      }

      memset(&send_pkt, 0, sizeof(send_pkt));
      send_pkt = sending_buffer[mindex];

      send_pkt.seq = htonl(send_pkt.seq);
      send_pkt.ack = htonl(send_pkt.ack);
      send_pkt.length = htons(send_pkt.length);

      print_diag(&send_pkt, SEND);
      //if (rand() % 100 >= 10) {
        sendto(sockfd, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr *)&server_addr,
              sizeof(struct sockaddr_in));
      //} else {
        //fprintf(stderr, "LOST PACKET\n");
      //}

      if (num_dup_acks >= 3) {
        num_dup_acks = 0;
      }
      if (msec >= 1000) {
        start_time = clock();
      }
    } else if (curr_sending_buffer_index < 20) { // Retransmit the loewst seq packet if more than 1000 msec has passed or if there are 3 duplicate acks
      int bytes_read = read(STDIN_FILENO, buffer, MSS);
      
      if (bytes_read > 0) {
        memset(&send_pkt, 0, sizeof(send_pkt));
        send_pkt.flags = 0;
        if (send_ack == 1) {
          send_pkt.flags = 2;
          send_pkt.ack = next_expected_packet;
          send_ack = 0;
        }

        send_pkt.length = bytes_read;
        send_pkt.seq = client_seq_num;
        for (int i = 0; i < bytes_read; i++) {
          send_pkt.payload[i] = buffer[i];
        }
        sending_buffer[curr_sending_buffer_index] = send_pkt;
        curr_sending_buffer_index = curr_sending_buffer_index + 1;
        client_seq_num = client_seq_num + send_pkt.length;
        
        send_pkt.seq = htonl(send_pkt.seq);
        send_pkt.ack = htonl(send_pkt.ack);
        send_pkt.length = htons(send_pkt.length);
        
        fprintf(stderr, "SENDING PAYLOAD: ");
      //  if (rand() % 100 >= 10) {
          print_diag(&send_pkt, SEND);
          sendto(sockfd, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr *)&server_addr,
                  sizeof(struct sockaddr_in));
       // } else {
         // fprintf(stderr, "LOST PACKET\n");
        //}

      } else if (send_ack == 1) {
        memset(&ack_pkt, 0, sizeof(ack_pkt));
        ack_pkt.flags = 2;
        ack_pkt.ack = htonl(next_expected_packet);
        ack_pkt.length = 0;
        send_ack = 0;

        fprintf(stderr, "SENDING ACK: ");
        print_diag(&ack_pkt, SEND);
       // if (rand() % 100 >= 10) {
          sendto(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr,
                sizeof(struct sockaddr_in));
        //}
      }

    } else if (send_ack == 1) {
       memset(&ack_pkt, 0, sizeof(ack_pkt));
       ack_pkt.flags = 2;
       ack_pkt.ack = htonl(next_expected_packet);
       ack_pkt.length = 0;
       send_ack = 0;
       
       fprintf(stderr, "SENDING ACK: ");
       print_diag(&ack_pkt, SEND);
       //if (rand() % 100 >= 10) {        
        sendto(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr,
                sizeof(struct sockaddr_in));      
       //}
    }

    if (curr_sending_buffer_index == 0) {
      start_time = clock();
    }

//    sleep(1);
    i = i + 1;
  }

  return 0;
}

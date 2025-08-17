#include "helpers.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: server <port>\n");
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

  /* Construct our address */
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;         // use IPv4
  server_addr.sin_addr.s_addr = INADDR_ANY; // accept all connections
                                            // same as inet_addr("0.0.0.0")
                                            // "Address string to network bytes"
  // Set receiving port
  int PORT = atoi(argv[1]);
  server_addr.sin_port = htons(PORT); // Big endian

  /* Let operating system know about our config */
  int did_bind =
      bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  // Error if did_bind < 0 :(
  if (did_bind < 0)
    return errno;

  struct sockaddr_in client_addr; // Same information, but about client
  socklen_t s = sizeof(struct sockaddr_in);
  uint8_t buffer[MSS];
  packet sending_buffer[20];
  packet receiving_buffer[20];
  int curr_sending_buffer_index = 0;
  int curr_receiving_buffer_index = 0;

  int client_connected = 0;
  int bytes_recvd;
  uint32_t server_seq_num = rand_uint();
  uint32_t client_seq_num;
  int send_ack = 0;
  uint32_t last_ack_num = 0;
  int num_dup_acks = 0;

  packet send_pkt;
  packet rec_pkt;
  packet ack_pkt;

  //Start the Handshake
  uint32_t next_expected_packet;
  // Initial Handshake
  int send_handshake = 0;
  while (1) {
    memset(&rec_pkt, 0, sizeof(rec_pkt));
    bytes_recvd = recvfrom(sockfd, &rec_pkt, sizeof(rec_pkt), 0, (struct sockaddr*) &client_addr, &s);
    if (bytes_recvd <= 0 && !client_connected)
      continue;
    // RECEIVING BYTES
    client_connected = 1;
    if (bytes_recvd > 0) {
      print_diag(&rec_pkt, RECV);
      int syn = rec_pkt.flags & 1;
      int ack = (rec_pkt.flags >> 1) & 1;
      if (syn == 1 && ack == 0) {
        client_seq_num = ntohl(rec_pkt.seq);
        send_handshake = 1;
      }
      if (syn == 0 && ack == 1) {
        client_seq_num = ntohl(rec_pkt.seq);
        send_ack = 1;
        if (rec_pkt.length > 0) {
          write(STDOUT_FILENO, rec_pkt.payload, ntohs(rec_pkt.length));
          next_expected_packet = client_seq_num + ntohs(rec_pkt.length);
        } else {
          next_expected_packet = client_seq_num + 1;
        }
        break;
      }
    }

    // SENDING BYTES
    if (send_handshake) {
      memset(&send_pkt, 0, sizeof(send_pkt));
      send_pkt.flags = 3; // set to 000..00011 (set the ack flag and the syn flag)
      send_pkt.seq = htonl(server_seq_num); // set the sequence to a random number
      send_pkt.ack = htonl(client_seq_num + 1); // set the ack flag to 1 plus the sequence number
      print_diag(&send_pkt, SEND);
      sendto(sockfd, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr_in));
      send_handshake = 0;
    }

  }


  // memset(&rec_pkt, 0, sizeof(rec_pkt));
  // while(1) {
  //   bytes_recvd = recvfrom(sockfd, &rec_pkt, sizeof(rec_pkt), 0, (struct sockaddr*) &client_addr, &s);
  //   if (bytes_recvd <= 0 && !client_connected)
  //     continue;
  //   client_connected = 1;
  //   if (bytes_recvd > 0) { // once we have received bytes, we can stop the first handshake
  //     print_diag(&rec_pkt, RECV);
  //     break;
  //   }
  // }
  // client_seq_num = ntohl(rec_pkt.seq); // grab the clients sequence number

  // // Send back the response for the initial handshake packet
  // memset(&send_pkt, 0, sizeof(send_pkt));
  // send_pkt.flags = 3; // set to 000..00011 (set the ack flag and the syn flag)
  // send_pkt.seq = htonl(server_seq_num); // set the sequence to a random number
  // send_pkt.ack = htonl(client_seq_num + 1); // set the ack flag to 1 plus the sequence number
  // client_seq_num =  client_seq_num + 1;
  // print_diag(&send_pkt, SEND);
  // sendto(sockfd, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr_in));

  // //Wait for the final response
  // memset(&rec_pkt, 0, sizeof(rec_pkt));
  // while (1) {
  //   bytes_recvd = recvfrom(sockfd, &rec_pkt, sizeof(rec_pkt), 0, (struct sockaddr*) &client_addr, &s);
  //   if (bytes_recvd <= 0 && !client_connected) {
  //     continue;
  //   }
  //   if (bytes_recvd > 0) {
  //     print_diag(&rec_pkt, RECV);
  //     send_ack = 1;
  //     //write(STDOUT_FILENO, rec_pkt.payload, rec_pkt.length);
  //     break;
  //   }
  // }

  sleep(1);
//  uint32_t next_expected_packet = client_seq_num + 1;
  server_seq_num = server_seq_num + 1;
  clock_t start_time = clock();
  // Client Sequence Number in client_seq_num
  // Server Sequence Number in server_seq_num
  while (1) {
    // RECEIVING DATA
    memset(&rec_pkt, 0, sizeof(rec_pkt));
    bytes_recvd = recvfrom(sockfd, &rec_pkt, sizeof(rec_pkt), 0,
                               (struct sockaddr *)&client_addr, &s);
    if (bytes_recvd <= 0 && !client_connected)
      continue;
    client_connected = 1;
    // Data available to write
    if (bytes_recvd > 0) {
      print_diag(&rec_pkt, RECV);
      int ack_flag = (rec_pkt.flags >> 1) & 1;

      if (ack_flag == 1) {
        uint32_t ack = ntohl(rec_pkt.ack);
        
        fprintf(stderr, "CURR NUM DUP ACKS: %d\n" ,num_dup_acks);
        if (ack == last_ack_num && ntohs(rec_pkt.length) == 0) {
          num_dup_acks = num_dup_acks + 1;
        } else {
          num_dup_acks = 0;
        }
        last_ack_num = ack;

        // Check the sending buffer for sequence numbers lower than the ack and remove them
        for (int i = 0; i < 20; i++) {
          if (sending_buffer[i].seq < ack && sending_buffer[i].seq > 0) {
            remove_element(sending_buffer, i, 20);
            i = i - 1;
            curr_sending_buffer_index = curr_sending_buffer_index - 1;
          }
        }

      }

      // if there was actually data sent in the packet (not just ack), then add it to the receving buffer and
      // print all expected packets
      if (ntohs(rec_pkt.length) > 0) {
        if (curr_receiving_buffer_index < 20 && ntohl(rec_pkt.seq) >= next_expected_packet) {
          receiving_buffer[curr_receiving_buffer_index] = rec_pkt;
          curr_receiving_buffer_index = curr_receiving_buffer_index + 1;
        }
        // Linear scan of the receiving buffer to write all expected packets to STDOUT
        for (int i = 0; i < 20; i++) {
          uint32_t curr_seq = ntohl(receiving_buffer[i].seq);
          if (curr_seq == next_expected_packet) {
            write(STDOUT_FILENO, receiving_buffer[i].payload, ntohs(receiving_buffer[i].length));
            next_expected_packet = curr_seq + ntohs(receiving_buffer[i].length);
            remove_element(receiving_buffer, i, 20);
            i = i - 1;
            curr_receiving_buffer_index = curr_receiving_buffer_index - 1;
          }
       }

        send_ack = 1; // send an ack indicating the next expected packet
      }
    }
    
    // SENDING DATA
    // Read from stdin

    clock_t diff = clock() - start_time;
    int msec = diff * 1000/CLOCKS_PER_SEC;

    // Retransmit the loewst seq packet if more than 1000 msec has passed or if there are 3 duplicate acks
    if (num_dup_acks >= 2 || msec >= 1000) {
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
        sendto(sockfd, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr *)&client_addr,
              sizeof(struct sockaddr_in));
      //} else {
        //  fprintf(stderr, "LOST RETRANSMISSION PACKET\n");
        //}

      if (num_dup_acks >= 3) {
        num_dup_acks = 0;
      }
      if (msec >= 1000) {
        start_time = clock();
      }
    } else if (curr_sending_buffer_index < 20) {
      int bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));
      
      if (bytes_read > 0) { // if there is data that was read, add it to a packet and attach an ack (if needed)
        fprintf(stderr, "SENDING PAYLOAD: ");
        memset(&send_pkt, 0, sizeof(send_pkt));
        send_pkt.flags = 0;
        if (send_ack == 1) {
          send_pkt.flags = 2;
          send_pkt.ack = next_expected_packet;
          send_ack = 0;
        }

        send_pkt.length = (uint16_t)(bytes_read);
        send_pkt.seq = server_seq_num;
        for (int i = 0; i < bytes_read; i++) {
          send_pkt.payload[i] = buffer[i];
        }

        sending_buffer[curr_sending_buffer_index] = send_pkt;
        curr_sending_buffer_index = curr_sending_buffer_index + 1;
        server_seq_num = server_seq_num + send_pkt.length;

        send_pkt.seq = htonl(send_pkt.seq);
        send_pkt.ack = htonl(send_pkt.ack);
        send_pkt.length = htons(send_pkt.length); 

        print_diag(&send_pkt, SEND);
        //if (rand() % 100 > 10) {
          sendto(sockfd, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr *)&client_addr,
                sizeof(struct sockaddr_in));
        //} else {
          //fprintf(stderr, "LOST PAYLOAD PACKET\n");
        //}

      } else if (send_ack == 1) { // if there is no data to be read and there is an ack that needs to be sent
        fprintf(stderr, "SENDING ACK: ");
        memset(&ack_pkt, 0, sizeof(ack_pkt));
        ack_pkt.flags = 2;
        ack_pkt.ack = htonl(next_expected_packet);
        ack_pkt.length = 0;
        send_ack = 0;

        print_diag(&ack_pkt, SEND);
        
        //if (rand() % 100 >= 10) {
          sendto(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr,
                sizeof(struct sockaddr_in));
        //} else {
          //fprintf(stderr, "LOST ACK PACKET\n");
        //}
      }

    } else if (send_ack == 1) { // if the sending buffer is full and there is an ack that needs to be sent
       fprintf(stderr, "SENDING ACK: ");
       memset(&ack_pkt, 0, sizeof(ack_pkt));
       ack_pkt.flags = 2;
       ack_pkt.ack = htonl(next_expected_packet);
       ack_pkt.length = 0;
       send_ack = 0;

       print_diag(&ack_pkt, SEND);
       //if (rand() % 100 >= 10) {
        sendto(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr,
                sizeof(struct sockaddr_in));    
       //} else {
         // fprintf(stderr, "LOST ACK PACKET\n");
       //}
    }

    if (curr_sending_buffer_index == 0) {
      start_time = clock();
    }

//    sleep(1);
  }

  return 0;
}
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdint.h>


#define RECV 0
#define SEND 1
#define RTOS 2
#define DUPA 3
#define MSS 1012

typedef struct {
	uint32_t ack;
	uint32_t seq;
	uint16_t length;
	uint8_t flags;
	uint8_t unused;
	uint8_t payload[MSS];
} packet;

uint32_t rand_uint() {
  srand(time(NULL)); // Seed the random number generator

  uint32_t max_half = UINT32_MAX / 2;

  // Generate a random number between 2 and max_half (inclusive)
  uint32_t random_num = (rand() % (max_half - 1)) + 2; 

  return random_num;
}
// remove element rem from src
void remove_element(packet * src, int index, int length) {
  for(int i = index; i < length - 1; i++) src[i] = src[i + 1];
  src[length - 1] = (packet) {0};
}

static inline void print_diag(packet * pkt, int diag) {
    switch (diag) {
    case RECV:
        fprintf(stderr, "RECV");
        break;
    case SEND:
        fprintf(stderr, "SEND");
        break;
    case RTOS:
        fprintf(stderr, "RTOS");
        break;
    case DUPA:
        fprintf(stderr, "DUPS");
        break;
    }

    int syn = pkt->flags & 0b01;
    int ack = pkt->flags & 0b10;
    fprintf(stderr, " %u ACK %u SIZE %hu FLAGS ", ntohl(pkt->seq),
            ntohl(pkt->ack), pkt->length);
    if (!syn && !ack) {
        fprintf(stderr, "NONE");
    } else {
        if (syn) {
            fprintf(stderr, "SYN ");
        }
        if (ack) {
            fprintf(stderr, "ACK ");
        }
    }
    fprintf(stderr, "\n");
}

int contains_seq(packet * arr, uint32_t seq, int len) {
    for (int i = 0; i < len; i++) {
        if (arr[i].seq == seq) {
            return 1;
        }
    }

    return 0;
}
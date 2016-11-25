/* 
 *	File : receiver.c
 */ 

#include "receiver.h"

Byte rxbuf[RXQSIZE];
QTYPE rcvq = { 0, 0, 0, RXQSIZE, rxbuf };
QTYPE *rxq = &rcvq;
Byte status = XON;			// XON atau XOFF
unsigned send_xon = 0,
send_xoff = 0;
int endFileReceived;

/* Socket */
int sockfd; // listen on sock_fd
struct sockaddr_in adhost;
struct sockaddr_in srcAddr;
unsigned int srcLen = sizeof(srcAddr);

void error(const char *message) {
	perror(message);
	exit(1);
}

static Byte *rcvchar(int sockfd, QTYPE *queue)
{
 	Byte* current;
 	char tempBuf[1];
 	char b[1];
 	static int counter = 1;

 	current = (Byte *) malloc(sizeof(Byte));
 	if (recvfrom(sockfd, tempBuf, 1, 0, (struct sockaddr *) &srcAddr, &srcLen) < 0)
 		error("ERROR: Gagal menerima karakter dari socket\n");

 	*current = tempBuf[0];

 	if (*current != Endfile) {
 		printf("Menerima byte ke-%d: ", counter++);
		switch (*current) {
			case CR:
				printf("\'<Esc>\'\n");
				break;
			case LF:
				printf("\'<NewLine>\'\n");
				break;
			case Endfile:
				printf("\'<EOF>\'\n");
				break;
			default:	
				printf("\'%c\'\n", *current);
				break;
		}
 	}

 	// menambahkan char ke buffer dan resync queue buffer
 	if (queue->count < 8) {
 		queue->rear = (queue->count > 0) ? (queue->rear+1) % 8 : queue->rear;
 		queue->data[queue->rear] = *current;
 		queue->count++;
 	}

 	// mengirim sinyal XOFF jika buffer mencapai Minimum Upperlimit
 	if(queue->count >= (MIN_UPPERLIMIT) && status == XON) {
 		printf("Buffer > minimum upperlimit\n");
 		printf("Mengirim XOFF.\n");
 		send_xoff = 1; send_xon = 0;
 		b[0] = status = XOFF;

 		if(sendto(sockfd, b, 1, 0,(struct sockaddr *) &srcAddr, srcLen) < 0)
 			error("ERROR: Gagal mengirim XOFF.\n");
 	}

 	return current;
}


void *childRProcess(void *threadid) {
	Byte *data,
	*current = NULL;

 	while (1) {
 		current = q_get(rxq, data);
 		sleep(2);
 	}

 	pthread_exit(NULL);
}

static Byte *q_get(QTYPE *queue, Byte *data)
/* q_get returns a pointer to the buffer where data is read or NULL if buffer is empty. */
{
 	Byte *current = NULL;
 	char b[1];
 	static int counter = 1;

 	// Hanya mengkonsumsi jika buffer tidak empty
 	if (queue->count > 0) {
 		current = (Byte *) malloc(sizeof(Byte));
 		*current = queue->data[queue->front];

 		queue->front++;
 		if (queue->front == 8) queue->front = 0;
 		queue->count--;

 		printf("Mengkonsumsi byte ke-%d: ", counter++);
		switch (*current) {
			case CR:
				printf("\'<Return>\'\n");
				break;
			case LF:
				printf("\'<NewLine>\'\n");
				break;
			case Endfile:
				printf("\'<EOF>\'\n");
				break;
			default:	
				printf("\'%c\'\n", *current);
				break;
		}
 	}


 	if (queue->count < MAX_LOWERLIMIT && status == XOFF) {
 		printf("Buffer < maximum lowerlimit\n");
 		printf("Mengirim XON\n");
 		send_xon = 1; send_xoff = 0;

 		b[0] = status = XON;
 		if(sendto(sockfd, b, 1, 0, (struct sockaddr *) &srcAddr, srcLen) < 0)
 			error("ERROR: Gagal mengirim XON.\n");
 	}	

 	// mengembalikan byte yg dikonsumsi
 	return current;
}

int main(int argc, char *argv[])
{
 	pthread_t thread[1];

 	if (argc<2) {
 		// argumen pemanggilan program kurang
 		printf("Usage: %s <port>\n", argv[0]);
 		return 0;
 	}

 	printf("Membuat socket lokal pada port %s...\n", argv[1]);
 	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
 		printf("ERROR: Create socket gagal.\n");

 	bzero((char*) &adhost, sizeof(adhost));
 	adhost.sin_family = AF_INET;
 	adhost.sin_port = htons(atoi(argv[1]));
 	adhost.sin_addr.s_addr = INADDR_ANY;

 	if(bind(sockfd, (struct sockaddr*) &adhost, sizeof(adhost)) < 0)
 		error("ERROR: bind socket gagal.\n");

 	endFileReceived = 0;

 	memset(rxbuf, 0, sizeof(rxbuf));
 	
 	// membuat child
 	if(pthread_create(&thread[0], NULL, childRProcess, 0)) 
 		error("ERROR: Gagal membuat proses anak");

	// parent process
	Byte c;
	while (1) {
 		c = *(rcvchar(sockfd, rxq));

 		if (c == Endfile)
 			endFileReceived = 1;
	}

	printf("Menerima EOF!\n");

	return 0;
}



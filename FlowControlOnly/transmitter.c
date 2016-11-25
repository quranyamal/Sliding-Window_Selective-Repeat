/* 
 *	File 	: transmitter.c 
 */

#include "transmitter.h"

/* NETWORKS */
int sockfd, port;							// sock file descriptor and port number
struct hostent *server;
struct sockaddr_in receiverAddress;			// receiver host information
int receiverAddrLen = sizeof(receiverAddress);

/* FILE AND BUFFERS */
FILE *myFile;			// file descriptor
char *receiverIP;		// buffer for Host IP address
char buf[BUFMAX+1];		// buffer for character to send
char xbuf[BUFMAX+1];	// buffer for receiving XON/XOFF characters

/* FLAGS */
int isXON = 1;			// flag for XON/XOFF sent
int isSocketOpen;		// flag to indicate if connection from socket is done

void error(const char *message) {
	perror(message);
	exit(1);
}

void *childProcess(void *threadid) {
	// Mengecek XON/XOFF
	struct sockaddr_in srcAddr;
	int srcLen = sizeof(srcAddr);
	while (isSocketOpen) {
		if (recvfrom(sockfd, xbuf, BUFMAX, 0, (struct sockaddr *) &srcAddr, &srcLen) != BUFMAX)
			error("ERROR: ukuran buffer besar\n");

		if (xbuf[0] == XOFF) {
			isXON = 0;
			printf("XOFF diterima.\n");
		} else {				// xbuf[0] == XON
			isXON = 1;
			printf("XON diterima.\n");
		}
	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	pthread_t thread[1];

	if (argc < 4) {
		// argumen kurang
		printf("Usage: %s <target-ip> <port> <filename>\n", argv[0]);
		return 0;
	}

	if ((server = gethostbyname(argv[1])) == NULL)
		error("ERROR: Alamat receiver salah.\n");

	// creating IPv4 data stream socket
	printf("Membuat socket untuk koneksi ke %s:%s ...\n", argv[1], argv[2]);
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		error("ERROR: Gagal membuat socket\n");

	// flag=1 -> connection is established
	isSocketOpen = 1;

	// inisialisasi socket host
	memset(&receiverAddress, 0, sizeof(receiverAddress));
	receiverAddress.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&receiverAddress.sin_addr.s_addr, server->h_length);
	receiverAddress.sin_port = htons(atoi(argv[2]));

	// membuka file
	myFile = fopen(argv[argc-1], "r");
	if (myFile == NULL) 
		error("ERROR: File tidak ditemukan.\n");

	if (pthread_create(&thread[0], NULL, childProcess, 0) != 0) 
		error("ERROR: Gagal membuat thread.\n");
	
	// proses parent
	// mengirimkan file (karakter isi file)
	// connect to receiver, and read the file per character
	int counter = 1;
	while ((buf[0] = fgetc(myFile)) != EOF) {
		if (isXON) {
			if (sendto(sockfd, buf, BUFMAX, 0, (const struct sockaddr *) &receiverAddress, receiverAddrLen) == -1)
				error("ERROR: Buffer > minimum upper limit!\n");
			
			printf("Mengirim byte ke-%d ", counter++);
			switch (buf[0]) {
				case CR:	printf("\'<Esc>\'\n");
							break;
				case LF:	printf("\'<NewLine>\'\n");
							break;
				default:	printf("\'%c\'\n", buf[0]);
							break;
			}
		} else {
			while (!isXON) {
				printf("Menunggu XON...\n");
				sleep(1);
			}
			if (isXON) {
				if (sendto(sockfd, buf, BUFMAX, 0, (const struct sockaddr *) &receiverAddress, receiverAddrLen) == -1)
					error("ERROR: Buffer > minimum upper limit!\n");
				
				printf("Mengirim byte ke-%d ", counter++);
				switch (buf[0]) {
					case CR:	printf("\'<Return>\'\n");
								break;
					case LF:	printf("\'<NewLine>\'\n");
								break;
					default:	printf("\'%c\'\n", buf[0]);
								break;
				}
			}
		}
		memset(buf, '\0', BUFMAX);
		sleep(1);
	} 
	
	printf("Mengirim byte ke-%d '<EOF>'\n", counter++);		// EOF
	
	

	// sending endfile to receiver, marking the end of data transfer
	buf[0] = Endfile;
	sendto(sockfd, buf, BUFMAX, 0, (const struct sockaddr *) &receiverAddress, receiverAddrLen);
	fclose(myFile);
	
	printf("Karakter selesai dikirim, menutup socket\n");
	
	close(sockfd);
	isSocketOpen = 0;
	printf("Socket ditutup!\n");
	
	printf("Finish!\n");
	return 0;
}

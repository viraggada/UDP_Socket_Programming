/***************************************************************
   Author - Virag Gada
   File - client.c
   Description - Source file for client
 ****************************************************************/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <strings.h>

/* Global Definitions */
#define MAX_BUFF_SIZE (512)
#define ENCRYPT_BYTE (0x1234)

/* Prompt for user on using the commands */
void promptUser(void){
  printf("\nCommands available to use:\n");
  printf("    get [file_name]\n");
  printf("    put [file_name]\n");
  printf("    delete [file_name]\n");
  printf("    ls\n");
  printf("    exit\n");
}

int main(int argc, char **argv) {
  /*Variables*/
  int socket_fd = 0;
  struct sockaddr_in *remoteAddr;
  struct sockaddr_in fromAddr;
  char command[50];
  int byte_count;
  char *token[2];
  int size_to_read;
  int size_to_send;
  char buffer[MAX_BUFF_SIZE];
  char holder[MAX_BUFF_SIZE-1];
  int i,j;
  int ACK[2]={0};
  int packetLoss = 0;
  int retransmitted = 0, counter = 0;
  struct stat file_info;
  FILE *fptr;
  struct timeval tv;
  tv.tv_sec = 5;  /* 5 Secs Timeout */
  tv.tv_usec = 0;

  if (argc < 3) {
    printf("USAGE: ./client <server_ip> <server_port>\n");
    exit(1);
  }

  struct hostent *he = gethostbyname(argv[1]);
  unsigned long server_addr_nbo = *(unsigned long *)(he->h_addr_list[0]);

  /* Configure server socket address structure (init to zero, IPv4,
     network byte order for port and address) */
  remoteAddr = (struct sockaddr_in *)calloc(1,sizeof(struct sockaddr_in));
  (*remoteAddr).sin_family = AF_INET;
  (*remoteAddr).sin_port = htons(atoi(argv[2]));
  (*remoteAddr).sin_addr.s_addr = server_addr_nbo;

  /*Create Socket*/
  if ((socket_fd = socket((*remoteAddr).sin_family, SOCK_DGRAM, 0)) < 0) {
    printf("Client failed to create socket\n");
    exit(1);
  }

  bzero(&fromAddr, sizeof(fromAddr));

  socklen_t fromAddrSize = sizeof(fromAddr);

  while(1) {
    /* Prompt the user everytime on how to send the command */
    promptUser();
    fgets(command,50,stdin);

    /* Add NULL terminator to user input*/
    if ((strlen(command)>0) && (command[strlen (command) - 1] == '\n'))
      command[strlen (command) - 1] = '\0';

    /* Send command to Server */
    if((byte_count = sendto(socket_fd,command,strlen(command),0,(struct sockaddr *)remoteAddr,sizeof(*remoteAddr)))<0) {
      printf("Unable to send data\n");
      exit(1);
    }

    /* Seperate values from buffer with delimiter " "(space) */
    token[0] = strtok(command," ");
    // Get the next value
    token[1] = strtok(NULL," ");

    /*Set Timeout for file transfers*/
    tv.tv_sec = 5;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
    /* Send get command to server */
    if(strcmp("get",token[0]) == 0){
      byte_count = recvfrom(socket_fd,buffer,MAX_BUFF_SIZE,0,(struct sockaddr *)&fromAddr,&fromAddrSize);
      if(strcmp("error",buffer) == 0){
         printf("Error Reading File\n");
      }
      else{
        size_to_read = MAX_BUFF_SIZE;
        /* Get the amount of file to expect */
        long int file_size = atoi(buffer);
        printf("Size: %ld\n",file_size);
        fptr=fopen(token[1],"w+");
        i = 0;
        while(file_size >= 2){
          /* Check if the data to be sent is less than the Max buffer size */
          if(file_size<MAX_BUFF_SIZE){
            size_to_read = file_size+1;
          }
          /* Send ACK or NACK based on the data and sequence number received
          Do this till we get the correct packet */
          do{
            /* Receive data from the client and check for valid data */
            byte_count = recvfrom(socket_fd,buffer,size_to_read,0,(struct sockaddr *)&fromAddr,&fromAddrSize);
            /* If packet is of correct size and sequence number than accept it and send ACK for that packet*/
            if((byte_count == size_to_read)&&(buffer[0] == (char)(i))) { //correct packet
              ACK[0]=1;
              ACK[1]=(char)i;
              sendto(socket_fd, ACK, sizeof(ACK), 0, (struct sockaddr*) remoteAddr,sizeof(*remoteAddr));
            }/* If packet size incomplete send NACK to resend the packet */
            else if(byte_count != size_to_read){ //packet incomplete
              ACK[0]=0;
              ACK[1]=buffer[0];
              sendto(socket_fd, ACK, sizeof(ACK), 0, (struct sockaddr*) remoteAddr,sizeof(*remoteAddr));
            }/* If the sequence number is of the previous packet so the ACK was lost,
            send an ACK again for that packet and discard the packet */
            else if(buffer[0]==(char)(i-1)){ //previous packet, retransmit case
              retransmitted = 1;
              counter++;
              ACK[0]=1;
              ACK[1]=buffer[0];
              sendto(socket_fd, ACK, sizeof(ACK), 0, (struct sockaddr*) remoteAddr,sizeof(*remoteAddr));
            }/* If packet received is out of order then send NACK*/
            else{//wrong sequence
              ACK[0]=0;
              ACK[1]=buffer[0];
              sendto(socket_fd, ACK, sizeof(ACK), 0, (struct sockaddr*) remoteAddr,sizeof(*remoteAddr));
            }
          }while((ACK[0]!=1));
          /* Write to file only if it is not a retransmitted packet */
          if(retransmitted == 0){
          for(j=0;j<byte_count-1;j++){
            /* Decrypt the file based on the XOR cipher */
            holder[j] = ENCRYPT_BYTE^buffer[j+1];
          }
          if(fwrite(holder,byte_count-1,1,fptr)<0)
          {
            printf("error writting file\n");
          }
          /* Decrement the amount of data that has been received */
          file_size=file_size-byte_count+1;
          printf("Times %d, ACK0 - %d,ACK1- %d, Sequence- %d, File Ptr - %ld, Left - %ld\n",i++,(int)ACK[0],(int)ACK[1],buffer[0],ftell(fptr),file_size);
          }
          else {
            retransmitted = 0;
          }
          /* Clear buffers for next write */
          memset(buffer,0,sizeof(buffer));
          memset(holder,0,sizeof(holder));
        }
        fclose(fptr);
        printf("ACK's retransmitted %d\n",counter);
        counter = 0;
        printf("File Received\n");
      }
      /* Send put command to server */
    }else if(strcmp("put",token[0]) == 0){
      if((fptr = fopen (token[1],"r")) == NULL){
        strcpy(buffer,"error");
        printf("Error reading\n");
        if (sendto(socket_fd, buffer, MAX_BUFF_SIZE, 0, (struct sockaddr*) remoteAddr,sizeof(*remoteAddr)) == -1) {
          printf("Send Failed\n");
        }
      }else{
        /* Get the file information */
        fstat(fileno(fptr),&file_info);
        /* Read the file size */
        long int file_size=file_info.st_size;
        /* Copy into buffer and sent it to the client */
        sprintf(buffer,"%ld",file_size);
        if (sendto(socket_fd, buffer, sizeof(file_size), 0, (struct sockaddr*) remoteAddr,sizeof(*remoteAddr)) == -1) {
          printf("Not Sending File\n");
          break;
        }
        ACK[0]=0;
        printf("Size - %ld\n", file_size);
        size_to_send = MAX_BUFF_SIZE;
        i = 0;
        while (file_size>=2)
        {
          /* Check if the data to be sent is less than the Max buffer size */
          if(file_size<MAX_BUFF_SIZE){
            size_to_send = file_size+1;
          }
          /*Clear all the buffers*/
          memset(buffer,0,sizeof(buffer));
          memset(holder,0,sizeof(holder));
          /*Read file data*/
          if(fread(holder,size_to_send-1,1,fptr)<=0){
            printf("unable to copy file into buffer\n");
          }else {
            /* Put the sequence number at the start of each packet */
            buffer[0]=(char)i;
            /* Encrypt the data using XOR cipher*/
            for(j=1;j<size_to_send;j++){
              buffer[j]=ENCRYPT_BYTE^holder[j-1];
            }
            /* Send the data until all the data is sent and we send the correct packet*/
            do{
              if(sendto(socket_fd,buffer,size_to_send,0, (struct sockaddr *)remoteAddr, sizeof(*remoteAddr))<0){
                printf("error in sending the file\n");
              }
              recvfrom(socket_fd,&ACK,sizeof(ACK),0,(struct sockaddr *)&fromAddr,&fromAddrSize);
              if(ACK[0] == 0){
                packetLoss++;
              }
            }while(ACK[0]!=1);
          }
          printf("File Ptr: %ld,  ACK - %d, Times - %d, Left - %ld\n", ftell(fptr),(int)ACK[0],i++,file_size);
          ACK[0] = 0;
            /* Decrement the amount of data that has been sent */
          file_size=file_size-size_to_send+1;
        }
        /* Print the total packets lost after each transfer */
        printf("Packet Loss - %d\n",packetLoss);
        packetLoss = 0;
        fclose(fptr);
      }
    }else{
      byte_count = recvfrom(socket_fd,buffer,MAX_BUFF_SIZE,0,(struct sockaddr *)&fromAddr,&fromAddrSize);
      if(byte_count > 0) {
        buffer[byte_count]=0;
        printf("Received from server - %s\n",buffer);
      }
    }

    /* Reset Timeout */
    tv.tv_sec = 0;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
  }
  memset(buffer,0,sizeof(buffer));
  memset(buffer,0,sizeof(holder));
  /* Close the socket and return */
  close(socket_fd);
  return 0;
}

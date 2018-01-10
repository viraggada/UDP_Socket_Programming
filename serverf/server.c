/***************************************************************
Author - Virag Gada
File - server.c
Description - Source file for server
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
#include <sys/time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <memory.h>
#include <dirent.h>
#include <errno.h>

/* Global Definitions */
#define MAX_BUFF_SIZE (512)
#define ENCRYPT_BYTE (0x1234)

/* Function Prototype */
int delete(char * file);

int main(int argc, char *argv[])
{
  /*Variables*/
  int sockfd;
  struct sockaddr_in *selfAddr;
  struct sockaddr_in toAddr;
  char buffer[MAX_BUFF_SIZE];
  char holder[MAX_BUFF_SIZE-1];
  int byte_count = 0;
  socklen_t toAddrSize;
  char *command_list[5];
  command_list[0] = "get";
  command_list[1] = "put";
  command_list[2] = "delete";
  command_list[3] = "ls";
  command_list[4] = "exit";
  int command_num = 0;
  char command[50] = {0};
  int size_to_read;
  int size_to_send;
  struct stat file_info;
  int i,j;
  int ACK[2]={0};
  int packetLoss = 0;
  int retransmitted = 0, counter = 0;
  char *token[2];
  FILE *fptr;
  struct timeval tv;
  tv.tv_sec = 5;  /* 5 Secs Timeout */
  tv.tv_usec = 0;

  if (argc != 2) {
    printf ("USAGE: ./server <port>\n");
    exit(1);
  }

  selfAddr = (struct sockaddr_in *)calloc(1,sizeof(struct sockaddr_in));
  (*selfAddr).sin_family = AF_INET;                //address family
  (*selfAddr).sin_port = htons(atoi(argv[1]));     //sets port to network byte order
  (*selfAddr).sin_addr.s_addr = htonl(INADDR_ANY); //sets local address

  /*Create Socket*/
  //For UDP since there is only one form of datagram service there are no variations so protocol = 0
  if((sockfd = socket((*selfAddr).sin_family,SOCK_DGRAM,0))< 0) {
    printf("Unable to create socket\n");
    exit(1);
  }
  printf("Socket Created\n");

  /*Call Bind*/
  if(bind(sockfd,(struct sockaddr *)selfAddr,sizeof(*selfAddr))<0) {
    printf("Unable to bind\n");
    exit(1);
  }
  printf("Socket Binded\n");

  memset(&toAddr,0,sizeof(toAddr));
  memset(buffer,0,sizeof(buffer));
  toAddrSize = sizeof(toAddr);

  /*listen*/
  while(1) {
    printf("waiting for data..\n");

    /* Accept */
    if((byte_count = recvfrom(sockfd,command,MAX_BUFF_SIZE,0,(struct sockaddr *)&toAddr,&toAddrSize)) < 0) {
      printf("Failed to receive\n");
    }
    printf("Command - %s\n", command);
    printf("Received packet from %s:%d\n", inet_ntoa((toAddr).sin_addr), ntohs((toAddr).sin_port));

    /*Seperate the commands and file name*/
    token[0] = strtok(command," ");
    /* Get the next value */
    token[1] = strtok(NULL," ");
    /* Perform actions based on commands */
    for(int i = 0; i<5; i++) {
      if(strcmp(command_list[i],token[0]) == 0) {
        command_num = i+1;
        break;
      }
    }

    /*Set Timeout for file transfers*/
    tv.tv_sec = 5;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));

    // Perform action based on input from user
    switch (command_num) {

      case 1: /* Get command from client */
        if((fptr = fopen (token[1],"r")) == NULL){
          strcpy(buffer,"error");
          printf("Error reading\n");
          if (sendto(sockfd, buffer, MAX_BUFF_SIZE, 0, (struct sockaddr*) &toAddr,toAddrSize) == -1) {
            printf("Send Failed\n");
          }
        }else{
          /* Get the file information */
          fstat(fileno(fptr),&file_info);
          /* Read the file size */
          long int file_size=file_info.st_size;
          /* Copy into buffer and sent it to the client */
          sprintf(buffer,"%ld",file_size);
          if (sendto(sockfd, buffer, sizeof(file_size), 0, (struct sockaddr*) &toAddr,toAddrSize) == -1) {
            printf("Not Sending File\n");
            break;
          }
          printf("Size - %ld\n", file_size);
          size_to_send = MAX_BUFF_SIZE;
          i = 0;
          ACK[0]=0;
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
            }
            else {
              /* Put the sequence number at the start of each packet */
              buffer[0]=(char)i;
              /* Encrypt the data using XOR cipher*/
              for(j=1;j<size_to_send;j++){
                buffer[j]=ENCRYPT_BYTE^holder[j-1];
              }
              /* Send the data until all the data is sent and we send the correct packet*/
              do{
                if(sendto(sockfd,buffer,size_to_send,0, (struct sockaddr *)&toAddr, toAddrSize)<0){
                  printf("error in sending the file\n");
                }
                recvfrom(sockfd,&ACK,sizeof(ACK),0,(struct sockaddr *)&toAddr,&toAddrSize);
                if(ACK[0]==0){
                  packetLoss++;
                }
              }while((ACK[0]!=1));
            }
            /* Decrement the amount of data that has been sent */
            file_size=file_size-size_to_send+1;
            printf("File Ptr: %ld, ACK - %d, Times - %d, Left - %ld, Sequence - %d\n", ftell(fptr),(int)ACK[0],i++,file_size,buffer[0]);
            ACK[0]=0;
          }
          /* Print the total packets lost after each transfer */
          printf("Packet Loss - %d\n",packetLoss);
          packetLoss = 0;
          fclose(fptr);
        }
        break;

      case 2: /* Put command from client */
        byte_count = recvfrom(sockfd,buffer,MAX_BUFF_SIZE,0,(struct sockaddr *)&toAddr,&toAddrSize);
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
          while(file_size>=2){
            /* Check if the data to be sent is less than the Max buffer size */
            if(file_size<MAX_BUFF_SIZE){
              size_to_read = file_size+1;
            }
            /* Send ACK or NACK based on the data and sequence number received
            Do this till we get the correct packet */
            do{
              /* Receive data from the client and check for valid data */
              byte_count = recvfrom(sockfd,buffer,size_to_read,0,(struct sockaddr *)&toAddr,&toAddrSize);
              /* If packet is of correct size and sequence number than accept it and send ACK for that packet*/
              if((byte_count == size_to_read)&&(buffer[0] == (char)(i))){
                ACK[0]=1;
                ACK[1]=(char)i;
                sendto(sockfd, ACK, sizeof(ACK), 0, (struct sockaddr*)&toAddr,toAddrSize);
              }/* If packet size incomplete send NACK to resend the packet */
              else if(byte_count != size_to_read){
                ACK[0]=0;
                ACK[1]=buffer[0];
                sendto(sockfd, ACK, sizeof(ACK), 0, (struct sockaddr*) &toAddr,toAddrSize);
              }/* If the sequence number is of the previous packet so the ACK was lost,
              send an ACK again for that packet and discard the packet */
              else if(buffer[0]==(char)(i-1)){ //previous pac2et, retransmit case
                retransmitted = 1;
                counter++;
                ACK[0]=1;
                ACK[1]=buffer[0];
                sendto(sockfd, ACK, sizeof(ACK), 0, (struct sockaddr*) &toAddr,toAddrSize);
              }/* If packet received is out of order then send NACK*/
              else{
                ACK[0]=0;
                ACK[1]=buffer[0];
                sendto(sockfd, ACK, sizeof(ACK), 0, (struct sockaddr*)&toAddr,toAddrSize);
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
                printf("Error writting file\n");
              }
                /* Decrement the amount of data that has been received */
              file_size=file_size-byte_count+1;
              printf("Times- %d, File Ptr - %ld, ACK0 - %d, ACK1- %d,Sequence - %d, Left - %ld\n",i++,ftell(fptr),(int)ACK[0],(int)ACK[1],buffer[0],file_size);
            }
            else {
              retransmitted = 0;
            }
            /* Clear buffers for next write */
            memset(buffer,0,sizeof(buffer));
            memset(holder,0,sizeof(holder));
          }
          fclose(fptr);
          printf("ACK's retransmitted - %d\n",counter);
          counter = 0;
          printf("File Received\n");
        }
        break;

      /* Delete command from client */
      case 3:
        printf("Delete file - %s\n",token[1]);
        int status;
        status = delete(token[1]);
        if(status == 0){
          strcpy(buffer,"File Removed");
        }
        else{
          strcpy(buffer,"Error Removing");
        }
        if (sendto(sockfd, buffer, MAX_BUFF_SIZE, 0, (struct sockaddr*) &toAddr,toAddrSize) == -1) {
          printf("Send Failed\n");
        }
        break;

      /* List command from client */
      case 4:
        printf("Sending list of files in this directory\n");
          DIR *d;

          struct dirent *dir;
          /* Get files in the current directory */
          d = opendir(".");
          if (d)
          {
            while ((dir = readdir(d)) != NULL)
            {
              strcat(buffer,dir->d_name);
              strcat(buffer," ");
            }
            closedir(d);
          }
          /* Now reply the client with the same data */
          if (sendto(sockfd, buffer, MAX_BUFF_SIZE, 0, (struct sockaddr*) &toAddr,toAddrSize) == -1) {
            printf("Send Failed\n");
          }
          break;

      case 5: /* Close server */
        strcpy(buffer,"Closed");
        if (sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*) &toAddr,toAddrSize) == -1) {
          printf("Send Failed\n");
        }
        close(sockfd);
        exit(1);
        break;

      default: /* If wrong command sent */
        sprintf(buffer,"%s %s\n","Command not recognized-",token[0]);
        //strcpy(buffer,"Command not recognized");
        if (sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*) &toAddr,toAddrSize) == -1) {
          printf("Send Failed\n");
        }
        break;
    }
    memset(buffer,0,sizeof(buffer));
    memset(holder,0,sizeof(holder));
    memset(command,0,sizeof(command));
    command_num = 0;
    /* Reset Timeout to enter blocking state */
    tv.tv_sec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
  }

  /*Close Socket*/
  close(sockfd);
  return 0;
}

int delete(char * file){
  int status = remove(file);
  if(status == 0){
    return 0;
  }
  else{
    return -1;
  }
}

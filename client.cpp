#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <strings.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "chunk.hpp"

#define SA struct sockaddr

void* waitForInput(void* action);
bool running= true;
bool cancelation= false;
pthread_mutex_t lock;

long length=0;
long fullOffset=0;

char* localPath;
char* fileName;
char* relativePath;

int main(int argc, char** argv){

    pthread_mutex_lock(&lock);
    localPath= argv[2];
    fileName= argv[3];
    relativePath= argv[4];
    pthread_mutex_unlock(&lock);

    pthread_t t;
    pthread_create(&t, NULL, waitForInput, NULL);

    int socket_id;
    struct sockaddr_in server_address;
    u_char buff[CHUNK_SIZE];
    int hc_size= 200 * CHUNK_SIZE;
    u_char* hugeBuff=(u_char*)malloc(hc_size*sizeof(u_char));

    if((socket_id = socket(AF_INET, SOCK_STREAM, 0))<0)
        printf("Cannot create socket...\n");

    bzero(&server_address, sizeof(server_address));
    server_address.sin_family= AF_INET;
    server_address.sin_port= htons(SERVICE_PORT);
    server_address.sin_addr.s_addr = inet_addr(argv[1]);

    if(connect(socket_id, (SA*)&server_address, sizeof(server_address)) != 0)
        printf("Cannot connect...\n");

    FILE *f;

    pthread_mutex_lock(&lock);
    f= fopen(localPath,"r");
    fseek(f,0,SEEK_END);
    length= ftell(f);
    pthread_mutex_unlock(&lock);

    fseek(f, 0,SEEK_SET);
    fclose(f);

    u_char mess[1024];

    pthread_mutex_lock(&lock);
    long fn_len= strlen(fileName);
    long rp_len= strlen(relativePath);
    char* v1= (char*)&length;

    char* v2= (char*)&fn_len;
    char* v3= (char*)&rp_len;
    
    for(int i=0;i<8;i++)
        mess[i]=v1[i];

    mess[8]=0x02;
    mess[9]=0x02;

    for(int i=0;i<8;i++)
        mess[i+10]=v2[i];

    mess[18]=0x02;
    mess[19]=0x02;

    for(int i=0;i<8;i++)
        mess[i+20]=v3[i];

    mess[28]=0x02;
    mess[29]=0x02;

    for(int i=0;i<fn_len;i++)
        mess[i+30]=fileName[i];

    mess[30+fn_len]=0x02;
    mess[31+fn_len]=0x02;

    for(int i=0;i<rp_len;i++)
        mess[i+32+fn_len]=relativePath[i];

    write(socket_id, mess, 1024);

    int order=0;
    int cur;
    int suma=0;
    int r= CHUNK_SIZE;
    long ll= length;
    long offset=0;
    long s_len= hc_size;

    if(s_len > length)
        s_len= length;
    
    f= fopen(localPath,"r");
    pthread_mutex_unlock(&lock);

    while((cur = fread(hugeBuff, s_len, sizeof(char),f) >0)){

        int chunk_num= s_len / CHUNK_SIZE;
        if(s_len % CHUNK_SIZE>0)
            chunk_num+=1;

        offset=0;
        for(int i=0; i<chunk_num;i++){
            memset(buff, '\0', CHUNK_SIZE);
            memcpy(buff,hugeBuff+offset, (s_len-offset>CHUNK_SIZE?CHUNK_SIZE:s_len-offset));
            offset+=CHUNK_SIZE;

            start:Chunk chunk(buff, (char*)"name", r, order);

            if(cancelation){
                printf("Transfer canceled by sender\n");
                goto end;
            }

            char pack[CHUNK_SIZE+1024];
            chunk.getBytes(pack);

            int writen= write(socket_id, pack, CHUNK_SIZE+1024);
            ll-=r;

            char response[12];
            read(socket_id, response, 12);

            if(response[0]!=0x0a
            && response[1]!=0x0a
            && response[2]!=0x0a
            && response[3]!=0x0a){
                printf("package resent...\n");
                goto start;
            }
            
            char to_int[4]={response[4], response[5], response[6], response[7]};
            int* res= (int*)&to_int;
            if(*res!=order){
                printf("package resent...\n");
                goto start;
            }

            suma+= r;
            order+= 1;

            if(ll <= CHUNK_SIZE)
                r=ll;
            else
                r= CHUNK_SIZE;

        }

        pthread_mutex_lock(&lock);
        fullOffset+= offset;

        if(length-fullOffset >= hc_size)
            s_len= hc_size;
        else
            s_len= length-fullOffset;

        pthread_mutex_unlock(&lock);

        memset(hugeBuff, '\0', hc_size);
    }

    end:
    running= false;
    pthread_cancel(t);
    free(hugeBuff);
    fclose(f);
    close(socket_id);
    pthread_exit(NULL);

    return 0;
}

void* waitForInput(void* action){

    char bb[256];
    memset(bb, '\0', 256);

    while(running){
        scanf("%s", &bb);
        strcat(bb, "\0");

        if(strcmp(bb,"read")==0){
            pthread_mutex_lock(&lock);
            printf("%s\t%ld of %ld\n", localPath, fullOffset, length);
            pthread_mutex_unlock(&lock);
        }else if(strcmp(bb, "cancel")==0){
            pthread_mutex_lock(&lock);
            cancelation= true;
            pthread_mutex_unlock(&lock);
        }

    }

    pthread_exit(NULL);
}
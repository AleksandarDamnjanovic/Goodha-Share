#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "chunk.hpp"

#define SA struct sockaddr

DWORD WINAPI waitForInput(LPVOID action);
bool running= true;
bool cancelation= false;
CRITICAL_SECTION critical;

long length=0;
long fullOffset=0;

char* localPath;
char* fileName;
char* relativePath;

int main(int argc, char** argv){

    WSADATA wsadata;
    InitializeCriticalSectionAndSpinCount(&critical, 0x00000400);

    EnterCriticalSection(&critical);
    localPath= argv[2];
    fileName= argv[3];
    relativePath= argv[4];
    LeaveCriticalSection(&critical);

    HANDLE t;
    DWORD t_id;
    t= CreateThread(NULL, 0, waitForInput, NULL, 0, &t_id);

    SOCKET socket_id;
    SOCKADDR_IN server_address;
    u_char buff[CHUNK_SIZE];
    int hc_size= 200 * CHUNK_SIZE;
    u_char* hugeBuff=(u_char*)malloc(hc_size*sizeof(u_char));

    int iResult = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVICE_PORT);
    server_address.sin_addr.s_addr= inet_addr(argv[1]);
    
    if((socket_id = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))<0){
        printf("Cannot create socket...\n");
        WSACleanup();
        return 1;
    }
    
    iResult = connect(socket_id, (SA*)&server_address, sizeof(server_address));
    if (iResult == SOCKET_ERROR) {
        closesocket(socket_id);
        socket_id = INVALID_SOCKET;
        WSACleanup();
        return 1;
    }

    FILE *f;

    EnterCriticalSection(&critical);
    f= fopen(localPath,"r");
    fseek(f,0,SEEK_END);
    length= ftell(f);
    fseek(f, 0,SEEK_SET);
    fclose(f);

    char mess[1024];
    memset(mess, '\0', 1024);

    long fn_len= strlen(fileName);
    long rp_len= strlen(relativePath);
    char* v1= (char*)&length;

    char* v2= (char*)&fn_len;
    char* v3= (char*)&rp_len;
    
    for(int i=0;i<8;i++){
        if(v1[i]=='\0')
            break;
        mess[i]=v1[i];
    }
    
    mess[8]=0x02;
    mess[9]=0x02;

    for(int i=0;i<8;i++){
        if(v2[i]=='\0')
            break;
        mess[i+10]=v2[i];
    }
        
    mess[18]=0x02;
    mess[19]=0x02;

    for(int i=0;i<8;i++){
        if(v3[i]=='\0')
            break;
        mess[i+20]=v3[i];
    }
    
    mess[28]=0x02;
    mess[29]=0x02;

    for(int i=0;i<fn_len;i++)
        mess[i+30]=fileName[i];

    mess[30+fn_len]=0x02;
    mess[31+fn_len]=0x02;

    for(int i=0;i<rp_len;i++)
        mess[i+32+fn_len]=relativePath[i];

    send(socket_id, mess, 1024, 0);

    int order=0;
    int cur;
    int suma=0;
    int r= CHUNK_SIZE;
    long ll= length;
    long offset=0;
    long s_len= hc_size;

    if(s_len > length)
        s_len= length;
    
    f= fopen(localPath,"rb");
    LeaveCriticalSection(&critical);

    while((cur = fread(hugeBuff, sizeof(char), s_len, f) >0)){

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
            memset(pack, '\0', CHUNK_SIZE+1024);
            chunk.getBytes(pack);

            send(socket_id, pack, CHUNK_SIZE+1024, 0);

            char response[12];
            recv(socket_id, response, 12, MSG_WAITALL);

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

            if(ll>r)
                ll-=r;
            else
                ll=0;

            if(ll <= CHUNK_SIZE)
                r=ll;
            else
                r= CHUNK_SIZE;

        }

        EnterCriticalSection(&critical);
        fullOffset+= offset;

        if(length-fullOffset >= hc_size)
            s_len= hc_size;
        else
            s_len= length-fullOffset;

        if(s_len<0)
            s_len=0; 

        LeaveCriticalSection(&critical);

        memset(hugeBuff, '\0', hc_size);
    }

    end:
    running= false;
    CloseHandle(t);
    free(hugeBuff);
    fclose(f);
    WSACleanup();
    closesocket(socket_id);
    DeleteCriticalSection(&critical);

    return 0;
}

DWORD WINAPI waitForInput(LPVOID action){

    char bb[256];
    memset(bb, '\0', 256);

    while(running){
        scanf("%s", &bb);
        strcat(bb, "\0");

        if(strcmp(bb,"read")==0){
            EnterCriticalSection(&critical);
            printf("%s\t%ld of %ld\n", localPath, fullOffset, length);
            LeaveCriticalSection(&critical);
        }else if(strcmp(bb, "cancel")==0){
            EnterCriticalSection(&critical);
            cancelation= true;
            LeaveCriticalSection(&critical);
        }

    }

    _endthread();
}
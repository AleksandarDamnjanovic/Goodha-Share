#include <stdio.h>
#include <stdlib.h>
#include "constants.h"
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <vector>
#include <string>
#include <algorithm>
#include <dirent.h>
#include <lmcons.h>

#define SA struct sockaddr

struct forSender{
    SOCKET sock;
    SOCKADDR_IN address;
};

DWORD WINAPI terminal(LPVOID ptr);
DWORD WINAPI sender(LPVOID sock);

void getCharIP_from_addr_in(SOCKADDR_IN address, char str[16]);

std::vector<std::string> servives;
CRITICAL_SECTION critical;

bool receiving= true;
bool communicating= true;
bool completed=false;
int port=-1;
SOCKET mainSocketID;
char hostName[256];

int main(int argc, char** argv){

    WSADATA wsadata;
    InitializeCriticalSectionAndSpinCount(&critical, 0x00000400);

    memset(hostName,'\0', 256);
    DWORD lll= 256;
    GetUserName(hostName, &lll);

    if(argc>1)
        for(int i=0;i<argc; i++)
            if(strcmp(argv[i],"-p")==0){
                sscanf(argv[i+1],"%d", &port);
                printf("Communication port set to: %d\n", port);
            }

    char buffer[512];

    int iResult = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    SOCKADDR_IN serverAddress, clientAddress;

    if((mainSocketID = socket(AF_INET, SOCK_DGRAM, 0))<0){
        printf("Cannot create socket...\n");
        WSACleanup();
        return 1;
    }

    ZeroMemory(&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family= AF_INET;
    serverAddress.sin_addr.s_addr= INADDR_ANY;
    if(port==-1)
        serverAddress.sin_port= htons(DETECTION_PORT);
    else
        serverAddress.sin_port= htons(port);

    iResult = bind(mainSocketID, (SA*)&serverAddress, sizeof(serverAddress));
    if (iResult == SOCKET_ERROR) {
        closesocket(mainSocketID);
        mainSocketID = INVALID_SOCKET;
        WSACleanup();
        return 1;
    }

    HANDLE t;
    DWORD communication;
    CreateThread(NULL, 0, terminal, NULL, 0, &communication);

    while(receiving){
        ZeroMemory(&clientAddress, sizeof(clientAddress));
        int len= sizeof(clientAddress);
        int n= recvfrom(mainSocketID, (char*)buffer, 512, 0,
        (struct sockaddr*)&clientAddress, (socklen_t*)&len);
        buffer[n]='\0';

        char str[INET_ADDRSTRLEN];
        char *ss= (char*)&str;
        memset(str, '\0', INET_ADDRSTRLEN);
        getCharIP_from_addr_in(clientAddress, ss);

        char record[512];
        memset(record, '\0', 512);
        strcpy(record, str);
        strcat(record, " ");

        char* name= strtok(buffer," ");
        name= strtok(NULL, " ");

        strcat(record, name);

        if((buffer[0]=='a' && buffer[1]=='s' && buffer[2]=='k') ||
        (buffer[0]=='a' && buffer[1]=='l' && buffer[2]=='i' && buffer[3]=='v' && buffer[4]=='e'))
            if(!std::count(servives.begin(), servives.end(), record)){
                EnterCriticalSection(&critical);
                servives.push_back(record);
                LeaveCriticalSection(&critical);
            }

            if(buffer[0]=='a' && buffer[1]=='s' && buffer[2]=='k'){
                char alive[6 + strlen(hostName)];
                strcpy(alive, "alive ");
                strcat(alive, hostName);
                SOCKET sock;
                sock= socket(AF_INET, SOCK_DGRAM, 0);
                SOCKADDR_IN addr;
                addr.sin_family = AF_INET;
                if(port==-1)
                    addr.sin_port= htons(DETECTION_PORT);
                else
                    addr.sin_port= htons(port);
                addr.sin_addr.s_addr = inet_addr(str);
                for(int i=0;i<3;i++){
                    sendto(sock, (const char *)alive, strlen(alive),
                    0, (SOCKADDR *) &addr,
                    sizeof(addr));
                }
                close(sock);
            }

    }

    end:

    CloseHandle(t);
    closesocket(mainSocketID);
    printf("Communication thread finished\n");
    printf("Socket closed\n");
    DeleteCriticalSection(&critical);
    return 0;
}

DWORD WINAPI terminal(LPVOID ptr){
    char buf[256];
    while(communicating){
        memset(buf,'\0',256);
        scanf("%s", &buf);
        if(strcmp(buf, "detect")==0){

            servives.clear();
            SOCKET socketID;
            SOCKADDR_IN serverAddress;

            socketID= socket(AF_INET, SOCK_DGRAM,0);

            if((socketID = socket(AF_INET, SOCK_DGRAM, 0))<0){
                printf("Cannot create socket...\n");
                _endthread();
            }else{
                forSender* sn;
                sn=(forSender*)malloc(sizeof(forSender));
                sn->sock= socketID;
                sn->address=serverAddress;

                HANDLE t;
                DWORD send_thread;
                t= CreateThread(NULL, 0, sender, sn, 0, &send_thread);
            }

        }else if(strcmp(buf, "kill")==0){
            receiving= false;
            communicating= false;
            printf("Detector killed by the user\n");
            close(mainSocketID);
            exit(0);
        }else if(strcmp(buf, "list")==0){
            for(int i=0;i<servives.size();i++){
                EnterCriticalSection(&critical);
                printf("%s\n", servives[i].c_str());
                LeaveCriticalSection(&critical);
            }
        }else if(strcmp(buf, "cancel")==0){
            printf("User have stopped scanning\n");
            completed= true;
        }

    }
    _endthread();
}

DWORD WINAPI sender(LPVOID sock){

    SOCKET socketID=((struct forSender*)sock)->sock;
    SOCKADDR_IN serverAddress=((struct forSender*)sock)->address;

    int a3=0, a4=0;
    char ask[5+ strlen(hostName)];
    memset(ask, '\0', strlen(ask));
    strcpy(ask, "ask ");
    strcat(ask, hostName);
    completed= false;

    printf("Scanning commenced\n");

    while(!completed){
        char ip[16];
        memset(ip, '\0', 16);
        sprintf(ip,"%d.%d.%d.%d", 192, 168, a3, a4);
        serverAddress.sin_family= AF_INET;
        serverAddress.sin_addr.s_addr= inet_addr(ip);

        if(port==-1)
            serverAddress.sin_port= htons(DETECTION_PORT);
        else
            serverAddress.sin_port= htons(port);

        for(int i=0;i<3;i++){
            sendto(socketID, (const char *)ask, strlen(ask),
            0, (SOCKADDR*) &serverAddress,
            sizeof(serverAddress));
        }

        a4+=1;
        if(a4==256){
            a3+=1;
            a4=0;
        }

        if(a3==256)
            completed=true;
    }

    free(sock);
    printf("Scanning finished\n");
    _endthread();
}

void getHex(char *hex, char *dest){

    char temp[2];

    for(int i=0; i<2;i++){
        char *ptr= hex+i;
        if(*ptr=='0')
            if(i==0)
                *dest= 0x00 | *dest;
            else
                *dest= 0x00 | *dest;
        else if(*ptr=='1')
            if(i==0)
                *dest= 0x10 | *dest;
            else
                *dest= 0x01 | *dest;
        else if(*ptr=='2')
            if(i==0)
                *dest= 0x20 | *dest;
            else
                *dest= 0x02 | *dest;
        else if(*ptr=='3')
            if(i==0)
                *dest= 0x30 | *dest;
            else
                *dest= 0x03 | *dest;
        else if(*ptr=='4')
            if(i==0)
                *dest= 0x40 | *dest;
            else
                *dest= 0x04 | *dest;
        else if(*ptr=='5')
            if(i==0)
                *dest= 0x50 | *dest;
            else
                *dest= 0x05 | *dest;
        else if(*ptr=='6')
            if(i==0)
                *dest= 0x60 | *dest;
            else
                *dest= 0x06 | *dest;
        else if(*ptr=='7')
            if(i==0)
                *dest= 0x70 | *dest;
            else
                *dest= 0x07 | *dest;
        else if(*ptr=='8')
            if(i==0)
                *dest= 0x80 | *dest;
            else
                *dest= 0x08 | *dest;
        else if(*ptr=='9')
            if(i==0)
                *dest= 0x90 | *dest;
            else
                *dest= 0x09 | *dest;
        else if(*ptr=='a')
            if(i==0)
                *dest= 0xa0 | *dest;
            else
                *dest= 0x0a | *dest;
        else if(*ptr=='b')
            if(i==0)
                *dest= 0xb0 | *dest;
            else
                *dest= 0x0b | *dest;
        else if(*ptr=='c')
            if(i==0)
                *dest= 0xc0 | *dest;
            else
                *dest= 0x0c | *dest;
        else if(*ptr=='d')
            if(i==0)
                *dest= 0xd0 | *dest;
            else
                *dest= 0x0d | *dest;
        else if(*ptr=='e')
            if(i==0)
                *dest= 0xe0 | *dest;
            else
                *dest= 0x0e | *dest;
        else if(*ptr=='f')
            if(i==0)
                *dest= 0xf0 | *dest;
            else
                *dest= 0x0f | *dest;

    }

}

void getCharIP_from_addr_in(SOCKADDR_IN address, char* str){
    char ip[8];
    memset(ip,'\0',8);
    in_addr b= address.sin_addr;
    sprintf(ip, "%x", b);

    memset(str, '\0', 16);
    char one[3];
    one[2]='\0';
    char sec[2];
    sec[1]='\0';
    int ip1=0, ip2=0, ip3=0, ip4=0;

    if(ip[7]=='\0'){

        one[0]=0x00;
        one[1]=ip[0];
        getHex(one, &sec[0]);

        int* ip11= (int*)&sec[0];
        *ip11= *ip11 & 0x000000ff;
        ip1= *ip11;
        memset(sec,'\0', 2);

        one[0]=ip[1];
        one[1]=ip[2];
        getHex(one, &sec[0]);

        int* ip22= (int*)&sec[0];
        *ip22= *ip22 & 0x000000ff;
        ip2= *ip22;
        memset(sec,'\0', 2);

        one[0]=ip[3];
        one[1]=ip[4];
        getHex(one, &sec[0]);

        int* ip33= (int*)&sec[0];
        *ip33= *ip33 & 0x000000ff;
        ip3= *ip33;
        memset(sec,'\0', 2);
        one[0]=ip[5];
        one[1]=ip[6];
        getHex(one, &sec[0]);

        int* ip44= (int*)&sec[0];
        *ip44= *ip44 & 0x000000ff;
        ip4= *ip44;

    }else{

        one[0]=ip[0];
        one[1]=ip[1];
        getHex(one, &sec[0]);

        int* ip11= (int*)&sec[0];
        *ip11= *ip11 & 0x000000ff;
        ip1= *ip11;
        memset(sec,'\0', 2);

        one[0]=ip[2];
        one[1]=ip[3];
        getHex(one, &sec[0]);

        int* ip22= (int*)&sec[0];
        *ip22= *ip22 & 0x000000ff;
        ip2= *ip22;
        memset(sec,'\0', 2);

        one[0]=ip[4];
        one[1]=ip[5];
        getHex(one, &sec[0]);

        int* ip33= (int*)&sec[0];
        *ip33= *ip33 & 0x000000ff;
        ip3= *ip33;
        memset(sec,'\0', 2);
        one[0]=ip[6];
        one[1]=ip[7];
        getHex(one, &sec[0]);

        int* ip44= (int*)&sec[0];
        *ip44= *ip44 & 0x000000ff;
        ip4= *ip44;
    }

    sprintf(str, "%d.%d.%d.%d", ip4, ip3, ip2, ip1);
}

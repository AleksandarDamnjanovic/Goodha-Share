#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include "chunk.hpp"
#include "constants.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <string>

typedef struct{
    int socket;
    char* fileName;
    char* relativePath;
    long length;
    long transferred;
    char ip[16];
    HANDLE thread;
}report;


DWORD WINAPI comThread(LPVOID ptr2);
DWORD WINAPI waitInput(LPVOID ptr1);
DWORD WINAPI engine(LPVOID ptr);
void resend(SOCKET conf, int order_num);
void populateFilter(char* fileAddress);

void getHex(char *hex, char *dest);
std::vector<report*>clients;
std::vector<char*>allowed;
std::vector<char*>blocked;
#define SA struct sockaddr
char buff[CHUNK_SIZE+1024];
bool run= true;
HANDLE generator;
SOCKET listener;
char* r_path;
CRITICAL_SECTION critical;

int main(int argc, char** argv){

    InitializeCriticalSectionAndSpinCount(&critical, 0x00000400);

    for(int i=0;i<argc;i++)
        if(argv[i][0]=='-' && argv[i][1]=='f')
            populateFilter(argv[i]);
        else if(argv[i][0]=='-' && argv[i][1]=='r')
            r_path= argv[i];
    
    struct addrinfo server_address;
    struct addrinfo *info= NULL;

    WSADATA wsadata;
    int iResult;

    iResult= WSAStartup(MAKEWORD(2,2), &wsadata);

    if(iResult!=0){
        printf("WSA Startup failded\n");
        return 1;
    }

    ZeroMemory(&server_address, sizeof(server_address));
    server_address.ai_family= AF_INET;
    server_address.ai_socktype= SOCK_STREAM;
    server_address.ai_protocol= IPPROTO_TCP;
    server_address.ai_flags= AI_PASSIVE;

    iResult= getaddrinfo(NULL, SERVICE_PORT_CHAR, &server_address, &info);

    if(iResult!=0){
        printf("Unable to get address info\n");
        printf("getaddrinfo failed with error: %d\n", info);
        WSACleanup();
        return 1;
    }

    listener = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (listener == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(info);
        WSACleanup();
        return 1;
    }

    iResult = bind(listener, info->ai_addr, (int)info->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(info);
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(info);

    iResult = listen(listener, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    printf("Into the loop:\n");

    HANDLE input;
    DWORD inputThreadID;
    input= CreateThread(NULL, 0, waitInput, NULL, 0, &inputThreadID);
    
    DWORD generatorThreadID;
    generator= CreateThread(NULL, 0, engine, (void*)&listener, 0, &generatorThreadID);
    
    while(run)
        sleep(1);

    WSACleanup();
    closesocket(listener);
    DeleteCriticalSection(&critical);

    return 0;
}

DWORD WINAPI comThread(LPVOID ptr1){
    SOCKET conf= (SOCKET)ptr1;

    report* r;

    EnterCriticalSection(&critical);
    for(int i=0;i<clients.size();i++)
        if(clients[i]->socket== (int)conf)
            r=clients[i];
    LeaveCriticalSection(&critical);

    char mess[1024];
    recv(conf, mess, 1024, MSG_WAITALL);

    char *ptr;
    char c_len[8]={mess[0], mess[1], mess[2], mess[3], mess[4], mess[5], mess[6], mess[7]};
    char fn_len[8]={mess[10], mess[11], mess[12], mess[13], mess[14], mess[15], mess[16], mess[17]};
    char rp_len[8]={mess[20], mess[21], mess[22], mess[23], mess[24], mess[25], mess[26], mess[27]};

    long* content_length= (long*)&c_len;
    long* fn_length= (long*)&fn_len;
    long* rp_length= (long*)&rp_len;

    char fileName[*fn_length+1];
    memset(fileName,'\0', *fn_length);
    for(int i=0;i<*fn_length;i++)
        fileName[i]= mess[30+i];

    fileName[*fn_length]='\0';

    char relativePath[*rp_length];
    memset(relativePath, '\0', *rp_length);
    for(int i=0;i<*rp_length;i++)
        relativePath[i]= mess[32+*fn_length+i];

    relativePath[*rp_length]='\0';    

    EnterCriticalSection(&critical);
    r->relativePath= relativePath;
    r->fileName= fileName;

    if(r_path!=0)
        r_path+=2;
    
    char f_name[(r_path==NULL?1:strlen(r_path)*sizeof(char))
    +strlen(r->relativePath)*sizeof(char)
    +strlen(r->fileName)*sizeof(char)];

    memset(f_name,'\0',strlen(f_name)*sizeof(char));
    strcpy(f_name, r_path==NULL?".":r_path);
    strcat(f_name, r->relativePath);
    strcat(f_name, r->fileName);
    LeaveCriticalSection(&critical);

    bool win= strstr(f_name, "/")==NULL?true:false;    
    int counter= 0;

    char limit[sizeof(char)*2];
    if(win)
        strcpy(limit, "\\");
    else
        strcpy(limit, "/");

    strcat(limit,"\0");
    
    for(int i=0; i<strlen(f_name);i++){
        if(limit[0]==f_name[i]){
            counter= i;
            char part[i+1];
            memset(part, '\0', strlen(part));
            for(int o=0;o<counter;o++)
                part[o]=f_name[o];

            part[counter]='\0';

            if(access(part, 0x00)!=0)
                mkdir(part);
        }
        
    }

    std::remove(f_name);
    HANDLE f;
    wchar_t* nn= (wchar_t*)f_name;
    f= CreateFileA(f_name, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    int n;
    int suma=0;
    DWORD size;

    char uul1[100];
    sprintf(uul1,"%d\0", *content_length);
    unsigned long ul1= std::strtoul(uul1, NULL, 0);
    DWORD len1= (DWORD)ul1;

    if(*content_length>=CHUNK_SIZE+1024)
        size= (DWORD)(CHUNK_SIZE+1024);
    else
        size= len1;

    EnterCriticalSection(&critical);
    r->length= *content_length;
    LeaveCriticalSection(&critical);

    int order_num=0;
    int order=0;
    loop:
    order=0;
    memset(buff,'\0', CHUNK_SIZE+1024);
    
    while(recv(conf, buff, CHUNK_SIZE+1024, MSG_WAITALL)){

        Chunk chunk(buff);

        if(buff[0]!=0x01 && buff[1]!=0x02){
            resend(conf, order_num);
            goto loop;
        }

        order= chunk.getOrderNumber();

        if(order!= order_num){
            resend(conf, order_num);
            goto loop;
        }
            
        int contentLength= chunk.getContentLength();
        char finalChunk[contentLength];
        chunk.getContent(finalChunk);

        LPDWORD sent=0;
        char uul[100];
        sprintf(uul,"%d\0",contentLength);
        unsigned long ul= std::strtoul(uul, NULL, 0);
        DWORD len= (DWORD)ul;
        int toSend=0;
        if(size==CHUNK_SIZE+1024)
            toSend= CHUNK_SIZE;
        else
            toSend= size;
        BOOL yes= WriteFile(f, finalChunk, toSend, sent, NULL);
        suma+=contentLength;

        char response[12];
        response[0]=0x0a;
        response[1]=0x0a;
        response[2]=0x0a;
        response[3]=0x0a;
        char* l= (char*)&order;
        response[4]=l[0];
        response[5]=l[1];
        response[6]=l[2];
        response[7]=l[3];
        response[8]=0x0a;
        response[9]=0x0a;
        response[10]=0x0a;
        response[11]=0x0a;
        send(conf, response, 12, 0);

        if(*content_length-suma<CHUNK_SIZE+1024)
            size=*content_length-suma;
        else
            size=CHUNK_SIZE+1024;

        order_num+=1;

        EnterCriticalSection(&critical);
        r->transferred= suma;
        LeaveCriticalSection(&critical);

        if(*content_length<=suma){
            printf("\n");
            goto end;
        }
            
    }

    end:

    int iResult = shutdown(conf, SD_SEND);
    if (iResult == SOCKET_ERROR)
        printf("shutdown failed with error: %d\n", WSAGetLastError());

    CloseHandle(f);
    memset(buff, '\0', 1024);
    
    printf("final\n");

    int e=-1;
    EnterCriticalSection(&critical);
    printf("%s\t%s\ttransfer completed\n", r->relativePath, r->fileName);
    for(int i=0;i<clients.size();i++)
        if(clients[i]->socket== (int)conf)
            e= i;

    LeaveCriticalSection(&critical);

    if(e!=-1)
        clients.erase(clients.begin()+e);

    closesocket(conf);
    free(r);
    _endthread();
}

bool c_flag= false;
bool ip_flag= false;
bool ip_rem_flag= false;
bool path_flag= false;
DWORD WINAPI waitInput(LPVOID ptr2){

    char bb[256];
    memset(bb,'\0',256);
    strcpy(bb,"");

    while(true){
        scanf("%[^\n]%*c",&bb);
        strcat(bb,"\0");

        if(strcmp(bb, "exit")==0){
            memset(bb,'\0',256);
            strcpy(bb,"");
            run= false;
            CloseHandle(generator);

            for(int i=0;i<clients.size();i++)
                close(clients[i]->socket);

            close(listener);
            printf("Application ended properly...\n");
            goto exit;
        }else if(strcmp(bb, "read")==0){
            EnterCriticalSection(&critical);
            for(int i=0;i<clients.size();i++){
                printf("%s\t%s\t%s\t%d of %d\n",
                clients[i]->ip,
                 clients[i]->relativePath,
                  clients[i]->fileName,
                   clients[i]->transferred,
                    clients[i]->length);
            }
            LeaveCriticalSection(&critical);
        }else if(strcmp(bb, "cancel")==0){
            c_flag= true;
            printf("Please, enter ip address of client you wish to break communication with:\n");
        }else if(strcmp(bb, "ip add")==0){
            ip_flag= true;
            printf("To change settings, please, enter:\n\t[allow or block] ip_address\n");
        }else if(strcmp(bb, "ip remove")==0){
            ip_rem_flag= true;
            printf("To remove ip from filter, please, enter:\n\tremove ip_address\n");
        }else if(strcmp(bb, "path")==0){
            path_flag= true;
            printf("To reset relative path, please, enter new path.\n");
        }else if(strcmp(bb, "filter")==0){
            for(int i=0;i<allowed.size()>0?allowed.size():blocked.size();i++)
                printf("%s\t%s\n",allowed.size()>0?"allowed ":"block ",
                allowed.size()>0?allowed[i]:blocked[i]);
        }else{
            if(c_flag){
                EnterCriticalSection(&critical);

                for(report* r:clients)
                    if(strcmp(r->ip,bb)==0){
                        CloseHandle(r->thread);
                        close(r->socket);
                    }
                    
                LeaveCriticalSection(&critical);
                c_flag= false;
            }else if(ip_flag){

                char* direction= strtok(bb," ");
                char* val= strtok(NULL, " ");
                char* vall=(char*)malloc(strlen(val) * sizeof(char));
                memset(vall, '\0', strlen(val)* sizeof(char));
                strcpy(vall, val);

                if(strcmp(direction, "allow")==0)
                    if(allowed.size()>0)
                        allowed.push_back(vall);
                    else
                        printf("Address filter is in block mode\n");
                else if(strcmp(direction, "block")==0)
                    if(blocked.size()>0)
                        blocked.push_back(vall);
                    else
                        printf("Address filter is in allow mode\n");

                ip_flag= false;
            }else if(ip_rem_flag){

                char* val= strtok(bb, " ");
                char* vall= strtok(NULL, " ");

                if(strcmp(val, "remove")!=0)
                    printf("Wrong command\n");
                else{

                    int index=-1;
                    for(int i=0;i<allowed.size()>0?allowed.size():blocked.size();i++)
                        if(strcmp(allowed.size()>0?allowed[i]:blocked[i], vall)==0)
                            index=i;

                    if(index!=-1)
                        allowed.size()>0?allowed.erase(allowed.begin()+index):
                        blocked.erase(blocked.begin()+index);

                }

                ip_rem_flag= false;
            }else if(path_flag){

                if(r_path!=NULL)
                    free(r_path);

                r_path= (char*)malloc((strlen(bb)+2)*sizeof(char));
                strcpy(r_path,"-r");
                strcat(r_path, bb);

                printf("Your path is set to: %s\n", r_path);
                path_flag= false;
            }

        }

    }

    exit:
    _endthread();
}

int test=0;
DWORD WINAPI engine(LPVOID ptr){
    
    while(run){
        bool jump= false;
        SOCKADDR_IN address={0};
        int addressSize= sizeof(address);

        SOCKET conf = accept((SOCKET)listener, (struct sockaddr*)&address, &addressSize);
        if (conf == INVALID_SOCKET) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            closesocket(listener);
            WSACleanup();
            _endthread();
        }

        char ip[8];
        memset(ip,'\0',8);
        in_addr b= address.sin_addr;
        sprintf(ip, "%x", b);
        
        char str[16];
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
        
        if(allowed.size()!=0){
            bool free= false;
            for(char* s: allowed){
                if(strcmp(s, str)==0){
                    free=true;
                    goto sol;
                }
            }
            
            sol:
            if(!free){
                close(conf);
                jump= true;;
            }

        }else if(blocked.size()!=0){
            for(char* s: blocked)
                if(strcmp(s, str)==0){
                    close(conf);
                    jump= true;
                }
        }

        if(!jump){
            HANDLE thread;
            DWORD threadID;
            report* r=(report*)malloc(sizeof(report));
            r->socket=conf;
            strcpy(r->ip, str);
            clients.push_back(r);
            test= conf;
            thread= CreateThread(NULL, 0, comThread, (void*)conf, 0, &threadID);
            r->thread= thread;
        }
        jump= false;
    }

    _endthread();
}


void resend(SOCKET conf, int order_num){
    char response[12];
    response[0]=0x0b;
    response[1]=0x0b;
    response[2]=0x0b;
    response[3]=0x0b;
    char* l= (char*)&order_num;
    response[4]=l[0];
    response[5]=l[1];
    response[6]=l[2];
    response[7]=l[3];
    response[8]=0x0b;
    response[9]=0x0b;
    response[10]=0x0b;
    response[11]=0x0b;
    send(conf, response, 12, 0);
}

void populateFilter(char* address){
    FILE *f;
    f= fopen(address+2, "r");
    fseek(f, 0, SEEK_END);
    long len= ftell(f);
    fseek(f, 0, SEEK_SET);
    char f_file[len+1];
    fread(f_file, len, sizeof(char), f);
    f_file[len]= '\0';
    fclose(f);

    char* line;
    line= strtok(f_file, "\n");
    bool allow= true;
    std::vector<char*> lines;
    while(line!=NULL){
        lines.push_back(line);
        line= strtok(NULL, "\n");
    }

    for(int i=0;i<lines.size();i++){
        char* token;
        token= strtok(lines[i], " ");

        while(token!=NULL){
            char* el= (char*)malloc(strlen(token) * sizeof(char));
            memset(el,'\0', strlen(token) + sizeof(char));
            strcpy(el, token);

            if(strcmp(token, "allow")==0)
                allow= true;
            else if(strcmp(token, "block")==0)
                allow= false;
            if(strcmp(token, "allow")!=0 && strcmp(token, "block")!=0)
                if(allow)
                    allowed.push_back(el);
                else
                    blocked.push_back(el);

            token= strtok(NULL, " ");
        }

    }

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
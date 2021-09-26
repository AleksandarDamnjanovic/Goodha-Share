#include <stdio.h>
#include <stdlib.h>
#include "constants.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <vector>
#include <string>
#include <algorithm>
#include <dirent.h>

struct forSender{
    int sock;
    sockaddr_in address;
};

void* terminal(void* ptr);
void* sender(void* sock);

std::vector<std::string> servives;
pthread_mutex_t lock;

bool receiving= true;
bool communicating= true;
bool completed=false;
int port=-1;
int mainSocketID;
char* hostName;

int main(int argc, char** argv){

    hostName= (char*)malloc(sizeof(char) * 256);
    gethostname(hostName, 256);

    if(argc>1)
        for(int i=0;i<argc; i++)
            if(strcmp(argv[i],"-p")==0){
                sscanf(argv[i+1],"%d", &port);
                printf("Communication port set to: %d\n", port);    
            }

    char buffer[512];
    struct sockaddr_in serverAddress, clientAddress;

    mainSocketID= socket(AF_INET, SOCK_DGRAM,0);

    if(mainSocketID<0){
        printf("Can't create socket\n");
        goto end;
    }else
        printf("Socket created\n");

    memset(&serverAddress, '\0', sizeof(serverAddress));

    serverAddress.sin_family= AF_INET;
    serverAddress.sin_addr.s_addr= INADDR_ANY;
    if(port==-1)
        serverAddress.sin_port= htons(DETECTION_PORT);
    else
        serverAddress.sin_port= htons(port);

    if(bind(mainSocketID, (const struct sockaddr*)&serverAddress, sizeof(serverAddress))<0){
        printf("Can't bind port with address\n");
        goto end;
    }else
        printf("Bind process successful\n");

    pthread_t communication;
    pthread_create(&communication, NULL, &terminal, NULL);

    while(receiving){
        memset(&clientAddress, '\0', sizeof(clientAddress));
        int len= sizeof(clientAddress);
        int n= recvfrom(mainSocketID, (char*)buffer, 512, MSG_WAITALL, 
        (struct sockaddr*)&clientAddress, (socklen_t*)&len);
        buffer[n]='\0';

        char str[INET_ADDRSTRLEN];
        memset(str, '\0', INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &clientAddress.sin_addr, str, INET_ADDRSTRLEN);

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
                pthread_mutex_lock(&lock);
                servives.push_back(record);
                pthread_mutex_unlock(&lock);
            }

            if(buffer[0]=='a' && buffer[1]=='s' && buffer[2]=='k'){
                char alive[6 + strlen(hostName)];
                strcpy(alive, "alive ");
                strcat(alive, hostName);
                int sock;
                sock= socket(AF_INET, SOCK_DGRAM, 0);
                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                if(port==-1)
                    addr.sin_port= htons(DETECTION_PORT);
                else
                    addr.sin_port= htons(port);
                addr.sin_addr.s_addr = inet_addr(str);
                for(int i=0;i<3;i++){
                    sendto(sock, (const char *)alive, strlen(alive),
                    MSG_CONFIRM, (const struct sockaddr *) &addr, 
                    sizeof(addr));  
                }
                close(sock);          
            }
      
    }

    end:

    free(hostName);
    close(mainSocketID);
    pthread_cancel(communication);
    printf("Communication thread finished\n");
    printf("Socket closed\n");
    return 0;
}

void* terminal(void* ptr){
    char buf[256];
    while(communicating){
        memset(buf,'\0',256);
        scanf("%s", &buf);
        if(strcmp(buf, "detect")==0){

            servives.clear();
            int socketID;
            struct sockaddr_in serverAddress;

            socketID= socket(AF_INET, SOCK_DGRAM,0);

            if(socketID<0){
                printf("Communication error. Can't establish connection with remove service.\n");
                close(socketID);
            }else{
                forSender* sn;
                sn=(forSender*)malloc(sizeof(forSender));
                sn->sock= socketID;
                sn->address=serverAddress;

                pthread_t send_thread;
                pthread_create(&send_thread, NULL, &sender, sn);
            }

        }else if(strcmp(buf, "kill")==0){
            receiving= false;
            communicating= false;
            printf("Detector killed by the user\n");
            close(mainSocketID);
            exit(0);
        }else if(strcmp(buf, "list")==0){
            for(int i=0;i<servives.size();i++){
                pthread_mutex_lock(&lock);
                printf("%s\n", servives[i].c_str());
                pthread_mutex_unlock(&lock);
            }    
        }else if(strcmp(buf, "cancel")==0){
            printf("User have stopped scanning\n");
            completed= true;
        }

    }
    return NULL;
}

void* sender(void* sock){

    int socketID=((struct forSender*)sock)->sock;
    sockaddr_in serverAddress=((struct forSender*)sock)->address;

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
            MSG_CONFIRM, (const struct sockaddr *) &serverAddress, 
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
    return NULL;
}
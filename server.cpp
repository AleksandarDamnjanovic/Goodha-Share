#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "chunk.hpp"
#include "constants.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct{
    int socket;
    char* fileName;
    char* relativePath;
    long length;
    long transferred;
    char ip[16];
    pthread_t thread;
}report;

void* comThread(void* client);
void* waitInput(void* ptr);
void* engine(void* ptr);
void resend(int* conf, int order_num);
void accepted(int* conf, int order);
void populateFilter(char* fileAddress);

std::vector<report*>clients;
std::vector<char*>allowed;
std::vector<char*>blocked;
#define SA struct sockaddr
char buff[CHUNK_SIZE+1024];
bool run= true;
pthread_t generator;
pthread_mutex_t lock;
int listener;
char* r_path;

int main(int argc, char** argv){

    for(int i=0;i<argc;i++)
        if(argv[i][0]=='-' && argv[i][1]=='f')
            populateFilter(argv[i]);
        else if(argv[i][0]=='-' && argv[i][1]=='r')
            r_path= argv[i];
    
    struct sockaddr_in server_address;

    if((listener= socket(AF_INET, SOCK_STREAM, 0))<0)
        printf("No connection...\n");

    bzero(&server_address, sizeof(server_address));
    server_address.sin_family= AF_INET;
    server_address.sin_addr.s_addr= htonl(INADDR_ANY);
    server_address.sin_port= htons(SERVICE_PORT);

    if((bind(listener, (SA*) &server_address, sizeof(server_address)))<0)
        printf("Cannot bind...\n");

    if((listen(listener, 10))!=0)
        printf("Cannot connect...\n");

    printf("Into the loop:\n");

    pthread_t input;
    pthread_create(&input, NULL, waitInput, NULL);
    
    pthread_create(&generator, NULL, engine, (void*)&listener);
    pthread_exit(NULL);

    close(listener);

    return 0;
}

void* comThread(void* client){
    int* conf= (int*)client;

    report* r;

    pthread_mutex_lock(&lock);
    for(int i=0;i<clients.size();i++)
        if(clients[i]->socket== *conf)
            r=clients[i];
    pthread_mutex_unlock(&lock);

    char mess[1024];
    recv(*conf, mess, 1024, MSG_WAITALL);

    char *ptr;
    char c_len[8]={mess[0], mess[1], mess[2], mess[3], mess[4], mess[5], mess[6], mess[7]};
    char fn_len[8]={mess[10], mess[11], mess[12], mess[13], mess[14], mess[15], mess[16], mess[17]};
    char rp_len[8]={mess[20], mess[21], mess[22], mess[23], mess[24], mess[25], mess[26], mess[27]};

    long* content_length= (long*)&c_len;
    long* fn_length= (long*)&fn_len;
    long* rp_length= (long*)&rp_len;

    if(*content_length==0 || *fn_len==0 || rp_len==0){

        memset(buff, '\0', CHUNK_SIZE + 1024);
    
        int e=-1;
        pthread_mutex_lock(&lock);
        printf("%s\t%s\tcommunication broken\n", r->relativePath, r->fileName);
        for(int i=0;i<clients.size();i++)
            if(clients[i]->socket== *conf)
                e= i;

        pthread_mutex_unlock(&lock);

        if(e!=-1)
            clients.erase(clients.begin()+e);

        close(*conf);
        free(r);
        pthread_exit(NULL);
    }
        
    char fileName[*fn_length+1];
    memset(fileName,'\0', strlen(fileName));
    for(int i=0;i<*fn_length;i++)
        fileName[i]= mess[30+i];

    fileName[*fn_length]='\0';

    if(fileName[strlen(fileName)-1]=='/' || fileName[strlen(fileName)-1]=='\\')
        fileName[strlen(fileName)-1]='\0';

    char relativePath[*rp_length+1];
    memset(relativePath, '\0', *rp_length+1);
    for(int i=0;i<*rp_length;i++)
        relativePath[i]= mess[32+*fn_length+i];

    relativePath[*rp_length]='\0';    

    pthread_mutex_lock(&lock);
    r->relativePath= relativePath;
    r->fileName= fileName;

    if(r_path!=0)
        r_path+=2;
    
    char f_name[(r_path==NULL || *r_path=='/' ?1:strlen(r_path)*sizeof(char))
    +strlen(r->relativePath)*sizeof(char)
    +strlen(r->fileName)*sizeof(char)];

    memset(f_name,'\0',strlen(f_name)*sizeof(char));
    strcpy(f_name, r_path==NULL || *r_path=='/'?".":r_path);
    strcat(f_name, r->relativePath);
    strcat(f_name, r->fileName);
    pthread_mutex_unlock(&lock);

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

            if(access(part, F_OK)!=0)
                mkdir(part,0777);
        }
        
    }

    remove(f_name);
    FILE *f;
    f= fopen(f_name,"ab");

    if(f==NULL){
        memset(buff, '\0', 1024);
    
    int e=-1;
    pthread_mutex_lock(&lock);
    printf("%s\t%s\ttransfer completed\n", r->relativePath, r->fileName);
    for(int i=0;i<clients.size();i++)
        if(clients[i]->socket== *conf)
            e= i;

    pthread_mutex_unlock(&lock);

    if(e!=-1)
        clients.erase(clients.begin()+e);

    close(*conf);
    free(r);
    pthread_exit(NULL);
    }
        
    int n;
    int suma=0;
    long size= CHUNK_SIZE+1024;

    pthread_mutex_lock(&lock);
    r->length= *content_length;
    pthread_mutex_unlock(&lock);

    int order_num=0;
    int order=0;
    loop:
    order=0;
    memset(buff,'\0',CHUNK_SIZE+1024);
    
    while((n= recv(*conf, buff, CHUNK_SIZE+1024, MSG_WAITALL))>0){

        Chunk chunk(buff, CHUNK_SIZE+1024);

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
        if(*content_length<CHUNK_SIZE+1024)
            contentLength=*content_length;
        char finalChunk[contentLength];
        chunk.getContent(finalChunk);

        if(content_length==0 || f==0)
            printf("here\n");

        int sent= fwrite(finalChunk, contentLength, sizeof(char), f);
        suma+=contentLength;

        accepted(conf, order);

        if(*content_length-suma<CHUNK_SIZE+1024)
            size=*content_length-suma;
        else
            size=CHUNK_SIZE+1024;

        order_num+=1;

        pthread_mutex_lock(&lock);
        r->transferred= suma;
        pthread_mutex_unlock(&lock);

        if(*content_length<=suma)
            goto end;

        chunk.clear();
    }

    end:
    fclose(f);
    memset(buff, '\0', 1024);
    
    int e=-1;
    pthread_mutex_lock(&lock);
    printf("%s\t%s\ttransfer completed\n", r->relativePath, r->fileName);
    for(int i=0;i<clients.size();i++)
        if(clients[i]->socket== *conf)
            e= i;

    pthread_mutex_unlock(&lock);

    if(e!=-1)
        clients.erase(clients.begin()+e);

    close(*conf);
    free(r);
    pthread_exit(NULL);
}

bool c_flag= false;
bool ip_flag= false;
bool ip_rem_flag= false;
bool path_flag= false;
void* waitInput(void* ptr){

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
            pthread_cancel(generator);

            for(int i=0;i<clients.size();i++)
                close(clients[i]->socket);

            close(listener);
            printf("Application ended properly...\n");
            goto exit;
        }else if(strcmp(bb, "read")==0){
            pthread_mutex_lock(&lock);
            for(int i=0;i<clients.size();i++){
                printf("%s\t%s\t%s\t%d of %d\n",
                clients[i]->ip,
                 clients[i]->relativePath,
                  clients[i]->fileName,
                   clients[i]->transferred,
                    clients[i]->length);
            }
            pthread_mutex_unlock(&lock);
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
                pthread_mutex_lock(&lock);

                for(report* r:clients)
                    if(strcmp(r->ip,bb)==0){
                        pthread_cancel(r->thread);
                        pthread_join(r->thread, NULL);
                        close(r->socket);
                    }
                    
                pthread_mutex_unlock(&lock);
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
    pthread_exit(NULL);
    exit(0);
}

void* engine(void* ptr){
    
    int* listener= (int*)ptr;
    while(run){
        bool jump= false;
        struct sockaddr_in addr;
        socklen_t addr_len;

        int conf= accept(*listener,(SA*) &addr, &addr_len);

        char str[INET_ADDRSTRLEN];
        memset(str, '\0', INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &addr.sin_addr, str, INET_ADDRSTRLEN);
            
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
            pthread_t thread;
            report* r=(report*)malloc(sizeof(report));
            r->socket=conf;
            strcpy(r->ip, str);
            r->thread= thread;
            clients.push_back(r);
            pthread_create(&thread, NULL, comThread, (void*)&conf);
        }
        jump= false;
    }

    pthread_exit(NULL);
}

void resend(int* conf, int order_num){
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
    write(*conf, response, 12);
}

void accepted(int* conf, int order){
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
    write(*conf, response, 12);
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
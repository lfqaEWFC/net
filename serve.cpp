#include<iostream>
#include<string.h>
#include"Threadpool.hpp"
#include"inetsockets_tcp.hpp"

using namespace std;

#define PORT_NUM "50000"
#define BACKLOG 50

struct addrinfo *hints;
struct addrinfo *result,*rp;
int lfd = 0,cfd = 0;

typedef struct outputarg{
    int cfd;
    sockaddr_storage claddr;
    socklen_t claddrlen = sizeof(claddr);
}outputarg;

void* output(void* arg){

    outputarg *output = (outputarg*)arg;
    int outcfd = output->cfd;
    char *recvbuf;
    recvbuf = new char[MAXBUF];
    char sendbuf[MAXBUF]="";

    strcpy(sendbuf,"serve : hello client");
    write(outcfd,sendbuf,strlen(sendbuf));

    address_str_portnum(recvbuf,MAXBUF,(sockaddr*)&output->claddr,output->claddrlen);
    cout << "recvice from : " << recvbuf << endl;

    close(outcfd);
    free(recvbuf);
    return NULL;
}

int main(){

    outputarg *args;
    args = new outputarg;
    lfd = inetlisten(PORT_NUM);
    if(lfd == -1){
        perror("inetlisten");
        return 0;
    }
    pool threadspool(5);

    for(;;){
        args->cfd = accept(lfd,(sockaddr*)&args->claddr,&args->claddrlen);
        if(args->cfd == -1){
            perror("accept");
            exit(EXIT_FAILURE);
        }
        threadspool.addtask(output,args);
    }

    return 0;

}
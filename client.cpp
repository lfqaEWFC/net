#include<iostream>
#include<string.h>
#include"Threadpool.hpp"
#include"inetsockets_tcp.hpp"

using namespace std;

#define PORT_NUM "50000"
#define BACKLOG 50

char recvbuf[MAXBUF]="";
char sendbuf[MAXBUF]=""; 
int cfd = 0,numread = 0;
char *local_ip = new char[MAXBUF];

int main(){

    get_local_ip(local_ip);
    cfd = inetconnect(local_ip,PORT_NUM);
    if(cfd == -1){
        perror("innetconnect");
        return 0;
    };

    numread = read(cfd,recvbuf,MAXBUF);
    cout << recvbuf << endl;

    close(cfd);
    return 0;

}
#include"inetsockets_tcp.hpp"
#include<unistd.h>
#include<fcntl.h>
#include<string.h>
#include<iostream>
#include<sys/stat.h>
#include<sys/sendfile.h>

using namespace std;

#define PORTNUM "2100"
#define CHUNK_SIZE (1024*1024)

int main(){

    int ctrl_cfd,data_cfd;
    char client_num[10];
    char* outputcmd;
    char* inputbuf = new char[MAXBUF];
    int recvlen = 0;
    outputcmd = new char[MAXBUF];
    char *local_ip = new char[MAXBUF];
    bool checkflag = true;

    get_local_ip(local_ip);
    ctrl_cfd = inetconnect(local_ip,PORTNUM);
    if(ctrl_cfd == -1){
        perror("inetconnect");
        return 0;
    }
    recv(ctrl_cfd,inputbuf,MAXBUF,0);
    strcpy(client_num,inputbuf);
    memset(inputbuf,0,MAXBUF);

    send(ctrl_cfd,"PASV",strlen("PASV")+1,0);
    cout << "PASV" << endl;
    recvlen = recv(ctrl_cfd,inputbuf,MAXBUF,0);
    if(recvlen == 0){
        perror("recv");
        return 0;
    }
    cout << inputbuf << endl;
    char delimiter[4]="(),";
    int cnt = 0;
    char **in_token = new char*[10];
    for(int i=0;i<10;i++){
        in_token[i] = new char[64];
    }
    char *token = strtok(inputbuf,delimiter);
    while(token != NULL){
        in_token[cnt] = token;
        cnt++; 
        token = strtok(NULL,delimiter);
    }
    char *data_ip = new char[MAXBUF];
    sprintf(data_ip,"%s.%s.%s.%s",in_token[1],in_token[2],in_token[3],in_token[4]);
    char *data_port = new char[MAXBUF];
    sprintf(data_port,("%d"),atoi(in_token[5])*256+atoi(in_token[6]));

    data_cfd = inetconnect(data_ip,data_port);
    if(data_cfd == -1){
       perror("inetconnect");
       return 0;
    }
    send(data_cfd,client_num,strlen(client_num)+1,0);

    while(1){

        if(checkflag){

            cout << "please scan the password" << endl;
            cin >> outputcmd;
            send(ctrl_cfd,outputcmd,strlen(outputcmd)+1,0); 

            recv(ctrl_cfd,inputbuf,MAXBUF,0);
            if(strcmp("230 Login successful.\r",inputbuf) == 0)
                checkflag = false;
            cout << inputbuf << endl;
            getchar();
            memset(outputcmd,0,sizeof(outputcmd));
            memset(inputbuf,0,sizeof(inputbuf));
            continue;                       
        }
        else{
            
            fgets(outputcmd,MAXBUF,stdin);
            outputcmd[strlen(outputcmd)-1] = '\0';
            if(send(ctrl_cfd,outputcmd,strlen(outputcmd),0) == 0){
                cout << "empty command" << endl;
                continue;
            }

            if(strcmp("QUIT",outputcmd) == 0){
                recv(ctrl_cfd,inputbuf,MAXBUF,0);
                cout << inputbuf << endl;
                break;
            }

            if(strstr(outputcmd,"LIST") != NULL){

                recv(ctrl_cfd,inputbuf,MAXBUF,0);
                if(strcmp("pair error",inputbuf) == 0){
                    cout << "Please establish a data connection first." << endl;
                    memset(outputcmd,0,sizeof(outputcmd));
                    memset(inputbuf,0,sizeof(inputbuf));
                    continue;
                }
                cout << inputbuf << endl;
                memset(outputcmd,0,sizeof(outputcmd));
                memset(inputbuf,0,sizeof(inputbuf));
            }
            else if(strstr(outputcmd,"RETR") != NULL){

                recv(ctrl_cfd,inputbuf,MAXBUF,0);
                if(strcmp("pair error",inputbuf) == 0){
                    cout << "Please establish a data connection first." << endl;
                    memset(outputcmd,0,sizeof(outputcmd));
                    memset(inputbuf,0,sizeof(inputbuf));
                    continue;
                }
                cout << inputbuf << endl;

                if(strstr(inputbuf,"wrong command") != NULL)
                    continue;

                if(strstr(inputbuf,"550") != nullptr)
                    continue;
                else{
                    int cnt = 0;
                    int creat_fd = 0;
                    char file_token[10][10];
                    char delim[4] = " \n/";
                    char *token = strtok(outputcmd,delim);
                    char *file_buf = new char[MAXBUF];
                    char file_name[64];
                    char creat_name[128];
                    char serve_end[64];

                    while(token != NULL){
                        strcpy(file_token[cnt],token);
                        token = strtok(NULL,delim);
                        cnt++;
                    }
                    strcpy(file_name,file_token[cnt-1]);   

                    sprintf(creat_name,"%s%s","RETR_",file_name);
                    creat_fd = open(creat_name,O_CREAT|O_RDWR|O_TRUNC,0754);
                    if(creat_fd == -1){
                        cout << "retr_creat error" << endl;
                    }

                    int file_recvcnt;
                    while(strcmp(serve_end,"226 Transfer complete") != 0){
                        int n = 0;
                        while (( n = recv(ctrl_cfd, serve_end, MAXBUF - 1, MSG_DONTWAIT)) != 0){ 
                            serve_end[n] = '\0';
                            if(errno == EAGAIN || errno == EWOULDBLOCK){
                                n = 0;
                                break;
                            }
                        }
                    }
                    while((file_recvcnt = recv(data_cfd,file_buf,MAXBUF-1,MSG_DONTWAIT)) > 0){
                        write(creat_fd,file_buf,file_recvcnt);
                        memset(file_buf,0,MAXBUF);
                    }

                    cout << serve_end << endl;
                    memset(outputcmd,0,sizeof(outputcmd));
                    memset(inputbuf,0,sizeof(inputbuf));
                    memset(serve_end,0,sizeof(serve_end));
                    close(data_cfd);
                    
                    continue;
                }

                memset(inputbuf,0,MAXBUF);

            }
            else if(strcmp("PASV",outputcmd) == 0){

                recvlen = recv(ctrl_cfd,inputbuf,MAXBUF,0);
                if(recvlen == 0){
                    perror("recv");
                    continue;
                }
                cout << inputbuf << endl;

                char delimiter[4]="(),";
                int cnt = 0;
                char **in_token = new char*[10];
                for(int i=0;i<10;i++){
                    in_token[i] = new char[64];
                }
                char *token = strtok(inputbuf,delimiter);
                while(token != NULL){
                    in_token[cnt] = token;
                    cnt++; 
                    token = strtok(NULL,delimiter);
                }
                char *data_ip = new char[MAXBUF];
                sprintf(data_ip,"%s.%s.%s.%s",in_token[1],in_token[2],in_token[3],in_token[4]);
                char *data_port = new char[MAXBUF];
                sprintf(data_port,("%d"),atoi(in_token[5])*256+atoi(in_token[6]));
            
                data_cfd = inetconnect(data_ip,data_port);
                if(data_cfd == -1){
                   perror("inetconnect");
                   return 0;
                }
                send(data_cfd,client_num,strlen(client_num)+1,0);
            }
            else if(strstr(outputcmd,"STOR") != NULL){
                int cnt = 0;
                int file_fd = 0;
                char *token;
                char file_token[10][256];
                char delim[3] = " \n";
                char filename[256];
                ssize_t file_size;
                ssize_t send_size;
                off_t off_set = 0;
                struct stat file_stat;
                
                recv(ctrl_cfd,inputbuf,MAXBUF,0);
                if(strcmp("pair error",inputbuf) == 0){
                    cout << "Please establish a data connection first." << endl;
                    memset(inputbuf,0,sizeof(inputbuf));
                    memset(outputcmd,0,sizeof(outputcmd));
                    continue;
                }
                cout << inputbuf << endl;
                memset(inputbuf,0,MAXBUF);

                token = strtok(outputcmd,delim);
                while(token != NULL){
                    strcpy(file_token[cnt],token);
                    token = strtok(NULL,delim);
                    cnt++; 
                }

                strcpy(filename,file_token[1]);
                
                file_fd = open(filename,O_RDONLY,0754);
                if(file_fd == -1){
                    perror("open");
                    continue;
                }
                if(stat(filename,&file_stat) == -1){
                    perror("stat");
                    continue;
                }
                file_size = file_stat.st_size;
                send_size = file_size;

                if(file_size > CHUNK_SIZE){
                    while(send_size > 0){
                        ssize_t hav_send = sendfile(data_cfd,file_fd,&off_set,CHUNK_SIZE);
                        send_size -= hav_send;
                    }
                }else
                    sendfile(data_cfd,file_fd,&off_set,CHUNK_SIZE);

                recv(ctrl_cfd,inputbuf,MAXBUF,0);
                cout << inputbuf << endl;
                close(data_cfd);

                continue;
                
            }
            else{
                recv(ctrl_cfd,inputbuf,MAXBUF,0);
                if(strcmp("pair error",inputbuf) == 0){
                    cout << "Please establish a data connection first." << endl;
                    memset(inputbuf,0,sizeof(inputbuf));
                    memset(outputcmd,0,sizeof(outputcmd));
                    continue;
                }
                cout << inputbuf << endl;
                memset(outputcmd,0,sizeof(outputcmd));
                memset(inputbuf,0,sizeof(inputbuf));
            }
        }

    }
    
}

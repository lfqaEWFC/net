#ifndef FTP_HPP
#include"Threadpool.hpp"
#include"inetsockets_tcp.hpp"
#include<fcntl.h>
#include<sys/epoll.h>
#include<iostream>
#include<string.h>
#include<dirent.h>
#include<sys/stat.h>
#include<sys/sendfile.h>

using namespace std;

#define PORTNUM "2100"
#define EPSIZE 1024
#define CHUNK_SIZE (1024*1024)

typedef struct data_args{

    int fd = 0;
    char clientnum[10];
    char retr_filename[64];
    char stor_filename[64];
    bool stor_flag = false;
    bool retr_flag = false;
    int ctrl_pair_fd = 0;
    int stor_filefd = 0;
    data_args *next; 
}data_args;

typedef struct ctrl_args{

    int fd = 0;
    int epfd = 0;
    bool checkflag = true;
    pool *data_pool;
    char clientnum[10];
    ctrl_args *next = nullptr;
    data_args *data_args_list = nullptr;
    ctrl_args *ctrl_args_list = nullptr;
}ctrl_args;


class FTP{

    public:

    FTP(ssize_t input_control_threads,ssize_t input_data_threads):
    control_threads(input_control_threads),
    data_threads(input_data_threads),
    listen_control_fd(-1),connect_fd(-1),epfd(-1),workfd_num(0),
    running(false){

        ctrl_pool = new pool(control_threads);
        data_pool = new pool(data_threads);
    }
    
    void init(){

        listen_control_fd = inetlisten(PORTNUM);
        if(listen_control_fd == -1){
            perror("inetlisten");
            return;
        }
        if(set_nonblocking(listen_control_fd) == -1)
            return;
        epfd = epoll_create(EPSIZE);
        if(epfd == -1){
            perror("epoll_creat");
            return;
        }
        ev.data.fd = listen_control_fd;
        ev.events = EPOLLIN | EPOLLET;
        if(epoll_ctl(epfd,EPOLL_CTL_ADD,listen_control_fd,&ev) == -1){
            perror("epoll_ctl");
            return;
        }
        running = true;

        return;     
    }

    void start(){

        while(running){
            workfd_num = epoll_wait(epfd,evlist,EPSIZE,-1);
            if(workfd_num == -1){
                perror("epoll_wait");
                break;
            }
            if(handle_sort(evlist,workfd_num) == -1){
                perror("handle");
                break;
            }
        }
    }

    ~FTP(){
        while(ctrl_args_list!= NULL){
            ctrl_args *prev = ctrl_args_list;
            ctrl_args_list = ctrl_args_list->next;
            free(prev);
        }
        while(data_args_list!=NULL){
            data_args *prev = data_args_list;
            data_args_list = data_args_list->next;
            free(prev);
        }
        free(data_pool);
        free(ctrl_pool);
    }

    private:

    int listen_control_fd,connect_fd;
    int epfd;
    int workfd_num;
    int client_num = 1000;
    bool running;
    pool* ctrl_pool;
    pool* data_pool;
    ssize_t control_threads;
    ssize_t data_threads;
    data_args *data_args_list;
    ctrl_args *ctrl_args_list;
    struct epoll_event ev;
    struct epoll_event evlist[EPSIZE];

    static int set_nonblocking(int fd) {

        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            perror("fcntl F_GETFL");
            return -1;
        }
        
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("fcntl F_SETFL");
            return -1;
        }
        return 0;
    }
    
    int handle_sort(epoll_event *evlist,int workfd_num){

        for(int i=0;i<workfd_num;i++){
            if (evlist[i].events & (EPOLLRDHUP | EPOLLERR)) {
                bool de_flag = true;
                close(evlist[i].data.fd);
                epoll_ctl(epfd, EPOLL_CTL_DEL, evlist[i].data.fd, NULL);
                data_args *data_prev = nullptr;
                data_args *de_data = data_args_list;
                ctrl_args *ctrl_prev = nullptr;
                ctrl_args *de_ctrl = ctrl_args_list;
                while(de_data != NULL){
                    if(de_data->fd == evlist[i].data.fd){
                        if(data_prev){
                            data_prev->next = de_data->next;
                            free(de_data);
                            de_flag = false;
                            cout << "close : " << evlist[i].data.fd << endl;
                            break;
                        }
                        data_prev = de_data;
                        de_data = de_data->next;
                        free(data_prev);
                        data_args_list = de_data;
                        de_flag = false;
                        cout << "close : " << evlist[i].data.fd << endl;
                        break;
                    }
                    data_prev = de_data;
                    de_data = de_data->next;
                }
                while(de_ctrl != NULL && de_flag){
                    if(de_ctrl->fd == evlist[i].data.fd){
                        if(ctrl_prev){
                            ctrl_prev->next = de_ctrl->next;
                            free(de_ctrl);
                            cout << "close : " << evlist[i].data.fd << endl;
                            break;
                        }
                        ctrl_prev = de_ctrl;
                        de_ctrl = de_ctrl->next;
                        free(ctrl_prev);
                        ctrl_args_list = de_ctrl;
                        cout << "close : " << evlist[i].data.fd << endl;
                        break;
                    }
                    ctrl_prev = de_ctrl;
                    de_ctrl = de_ctrl->next;
                }
                continue;
            }
            if(evlist[i].data.fd == listen_control_fd){
                if(handle_accept(listen_control_fd,false) == -1)
                    return -1;
                continue;
            }
            else{
                char* portnum;
                portnum = new char[MAXBUF];
                strcpy(portnum,isportnum(evlist[i].data.fd));

                if(portnum == NULL)
                    return -1;
                if(strcmp(portnum,"2100") == 0){
                    ctrl_args *ctrl_inargs;
                    ctrl_inargs = ctrl_args_list;
                    while(ctrl_inargs != NULL){
                        if(ctrl_inargs->fd == evlist[i].data.fd)
                            break;
                        ctrl_inargs = ctrl_inargs->next;
                    }
                    if(ctrl_inargs == NULL){
                        cout << "can not find ctrl_args" << endl;
                        return -1;
                    }
                    ctrl_pool->addtask(ctrl_fun,(void*)ctrl_inargs);
                    continue;
                }
                else{
                    if(atoi(portnum)<=41024 && atoi(portnum)>=1024){              
                        int optval = 0;
                        socklen_t len = sizeof(optval);
                        if(getsockopt(evlist[i].data.fd, SOL_SOCKET, SO_ACCEPTCONN, &optval, &len) == -1) {
                            perror("getsockopt SO_ACCEPTCONN");
                                return -1;
                            }
                        if(optval) {
                            if(handle_accept(evlist[i].data.fd,true) == -1)
                                return -1;
                            continue;  
                        }else{
                            data_args *data_inargs;
                            data_inargs = data_args_list;
                            while(data_inargs != NULL){
                                if(data_inargs->fd == evlist[i].data.fd)
                                    break;
                                data_inargs = data_inargs->next;
                            }
                            if(data_inargs == NULL){
                                cout << "can not find data_args" << endl;
                                return -1;
                            }
                            data_pool->addtask(data_fun,data_inargs);
                            continue;
                        }
                    }else{
                        cout << "error_sort" << endl;
                        return -1;
                    }
                }
            }
        }
        return 0;
    }

    int handle_accept(int listen_fd,bool data_flag){

        connect_fd = accept(listen_fd,NULL,NULL);
        cout << "open : " << connect_fd << endl;
        if(connect_fd == -1){
            perror("accept");
            return -1;
        }

        ev.events = EPOLLIN|EPOLLET|EPOLLRDHUP;
        ev.data.fd = connect_fd;
        if(epoll_ctl(epfd,EPOLL_CTL_ADD,connect_fd,&ev) == -1){
            perror("epoll_ctl");
            return -1;
        };

        if(data_flag){
            
            char input[10];
            if(data_args_list == NULL){
                data_args_list = new data_args;
                data_args_list->fd = connect_fd;
                data_args_list->next = NULL;
                recv(connect_fd,input,10,0);
                cout << input << endl;
                strcpy(data_args_list->clientnum,input);             
            }else{
                data_args *data_add = new data_args;
                data_args *data_tail;
                data_add->fd = connect_fd;
                data_add->next = NULL;
                recv(connect_fd,input,10,0);
                cout << input << endl;
                strcpy(data_add->clientnum,input);                 
                data_tail = data_args_list;
                while(data_tail->next != NULL){
                    data_tail = data_tail->next;
                }
                data_tail->next = data_add;
            }

            ctrl_args *chag_args = ctrl_args_list;
            while(chag_args != NULL){
                chag_args->data_args_list = data_args_list;
                chag_args = chag_args->next;
            }

            epoll_ctl(epfd, EPOLL_CTL_DEL, listen_fd, nullptr);
            close(listen_fd);

        }else{
            if(ctrl_args_list == NULL){
                ctrl_args_list = new ctrl_args;
                ctrl_args_list->fd = connect_fd;
                ctrl_args_list->epfd = epfd;
                ctrl_args_list->data_pool = data_pool;
                ctrl_args_list->next = NULL;
                sprintf(ctrl_args_list->clientnum,"%d",++client_num);
                send(connect_fd,ctrl_args_list->clientnum,strlen(ctrl_args_list->clientnum),0);                
            }else{
                ctrl_args *ctrl_add = new ctrl_args;
                ctrl_args *ctrl_tail;
                ctrl_add->fd = connect_fd;
                ctrl_add->next = NULL;  
                ctrl_add->epfd = epfd;
                ctrl_add->data_pool = data_pool;
                sprintf(ctrl_add->clientnum,"%d",++client_num);
                send(connect_fd,ctrl_add->clientnum,strlen(ctrl_add->clientnum),0);                
                ctrl_tail = ctrl_args_list;
                while(ctrl_tail->next != NULL){
                    ctrl_tail = ctrl_tail->next;
                }
                ctrl_tail->next = ctrl_add;
            }

            ctrl_args *chag_args = ctrl_args_list;
            while(chag_args != NULL){
                chag_args->ctrl_args_list = ctrl_args_list;
                chag_args = chag_args->next;
            }

        }

        return 0;
    }

    char* isportnum(int fd){

        sockaddr addr;
        socklen_t len = sizeof(sockaddr);
        char* result;
        result = new char[MAXBUF];
            getsockname(fd,&addr,&len);

        return address_str_portnum(result,MAXBUF,&addr,len);
        
    }

    static void *ctrl_fun(void *args) {

        char recvbuf[MAXBUF];
        char sendbuf[MAXBUF];
        ssize_t n;
        ctrl_args *new_arg = (ctrl_args*) args;

        while ((n = recv(new_arg->fd, recvbuf, MAXBUF - 1, MSG_DONTWAIT)) != 0){ 
            recvbuf[n] = '\0';
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                n = 0;
                break;
            }
        }

        if (n == 0) {
            if(strcmp(recvbuf,"PASV") == 0){
                char portnum_str[MAXBUF];
                char *result = new char[MAXBUF];
                sockaddr_storage addr;
                socklen_t len = sizeof(sockaddr_storage);

                srand(time(NULL));
                int listen_data_portnum = rand()%40000+1024;
                sprintf(portnum_str,"%d",listen_data_portnum);
                int listen_data_fd = inetlisten((const char*)portnum_str);
                while(listen_data_fd == -1){
                    perror("inetlisten");
                    listen_data_portnum = rand()%40000+1024;
                    sprintf(portnum_str,"%d",listen_data_portnum);
                    listen_data_fd = inetlisten((const char*)portnum_str);
                }
                set_nonblocking(listen_data_fd);

                struct epoll_event ev;
                ev.data.fd = listen_data_fd;
                ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(new_arg->epfd,EPOLL_CTL_ADD,listen_data_fd,&ev);
                
                getsockname(listen_data_fd,(sockaddr*)&addr,&len);
                address_str_portnum(result,MAXBUF,(sockaddr*)&addr,len) == NULL;
                const char delimiter[5] = "().,";
                char *token;
                char **res_token = new char*[10];
                for(int i=0;i<10;i++){
                    res_token[i] = new char[10];
                }
                int cnt = 0;
                cout << result << endl;
                token = strtok(result,delimiter);
                while(token != NULL){
                    res_token[cnt] = token;
                    token = strtok(NULL,delimiter);
                    cnt++;
                }
                sprintf(result,"(%s,%s,%s,%s,%d,%d)",res_token[0],res_token[1],res_token[2],res_token[3],atoi(res_token[4])/256,atoi(res_token[4])%256);
                sprintf(sendbuf,"%s %s","227 entering passive mode",result);
                send(new_arg->fd,sendbuf,sizeof(sendbuf),0);

                memset(sendbuf,0,sizeof(sendbuf));
                delete[] res_token;
                free(result);
            }
            else {

                if(strstr(recvbuf,"QUIT") != NULL){
                    if(send(new_arg->fd,"221 Goodbye.",strlen("221 Goodbye.")+1,0) == -1)
                        perror("send");
                    return NULL;
                }

                data_args *data_pair = new_arg->data_args_list;
                while(data_pair != NULL){
                    if(strcmp(data_pair->clientnum,new_arg->clientnum) == 0){
                        data_pair->ctrl_pair_fd = new_arg->fd;
                        break;
                    }
                    data_pair = data_pair->next;
                }
                if(data_pair == NULL){
                    send(new_arg->fd,"pair error",strlen("pair error")+1,0);
                    return NULL;
                }
                else{
                    char pairchk[10];
                    send(new_arg->fd,"pair succeed",strlen("pair succeed")+1,0);
                    recv(new_arg->fd,pairchk,10,0);
                }

                if(new_arg->checkflag){

                    if(strcmp(recvbuf,"mcy060529") == 0){
                        if(send(new_arg->fd,"230 Login successful.\r",strlen("230 Login successful.\r")+1,0) == -1){
                            perror("send");
                        }
                        new_arg->checkflag = false;
                        cout << "check" << endl;
                        return NULL;
                    }else{
                        if(send(new_arg->fd,"530 Wrong password.\r",strlen("530 Wrong password.\r")+1,0) == -1){
                            perror("send");
                        }
                        return NULL;
                    }
                }

                if(strstr(recvbuf,"LIST") != NULL){

                    char dirpath[MAXBUF];
                    char dirmsg[MAXBUF];
                    char tmp_buf[MAXBUF*2];
                    DIR *dirp;

                    if(strcmp(recvbuf,"LIST") == 0)
                        getcwd(dirpath,sizeof(dirpath));
                    else{
                        int cnt = 0;
                        char recv_token[10][10];
                        char delim[3] = " \n";
                        char *token = strtok(recvbuf,delim);
                        while(token != NULL){
                            strcpy(recv_token[cnt],token);
                            token = strtok(NULL,delim);
                            cnt++;
                        }
                        if(cnt > 2){
                            send(data_pair->fd,"wrong dictory",strlen("wrong dictory"),0);
                            return NULL;
                        }
                        strcpy(dirpath,recv_token[1]);
                    }

                    dirp = opendir(dirpath);
                    if(dirp == NULL){
                        perror("opendir");
                        if(send(data_pair->fd,"wrong dictory",strlen("wrong dictory"),0) == -1)
                            perror("send");
                        return NULL;
                    }                    
                    struct dirent *file;
                    while((file = readdir(dirp)) != NULL){
                        if(strlen(dirmsg) == 0){
                            strcpy(dirmsg,file->d_name);
                            continue;
                        }
                        sprintf(tmp_buf,"%s %s",dirmsg,file->d_name);
                        strncpy(dirmsg,tmp_buf,MAXBUF-1);
                    }
                    if(send(data_pair->fd,dirmsg,strlen(dirmsg)+1,0) == -1)
                        perror("send");

                    return NULL;
                }
                else if(strstr(recvbuf,"RETR") != NULL){

                    data_pair->retr_flag = true;
                    int cnt = 0;
                    char recv_token[10][10];
                    char delim[3] = " \n";
                    char *token = strtok(recvbuf,delim);
                    while(token != NULL){
                        strcpy(recv_token[cnt],token);
                        token = strtok(NULL,delim);
                        cnt++;
                    }
                    if(cnt > 2){
                        if(send(new_arg->fd,"wrong command",strlen("wrong command")+1,0) == -1)
                            perror("send");
                        return NULL;
                    }
                    strcpy(data_pair->retr_filename,recv_token[1]);

                    if(open(data_pair->retr_filename,O_RDONLY,0644) == -1){
                       if(send(new_arg->fd,"550 file not found or denied.",strlen("550 file not found or denied.")+1,0) == -1)
                            perror("send");
                       return NULL; 
                    }
                    else{
                        struct stat size_stat;
                        stat(data_pair->retr_filename,&size_stat);
                        sprintf(sendbuf,"%s %s (%lld %s)","150 Opening BINARY mode data connection for",data_pair->retr_filename,(long long)size_stat.st_size,"bytes");
                        if(send(new_arg->fd,sendbuf,strlen(sendbuf)+1,0) == -1)
                            perror("send"); 
                    }

                    new_arg->data_pool->addtask(data_fun,data_pair);

                    memset(sendbuf,0,sizeof(sendbuf));
                    return NULL;
                }
                else if(strstr(recvbuf,"STOR") != NULL){
                    data_pair->stor_flag = true;
                    int cnt = 0;
                    char recv_token[10][10];
                    char delim[4] = " \n/";
                    char *creat_name = new char[MAXBUF];
                    char *token = strtok(recvbuf,delim);

                    while(token != NULL){
                        strcpy(recv_token[cnt],token);
                        token = strtok(NULL,delim);
                        cnt++;
                    }
                    strcpy(data_pair->stor_filename,recv_token[cnt-1]);

                    sprintf(creat_name,"%s%s","STOR_",data_pair->stor_filename);
                    
                    data_pair->stor_filefd = open(creat_name,O_CREAT|O_RDWR|O_TRUNC,0754);
                    if(data_pair->stor_filefd == -1){
                        if(send(new_arg->fd,"550 file not found or denied.",strlen("550 file not found or denied.")+1,0) == -1)
                            perror("send");
                        return NULL;
                    }else{
                        sprintf(sendbuf,"%s %s","150 Opening BINARY mode data connection for",creat_name);
                        if(send(new_arg->fd,sendbuf,strlen(sendbuf)+1,0) == -1)
                            perror("send"); 
                    }

                    return NULL;
                }
                else{
                    if(send(new_arg->fd,"wrong command",strlen("wrong command")+1,0) == -1){
                        perror("send");
                    }
                    return NULL;
                }

            }
        }
        else if (n == -1) {
            perror("recv");
            return NULL;
        }
        
        return NULL;
    }

    static void *data_fun(void *args){

        char recvbuf[MAXBUF];
        char sendbuf[MAXBUF];
        ssize_t n;
        data_args *new_arg = (data_args*) args;

        if(new_arg->retr_flag){

            int file_fd = 0;
            ssize_t file_size = 0;
            ssize_t send_size;
            off_t off_set = 0;
            struct stat file_stat;

            if(stat(new_arg->retr_filename,&file_stat) == -1){
                perror("stat");
            }
            file_size = file_stat.st_size;
            send_size = file_size;
            file_fd = open(new_arg->retr_filename,O_RDONLY,0754);

            if(file_size > CHUNK_SIZE){
                while(send_size > 0){
                    ssize_t hav_send = sendfile(new_arg->fd,file_fd,&off_set,CHUNK_SIZE);
                    send_size -= hav_send;
                }
            }else{
                sendfile(new_arg->fd,file_fd,&off_set,CHUNK_SIZE);
            }

            if(send(new_arg->ctrl_pair_fd,"226 Transfer complete",strlen("226 Transfer complete")+1,0) == -1)
                perror("send");

            new_arg->retr_flag = false;
            return NULL;
            
        }
        else if(new_arg->stor_flag){
            
            char *file_buf = new char[MAXBUF];
            int file_recvcnt = 0;

            while((file_recvcnt = recv(new_arg->fd,file_buf,MAXBUF-1,MSG_DONTWAIT)) > 0){
                write(new_arg->stor_filefd,file_buf,file_recvcnt);
                memset(file_buf,0,MAXBUF);
            }
            
            send(new_arg->ctrl_pair_fd,"226 Transfer complete",strlen("226 Transfer complete")+1,0);
        }

        new_arg->stor_flag = false;
        return NULL;
    } 

};

#endif

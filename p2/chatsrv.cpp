/** MAC OS **/

#include <iostream>
#include <cstring>
#include <list>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/event.h>
#include <errno.h>

#define MSG_NOSIGNAL 0x2000
#define MAXLINE 1024
#define NEVENTS 1000
#define TIMEOUT_SEC 60

using std::endl;
using std::cerr;
using std::cout;
using std::string;
using std::list;

typedef unsigned int uint;

/* 
 * Client class
 */

class Client
{
    int fd;          // descriptor
    string ip;       // ip-address
    string message;  // message to send

public:

    Client(int f = 0, string i = "", string m = ""): fd(f), ip(i), message(m) {}
    
    int get_fd() { return this->fd; }
    void set_fd(int fd) { this->fd = fd; }
    
    string get_ip() { return ip; }
    void set_ip(struct sockaddr_in *sa) {
        char tmp[20];
        inet_ntop(sa->sin_family, &sa, tmp, MAXLINE);
        ip = string(tmp);
    }
    
    string get_message() { return message; }
    void set_message(string m) { message = m; }
    void append_message(string a_m) { message += a_m; }

    void send_to_all(list<Client> clients)
    {
        for (auto it = clients.begin(); it != clients.end(); it++) {
            const char *wrapped_ip = string("{"+ip+"} >").c_str();
            if (fd != it->get_fd()) {
                /* outer client */
                send(it->get_fd(), wrapped_ip, strlen(wrapped_ip), MSG_NOSIGNAL);
                send(it->get_fd(), message.c_str(), strlen(message.c_str()), MSG_NOSIGNAL);
            } else {
                /* its me! */
                string my_ip = "{127.0.0.1} >";
                send(fd, my_ip.c_str(), strlen(my_ip.c_str()), MSG_NOSIGNAL);
                send(fd, message.c_str(), strlen(message.c_str()), MSG_NOSIGNAL);
            }
        }
    }
};


/* DECLARATIONS */

void CHECK_ERROR(int t);

int set_nonblock(int fd);
void fill_sock_addr_default(sockaddr_in& SockAddr);
int set_change(int kq, struct kevent *ch, int fd, int filter, int action);
string handle_mes(string &str);
list<Client>::iterator find_client(list<Client>::iterator b, list<Client>::iterator e, int fd);
void terminate_passive_clients(list<Client> clients);

int main()
{
    list<Client> clients;
    int err;

    struct sockaddr_in SockAddr;
    int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    fill_sock_addr_default(SockAddr);
    
    int optval = 1;
    setsockopt(MasterSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    
    err = bind(MasterSocket, (sockaddr *) &SockAddr, sizeof(SockAddr));
    CHECK_ERROR(err);
    
    err = listen(MasterSocket, SOMAXCONN);
    CHECK_ERROR(err);
    
    int kq = kqueue();
    CHECK_ERROR(kq);
    
    struct kevent Events[NEVENTS];
    struct kevent Changes[NEVENTS];
    struct timespec timer = {TIMEOUT_SEC, 0};
    
    err = set_change(kq, Changes, MasterSocket, EVFILT_READ, EV_ADD);
    CHECK_ERROR(err);
    
    while(true) {
        int N = kevent(kq, NULL, 0, Events, NEVENTS, &timer);
        CHECK_ERROR(N);
        
        if (N == 0) {
            terminate_passive_clients(clients);
            clients.clear();
        }

        if (N > 0) {
            for (uint i = 0; i < N; i++) {
                if (Events[i].flags & EV_ERROR) {
                    cerr << "EVENT_ERROR: " << strerror(Events[i].data) << endl;
                }
                if (Events[i].ident == MasterSocket && (Events[i].flags & EVFILT_READ)) {
                    struct sockaddr_in ClientSockAddr;
                    socklen_t socklen = sizeof(ClientSockAddr);
    
                    int SlaveSocket = accept(MasterSocket, (struct sockaddr *)&ClientSockAddr, &socklen);
                    CHECK_ERROR(SlaveSocket);
                    
                    err = set_nonblock(SlaveSocket);
                    CHECK_ERROR(err);
                    
                    err = set_change(kq, Changes, SlaveSocket, EVFILT_READ, EV_ADD);
                    CHECK_ERROR(err);
                    
                    Client newClient = Client(SlaveSocket);
                    newClient.set_ip(&ClientSockAddr);
                    clients.push_back(newClient);
                    cout << "[LOG]:{" << newClient.get_ip() << "} >" << "accepted connection" << endl;
                    cout.flush();
                    send(SlaveSocket, "Welcome!\n", 10, MSG_NOSIGNAL);
                }
                
                if (Events[i].ident != MasterSocket && (Events[i].flags & EVFILT_READ)) {
                    char mes[MAXLINE+1];
                    bzero(mes, MAXLINE+1);
                    
                    auto sender = find_client(clients.begin(), clients.end(), Events[i].ident);
                    int result = 0;
                    do {
                        result = recv(Events[i].ident, mes, MAXLINE, MSG_NOSIGNAL);
                        if (result <= 0) {
                            cout << "[LOG]:{" << sender->get_ip() << "} >" << "connection terminated" << endl;
                            cout.flush();
                            shutdown(Events[i].ident, SHUT_RDWR);
                            close(Events[i].ident);
                            clients.erase(find_client(clients.begin(), clients.end(), Events[i].ident));
                            continue;
                        }
                        if (result > 0) {
                            string str_mes = string(mes);
                            while (!str_mes.empty()) {
                                if (str_mes.find('\n') != string::npos) {
                                    sender->append_message(handle_mes(str_mes));
                                    cout << "[LOG]:{" <<sender->get_ip() << "} >" << sender->get_message();
                                    cout.flush();
                                    sender->send_to_all(clients);
                                    sender->set_message("");
                                } else {
                                    sender->append_message(str_mes);
                                    str_mes.clear();
                                }
                            }
                        }
                    } while (result > MAXLINE-1);
                }
            }
        }
    }
    shutdown(MasterSocket, SHUT_RDWR);
    close(MasterSocket);
    close(kq);
    cout << "Server succesfully shut down" << endl;
    cout.flush();
    return 0;
}



/* DEFINITIONS */

int set_nonblock(int fd)
{
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}

void fill_sock_addr_default(sockaddr_in& SockAddr)
{
    bzero(&SockAddr, sizeof(SockAddr));
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(3100);
    SockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
}

int set_change(int kq, struct kevent *ch, int fd, int filter, int action)
{
    static int nchanges = 0;
    
    bzero(&ch[nchanges], sizeof(ch));
    EV_SET(&ch[nchanges], fd, filter, action, 0, 0, 0);
    kevent(kq, &ch[nchanges], 1, NULL, 0, NULL);
    return ++nchanges;
}


string handle_mes(string &str)
{
    std::size_t pos = str.find('\n');
    string mes = str.substr(0, pos + 1);
    str.erase(0, pos + 1);
    return mes;
}


list<Client>::iterator find_client(list<Client>::iterator b, list<Client>::iterator e, int fd)
{
    return std::find_if(b, e, [fd](Client& x) {return x.get_fd() == fd;});
}

void terminate_passive_clients(list<Client> clients)
{
    cout << "[Timeout " << TIMEOUT_SEC << " sec] Terminating clients...";
    for (auto it = clients.begin(); it!= clients.end(); it++) {
        cout << "[LOG]:" << it->get_ip() << "connection terminated" << endl;
        shutdown(it->get_fd(), SHUT_RDWR);
        close(it->get_fd());
    }
}

 void CHECK_ERROR(int t)
 {
    if (t < 0) {
        cerr << strerror(t) << endl << "Shutting down..." << endl;
        exit(1);
    }
 }




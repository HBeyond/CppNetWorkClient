#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

using namespace std;
int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    fflush(stdout);
    struct sockaddr_in addrto;
    bzero(&addrto, sizeof(struct sockaddr_in));
    addrto.sin_family = AF_INET;
    addrto.sin_addr.s_addr = htonl(INADDR_ANY);
    addrto.sin_port = htons(6000);
    socklen_t len = sizeof(addrto);
    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        cout << "socket error..." << endl;
        return -1;
    }
    const int opt = -1;
    int nb = 0;
    nb = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt));
    if (nb == -1) {
        cout << "set socket errror..." << endl;
        return -1;
    }
    if (bind(sock, (struct sockaddr*)&(addrto), len) == -1) {
        cout << "bind error..." << endl;
        return -1;
    }
    char msg[100] = {0};
    size_t receive_times = 0;
    while (1) {
        int ret = recvfrom(sock, msg, 100, 0, (struct sockaddr*)&addrto, &len);
        if (ret <= 0) {
            cout << "read error..." << endl;
        } else {
            printf("%s\t", msg);
            cout << "receive_time: " << receive_times << endl;
            ++receive_times;
            cout << "server ip = " << inet_ntoa(addrto.sin_addr) << endl;
        }
        sleep(1);
    }
    return 0;
}

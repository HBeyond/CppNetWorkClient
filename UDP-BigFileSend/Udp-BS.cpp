#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include "opencv2/opencv.hpp"

#define SERVER_PORT 7777
#define BUFFER_SIZE 500
#define FILE_NAME_MAX_SIZE 512

/* 包头 */
typedef struct {
    long int id;
    int buf_size;
    unsigned int crc32val;  //每一个buffer的crc32值
    int errorflag;
    int saveCommand;
} PackInfo;

/* 接收包 */
struct RecvPack {
    PackInfo head;
    char buf[BUFFER_SIZE];
} data;

//----------------------crc32----------------
static unsigned int crc_table[256];
static void init_crc_table(void);
static unsigned int crc32(unsigned int crc, unsigned char *buffer, unsigned int size);
/* 第一次传入的值需要固定,如果发送端使用该值计算crc校验码, 那么接收端也同样需要使用该值进行计算 */
unsigned int crc = 0xffffffff;

/*
 * 初始化crc表,生成32位大小的crc表
 * 也可以直接定义出crc表,直接查表,
 * 但总共有256个,看着眼花,用生成的比较方便.
 */
static void init_crc_table(void) {
    unsigned int c;
    unsigned int i, j;

    for (i = 0; i < 256; i++) {
        c = (unsigned int)i;

        for (j = 0; j < 8; j++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }

        crc_table[i] = c;
    }
}

/* 计算buffer的crc校验码 */
static unsigned int crc32(unsigned int crc, unsigned char *buffer, unsigned int size) {
    unsigned int i;

    for (i = 0; i < size; i++) {
        crc = crc_table[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);
    }

    return crc;
}

//主函数入口
using namespace std;
int main() {
    /* 发送id */
    long int send_id = 0;

    /* 接收id */
    int receive_id = 0;

    int imgSerialNum = 0;

    /* 服务端地址 */
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    //    server_addr.sin_addr.s_addr = inet_addr("192.168.26.33");
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);
    socklen_t server_addr_length = sizeof(server_addr);

    /* 创建socket */
    int client_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket_fd < 0) {
        cout << "Create Socket Failed:" << endl;
        exit(1);
    }

    // crc32
    init_crc_table();

    // time test
    clock_t Begin = clock();

    /* 数据传输 */
    while (1) {
        string path = "/home/beyoung/Desktop/mac-ubuntu-share/1_IDSCameraDev/3_SAPro_ImageSave75/";
        string file_name = path + to_string(imgSerialNum) + ".jpg";
        string savePath = "/home/beyoung/Desktop/mac-ubuntu-share/1_IDSCameraDev/7_UDPChooseSave/";
        string save_file_name = savePath + to_string(imgSerialNum) + ".jpg";

        /* 打开文件 */
        FILE *fp = fopen(file_name.c_str(), "r");
        if (NULL == fp) {
            cout << "File Not Found: " << file_name << endl;
            break;
        } else {
            int len = 0;
            /* 每读取一段数据，便将其发给客户端 */
            while (1) {
                PackInfo pack_info;

                bzero((char *)&data, sizeof(data));  // ljh socket发送缓冲区清零

                cout << "receive_id: " << receive_id << endl;
                cout << "send_id: " << send_id << endl;

                if (receive_id == send_id) {
                    ++send_id;

                    if ((len = fread(data.buf, sizeof(char), BUFFER_SIZE, fp)) > 0) {
                        data.head.id = send_id;   /* 发送id放进包头,用于标记顺序 */
                        data.head.buf_size = len; /* 记录数据长度 */
                        data.head.crc32val = crc32(crc, reinterpret_cast<unsigned char *>(data.buf), sizeof(data));
                        cout << "len =: " << len << endl;
                        cout << "data.head.crc32val=: " << data.head.crc32val << endl;
                    // cout <<"data.buf=%s\n",data.buf);

                    resend:
                        if (sendto(client_socket_fd, (char *)&data, sizeof(data), 0, (struct sockaddr *)&server_addr,
                                   server_addr_length) < 0) {
                            cout << "Send File Failed:" << endl;
                            break;
                        }

                        /* 接收确认消息 */
                        recvfrom(client_socket_fd, (char *)&pack_info, sizeof(pack_info), 0,
                                 (struct sockaddr *)&server_addr, &server_addr_length);
                        receive_id = pack_info.id;
                        //如果确认包提示数据错误
                        if (pack_info.errorflag == 1) {
                            pack_info.errorflag = 0;
                            goto resend;
                        }

                        // usleep(50000);

                    } else {
                        break;
                    }
                } else {
                    /* 如果接收的id和发送的id不相同,重新发送 */
                    if (sendto(client_socket_fd, (char *)&data, sizeof(data), 0, (struct sockaddr *)&server_addr,
                               server_addr_length) < 0) {
                        cout << "Send File Failed:" << endl;
                        break;
                    }

                    cout << "repeat send" << endl;

                    /* 接收确认消息 */
                    recvfrom(client_socket_fd, (char *)&pack_info, sizeof(pack_info), 0,
                             (struct sockaddr *)&server_addr, &server_addr_length);
                    receive_id = pack_info.id;

                    // usleep(50000);
                }
            }

            //发送结束包 0字节目的告诉客户端发送完毕
            if (sendto(client_socket_fd, (char *)&data, 0, 0, (struct sockaddr *)&server_addr, server_addr_length) <
                0) {
                cout << "Send 0 char  Failed:" << endl;
                break;
            }
            cout << "client send file end 0 char" << endl;
            cout << "File:%s Transfer Successful:" << file_name << endl;

            /* 关闭文件 */
            fclose(fp);

            // receive save command
            PackInfo packInfo;
            packInfo.saveCommand = 0;
            cout << "packInfo.saveCommand = " << packInfo.saveCommand << endl;
            recvfrom(client_socket_fd, (char *)&packInfo, sizeof(packInfo), 0, (struct sockaddr *)&server_addr,
                     &server_addr_length);
            cout << "packInfo.saveCommand = " << packInfo.saveCommand << endl;
            if (packInfo.saveCommand == 1) {
                cv::Mat img = cv::imread(file_name, CV_LOAD_IMAGE_COLOR);
                cv::imwrite(save_file_name, img);
                cout << "save image " << save_file_name << endl;
            }

            //清零id，准备发送下一个文件
            /* 发送id */
            send_id = 0;
            /* 接收id */
            receive_id = 0;
        }

        imgSerialNum += 1;
    }

    clock_t End = clock();
    cout << "transmition time is: " << double(End - Begin) / CLOCKS_PER_SEC << endl;
    close(client_socket_fd);
    return 0;
}

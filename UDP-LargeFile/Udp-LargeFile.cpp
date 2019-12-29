/*************************************************************************
  > File Name: client.c
  > Author: ljh
 ************************************************************************/
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

#define SERVER_PORT 8888
#define BUFFER_SIZE 500
#define FILE_NAME_MAX_SIZE 512

/* 包头 */
typedef struct {
    long int id;
    int buf_size;
    unsigned int crc32val;  //每一个buffer的crc32值
    int errorflag;
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
    long int id;
    unsigned int crc32tmp;
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

    /* 输入文件名到缓冲区 */
    //    char file_name[FILE_NAME_MAX_SIZE + 1];
    //    bzero(file_name, FILE_NAME_MAX_SIZE + 1);
    //    cout <<"Please Input File Name On Server: ");
    //    scanf("%s", file_name);

    //    char file_name[FILE_NAME_MAX_SIZE + 1] =
    //        "/home/beyoung/Desktop/mac-ubuntu-share/1_IDSCameraDev/5_SAPro_ImageSave100/199.jpg";
    string path = "/home/beyoung/Desktop/mac-ubuntu-share/1_IDSCameraDev/5_SAPro_ImageSave100/";
    string savePath = "/home/beyoung/Desktop/mac-ubuntu-share/1_IDSCameraDev/6_UDPBigFileSave/";

    for (imgSerialNum;; ++imgSerialNum) {
        id = 1;
        // crc32
        init_crc_table();
        string file_name = path + to_string(imgSerialNum) + ".jpg";
        char buffer[BUFFER_SIZE];
        bzero(buffer, BUFFER_SIZE);
        strncpy(buffer, file_name.c_str(),
                strlen(file_name.c_str()) > BUFFER_SIZE ? BUFFER_SIZE : strlen(file_name.c_str()));

        /* 发送文件名 */
        if (sendto(client_socket_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, server_addr_length) < 0) {
            cout << "Send File Name Failed:" << endl;
            exit(1);
        }

        /* 打开文件，准备写入 */
        string saveName = savePath + to_string(imgSerialNum) + ".jpg";
        FILE *fp = fopen(saveName.c_str(), "w");
        if (NULL == fp) {
            cout << "File Can Not Open To Write: " << saveName << endl;
            exit(1);
        }

        /* 从服务器接收数据，并写入文件 */
        int len = 0;

        while (1) {
            PackInfo pack_info;  //定义确认包变量

            if ((len = recvfrom(client_socket_fd, (char *)&data, sizeof(data), 0, (struct sockaddr *)&server_addr,
                                &server_addr_length)) > 0) {
                cout << "len = " << len;
                crc32tmp = crc32(crc, reinterpret_cast<unsigned char *>(data.buf), sizeof(data));

                // crc32tmp=5;
                cout << "-------------------------" << endl;
                cout << "data.head.id= " << data.head.id << endl;
                cout << "id= " << id << endl;

                if (data.head.id == id) {
                    cout << "crc32tmp= " << crc32tmp << endl;
                    cout << "data.head.crc32val= " << data.head.crc32val;
                    // cout <<"data.buf=%s\n",data.buf);

                    //校验数据正确
                    if (data.head.crc32val == crc32tmp) {
                        cout << "rec data success" << endl;

                        pack_info.id = data.head.id;
                        pack_info.buf_size = data.head.buf_size;  //文件中有效字节的个数，作为写入文件fwrite的字节数
                        ++id;                                     //接收正确，准备接收下一包数据

                        /* 发送数据包确认信息 */
                        if (sendto(client_socket_fd, (char *)&pack_info, sizeof(pack_info), 0,
                                   (struct sockaddr *)&server_addr, server_addr_length) < 0) {
                            cout << "Send confirm information failed!" << endl;
                        }
                        /* 写入文件 */
                        if (fwrite(data.buf, sizeof(char), data.head.buf_size, fp) < data.head.buf_size) {
                            cout << "File Write Failed: " << file_name << endl;
                            break;
                        }
                    } else {
                        pack_info.id = data.head.id;  //错误包，让服务器重发一次
                        pack_info.buf_size = data.head.buf_size;
                        pack_info.errorflag = 1;

                        cout << "rec data error,need to send again" << endl;
                        /* 重发数据包确认信息 */
                        if (sendto(client_socket_fd, (char *)&pack_info, sizeof(pack_info), 0,
                                   (struct sockaddr *)&server_addr, server_addr_length) < 0) {
                            cout << "Send confirm information failed!" << endl;
                        }
                    }

                }                           // if(data.head.id == id)
                else if (data.head.id < id) /* 如果是重发的包 */
                {
                    pack_info.id = data.head.id;
                    pack_info.buf_size = data.head.buf_size;

                    pack_info.errorflag = 0;  //错误包标志清零

                    cout << "data.head.id < id" << endl;
                    /* 重发数据包确认信息 */
                    if (sendto(client_socket_fd, (char *)&pack_info, sizeof(pack_info), 0,
                               (struct sockaddr *)&server_addr, server_addr_length) < 0) {
                        cout << "Send confirm information failed!" << endl;
                    }
                }  // else if(data.head.id < id) /* 如果是重发的包 */

            } else  //接收完毕退出
            {
                break;
            }
        }

        cout << "Receive File From Server IP Successful: " << file_name << endl;
        fclose(fp);
    }
    close(client_socket_fd);
    return 0;
}

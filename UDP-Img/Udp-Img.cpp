//
// Created by parallels on 7/18/19.
//

#ifndef SENSORPOSECALIBRATION_UDPCLIENT_H
#define SENSORPOSECALIBRATION_UDPCLIENT_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string>

typedef struct tagUDPHeader {
    long int serial_number;    //
    int type;                  // image, text or command
    int save_command;          // save command: 1=save, 0=ignore
    unsigned long image_size;  // total image_size
    unsigned int crc32val;     // check bit
    int errorflag;             // check fail flage: 1-true-error, 0-false-no error
} UDPHeader;

// original header
typedef struct tagTransmitData {
    //    int data_length;      // header_length+image_size;
    //    int header_length;    // header of udp
    //    int image_offset;     // ?
    UDPHeader udpHeader;  // one UDPHeader to contain the transmit data information
    char data_area[500];  // including the header and image
} TransmitData;

/**
 * @brief The UDPClient class
 */
class UDPClient {
   public:
    UDPClient(std::string server_ip, int port_number);
    ~UDPClient();

    int InitSocket();
    int RecvFrom(char *msg_recv_buffer, int buffer_size);
    int SendTo(char *msg_send_buffer, int length);
    int CloseSocket();
    int SendOneImg(void *data);  // ClientConnectToFA::SendDataToServer in SA project
    void Init_crc_table(void);
    unsigned int Crc32(unsigned int crc, unsigned char *buffer, unsigned int size);
    int SaveImage(std::string &file_name, std::string &save_name);
    std::string NetworkFinder();
    int IsServerIPValid(std::string &serverIP);

   private:
    std::string server_ip_;
    int port_number_;
    int sock_client_fd_;
    struct sockaddr_in server_addr_;
    TransmitData data_;            // the data passed from client once, that contain piece image information
    int recvBufferSize_;           // the buffer size of one data_.data_area
    unsigned int crc_table_[256];  // check value table
    unsigned int crc_;             // check initial value
};

#endif  // SENSORPOSECALIBRATION_UDPCLIENT_H

//
// Created by parallels on 7/18/19.
//
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include "opencv2/opencv.hpp"

/**
 * @brief UDPClient::UDPClient
 * @param server_ip
 * @param port_number
 */
UDPClient::UDPClient(std::string server_ip, int port_number) {
    server_ip_ = server_ip;
    port_number_ = port_number;
    recvBufferSize_ = 500;
    crc_ = 0xffffffff;

    std::cout << "server ip: " << server_ip << std::endl;
    if (server_ip.empty()) {
        std::cout << "server ip is empty, trun on the NetWorkFinder" << std::endl;
        server_ip_ = NetworkFinder();
    }

    InitSocket();
}

/**
 * @brief UDPClient::~UDPClient
 */
UDPClient::~UDPClient() { CloseSocket(); }

/**
 * @brief UDPClient::InitSocket
 * @return
 */
int UDPClient::InitSocket() {
    memset(&server_addr_, 0, sizeof(sockaddr_in));
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_addr.s_addr = inet_addr(server_ip_.c_str());
    server_addr_.sin_port = htons(port_number_);

    if ((sock_client_fd_ = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket error");
        return 0;
    }

    //设置端口可重用
    //    int on = 1;
    //    setsockopt(sock_client_fd_, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));

    return 1;
}

/**
 * @brief UDPClient::RecvFrom
 * @param msg_recv_buffer
 * @param buffer_size
 * @return
 */
int UDPClient::RecvFrom(char *msg_recv_buffer, int buffer_size) {
    int len = 0;
    int sin_size = sizeof(sockaddr_in);
    if ((len = recvfrom(sock_client_fd_, msg_recv_buffer, buffer_size, 0, (struct sockaddr *)&server_addr_,
                        (socklen_t *)&sin_size)) < 0) {
        perror("recvfrom error");
        return 0;
    }
    // printf("received packet from %s:/n",inet_ntoa(remote_addr.sin_addr));
    if (len < buffer_size) msg_recv_buffer[len] = '/0';

    return 1;
}

/**
 * @brief UDPClient::SendTo
 * @param msg_send_buffer
 * @param length
 * @return
 */
int UDPClient::SendTo(char *msg_send_buffer, int length) {
    int len = 0;
    if ((len = sendto(sock_client_fd_, msg_send_buffer, length, 0, (struct sockaddr *)&server_addr_,
                      sizeof(struct sockaddr))) < 0) {
        std::cout << "len = " << len << std::endl;
        perror("sendto error");
        return 0;
    }

    return 1;
}

/**
 * @brief UDPClient::CloseSocket
 * @return
 */
int UDPClient::CloseSocket() {
    close(sock_client_fd_);

    return 1;
}

/*
 * 初始化crc表,生成32位大小的crc表
 * 也可以直接定义出crc表,直接查表,
 * 但总共有256个,看着眼花,用生成的比较方便.
 */
void UDPClient::Init_crc_table(void) {
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

        crc_table_[i] = c;
    }
}

/* 计算buffer的crc校验码 */
unsigned int UDPClient::Crc32(unsigned int crc, unsigned char *buffer, unsigned int size) {
    unsigned int i;

    for (i = 0; i < size; i++) {
        crc = crc_table_[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);
    }

    return crc;
}

// send one image
int UDPClient::SendOneImg(void *file) {
    std::cout << "send images" << std::endl;
    // experience feature
    FILE *fp = (FILE *)file;
    // send id
    long int send_id = 0;
    // receive id
    int receive_id = 0;
    // crc32
    Init_crc_table();
    int len = 0;

    // send by segment
    while (1) {
        // data preparation
        UDPHeader pack_info;
        bzero((char *)&data_, sizeof(data_));  // clear the send buffer

        std::cout << "receive_id: " << receive_id << std::endl;
        std::cout << "send_id: " << send_id << std::endl;

        if (receive_id == send_id) {
            ++send_id;

            if ((len = fread(data_.data_area, sizeof(char), recvBufferSize_, fp)) > 0) {
                data_.udpHeader.serial_number = send_id;  // put the send id into the package information
                data_.udpHeader.image_size = len;         // record the image segment size
                std::cout << "len =: " << len << std::endl;
                data_.udpHeader.crc32val =
                    Crc32(crc_, reinterpret_cast<unsigned char *>(data_.data_area), sizeof(data_.data_area));
                std::cout << "data.head.crc32val=: " << data_.udpHeader.crc32val << std::endl;

            resend:
                if (SendTo((char *)&data_, sizeof(data_)) == 0) {
                    std::cout << "Send File Failed:" << std::endl;
                    break;
                }

                //                // receive verification information from server
                //                RecvFrom((char *)&pack_info, sizeof(pack_info));
                //                receive_id = pack_info.serial_number;
                //                // if the information from server gives a error, re-send
                //                if (pack_info.errorflag == 1) {
                //                    pack_info.errorflag = 0;
                //                    goto resend;
                //                }
                receive_id += 1;

                usleep(100);

            } else {
                break;
            }
        } else {
            // if the receive id is different to the send id
            if (SendTo((char *)&data_, sizeof(data_)) == 0) {
                std::cout << "Send File Failed:" << std::endl;
                break;
            }

            std::cout << "repeat send" << std::endl;

            //            // receive verification information from server again
            //            RecvFrom((char *)&pack_info, sizeof(pack_info));
            //            receive_id = pack_info.serial_number;

            // usleep(50000);
        }
    }

    // send byte 0 to Server to tell the information from Client is over
    data_.udpHeader.image_size = 0;
    if (SendTo((char *)&data_, sizeof(data_)) == 0) {
        std::cout << "Send 0 char Failed:" << std::endl;
    }
    std::cout << "client send file end 0 char" << std::endl;
    std::cout << "File Transfer Successful" << std::endl;

    return 1;
}

// save image
int UDPClient::SaveImage(std::string &file_name, std::string &save_path) {
    std::time_t t = std::time(0);  // t is an integer type
    std::string save_name = save_path + std::to_string(t) + ".jpg";
    // receive save command
    UDPHeader packInfo;
    packInfo.save_command = 0;
    std::cout << "packInfo.saveCommand = " << packInfo.save_command << std::endl;
    RecvFrom((char *)&packInfo, sizeof(packInfo));
    std::cout << "packInfo.saveCommand = " << packInfo.save_command << std::endl;
    if (packInfo.save_command == 1) {
        cv::Mat img = cv::imread(file_name, CV_LOAD_IMAGE_COLOR);
        cv::imwrite(save_name, img);
        std::cout << "save image " << save_name << std::endl;

        // send the saved information to server
        packInfo.save_command = 2;
        SendTo((char *)&packInfo, sizeof(packInfo));
    } else {
        packInfo.save_command = 3;
        SendTo((char *)&packInfo, sizeof(packInfo));
    }
    return 1;
}

// find the server ip
std::string UDPClient::NetworkFinder() {
    std::cout << "Find Server IP ..." << std::endl;
    setvbuf(stdout, NULL, _IONBF, 0);
    fflush(stdout);
    // newly create a sockaddr_in object
    struct sockaddr_in addrto;
    bzero(&addrto, sizeof(struct sockaddr_in));
    addrto.sin_family = AF_INET;
    addrto.sin_addr.s_addr = htonl(INADDR_ANY);
    addrto.sin_port = htons(6000);
    socklen_t len = sizeof(addrto);
    // create the socket: sock
    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        std::cout << "socket error..." << std::endl;
        exit(1);
    }
    // set the socket
    const int opt = -1;
    int nb = 0;
    nb = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt));
    if (nb == -1) {
        std::cout << "set socket errror..." << std::endl;
        exit(1);
    }
    // bind the specific port to listen the broadcast of server
    if (nb = bind(sock, (struct sockaddr *)&(addrto), len) == -1) {
        std::cout << "bind error..." << std::endl;
        exit(1);
    }
    // begin receive broadcast
    char msg[20] = {0};
    std::cout << "Receiving broadcast..." << std::endl;
    int ret = recvfrom(sock, msg, 20, 0, (struct sockaddr *)&addrto, &len);
    if (ret <= 0) {
        std::cout << "read error..." << std::endl;
    } else {
        printf("%s\n", msg);
        std::string server_ip = inet_ntoa(addrto.sin_addr);
        std::cout << "server ip = " << server_ip << std::endl;
        if (IsServerIPValid(server_ip)) {
            char sendMsg[5] = "1";
            ret = sendto(sock, sendMsg, strlen(sendMsg), 0, (struct sockaddr *)&addrto, len);
            if (ret < 0) {
                std::cout << "send stop broadcast fail" << std::endl;
            } else {
                std::cout << "send stop broadcast success" << std::endl;
            }
            close(sock);
            return server_ip;
        }
    }
    return "";
}

// Check the server IP whether it is valid
int UDPClient::IsServerIPValid(std::string &serverIP) {
    if (serverIP.empty()) {
        std::cout << "server IP is invalid" << std::endl;
        return 0;
    }
    char IP1[100], cIP[4];
    int len = serverIP.size();
    int i = 0, j = len - 1;
    int k, m = 0, n = 0, num = 0;
    const char *serverIPChar = serverIP.c_str();
    // get rid of the end char /0
    while (serverIPChar[i++] == ' ')
        ;
    while (serverIPChar[j--] == ' ')
        ;

    for (k = i - 1; k <= j + 1; k++) {
        IP1[m++] = *(serverIPChar + k);
    }
    IP1[m] = '\0';

    char *p = IP1;

    while (*p != '\0') {
        if (*p == ' ' || *p < '0' || *p > '9') return 0;
        // Save the first character of each subsection and use it to determine if the subsection starts with 0
        cIP[n++] = *p;

        int sum = 0;  // The value of each subsection and should be between 0 and 255.
        while (*p != '.' && *p != '\0') {
            if (*p == ' ' || *p < '0' || *p > '9') return 0;
            sum = sum * 10 + *p - 48;  // Convert each substring to an integer
            p++;
        }
        if (*p == '.') {
            if ((*(p - 1) >= '0' && *(p - 1) <= '9') &&
                (*(p + 1) >= '0' && *(p + 1) <= '9'))  // Determine if there is a number before and after ".", if not,
                                                       // it is an invalid IP, such as "1.1.127."
                num++;  // Record the number of occurrences of ".", which cannot be greater than 3
            else
                return 0;
        };
        if ((sum > 255) || (sum > 0 && cIP[0] == '0') || num > 3)
            // Invalid IP if the value of the subsection is >255 or a non-zero subsection beginning with 0 or the number
            // of "." >3
            return 0;

        if (*p != '\0') p++;
        n = 0;
    }
    if (num != 3) return false;
    return 1;
}

// main
int main(int argc, char *argv[]) {
    std::cout << "begin" << std::endl;
    // create a UDPClient object
    // UDPClient udpClient("INADDR_ANY", 7777);
    UDPClient udpClient("", 7777);
    size_t imgSerialNum = 0;
    // begin send images
    while (1) {
        // experience with local images
        // image path
        std::string path = "/home/beyoung/Desktop/mac-ubuntu-share/1_IDSCameraDev/3_SAPro_ImageSave75/";
        std::string file_name = path + std::to_string(imgSerialNum) + ".jpg";
        std::cout << "file name: " << file_name << std::endl;
        std::string savePath = "/home/beyoung/Desktop/mac-ubuntu-share/1_IDSCameraDev/7_UDPChooseSave/";
        // open image
        FILE *fp = fopen(file_name.c_str(), "r");
        //        char buffTmp[100];
        //        if (fread(buffTmp, sizeof(char), 100, fp) == 0) {
        //            std::cout << "image " << file_name << " is empty, jump it" << std::endl;
        //            imgSerialNum += 1;
        //            continue;
        //        }
        // send one image -- // ClientConnectToFA::SendDataToServer in SA project
        udpClient.SendOneImg(fp);
        fclose(fp);
        // save one image
        udpClient.SaveImage(file_name, savePath);
        imgSerialNum += 1;
    }

    udpClient.CloseSocket();
    return 1;
}

#include <iostream>
int submain(int argc, char *argv[]) {
    size_t serialNum = 0;
    std::string ImgFolder =
        "/home/beyoung/Desktop/mac-ubuntu-share/work/5_sensorCalibration/8_RDB-43348-ToFA/imageSave/";
    FILE *fq;
    char sendBuff[60000] = "IamClient";
    char recBuff[60000];
    UDPClient udpClient("INADDR_ANY", 7777);
    udpClient.InitSocket();
    while (1) {
        // open image
        std::string imageName = ImgFolder + std::to_string(serialNum) + ".jpg";
        if ((fq = fopen(imageName.c_str(), "rb")) == NULL) {
            std::cout << "no such a image" << std::endl;
            printf("File open.\n");
            break;
        } else if (!feof(fq)) {
            fread(sendBuff, 1, sizeof(sendBuff), fq);
            std::cout << "image " << serialNum << " has been wrote to the buffer" << std::endl;
        }
        // send image to server
        if (udpClient.SendTo(sendBuff, sizeof(sendBuff))) {
            std::cout << "Client has sent a message" << std::endl;
        } else {
            std::cout << "Client send info fail" << std::endl;
            break;
        }
        if (udpClient.RecvFrom(recBuff, sizeof(recBuff))) {
            std::cout << "Client has sent a message to Server" << std::endl;
        } else {
            std::cout << "Client sent a message to Server fail" << std::endl;
            break;
        }
        serialNum += 1;
    }

    udpClient.CloseSocket();
    std::cout << "Client close" << std::endl;
    return 1;
}

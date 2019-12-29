//#define BOOST_ASIO_NO_DEPRECATED
#include <iostream>
#include "boost/asio.hpp"
#include "boost/thread.hpp"

using namespace boost::asio;

void echoExample() {
    // ip::udp::endpoint ep(ip::address::from_string("127.0.0.1"), 8001);

    // void sync_echo(std::string msg) {
    //    io_service service;
    //    ip::udp::socket sock(service, ip::udp::endpoint(ip::udp::v4(), 0));
    //    sock.send_to(buffer(msg), ep);
    //    char buff[1024];
    //    ip::udp::endpoint sender_ep;
    //    int bytes = sock.receive_from(buffer(buff), sender_ep);
    //    std::string copy(buff, bytes);
    //    std::cout << "server echoed our " << msg << ": " << (copy == msg ? "OK" : "FAIL") << std::endl;
    //    sock.close();
    //}

    // int main(int argc, char* argv[]) {
    //    char* messages[] = {"John says hi", "so does James", "Lucy got home", 0};
    //    boost::thread_group threads;
    //    for (char** message = messages; *message; ++message) {
    //        threads.create_thread(boost::bind(sync_echo, *message));
    //        boost::this_thread::sleep(boost::posix_time::millisec(100));
    //    }
    //    threads.join_all();
    //    return 1;
    //}
}

/**
 * @brief Data collection command, received from FA and EM, which used to control the data collection process
 */
enum CommandType {
    COMMAND_NONE = 0,
    COMMAND_CAPTURE_NORMAL_IMAGE = 5,
    COMMAND_CAPTURE_SPECIAL_IMAGE = 10,
    COMMAND_BROADCAST = 11,
    COMMAND_BROADCAST_RESPONSE = 12,
    COMMAND_COUNT
};

const int gMaxUdpPakcetSize = 1024 * 30;  // packet size of udp sender
const int resize_scale = 4;               // image resize size
const int kRecvBufferSize = 16;           // command receiver buffer size

#pragma pack(push, 1)  // memory alignment
/**
  @ @brief tag udp header. information of udp packet
 */
typedef struct tagUDPHeader {
    uint32_t uIndex;           // index
    uint32_t uTotalDataLen;    // total data size
    uint32_t uCurrentDataLen;  // current sent size
    uint16_t uSplitCount;      // split count
    uint16_t uCurrentIndex;    // current packet sub index
    uint32_t uCurrentOffset;   // current packet data offset
    uint16_t uFinish;          // finish flag，0：finish，1：not finish
} UDPHeader;

/**
  @ @brief tag transmit data. struct to contain the information transmit to FA
 */
typedef struct tagTransmitData {
    UDPHeader udpHeader;                // one UDPHeader to contain the transmit data information
    char data_area[gMaxUdpPakcetSize];  // including the header and image
} TransmitData;

/**
  @ @brief sa request header. information of the response between FA and Sa without any data
 */
struct SARequestHeader {
    uint32_t request_seq;
    CommandType request_type;
    // save command: 0=ignore, 1=save normal, 2=save special
};
#pragma pack(pop)  // memory alignment

/**
 * @brief The client class
 */
class UDPClient {
   public:
    /**
     * @brief UDPClient
     * @param server_ip
     * @param port_number
     * @param service
     */
    UDPClient(std::string& server_ip, int port_number, io_service& service)
        : /*server_ip_(server_ip),
          port_number_(port_number),*/
          sock_(service, ip::udp::endpoint(ip::udp::v4(), 0)),
          server_ep_(ip::udp::endpoint(ip::address::from_string(server_ip), port_number)) {
        server_ip_ = server_ip;
        port_number_ = port_number;
        sock_ = ip::udp::socket(service, ip::udp::endpoint(ip::udp::v4(), 0));
        server_ep_ = ip::udp::endpoint(ip::address::from_string(server_ip), port_number);
    }

    /**
     * @brief InitSocket
     */
    void InitSocket() { ip::udp::endpoint ep(ip::address::from_string(server_ip_), port_number_); }

    /**
     * @brief RecvFrom
     * @param saRequest
     * @return
     */
    int RecvFrom(SARequestHeader& saRequest);

    /**
     * @brief SendTo
     * @param msg_send_buffer
     * @param length
     * @return
     */
    int SendTo(void* msg_send_buffer, int length) {
        //        char* msg;
        //        msg = new char[sizeof(UDPHeader) + gMaxUdpPakcetSize];
        //        memcpy(msg, msg_send_buffer, sizeof(UDPHeader) + gMaxUdpPakcetSize);
        //        std::cout << msg_send_buffer << std::endl;
        //        char* msg = (char*)&msg_send_buffer;
        //        std::cout << msg_send_buffer << std::endl;
        sock_.send_to(buffer(msg_send_buffer, length), server_ep_);
        return 0;
    }

    /**
     * @brief CloseSocket
     */
    void CloseSocket() { sock_.close(); }

    /**
     * @brief loadImage
     */
    void loadImage() {}

   private:
    std::string server_ip_;
    int port_number_;
    ip::udp::socket sock_;
    ip::udp::endpoint server_ep_;
};

/**
 * @brief main
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char* argv[]) {
    io_service service;
    std::string server_ip = "127.0.0.1";
    int server_port = 8000;
    UDPClient udpClient(server_ip, server_port, service);
    char* msg_str = "hello motro";
    //    TransmitData* transData;
    //    transData->udpHeader.uIndex = 0;
    //    transData->udpHeader.uFinish = 0;
    //    transData->udpHeader.uSplitCount = 1;
    //    transData->udpHeader.uCurrentIndex = 0;
    //    transData->udpHeader.uTotalDataLen = gMaxUdpPakcetSize;
    //    transData->udpHeader.uCurrentOffset = 0;
    //    transData->udpHeader.uCurrentDataLen = gMaxUdpPakcetSize;
    //    memcpy(transData->data_area, msg_str.c_str(), gMaxUdpPakcetSize);
    udpClient.SendTo(msg_str, gMaxUdpPakcetSize);
    return 0;
}

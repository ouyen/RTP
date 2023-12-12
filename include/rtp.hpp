#ifndef __RTP_H
#define __RTP_H

#include <stdint.h>
#include <chrono>
#include <deque>
#include <memory>
#include "crc32.hpp"
#include "udp.hpp"

// #ifdef __cplusplus
extern "C" {
// #endif

#define PAYLOAD_MAX 1461

// flags in the rtp header
typedef enum RtpHeaderFlag {
    RTP_SYN = 0b0001,
    RTP_ACK = 0b0010,
    RTP_FIN = 0b0100,
} rtp_header_flag_t;

typedef struct __attribute__((__packed__)) RtpHeader {
    uint32_t seq_num;   // Sequence number
    uint16_t length;    // Length of data; 0 for SYN, ACK, and FIN packets
    uint32_t checksum;  // 32-bit CRC
    uint8_t flags;      // See at `RtpHeaderFlag`
} rtp_header_t;

typedef struct __attribute__((__packed__)) RtpPacket {
    rtp_header_t rtp;           // header
    char payload[PAYLOAD_MAX];  // data
} rtp_packet_t;

// #ifdef __cplusplus
}
// #endif
class SlidingWindow {
   private:
    int window_size;
    // deque<string> window;
   protected:
    deque<bool> acked;

   public:
    SlidingWindow(int window_size) : window_size(window_size) {
        // window.resize(window_size);
        acked.resize(window_size);
    }
    int get_window_size() const { return window_size; }
    void slide() {
        while (acked.front()) {
            acked.pop_front();
            acked.push_back(0);
            // window.pop_front();
        }
    }
};

enum RTPMode {
    GBN = 0,
    SR = 1,
};

class RTP {
   public:
    std::unique_ptr<UDPSocket> udp_socket;
    // unique_ptr<SlidingWindow> sliding_window;
    SlidingWindow sliding_window;
    RTPMode mode;
    uint32_t seq_num_start = 0;
    inline uint32_t seq_biased_index(uint32_t index) {
        return seq_num_start + index + 1;
    }

    const static int MAX_DATA_LEN = 1461;
    RTP(int windows_size, int mode) : sliding_window(windows_size) {
        this->mode = static_cast<RTPMode>(mode);
    }
    static string make_head(uint32_t seq_num,
                            uint16_t length,
                            uint8_t check_sum,
                            uint8_t flags) {
        rtp_header_t header;
        header.seq_num = seq_num;
        header.length = length;
        header.flags = flags;
        header.checksum = check_sum;
        return string((char*)&header, sizeof(rtp_header_t));
    }
    string make_packet(uint32_t seq_num, const string& full_data) {
        rtp_packet_t packet;
        memset(&packet, 0, sizeof(rtp_packet_t));
        string payload = full_data.substr(
            (seq_num - seq_num_start - 1) * MAX_DATA_LEN, MAX_DATA_LEN);

        packet.rtp.seq_num = seq_num;
        packet.rtp.length = payload.size();
        packet.rtp.flags = 0;
        packet.rtp.checksum = compute_checksum(payload);

        memcpy(packet.payload, payload.c_str(), payload.size());
    }

    rtp_header_t receive_head() {
        string data = udp_socket->receive();
        rtp_header_t header;
        // fill with 0
        memset(&header, 0, sizeof(rtp_header_t));
        // if (data.size() < sizeof(rtp_header_t))
        memcpy(&header, data.c_str(), min(sizeof(rtp_header_t), data.size()));
        return header;
    }

    bool check_time_out(std::chrono::_V2::system_clock::time_point start,int ms=100){
        auto end=std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();
        return (duration>ms);
    }

    bool send_wait_for_reply(const string& data,
                             uint32_t target_seq_num,
                             uint32_t target_flag) {
        for (uint32_t i = 0; i <= MAX_TRY; ++i) {
            udp_socket->send(data);
            LOG_DEBUG("Data sent, wait for reply: %u\n", target_seq_num);
            // auto start = chrono::high_resolution_clock::now();
            udp_socket->set_timeout(0, 100);
            rtp_header_t header = receive_head();
            if (header.flags == target_flag &&
                header.seq_num == target_seq_num) {
                LOG_DEBUG("Reply received: %u\n", target_seq_num);
                return 0;
            }
            // auto end = std::chrono::high_resolution_clock::now();
            // auto duration =
            //     std::chrono::duration_cast<std::chrono::milliseconds>(end -
            //                                                           start)
            //         .count();
            // if (duration > 100) {
            //     LOG_DEBUG("timeout\n");
            //     continue;
            // }

            if (i == MAX_TRY) {
                LOG_FATAL("MAX_TRY\n");
                return 1;
            } else {
                LOG_DEBUG("timeout\n");
            }
        }
        return 1;
    }
    const uint32_t MAX_TRY = 50;
};

class RTPServer : private RTP {
   private:
    // UDPServer udp_server;

   public:
    RTPServer(int port, int window_size, int mode);
    // ~RTPServer();
    void receive(const string& filepath);
    void close();
};

class RTPClient : private RTP {
   private:
    // UDPClient udp_client;

   public:
    RTPClient(const string& ip, int port, int window_size, int mode);
    // ~RTPClient();
    // int send(const string& buf, uint64_t length = -1);
    int send(const string& filepath);
    int send_gbn(const string& filepath);
    int send_sr(const string& filepath);

    void close();
};

#endif  // __RTP_H

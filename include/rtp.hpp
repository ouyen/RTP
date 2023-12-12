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

class CRC32 {
    static uint32_t crc32_for_byte(uint32_t r) {
        for (int j = 0; j < 8; ++j)
            r = (r & 1 ? 0 : (uint32_t)0xEDB88320L) ^ r >> 1;
        return r ^ (uint32_t)0xFF000000L;
    }

    static void crc32(const void* data, size_t n_bytes, uint32_t* crc) {
        static uint32_t table[0x100];
        if (!*table)
            for (size_t i = 0; i < 0x100; ++i)
                table[i] = crc32_for_byte(i);
        for (size_t i = 0; i < n_bytes; ++i)
            *crc = table[(uint8_t)*crc ^ ((uint8_t*)data)[i]] ^ *crc >> 8;
    }

   public:
    // Computes checksum for `n_bytes` of data
    //
    // Hint 1: Before computing the checksum, you should set everything up
    // and set the "checksum" field to 0. And when checking if a packet
    // has the correct check sum, don't forget to set the "checksum" field
    // back to 0 before invoking this function.
    //
    // Hint 2: `len + sizeof(rtp_header_t)` is the real length of a rtp
    // data packet.
    uint32_t operator()(const string& data) {
        const void* pkt = data.c_str();
        size_t n_bytes = data.size();
        uint32_t crc = 0;
        crc32(pkt, n_bytes, &crc);
        return crc;
    }
    uint32_t operator()(const char* data, size_t n_bytes) {
        const void* pkt = data;
        uint32_t crc = 0;
        crc32(pkt, n_bytes, &crc);
        return crc;
    }
    uint32_t operator()(const struct RtpHeader& head) {
        return (*this)((char*)(&head), 11);
    }

    uint32_t operator()(const struct RtpPacket& packet) {
        uint16_t len = packet.rtp.length;
        return (*this)((char*)(&packet), sizeof(struct RtpHeader) + len);
    }

    static bool check_crc(struct RtpHeader& head) {
        uint32_t crc = head.checksum;
        head.checksum = 0;
        uint32_t cal_crc = 0;
        crc32(&head, 11, &cal_crc);
        bool result = (crc == cal_crc);
        head.checksum = crc;
        return result;
    }
    static bool check_crc(struct RtpPacket& packet) {
        uint32_t crc = packet.rtp.checksum;
        packet.rtp.checksum = 0;
        uint16_t len = packet.rtp.length;

        uint32_t cal_crc = 0;
        crc32(&packet, 11 + len, &cal_crc);

        bool result = (crc == cal_crc);
        packet.rtp.checksum = crc;
        return result;
    }
};

static CRC32 compute_checksum;

class SlidingWindow {
   private:
    uint32_t window_size;
    // deque<string> window;
   protected:
    deque<bool> acked;

   public:
    SlidingWindow(uint32_t window_size) : window_size(window_size) {
        // window.resize(window_size);
        acked.resize(window_size);
    }
    uint32_t get_window_size() const { return window_size; }
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
    uint32_t seq_biased_index(uint32_t index) {
        return seq_num_start + index + 1;
    }

    const static int MAX_DATA_LEN = 1461;
    RTP(int windows_size, int mode) : sliding_window(windows_size) {
        this->mode = static_cast<RTPMode>(mode);
    }
    static string make_head(uint32_t seq_num, uint8_t flags) {
        rtp_header_t header;
        // set to zero
        memset(&header, 0, sizeof(rtp_header_t));
        header.seq_num = seq_num;
        header.length = 0;
        header.flags = flags;
        header.checksum = compute_checksum(header);
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
        // packet.rtp.checksum = compute_checksum(payload);

        memcpy(packet.payload, payload.c_str(), payload.size());

        packet.rtp.checksum = compute_checksum(packet);

        return string((char*)&packet);
    }

    int send_packet(uint32_t seq_num, const string& full_data) {
        rtp_packet_t packet;
        memset(&packet, 0, sizeof(rtp_packet_t));
        string payload = full_data.substr(
            (seq_num - seq_num_start - 1) * MAX_DATA_LEN, MAX_DATA_LEN);

        packet.rtp.seq_num = seq_num;
        packet.rtp.length = payload.size();
        packet.rtp.flags = 0;
        // packet.rtp.checksum = compute_checksum(payload);

        memcpy(packet.payload, payload.c_str(), payload.size());

        packet.rtp.checksum = compute_checksum(packet);

        return udp_socket->send(string((char*)&packet.rtp, 11) + payload,
                                11 + payload.size());
    }

    rtp_header_t receive_head() {
        string data = udp_socket->receive(11);
        rtp_header_t header;
        // fill with 0
        memset(&header, 0, sizeof(rtp_header_t));
        // if (data.size() < sizeof(rtp_header_t))
        memcpy(&header, data.c_str(), min(sizeof(rtp_header_t), data.size()));
        // if (data == "") {
        //     header.flags = 0x11;
        // }
        return header;
    }

    bool check_time_out(std::chrono::_V2::system_clock::time_point start,
                        int ms = 100) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();
        return (duration > ms);
    }

    bool send_wait_for_reply(const string& data,
                             uint32_t target_seq_num,
                             uint32_t target_flag) {
        for (uint32_t i = 0; i <= MAX_TRY; ++i) {
            int send_return = udp_socket->send(data);
            if (send_return == -1) {
                LOG_FATAL("Send failed\n");
                return 1;
            }
            LOG_DEBUG("Data sent %u, wait for ACK: %u\n", target_seq_num,
                      target_seq_num);
            // auto start = chrono::high_resolution_clock::now();
            udp_socket->set_timeout(0, 100);
            rtp_header_t header = receive_head();
            if (CRC32::check_crc(header) && header.flags == target_flag &&
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
                LOG_FATAL("Timeout, send again\n");
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
    uint32_t final_seq_num = 0;

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

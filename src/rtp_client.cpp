#include <chrono>
#include <ctime>
#include <random>
#include "crc32.hpp"
#include "file.hpp"
#include "rtp.hpp"
#include "util.h"

using namespace std;

// server is receiver
// client is sender
RTPClient::RTPClient(const string& ip, int port, int window_size, int mode)
    : RTP(window_size, mode) {
    udp_socket = make_unique<UDPClient>(ip, port);

    // random seq_num_start
    random_device os_seed;
    const uint_least32_t seed = os_seed();
    mt19937 eng(seed);
    uniform_int_distribution<> dist(0, 0xffffff);
    uint32_t seq_num_start = dist(eng);
    LOG_DEBUG("seq_num_start: %u\n", seq_num_start);

    // send SYN
    string data = RTP::make_head(seq_num_start, 0, 0, RTP_SYN);
    if (!send_wait_for_reply(data, seq_num_start + 1, RTP_SYN | RTP_ACK)) {
        data = RTP::make_head(seq_num_start + 1, 0, 0, RTP_ACK);
        // usleep(200);
        // for (;;)
        {
            // sleep(1);
            udp_socket->send(data);
            LOG_DEBUG("Send ACK: %u\n", seq_num_start + 1);
        }
    }
}

int RTPClient::send(const string& filepath) {
    if (mode == GBN) {
        return send_gbn(filepath);
    } else if (mode == SR) {
        return send_sr(filepath);
    } else {
        LOG_FATAL("Unknown mode\n");
        return 1;
    }
}

int RTPClient::send_gbn(const string& filepath) {
    string file_data = file_to_string(filepath);
    uint32_t file_size = file_data.size();

    int max_index =
        file_size / RTP::MAX_DATA_LEN + bool(file_size % RTP::MAX_DATA_LEN);

    LOG_DEBUG("file_size: %u , slide to %u pieces\n", file_size, max_index);

    // send file
    for (int same_window_count = 0; same_window_count < MAX_TRY;
         ++same_window_count) {
        // send data in window
        int sliding_window_start = 0;
        for (int i = sliding_window_start;
             i < sliding_window.get_window_size() + sliding_window_start; ++i) {
            if (i >= max_index) {
                break;
            }

            udp_socket->send(make_packet(seq_biased_index(i), file_data));
            LOG_DEBUG("Data sent: %u\n", seq_biased_index(i));
        }

        // wait for response
        int curr_max_reply = sliding_window_start;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < MAX_TRY; ++i) {
            udp_socket->set_timeout(0, 100);
            rtp_header_t header = receive_head();
            if (header.checksum == 0 && header.length == 0) {
                if (header.flags == RTP_ACK) {
                    LOG_DEBUG("ACK received: %u\n", header.seq_num);
                    if (header.seq_num - 1 - seq_num_start > curr_max_reply) {
                        curr_max_reply = header.seq_num - 1 - seq_num_start;
                        start = std::chrono::high_resolution_clock::now();
                    }
                } else if (sliding_window_start == 0 &&
                           header.flags == RTP_SYN | RTP_ACK &&
                           header.seq_num == seq_num_start + 1) {
                    LOG_DEBUG("SYNACK received: %u\n", header.seq_num);
                    auto data =
                        RTP::make_head(seq_num_start + 1, 0, 0, RTP_ACK);
                    {
                        udp_socket->send(data);
                        LOG_DEBUG("Send ACK: %u\n", seq_num_start + 1);
                    }
                }
            }
            if (check_time_out(start)) {
                LOG_DEBUG("Timeout\n");
                break;
            }
        }
        if (curr_max_reply != sliding_window_start) {
            --same_window_count;
        }
        sliding_window_start = curr_max_reply + 1;
        if (sliding_window_start >= max_index) {
            LOG_DEBUG("All data sent\n");
            break;
        }
    }
}

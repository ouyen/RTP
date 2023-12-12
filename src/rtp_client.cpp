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
    seq_num_start = dist(eng);

    seq_num_start = 0x4c2e35d0;

    LOG_DEBUG("seq_num_start: %u\n", seq_num_start);

    // send SYN
    string data = RTP::make_head(seq_num_start, RTP_SYN);

    LOG_DEBUG("Send SYN: %u\n", seq_num_start);
    if (!send_wait_for_reply(data, seq_num_start + 1, RTP_SYN | RTP_ACK)) {
        data = RTP::make_head(seq_num_start + 1, RTP_ACK);
        // usleep(200);
        // for (;;)
        {
            // sleep(1);
            udp_socket->send(data);
            LOG_DEBUG("Send ACK for SYN: %u\n", seq_num_start + 1);
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

    uint32_t max_index =
        file_size / RTP::MAX_DATA_LEN + bool(file_size % RTP::MAX_DATA_LEN);

    final_seq_num = seq_biased_index(max_index);
    // cout<<"final_seq_num: "<<final_seq_num<<endl;
    // cout<<"start: "<<seq_num_start<<endl;

    LOG_DEBUG("file_size: %u , slide to %u pieces\n", file_size, max_index);
    uint32_t sliding_window_start = 0;
    // send file
    for (uint32_t same_window_count = 0; same_window_count < MAX_TRY;
         ++same_window_count) {
        // send data in window

        for (uint32_t i = sliding_window_start;
             i < sliding_window.get_window_size() + sliding_window_start; ++i) {
            if (i >= max_index) {
                break;
            }
            LOG_DEBUG("Data sent: %u\n", seq_biased_index(i));
            send_packet(seq_biased_index(i), file_data);
        }

        // wait for response
        uint32_t curr_max_reply = sliding_window_start;

        auto start = std::chrono::high_resolution_clock::now();

        for (uint32_t i = 0; i < MAX_TRY; ++i) {
            // udp_socket->set_timeout(0, 0);
            rtp_header_t header = receive_head();
            if (header.length == 0) {
                LOG_DEBUG("Recevie flag:%u\n", uint32_t(header.flags));
                if (header.flags == 0) {
                    LOG_DEBUG("No ACK in 100ms\n");
                    continue;
                }
                if (CRC32::check_crc(header) == false) {
                    LOG_DEBUG("CRC ERROR\n");
                    continue;
                }
                if (header.flags == RTP_ACK) {
                    uint32_t expect_next = (header.seq_num - 1 - seq_num_start);
                    LOG_DEBUG("ACK received: %u, Except: %u\n", header.seq_num,
                              expect_next);
                    if ((header.seq_num - 1 - seq_num_start) > curr_max_reply) {
                        curr_max_reply = header.seq_num - 1 - seq_num_start;
                        start = std::chrono::high_resolution_clock::now();
                        // break;
                    }
                } else if (sliding_window_start == 0 &&
                           header.flags == (RTP_SYN | RTP_ACK) &&
                           header.seq_num == seq_num_start + 1) {
                    LOG_DEBUG("SYNACK received: %u\n", header.seq_num);
                    auto data = RTP::make_head(seq_num_start + 1, RTP_ACK);
                    {
                        udp_socket->send(data);
                        LOG_DEBUG("Send ACK: %u\n", seq_num_start + 1);
                    }
                }
            }
            if (check_time_out(start)) {
                LOG_DEBUG("Sliding Windows Timeout\n");
                break;
            }
        }

        if (curr_max_reply != sliding_window_start) {
            --same_window_count;
        }
        LOG_DEBUG("Sliding Window start:from %u to %u\n", sliding_window_start,
                  curr_max_reply);
        sliding_window_start = curr_max_reply;
        if (sliding_window_start >= max_index) {
            LOG_DEBUG("All data sent\n");
            break;
        }
    }
    return 0;
}

int RTPClient::send_sr(const string& filepath) {
    return send_gbn(filepath);
}

void RTPClient::close() {
    LOG_DEBUG("Send FIN: %u\n", final_seq_num);
    string data = RTP::make_head(final_seq_num, RTP_FIN);
    bool result = send_wait_for_reply(data, final_seq_num, RTP_FIN | RTP_ACK);
    LOG_DEBUG("Fin ack received?: %d\n", result);
    udp_socket->close();
}
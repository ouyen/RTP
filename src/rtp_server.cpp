#include <chrono>
#include <string>
#include <vector>
#include "crc32.hpp"
#include "file.hpp"
#include "rtp.hpp"
#include "util.h"

using namespace std;

// server is receiver
// client is sender
RTPServer::RTPServer(int port, int window_size, int mode)
    : RTP(window_size, mode) {
    udp_socket = make_unique<UDPServer>(port);
    // udp_socket->bind(port);
    // this->window_size = window_size;
    // this->mode = mode;

    // wait for client to connect
    // receive SYN
    for (;;) {
        udp_socket->set_timeout(5, 0);
        rtp_header_t header = receive_head();
        if (header.flags == RTP_SYN) {
            LOG_DEBUG("SYN received\n");
            seq_num_start = header.seq_num;
            LOG_DEBUG("seq_num_start: %u\n", seq_num_start);
            cout << "checksum: " << header.checksum << endl;
            cout << "seq_num: " << header.seq_num << endl;
            cout << "flags: " << uint32_t(header.flags) << endl;
            cout << "length: " << header.length << endl;
            // send SYNACK

            string reply = RTP::make_head(seq_num_start + 1, RTP_SYN | RTP_ACK);
            send_wait_for_reply(reply, seq_num_start + 1, RTP_ACK);
            break;
        } else {
            LOG_FATAL("No SYN received\n");
            return;
        }
    }
}

void RTPServer::receive(const string& filepath) {
    for (;;) {
        udp_socket->set_timeout(0, 100);
        rtp_packet_t packet = receive_packet();

        LOG_DEBUG("FLAG: get someting\n");
        LOG_DEBUG("SEQ: %u;LEN:%u ; CRC:%u; FLAG:%u\n", packet.rtp.seq_num,
                  packet.rtp.length, packet.rtp.checksum, packet.rtp.flags);

        if (packet.rtp.checksum == 0 && packet.rtp.flags == 0 &&
            packet.rtp.length == 0 && packet.rtp.seq_num == 0) {
            LOG_DEBUG("Null\n");
            continue;
        }

        if (sliding_window.too_large(packet.rtp.seq_num - seq_num_start - 1)) {
            LOG_DEBUG("Out of range\n");
            continue;
        }

        LOG_DEBUG("CRC check\n");

        // uint32_t crc = packet.rtp.checksum;
        // packet.rtp.checksum = 0;
        // uint32_t cal_crc = 0;
        // CRC32::crc32(&packet, 11 + packet.rtp.length, &cal_crc);

        // bool result = (crc == cal_crc);
        // packet.rtp.checksum = crc;
        if (CRC32::check_crc(packet) == false) {
        // if (result == false) {
            LOG_DEBUG("CRC check failed\n");
            continue;
        }
        auto flag = packet.rtp.flags;

        // LOG_DEBUG("FLAG: %u\n", flag);

        switch (flag) {
            case RTP_FIN:
                LOG_DEBUG("FIN received\n");

                LOG_DEBUG("Send FINACK\n");
                udp_socket->send(
                    RTP::make_head(packet.rtp.seq_num, RTP_FIN | RTP_ACK));
                sliding_window.save_to_file(filepath);
                return close();
            case 0: {
                uint32_t seq = packet.rtp.seq_num;
                if (seq == 0) {
                    LOG_DEBUG("Null\n");
                    continue;
                }
                uint32_t index = seq - seq_num_start - 1;
                LOG_DEBUG("Receive Data,index: %u, SEQ: %u\n", index, seq);

                // TODO: check if out of range
                if (sliding_window.too_large(index)) {
                    LOG_DEBUG("Out of range\n");
                    continue;
                } else {
                    if (!sliding_window.is_old(index)) {
                        if (sliding_window.is_set(index) == true) {
                            LOG_DEBUG("Already received\n");
                        } else {
                            LOG_DEBUG("New data\n");
                            sliding_window.set(index, packet);
                        }
                    }
                    uint32_t reply_seq =
                        (mode == GBN)
                            ? seq_num_start + 1 + sliding_window.get_start()
                            : seq;
                    udp_socket->send(RTP::make_head(reply_seq, RTP_ACK));
                    LOG_DEBUG("Send ACK for %u ;reply_seq:%u\n", seq,
                              reply_seq);
                }
            } break;
            case RTP_ACK:
                LOG_DEBUG("ACK received\n");
                break;
            default:
                LOG_DEBUG("Unknown flag\n");
                break;
        }
    }
}

void RTPServer::close() {
    udp_socket->close();
}
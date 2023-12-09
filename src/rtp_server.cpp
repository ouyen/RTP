#include <chrono>
#include "crc32.hpp"
#include "rtp.hpp"
#include "util.h"

using namespace std;

// server is receiver
// client is sender
RTPServer::RTPServer(int port, int window_size, int mode) {
    udp_socket = make_unique<UDPServer>(port);
    // udp_socket->bind(port);
    // this->window_size = window_size;
    // this->mode = mode;

    // wait for client to connect
    // receive SYN
    for (;;) {
        rtp_header_t header = receive_head();
        if (header.flags == RTP_SYN) {
            LOG_DEBUG("SYN received\n");
            seq_num_start = header.seq_num;
            LOG_DEBUG("seq_num_start: %u\n", seq_num_start);
            // send SYNACK

            string reply =
                RTP::make_head(seq_num_start + 1, 0, 0, RTP_SYN | RTP_ACK);
            send_wait_for_reply(reply, seq_num_start + 1, RTP_ACK);
            break;
        }
    }
}
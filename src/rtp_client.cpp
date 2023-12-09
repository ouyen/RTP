#include <chrono>
#include "crc32.hpp"
#include "rtp.hpp"
#include "util.h"
#include <random>

using namespace std;

// server is receiver
// client is sender
RTPClient::RTPClient(const string& ip,int port,int windows_size,int mode){
    udp_socket = make_unique<UDPClient>(ip,port);

    // random seq_num_start
    random_device os_seed;
    const uint_least32_t seed = os_seed();
    mt19937 eng(seed);
    uniform_int_distribution<> dist(0, 0xffffff);
    uint32_t seq_num_start = dist(eng);
    LOG_DEBUG("seq_num_start: %u\n", seq_num_start);

    // send SYN
    string data = RTP::make_head(seq_num_start, 0, 0, RTP_SYN);
    if(!send_wait_for_reply(data, seq_num_start+1, RTP_SYN | RTP_ACK)){
        data = RTP::make_head(seq_num_start+1, 0, 0, RTP_SYN);
        udp_socket->send(data);
    }
}
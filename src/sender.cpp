#include "rtp.hpp"
#include "util.h"

int main(int argc, char** argv) {
    if (argc != 6) {
        LOG_FATAL(
            "Usage: ./sender [receiver ip] [receiver port] [file path] "
            "[window size] [mode]\n");
    }

    // your code here
    string ip = argv[1];
    int port = atoi(argv[2]);
    string file_path = argv[3];
    int window_size = atoi(argv[4]);
    int mode = atoi(argv[5]);

    RTPClient client(ip,port, window_size, mode);
    client.send(file_path);

    LOG_DEBUG("Sender: exiting...\n");
    return 0;
}

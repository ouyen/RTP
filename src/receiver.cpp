#include "rtp.hpp"
#include "util.h"
#include<string>
using namespace std;

int main(int argc, char **argv) {
    // LOG_MSG("%d\n",2);
    if (argc != 5) {
        LOG_FATAL("Usage: ./receiver [listen port] [file path] [window size] "
                  "[mode]\n");
    }

    // your code here
    int port = atoi(argv[1]);
    string file_path = argv[2];
    int window_size = atoi(argv[3]);
    int mode = atoi(argv[4]);

    RTPServer server(port, window_size, mode);

    LOG_DEBUG("Receiver: exiting...\n");
    return 0;
}

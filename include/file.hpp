#pragma once

#include <deque>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>
#include "rtp.hpp"

using namespace std;

inline string file_to_string(const string& path) {
    std::ifstream t(path);
    return string((std::istreambuf_iterator<char>(t)),
                  std::istreambuf_iterator<char>());
}

inline void string_to_file(const string& path, const string& data) {
    ofstream fout(path);
    fout << data;
    fout.close();
}

inline void string_to_file(ofstream& fout, const string& data) {
    fout << data;
}

inline void string_list_to_file(const string& path,
                                const vector<string>& data) {
    ofstream fout(path);
    for (auto& s : data) {
        fout << s.substr(0, 1461);
    }
    // fout<<ends;
    fout.close();
}

// inline void packet_list_to_string(const string& path,
//                                   const vector<rtp_packet_t>& data) {
//     ofstream fout(path);
//     for (auto& s : data) {
//         fout << s.payload;
//     }
//     // fout<<ends;
//     fout.close();
// }
#include <deque>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>

using namespace std;

string file_to_string(const string& path) {
    std::ifstream t(path);
    return string((std::istreambuf_iterator<char>(t)),
            std::istreambuf_iterator<char>());
}

void string_to_file(const string& path,const string& data){
    ofstream fout(path);
    fout << data;
    fout.close();
}

void string_to_file( ofstream& fout,const string& data){
    fout << data;
}
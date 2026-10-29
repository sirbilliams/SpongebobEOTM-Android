#include "Sb2File.h"
#include <string.h>

namespace sbscript {

bool sb2_decode(const unsigned char* data, unsigned size, std::vector<std::string>& records, std::string& err)
{
    records.clear();
    err.clear();
    if (size < 2) { err = "file shorter than the record count"; return false; }
    unsigned count = (unsigned)data[0] | ((unsigned)data[1] << 8);
    unsigned pos = 2;
    for (unsigned i = 0; i < count; ++i) {
        if (pos + 2 > size) { err = "truncated record header"; return false; }
        int len = (short)((unsigned)data[pos] | ((unsigned)data[pos + 1] << 8));
        pos += 2;
        if (len > 511) { err = "record longer than the 512-byte buffer"; return false; }
        if (len < 0) len = 0;
        if (pos + (unsigned)len > size) { err = "truncated record body"; return false; }
        char buf[512];
        memset(buf, 0, sizeof buf);
        for (int k = 0; k < len; ++k) buf[k] = (char)(data[pos + k] ^ 0x30);
        pos += (unsigned)len;
        records.push_back(std::string(buf));   // C string: stops at the first NUL
    }
    return true;
}

SbTok::SbTok(const std::string& line, const char* delims) : s_(line), delims_(delims), pos_(0) {}

bool SbTok::next(std::string& out)
{
    while (pos_ < s_.size() && delims_.find(s_[pos_]) != std::string::npos) ++pos_;
    if (pos_ >= s_.size()) return false;
    unsigned start = pos_;
    while (pos_ < s_.size() && delims_.find(s_[pos_]) == std::string::npos) ++pos_;
    out = s_.substr(start, pos_ - start);
    if (pos_ < s_.size()) ++pos_;   // strtok overwrites the delimiter with NUL
    return true;
}

}  // namespace sbscript

/*  .sb2 record container and the strtok-style field splitter shared by the script, dialogue,
    comment and help loaders of sb2.exe (0x43dae0, 0x4122f0, 0x4106d0, 0x419210).  See docs/script.md.  */
#pragma once
#include <string>
#include <vector>

namespace sbscript {

// Where files come from (the original reads them with fopen relative to the game folder).
// port: paths use '/' instead of '\\'; the engine side maps them onto the data folder / APK assets.
class FileSource {
public:
    virtual ~FileSource() {}
    virtual bool readFile(const char* path, std::vector<unsigned char>& out) = 0;
};

// File layout: u16 count; count x { i16 len; u8 text[len] } with every text byte XOR 0x30.
// The exe reads each record into a zeroed 512-byte buffer and treats it as a C string.
// port: len is limited to 511 (the exe would overflow its stack buffer); `err` says what stopped
// the decode.  Returns false on a truncated file or an oversized record, `records` keeps what was read.
bool sb2_decode(const unsigned char* data, unsigned size, std::vector<std::string>& records, std::string& err);

// MSVC strtok with the delimiter string "\t\n" (0x454c0c): skips leading delimiters, so empty fields
// vanish and later columns shift left, exactly as in the exe.
class SbTok {
 public:
    explicit SbTok(const std::string& line, const char* delims = "\t\n");
    // Next token, or false at the end of the line.
    bool next(std::string& out);
 private:
    std::string s_;
    std::string delims_;
    unsigned pos_;
};

}  // namespace sbscript

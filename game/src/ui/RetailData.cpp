#include "RetailData.h"
#include <stdio.h>
#include <stdexcept>
static unsigned long le(const std::vector<unsigned char>& b, size_t p, size_t n) {
    if (p > b.size() || n > b.size()-p) throw std::runtime_error("sb2.exe: truncated PE data");
    unsigned long v=0;
    for(size_t i=0;i<n;++i) v |= (unsigned long)b[p+i] << (8*i);
    return v;
}
RetailData::RetailData() {
    FILE *f=fopen("sb2.exe","rb");
    if(!f) throw std::runtime_error("sb2.exe: required retail data tables missing");
    unsigned char b[4096]; size_t n;
    while((n=fread(b,1,sizeof b,f))!=0) file.insert(file.end(),b,b+n);
    fclose(f);
    if(le(file,0,2)!=0x5a4d || le(file,le(file,0x3c,4),4)!=0x4550)
        throw std::runtime_error("sb2.exe: invalid PE image");
}
size_t RetailData::offset(unsigned long address) const {
    size_t pe=le(file,0x3c,4), opt=pe+24;
    unsigned long base=le(file,opt+28,4);
    if(address<base) throw std::runtime_error("sb2.exe: invalid data address");
    unsigned long rva=address-base;
    size_t sections=opt+le(file,pe+20,2), count=le(file,pe+6,2);
    for(size_t i=0;i<count;++i) {
        size_t s=sections+40*i; unsigned long va=le(file,s+12,4), size=le(file,s+16,4);
        if(rva>=va && rva-va<size) return le(file,s+20,4)+rva-va;
    }
    throw std::runtime_error("sb2.exe: unmapped data address");
}
unsigned long RetailData::word(unsigned long a) const { return le(file,offset(a),4); }
std::string RetailData::text(unsigned long a) const {
    size_t p=offset(a); std::string s;
    for(size_t i=0;i<1024 && p+i<file.size();++i) {
        if(!file[p+i]) return s;
        s+=(char)file[p+i];
    }
    throw std::runtime_error("sb2.exe: unterminated data string");
}

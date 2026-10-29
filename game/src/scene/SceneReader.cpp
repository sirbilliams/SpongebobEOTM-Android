// sb2.exe 0x437210 (data portion), 0x42a480 and the record readers in SceneData.cpp.
#include "SceneData.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <limits>

namespace sb { namespace scene {
namespace {
class Reader {
public:
    const unsigned char *bytes;
    size_t size, pos;
    Reader(const unsigned char *p, size_t n) : bytes(p), size(n), pos(0) {}
    void fail(const char *message) const {
        std::ostringstream s; s << "offset " << pos << ": " << message;
        throw std::runtime_error(s.str());
    }
    void take(void *out, size_t n) {
        if (n>size-pos) fail("short read");
        if (n) memcpy(out,bytes+pos,n);
        pos+=n;
    }
    uint16_t word() {
        unsigned char b[2]; take(b,2);
        return static_cast<uint16_t>(b[0] | (static_cast<unsigned>(b[1])<<8));
    }
    uint32_t dword() {
        unsigned char b[4]; take(b,4);
        return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1])<<8) |
            (static_cast<uint32_t>(b[2])<<16) | (static_cast<uint32_t>(b[3])<<24);
    }
    float real() {
        uint32_t bits=dword(); float f; memcpy(&f,&bits,4); return f;
    }
    bool optional(uint32_t tag) {
        if (pos==size) return false;
        uint32_t found=dword();
        if (found==tag) return true;
        pos-=4; // exe seeks -4 relative to current position on mismatch
        return false;
    }
    Record record(uint32_t expected=0) {
        Record r; r.begin=pos; r.tag=dword();
        // port: validate fixed-record tags; the original fixed readers merely store them.
        if (expected && r.tag!=expected) fail("unexpected record tag");
        if (!expected && (r.tag==CAMR || r.tag==LITE)) fail("invalid object factory tag");
        const Schema *s=schema(r.tag);
        if (!s) fail("unsupported object factory tag");
        // These three factory classes inherit the empty 0x440c80 reader.
        if (!s->fieldCount) { r.end=pos; return r; }
        r.version=word();
        // The exe compares equality after each field, rather than testing version ranges.
        // Zero and versions above the last known field therefore read the complete record.
        unsigned char *out=reinterpret_cast<unsigned char *>(&r.data);
        for (size_t i=0;i<s->fieldCount;++i) {
            const FieldInfo &f=s->fields[i]; void *dest=out+f.memberOffset;
            if (f.kind==Text) take(dest,f.diskSize);
            else if (f.kind==Word16) { uint16_t v=word(); memcpy(dest,&v,2); }
            else if (f.kind==Word32) { uint32_t v=dword(); memcpy(dest,&v,4); }
            else { float v=real(); memcpy(dest,&v,4); }
            ++r.fieldsRead;
            if (r.version==i+1) break;
        }
        r.end=pos; return r;
    }
    void grid(Grid &g) {
        if (!optional(0x47524944)) return;
        g.present=true;
        g.halfSize=real(); g.o0008=real(); g.o000c=real();
        g.o0014=real(); g.o0010=real(); // exe reads +20 before +16
        const float extent=g.halfSize+g.halfSize;
        // port: reject dimensions that would overwrite the exe's fixed 30-byte rows.
        if (!(extent>=0 && extent<=30)) fail("grid dimension outside fixed row capacity");
        // Integer loop counters compare against a float in the original: ceil, not truncation.
        g.side=0; while (static_cast<float>(g.side)<extent) ++g.side;
        for (unsigned y=0;y<g.side;++y)
            for (unsigned x=0;x<g.side;++x) take(&g.cells[y][x],1);
    }
};
}

bool read(const unsigned char *bytes, size_t size, Scene &output, std::string &error) {
    if (sizeof(float)!=4 || !std::numeric_limits<float>::is_iec559) {
        error="scene reader requires IEEE binary32 floats"; return false;
    }
    if (!bytes && size) { error="null scene buffer"; return false; }
    try {
        Reader r(bytes,size); Scene s; s.fileSize=size;
        s.camera=r.record(CAMR);
        // Port scope: the installed game uses camera versions 22 and 23. Other record
        // versions remain independent, including CHAR 31/32 and ANIM 27/28.
        if (s.camera.version!=22 && s.camera.version!=23) r.fail("unsupported camera version");
        for (unsigned i=0;i<5 && r.pos<size;++i) s.tiles.push_back(r.record(LITE));
        if (r.optional(0x53545054)) {
            s.hasStartPoints=true;
            for (unsigned i=0;i<10;++i) {
                StartPoint &p=s.startPoints[i];
                p.x=r.real(); p.y=r.real(); p.z=r.real(); p.heading=r.word(); p.unknown=r.word();
            }
        }
        r.grid(s.grids[0]); r.grid(s.grids[1]);
        while (r.pos<size) s.elements.push_back(r.record());
        if (r.pos!=size) r.fail("file not consumed exactly");
        s.bytesConsumed=r.pos; output=s; error.clear(); return true;
    } catch (const std::exception &e) { error=e.what(); return false; }
}

bool load(const std::string &path, Scene &output, std::string &error) {
    std::ifstream f(path.c_str(),std::ios::binary);
    if (!f) { error="cannot open "+path; return false; }
    std::vector<unsigned char> bytes;
    char buffer[4096];
    while (f) {
        f.read(buffer,sizeof(buffer)); std::streamsize n=f.gcount();
        bytes.insert(bytes.end(),buffer,buffer+n);
    }
    if (!f.eof()) { error="failed reading "+path; return false; }
    return read(bytes.empty()?0:&bytes[0],bytes.size(),output,error);
}
} }

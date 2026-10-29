#include "../../game/src/scene/SceneData.h"
#include <dirent.h>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
using namespace sb::scene;
int main(int argc,char **argv) {
    const std::string root=argc>1?argv[1]:".game/scenes";
    DIR *d=opendir(root.c_str());
    if (!d) { std::cerr << "Cannot open " << root << '\n'; return 1; }
    std::vector<std::string> names;
    for (dirent *e=readdir(d);e;e=readdir(d)) {
        std::string n=e->d_name;
        if (n.size()>=4 && n.substr(n.size()-4)==".scn") names.push_back(n);
    }
    closedir(d); std::sort(names.begin(),names.end());
    size_t passed=0, failed=0, tiles=0, starts=0, chars=0, objects=0, bounds=0;
    for (size_t i=0;i<names.size();++i) {
        Scene s; std::string error;
        if (!load(root+"/"+names[i],s,error) || s.bytesConsumed!=s.fileSize) {
            std::cout << names[i] << " FAIL " << error << '\n'; ++failed; continue;
        }
        const size_t sp=s.hasStartPoints?10:0;
        std::cout << names[i] << " OK bytes=" << s.bytesConsumed << '/' << s.fileSize
            << " camera=" << s.camera.version << " tiles(LITE)=" << s.tiles.size()
            << " starts=" << sp << " characters=" << s.count(CharacterTag)
            << " objects2D=" << s.count(Object2DTag) << " elements=" << s.elements.size() << '\n';
        // Exercise the checked stream with incomplete final fields and trailing garbage.
        std::ifstream f((root+"/"+names[i]).c_str(),std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),
                                         std::istreambuf_iterator<char>());
        Scene sentinel; sentinel.bytesConsumed=123456;
        bool safe=true;
        if (bytes.empty() || read(&bytes[0],bytes.size()-1,sentinel,error) ||
            sentinel.bytesConsumed!=123456) safe=false;
        for (unsigned tail=1;tail<=4;++tail) {
            bytes.push_back(0);
            if (read(&bytes[0],bytes.size(),sentinel,error) ||
                sentinel.bytesConsumed!=123456) safe=false;
        }
        if (!safe) { ++failed; std::cerr << names[i] << " boundary check failed\n"; }
        else bounds+=5;
        ++passed; tiles+=s.tiles.size(); starts+=sp; chars+=s.count(CharacterTag); objects+=s.count(Object2DTag);
    }
    std::cout << "TOTAL " << passed << '/' << names.size() << " passed, " << failed
        << " failed; tiles(LITE)=" << tiles << " starts=" << starts << " characters=" << chars
        << " objects2D=" << objects << "; boundary checks=" << bounds << '\n';
    return failed || names.empty()?1:0;
}

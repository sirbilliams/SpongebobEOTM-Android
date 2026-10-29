// Read the player's PE data tables without embedding executable data in the port.
#ifndef SB_RETAIL_DATA_H
#define SB_RETAIL_DATA_H
#include <vector>
#include <string>
class RetailData {
public:
    RetailData();
    unsigned long word(unsigned long address) const;
    std::string text(unsigned long address) const;
private:
    std::vector<unsigned char> file;
    size_t offset(unsigned long address) const;
};
#endif

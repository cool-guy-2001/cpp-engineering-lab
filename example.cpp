#include <iostream>
#include <vector>

int main() {
    std::uint32_t shuff = 32;
    std::vector<uint8_t> data;
    for (int i = 0; i < 4; i++) {
        auto tmp = static_cast<uint8_t>((shuff >> 8 * i) & 0xff);
        data.push_back(tmp);
    }
}

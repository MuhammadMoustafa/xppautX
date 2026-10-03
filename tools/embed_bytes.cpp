/* Embed one blob as a C++ span. Usage: embed_bytes out.cpp symbol file */
#include "embed_data.h"
#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 4) {
        std::cerr << "tools/embed_bytes.cpp:1: usage: embed_bytes out.cpp symbol file\n";
        return 2;
    }
    const char *current = argv[1];
    try {
        auto out = xpp::embed::output(current);
        current = argv[3];
        xpp::embed::write_bytes(out, "bytes", current);
        current = argv[1];
        out << "}\nconst std::span<const unsigned char> " << argv[2]
            << "{bytes, sizeof(bytes) - 1};\n}\n";
        out.close();
    } catch (const std::exception &e) {
        std::cerr << current << ":1: embed_bytes: " << e.what() << '\n';
        return 1;
    }
}

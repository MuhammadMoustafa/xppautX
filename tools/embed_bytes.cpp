/* Embed one blob as a C++ span. Usage: embed_bytes out.cpp symbol file */
#include "embed_data.h"
#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 4) {
        std::cerr << "tools/embed_bytes.cpp:1: usage: embed_bytes out.cpp symbol file\n";
        return 2;
    }
    return xpp::embed::generate(argv[1], "embed_bytes", [&](std::ostream &out) {
        xpp::embed::write_bytes(out, "bytes", argv[3]);
        out << "}\nconst std::span<const unsigned char> " << argv[2]
            << "{bytes, sizeof(bytes) - 1};\n}\n";
    });
}

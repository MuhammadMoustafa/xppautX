/* Embed web2/dist in a C++ asset table. Usage: embed out.cpp file... */
#include "embed_data.h"
#include <filesystem>
#include <iostream>

namespace {
const char *type_of(const std::string &path)
{
    const auto dot = path.rfind('.');
    const auto ext = dot == std::string::npos ? std::string_view{} : std::string_view(path).substr(dot);
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".js") return "text/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".woff2") return "font/woff2";
    return "application/octet-stream";
}
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        std::cerr << "tools/embed.cpp:1: usage: embed out.cpp file...\n";
        return 2;
    }
    const char *current = argv[1];
    try {
        auto out = xpp::embed::output(current);
        for (int i = 2; i < argc; ++i) {
            current = argv[i];
            xpp::embed::write_bytes(out, "a" + std::to_string(i), current);
        }
        current = argv[1];
        out << "const WebAsset assets[] = {\n";
        for (int i = 2; i < argc; ++i) {
            const std::string base = std::filesystem::path(argv[i]).filename().string();
            out << "  {" << std::quoted(base == "index.html" ? "/" : "/" + base)
                << ", " << std::quoted(type_of(base)) << ", {a" << i << ", sizeof(a"
                << i << ") - 1}},\n";
        }
        out << "};\n}\nconst std::span<const WebAsset> web_assets{assets};\n}\n";
        out.close();
    } catch (const std::exception &e) {
        std::cerr << current << ":1: embed: " << e.what() << '\n';
        return 1;
    }
}

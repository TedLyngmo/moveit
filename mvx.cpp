#include <libintl.h> // bindtextdomain, textdomain

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

template<unsigned N>
consteval std::uintmax_t pow1000() {
    if constexpr(N == 0)
        return 1ull;
    else
        return 1000ull * pow1000<N - 1>();
}

template<unsigned N>
constexpr std::uintmax_t pow1000_v = pow1000<N>();

template<unsigned N>
constexpr std::uintmax_t pow1024_v = 1ull << (N * 10);

std::uintmax_t conv_to_bytes(const std::string& str) {
    auto B = [](std::uintmax_t x) { return x; };
    auto kB = [](std::uintmax_t x) { return x * pow1000_v<1>; };
    auto KB = [](std::uintmax_t x) {
        std::cerr << "Warning: Legacy prefix K used. Use Ki instead.\n";
        return x * pow1024_v<1> << 0;
    };
    auto KiB = [](std::uintmax_t x) { return x * pow1024_v<1>; };
    auto MB = [](std::uintmax_t x) { return x * pow1000_v<2>; };
    auto MiB = [](std::uintmax_t x) { return x * pow1024_v<2>; };
    auto GB = [](std::uintmax_t x) { return x * pow1000_v<3>; };
    auto GiB = [](std::uintmax_t x) { return x * pow1024_v<3>; };
    auto TB = [](std::uintmax_t x) { return x * pow1000_v<4>; };
    auto TiB = [](std::uintmax_t x) { return x * pow1024_v<4>; };
    auto PB = [](std::uintmax_t x) { return x * pow1000_v<5>; };
    auto PiB = [](std::uintmax_t x) { return x * pow1024_v<5>; };
    auto EB = [](std::uintmax_t x) { return x * pow1000_v<6>; };
    auto EiB = [](std::uintmax_t x) { return x * pow1024_v<6>; };

    std::unordered_map<std::string, std::uintmax_t (*)(std::uintmax_t)> unit_map{
        {"B", B},     {"k", kB},    {"kB", kB}, {"K", KB},   {"KB", KB},   {"Ki", KiB},  {"KiB", KiB}, {"M", MB},   {"MB", MB},
        {"Mi", MiB},  {"MiB", MiB}, {"G", GB},  {"GB", GB},  {"Gi", GiB},  {"GiB", GiB}, {"T", TB},    {"TB", TB},  {"Ti", TiB},
        {"TiB", TiB}, {"P", PB},    {"PB", PB}, {"Pi", PiB}, {"PiB", PiB}, {"E", EB},    {"EB", EB},   {"Ei", EiB}, {"EiB", EiB},
    };

    std::size_t pos;
    auto sbytes_to_move = std::stoll(str, &pos);

    if(sbytes_to_move < 0) throw std::runtime_error("Negative amount of bytes to move");

    auto bytes_to_move = static_cast<std::uintmax_t>(sbytes_to_move);

    if(pos != str.size()) {
        if(auto it = unit_map.find(str.substr(pos)); it == unit_map.end()) {
            throw std::runtime_error("Unknown prefix \"" + str.substr(pos) + '"');
        } else {
            bytes_to_move = it->second(bytes_to_move);
        }
    }

    return bytes_to_move;
}

std::filesystem::path path_without_top_dir(const std::filesystem::path& path) {
    std::filesystem::path rv;
    if(auto pit = path.begin(); pit != path.end()) {
        for(++pit; pit != path.end(); ++pit) {
            rv /= *pit;
        }
    }
    return rv;
}

auto get_files_in_dir_with_oldest_first(const std::filesystem::path& path) {
    std::set<std::pair<std::filesystem::file_time_type, std::filesystem::path>> move_order;

    for(const auto& dirent : std::filesystem::recursive_directory_iterator(path)) {
        if(not dirent.is_regular_file()) {
            continue;
        }
        move_order.emplace(std::filesystem::last_write_time(dirent), path_without_top_dir(dirent));
    }

    // recursive_diractory_iterator may have problems in deep directory trees.
    // Some implementations keep too many file descriptors open and will fail,
    // so here's a manual version that opens one directory at a time.
    /*
    std::vector<std::filesystem::path> dirs{path};

    while(not dirs.empty()) {
        std::vector<std::filesystem::path> curdirs = std::move(dirs);
        dirs.clear();
        for(auto& dir : curdirs) {
            for(const auto& dirent : std::filesystem::directory_iterator(dir)) {
                if(not dirent.is_regular_file()) {
                    if(dirent.is_directory()) dirs.push_back(dirent);
                    continue;
                }
                move_order.emplace(std::filesystem::last_write_time(dirent),
                                   dirent.path());
            }
        }
    }
    */

    return move_order;
}

int moveit(std::filesystem::path src, std::filesystem::path dest, std::uintmax_t bytes_to_move) {
    if(not std::filesystem::directory_entry(dest).is_directory())
        throw std::runtime_error('"' + dest.string() + "\" is not a directory");

    std::uintmax_t moved_bytes = 0;

    for(const auto& [modtime, relpath] : get_files_in_dir_with_oldest_first(src)) {
        auto srcpath = src / relpath;
        auto destpath = dest / relpath;

        std::error_code ec{};

        if(auto size = std::filesystem::file_size(srcpath, ec); !ec) {
            if(std::filesystem::create_directories(destpath.parent_path(), ec); ec) {
                // failed making sure that the destination directory exists
                std::cerr << ' ' << destpath.parent_path() << ": " << ec.message();
            } else if(std::filesystem::rename(srcpath, destpath, ec); ec) {
                // could not move the file
                std::cerr << ' ' << destpath << ": " << ec.message() << '\n';
            } else {
                // file moved ok, add count and print the moved file
                moved_bytes += size;
                std::cout << ' ' << relpath << '\n';
                auto parent = srcpath.parent_path();
                if(std::filesystem::is_empty(parent, ec)) {
                    // the directory is empty after the move, remove it
                    if(std::filesystem::remove(parent, ec); ec) {
                        // could not remove it for some reason
                        std::cerr << ' ' << parent << ": " << ec.message() << '\n';
                    }
                }
                if(moved_bytes >= bytes_to_move) break;
            }
        }
    }

    std::cout << "Moved " << moved_bytes << " bytes\n";
    return 0;
}

int maincpp(std::string_view program, std::span<const char*> args) {
    std::ios::sync_with_stdio(false);
    std::locale::global(std::locale(""));

    std::cin.imbue(std::locale());
    std::cout.imbue(std::locale());
    std::cerr.imbue(std::locale());
    std::clog.imbue(std::locale());

    std::wcin.imbue(std::locale());
    std::wcout.imbue(std::locale());
    std::wcerr.imbue(std::locale());
    std::wclog.imbue(std::locale());

    if(args.size() != 3) {
        std::cerr << "USAGE: " << program.data() << " source_dir destination_dir least_amount_of_bytes_to_move\n";
        return 1;
    }

    try {
        auto bytes_to_move = conv_to_bytes(args[2]);
        return moveit(args[0], args[1], bytes_to_move);
    } catch(const std::exception& ex) {
        std::cerr << program.data() << ": ERROR: " << ex.what() << '\n';
        return 1;
    }
}

int main(int argc, char* argv[]) {
    return maincpp(argv[0], {const_cast<const char**>(argv + 1), const_cast<const char**>(argv + argc)});
}

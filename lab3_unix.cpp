#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <openssl/sha.h>

namespace fs = std::filesystem;

std::string get_hash(const fs::path& file_path) {
    std::ifstream input(file_path, std::ios::binary);
    if (!input) {
        return {};
    }

    SHA_CTX ctx;
    SHA1_Init(&ctx);
    char buf[8192];
    while (input.read(buf, sizeof(buf)) || input.gcount() > 0) {
        SHA1_Update(&ctx, buf, static_cast<std::size_t>(input.gcount()));
    }
    if (input.bad()) {
        return {};
    }

    unsigned char raw[SHA_DIGEST_LENGTH];
    SHA1_Final(raw, &ctx);
    return std::string(reinterpret_cast<const char*>(raw), SHA_DIGEST_LENGTH);
}

std::vector<fs::path> collect_files(const fs::path& root, std::size_t& fail_count) {
    std::vector<fs::path> result;
    try {
        for (const auto& item : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
            std::error_code ec;
            if (!item.is_symlink(ec) && item.is_regular_file(ec)) {
                result.push_back(item.path());
            }
        }
    } catch (const fs::filesystem_error& err) {
        std::cerr << "Ошибка обхода: " << err.what() << '\n';
        ++fail_count;
    }
    return result;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " <каталог>\n";
        return 1;
    }

    fs::path root(argv[1]);
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        std::cerr << "Ошибка: каталог не найден: " << root << '\n';
        return 1;
    }

    std::size_t scanned_files = 0;
    std::size_t created_links = 0;
    std::size_t same_inode_count = 0;
    std::size_t fail_count = 0;

    std::vector<fs::path> entries = collect_files(root, fail_count);
    std::unordered_map<std::string, fs::path> unique_by_hash;

    for (const fs::path& current : entries) {
        ++scanned_files;

        std::string digest = get_hash(current);
        if (digest.empty()) {
            std::cerr << "Ошибка чтения файла: " << current << '\n';
            ++fail_count;
            continue;
        }

        auto found = unique_by_hash.find(digest);
        if (found == unique_by_hash.end()) {
            unique_by_hash.emplace(std::move(digest), current);
            continue;
        }

        const fs::path& original = found->second;
        if (fs::equivalent(original, current, ec) && !ec) {
            ++same_inode_count;
            continue;
        }

        fs::path temp_link = current.parent_path() / (".dedup_" + current.filename().string());
        fs::create_hard_link(original, temp_link, ec);
        if (!ec) {
            fs::rename(temp_link, current, ec);
            if (ec) {
                std::error_code tmp_ec;
                fs::remove(temp_link, tmp_ec);
            }
        }

        if (ec) {
            std::cerr << "Ошибка создания ссылки для " << current << ": " << ec.message() << '\n';
            ++fail_count;
        } else {
            std::cout << "Ссылка: " << current.string() << " -> " << original.string() << '\n';
            ++created_links;
        }
    }

    std::cout << "Проверено файлов: " << scanned_files
              << ", создано ссылок: " << created_links
              << ", уже связаны: " << same_inode_count
              << ", ошибок: " << fail_count << '\n';

    return 0;
}

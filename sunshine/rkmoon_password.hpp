// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <algorithm>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <array>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rkmoon_sunshine {
// One UTF-8 line, optional final LF, 1..256 bytes. Missing file uses the default.
inline std::string read_password_file(const std::filesystem::path &path) {
  int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
  if (fd < 0) {
    if (errno == ENOENT) return "kvm";
    throw std::runtime_error("Invalid private password file");
  }
  struct stat st {};
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
      (st.st_mode & 07777) != 0600 || st.st_size < 1 || st.st_size > 257) {
    close(fd); throw std::runtime_error("Invalid private password file");
  }
  char bytes[258] {};
  ssize_t count = read(fd, bytes, sizeof(bytes));
  close(fd);
  std::string value;
  if (count > 0 && count <= 257) value.assign(bytes, count);
  OPENSSL_cleanse(bytes, sizeof(bytes));
  if (!value.empty() && value.back() == '\n') value.pop_back();
  if (value.empty() || value.size() > 256 || value.find_first_of("\r\n") != std::string::npos ||
      value.find(char{}) != std::string::npos) {
    OPENSSL_cleanse(value.data(), value.size());
    throw std::runtime_error("Invalid private password file");
  }
  return value;
}
// Never log passwords or headers. Each HTTP request authenticates anew.
class password_auth {
  std::array<unsigned char, 32> expected_ {};
  std::mutex mutex_;
  double tokens_ = 10;
  std::chrono::steady_clock::time_point last_ = std::chrono::steady_clock::now();
  static auto digest(std::string_view value) {
    std::array<unsigned char, 32> result {};
    unsigned int size = 0;
    if (EVP_Digest(value.data(), value.size(), result.data(), &size, EVP_sha256(), nullptr) != 1 || size != result.size())
      throw std::runtime_error("Password authentication initialization failed");
    return result;
  }
  void set_password(std::string_view password) {
    std::string plain = "kvm:" + std::string(password);
    std::string encoded(4 * ((plain.size() + 2) / 3) + 1, char{});
    auto size = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(encoded.data()),
                    reinterpret_cast<const unsigned char *>(plain.data()), static_cast<int>(plain.size()));
    encoded.resize(size);
    expected_ = digest("Basic " + encoded);
    OPENSSL_cleanse(plain.data(), plain.size());
    OPENSSL_cleanse(encoded.data(), encoded.size());
  }
  bool authorize_locked(std::string_view header) {
    // Bounded global failure budget; no attacker-controlled per-IP allocation.
    const auto now = std::chrono::steady_clock::now();
    tokens_ = std::min(10.0, tokens_ + std::chrono::duration<double>(now-last_).count());
    last_ = now;
    if (tokens_ < 1) return false;
    if (header.size() <= 1024) {
      auto actual = digest(header);
      if (CRYPTO_memcmp(actual.data(), expected_.data(), actual.size()) == 0) return true;
    }
    tokens_ -= 1;
    return false;
  }
public:
  explicit password_auth(std::string_view password = "kvm") { set_password(password); }
  bool authorize(std::string_view header) {
    std::lock_guard lock(mutex_);
    return authorize_locked(header);
  }
  bool authorize_file(const std::filesystem::path &path, std::string_view header) {
    std::lock_guard lock(mutex_);
    try {
      auto password = read_password_file(path);
      set_password(password);
      OPENSSL_cleanse(password.data(), password.size());
    } catch (...) {
      authorize_locked("");
      return false;
    }
    return authorize_locked(header);
  }

};
}

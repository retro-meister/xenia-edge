/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge. All rights reserved.                            *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "xenia/vfs/devices/xcontent_devices/svod_container_device.h"

#include "third_party/catch/include/catch.hpp"

namespace xe::vfs::test {

TEST_CASE("SVOD ignores macOS metadata beside its fragments", "[svod]") {
  struct TemporaryDirectory {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("xenia-svod-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() {
      std::error_code error;
      std::filesystem::remove_all(path, error);
    }
  } temporary;
  const auto package = temporary.path / "package";
  const auto fragments = temporary.path / "package.data";
  REQUIRE(std::filesystem::create_directories(fragments));

  // A two-fragment package with an empty root directory in Data0000.
  XContentContainerHeader header{};
  header.content_header.magic = XContentPackageType::kLive;
  header.content_metadata.volume_type = XContentVolumeType::kSvod;
  header.content_metadata.data_file_count = 2;
  {
    std::ofstream file(package, std::ios::binary);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    REQUIRE(file.good());
  }
  std::vector<uint8_t> data(0x14000);
  std::memcpy(data.data() + 0x2000, "MICROSOFT*XBOX*MEDIA", 20);
  const uint32_t root_block = 1;
  std::memcpy(data.data() + 0x2014, &root_block, sizeof(root_block));
  data[0x2800] = data[0x2801] = 0xFF;
  {
    std::ofstream file(fragments / "Data0000", std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    REQUIRE(file.good());
  }
  std::ofstream(fragments / "Data0001") << "fragment";

  bool should_load = true;
  SECTION("A clean package still loads") {}
  SECTION("AppleDouble sidecars do not count as fragments") {
    std::ofstream(fragments / "._Data0000") << "metadata";
    std::ofstream(fragments / "._Data0001") << "metadata";
  }
  SECTION("Finder metadata does not count as a fragment") {
    std::ofstream(fragments / ".DS_Store") << "metadata";
  }
  SECTION("A sidecar cannot replace a missing fragment") {
    std::filesystem::remove(fragments / "Data0001");
    std::ofstream(fragments / "._Data0001") << "metadata";
    should_load = false;
  }
  SECTION("A directory cannot replace a fragment") {
    std::filesystem::remove(fragments / "Data0001");
    std::filesystem::create_directory(fragments / "Data0001");
    should_load = false;
  }
  SECTION("An unexpected file is still rejected") {
    std::ofstream(fragments / "unexpected") << "extra";
    should_load = false;
  }

  SvodContainerDevice device("test:", package);
  REQUIRE(device.Initialize() == should_load);
  if (should_load) {
    // Metadata must not be opened or included in the mapped data size.
    REQUIRE(device.data_size() == data.size() + 8);
  }
}

}  // namespace xe::vfs::test

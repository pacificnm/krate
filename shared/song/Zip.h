#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Reads files out of a zip archive (stored or deflated entries), enough for
// tab files (docs/SONG_PLAYER.md). No Qt; inflates with zlib.
namespace krate::song {

struct ZipEntry
{
    std::string name;
    int method = 0; // 0 stored, 8 deflated
    std::uint32_t compressedSize = 0;
    std::uint32_t size = 0;
    std::uint32_t localHeader = 0;
};

class ZipArchive
{
public:
    // Reads the central directory; false if this isn't a zip archive we can read.
    bool open(std::span<const std::uint8_t> data);

    const std::vector<ZipEntry> &entries() const noexcept { return m_entries; }
    const ZipEntry *find(const std::string &name) const noexcept;
    // The entry's contents; nullopt if it is damaged, too large or compressed
    // in a way we don't read.
    std::optional<std::vector<std::uint8_t>> read(const ZipEntry &entry,
                                                  std::size_t maxSize = std::size_t(256) << 20) const;

private:
    std::span<const std::uint8_t> m_data;
    std::vector<ZipEntry> m_entries;
};

} // namespace krate::song

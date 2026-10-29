#include "song/Zip.h"

#include <zlib.h>

namespace krate::song {

namespace {

constexpr std::uint32_t kEndOfDirectory = 0x06054b50;
constexpr std::uint32_t kDirectoryEntry = 0x02014b50;
constexpr std::uint32_t kLocalHeader = 0x04034b50;

std::uint32_t le32(std::span<const std::uint8_t> d, std::size_t at)
{
    if (at + 4 > d.size())
        return 0;
    return std::uint32_t(d[at]) | std::uint32_t(d[at + 1]) << 8 | std::uint32_t(d[at + 2]) << 16
           | std::uint32_t(d[at + 3]) << 24;
}

std::uint16_t le16(std::span<const std::uint8_t> d, std::size_t at)
{
    if (at + 2 > d.size())
        return 0;
    return std::uint16_t(d[at] | d[at + 1] << 8);
}

} // namespace

bool ZipArchive::open(std::span<const std::uint8_t> data)
{
    m_data = data;
    m_entries.clear();
    if (data.size() < 22)
        return false;
    // The end-of-directory record is in the last 22 + 65535 (comment) bytes.
    std::size_t eocd = data.size();
    const std::size_t lowest = data.size() > 22 + 65535 ? data.size() - 22 - 65535 : 0;
    for (std::size_t i = data.size() - 22 + 1; i-- > lowest;)
        if (le32(data, i) == kEndOfDirectory) {
            eocd = i;
            break;
        }
    if (eocd == data.size())
        return false;
    const std::size_t count = le16(data, eocd + 10);
    std::size_t p = le32(data, eocd + 16);
    for (std::size_t i = 0; i < count; ++i) {
        if (p + 46 > data.size() || le32(data, p) != kDirectoryEntry)
            return false;
        ZipEntry e;
        e.method = le16(data, p + 10);
        e.compressedSize = le32(data, p + 20);
        e.size = le32(data, p + 24);
        const std::size_t nameLength = le16(data, p + 28);
        const std::size_t extraLength = le16(data, p + 30);
        const std::size_t commentLength = le16(data, p + 32);
        e.localHeader = le32(data, p + 42);
        if (p + 46 + nameLength > data.size())
            return false;
        e.name.assign(reinterpret_cast<const char *>(data.data() + p + 46), nameLength);
        m_entries.push_back(std::move(e));
        p += 46 + nameLength + extraLength + commentLength;
    }
    return true;
}

const ZipEntry *ZipArchive::find(const std::string &name) const noexcept
{
    for (const ZipEntry &e : m_entries)
        if (e.name == name)
            return &e;
    return nullptr;
}

std::optional<std::vector<std::uint8_t>> ZipArchive::read(const ZipEntry &entry, std::size_t maxSize) const
{
    const std::size_t local = entry.localHeader;
    if (local + 30 > m_data.size() || le32(m_data, local) != kLocalHeader)
        return std::nullopt;
    const std::size_t start = local + 30 + le16(m_data, local + 26) + le16(m_data, local + 28);
    if (start > m_data.size() || entry.compressedSize > m_data.size() - start || entry.size > maxSize)
        return std::nullopt;
    const std::span<const std::uint8_t> raw = m_data.subspan(start, entry.compressedSize);
    if (entry.method == 0) {
        if (entry.compressedSize != entry.size)
            return std::nullopt;
        return std::vector<std::uint8_t>(raw.begin(), raw.end());
    }
    if (entry.method != 8)
        return std::nullopt;
    std::vector<std::uint8_t> out(entry.size);
    z_stream z{};
    if (inflateInit2(&z, -MAX_WBITS) != Z_OK) // raw deflate, as zip stores it
        return std::nullopt;
    z.next_in = const_cast<Bytef *>(raw.data());
    z.avail_in = uInt(raw.size());
    z.next_out = out.data();
    z.avail_out = uInt(out.size());
    const int result = inflate(&z, Z_FINISH);
    const bool complete = result == Z_STREAM_END && z.total_out == entry.size;
    inflateEnd(&z);
    if (!complete)
        return std::nullopt;
    return out;
}

} // namespace krate::song

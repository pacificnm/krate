#include "song/Xml.h"

#include <cstdint>

namespace krate::song::xml {

namespace {

constexpr int kMaxDepth = 256;

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
bool isNameChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'
           || c == '.' || c == ':' || (static_cast<unsigned char>(c) >= 0x80);
}

void appendUtf8(std::string &out, std::uint32_t cp)
{
    if (cp < 0x80) {
        out += char(cp);
    } else if (cp < 0x800) {
        out += char(0xC0 | (cp >> 6));
        out += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += char(0xE0 | (cp >> 12));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x110000) {
        out += char(0xF0 | (cp >> 18));
        out += char(0x80 | ((cp >> 12) & 0x3F));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
}

class Parser
{
public:
    explicit Parser(std::string_view s) : m_s(s) {}

    Document run()
    {
        Document doc;
        skipMisc();
        if (!at('<')) {
            doc.error = "no document element";
            return doc;
        }
        if (!element(doc.root, 0))
            doc.error = m_error.empty() ? "malformed XML" : m_error;
        return doc;
    }

private:
    bool at(char c) const { return m_p < m_s.size() && m_s[m_p] == c; }
    bool startsWith(std::string_view w) const { return m_s.substr(m_p, w.size()) == w; }
    bool fail(const char *why)
    {
        if (m_error.empty())
            m_error = std::string(why) + " at byte " + std::to_string(m_p);
        return false;
    }
    void skipSpace()
    {
        while (m_p < m_s.size() && isSpace(m_s[m_p]))
            ++m_p;
    }
    bool skipPast(std::string_view end)
    {
        const std::size_t found = m_s.find(end, m_p);
        if (found == std::string_view::npos)
            return false;
        m_p = found + end.size();
        return true;
    }
    // Whitespace, comments, processing instructions and DOCTYPE outside the root.
    void skipMisc()
    {
        for (;;) {
            skipSpace();
            if (startsWith("<?")) {
                if (!skipPast("?>"))
                    return;
            } else if (startsWith("<!--")) {
                if (!skipPast("-->"))
                    return;
            } else if (startsWith("<!DOCTYPE")) {
                if (!skipPast(">"))
                    return;
            } else {
                return;
            }
        }
    }
    std::string name()
    {
        const std::size_t start = m_p;
        while (m_p < m_s.size() && isNameChar(m_s[m_p]))
            ++m_p;
        return std::string(m_s.substr(start, m_p - start));
    }
    // Text up to `stop`, with entities decoded.
    bool text(std::string &out, char stop)
    {
        while (m_p < m_s.size() && m_s[m_p] != stop) {
            const char c = m_s[m_p];
            if (c != '&') {
                out += c;
                ++m_p;
                continue;
            }
            const std::size_t semi = m_s.find(';', m_p);
            if (semi == std::string_view::npos || semi - m_p > 12)
                return fail("bad entity");
            const std::string_view ent = m_s.substr(m_p + 1, semi - m_p - 1);
            if (ent == "lt") {
                out += '<';
            } else if (ent == "gt") {
                out += '>';
            } else if (ent == "amp") {
                out += '&';
            } else if (ent == "quot") {
                out += '"';
            } else if (ent == "apos") {
                out += '\'';
            } else if (ent.size() > 1 && ent[0] == '#') {
                std::uint32_t cp = 0;
                const bool hex = ent[1] == 'x' || ent[1] == 'X';
                for (std::size_t i = hex ? 2 : 1; i < ent.size(); ++i) {
                    const char d = ent[i];
                    int v = -1;
                    if (d >= '0' && d <= '9')
                        v = d - '0';
                    else if (hex && d >= 'a' && d <= 'f')
                        v = d - 'a' + 10;
                    else if (hex && d >= 'A' && d <= 'F')
                        v = d - 'A' + 10;
                    if (v < 0)
                        return fail("bad character reference");
                    cp = cp * (hex ? 16 : 10) + std::uint32_t(v);
                    if (cp > 0x10FFFF)
                        return fail("bad character reference");
                }
                appendUtf8(out, cp);
            } else {
                return fail("unknown entity");
            }
            m_p = semi + 1;
        }
        return true;
    }
    bool element(Node &node, int depth)
    {
        if (depth > kMaxDepth)
            return fail("elements nested too deeply");
        ++m_p; // '<'
        node.name = name();
        if (node.name.empty())
            return fail("element without a name");
        for (;;) { // attributes
            skipSpace();
            if (startsWith("/>")) {
                m_p += 2;
                return true;
            }
            if (at('>')) {
                ++m_p;
                break;
            }
            std::string key = name();
            if (key.empty())
                return fail("bad attribute");
            skipSpace();
            if (!at('='))
                return fail("attribute without a value");
            ++m_p;
            skipSpace();
            if (!at('"') && !at('\''))
                return fail("unquoted attribute value");
            const char quote = m_s[m_p++];
            std::string value;
            if (!text(value, quote) || !at(quote))
                return fail("unterminated attribute value");
            ++m_p;
            node.attributes.emplace_back(std::move(key), std::move(value));
        }
        for (;;) { // content
            if (!text(node.text, '<'))
                return false;
            if (m_p >= m_s.size())
                return fail("unterminated element");
            if (startsWith("</")) {
                m_p += 2;
                if (name() != node.name)
                    return fail("mismatched end tag");
                skipSpace();
                if (!at('>'))
                    return fail("bad end tag");
                ++m_p;
                return true;
            }
            if (startsWith("<![CDATA[")) {
                const std::size_t end = m_s.find("]]>", m_p + 9);
                if (end == std::string_view::npos)
                    return fail("unterminated CDATA");
                node.text.append(m_s.substr(m_p + 9, end - m_p - 9));
                m_p = end + 3;
            } else if (startsWith("<!--")) {
                if (!skipPast("-->"))
                    return fail("unterminated comment");
            } else if (startsWith("<?")) {
                if (!skipPast("?>"))
                    return fail("unterminated processing instruction");
            } else {
                node.children.emplace_back();
                if (!element(node.children.back(), depth + 1))
                    return false;
            }
        }
    }

    std::string_view m_s;
    std::size_t m_p = 0;
    std::string m_error;
};

std::string_view trim(std::string_view s)
{
    while (!s.empty() && isSpace(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && isSpace(s.back()))
        s.remove_suffix(1);
    return s;
}

} // namespace

std::string_view Node::attribute(std::string_view key) const noexcept
{
    for (const auto &[k, v] : attributes)
        if (k == key)
            return v;
    return {};
}

const Node *Node::child(std::string_view childName) const noexcept
{
    for (const Node &c : children)
        if (c.name == childName)
            return &c;
    return nullptr;
}

std::string_view Node::childText(std::string_view childName) const noexcept
{
    const Node *c = child(childName);
    return c ? c->trimmedText() : std::string_view();
}

std::string_view Node::trimmedText() const noexcept { return trim(text); }

Document parse(std::string_view xml)
{
    if (xml.substr(0, 3) == "\xEF\xBB\xBF") // a UTF-8 byte order mark
        xml.remove_prefix(3);
    return Parser(xml).run();
}

} // namespace krate::song::xml

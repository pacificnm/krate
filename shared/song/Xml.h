#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

// A small XML reader for tab files (docs/SONG_PLAYER.md): elements,
// attributes, text, CDATA, the five named entities and character references.
// Comments, processing instructions and DOCTYPE are skipped; namespaces are not
// interpreted. It reads the whole document into a tree. No Qt.
namespace krate::song::xml {

struct Node
{
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::string text; // the element's own text (CDATA included), not its children's
    std::vector<Node> children;

    std::string_view attribute(std::string_view key) const noexcept;
    const Node *child(std::string_view childName) const noexcept;
    // The child's text, trimmed; empty if there is no such child.
    std::string_view childText(std::string_view childName) const noexcept;
    std::string_view trimmedText() const noexcept;
};

struct Document
{
    Node root;         // the document element
    std::string error; // set when the document isn't well formed
    bool ok() const { return error.empty(); }
};

Document parse(std::string_view xml);

} // namespace krate::song::xml

#pragma once
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Cititor XML pentru documentele Reuters furnizate. Nu necesita biblioteci externe.
// Suporta atribute, noduri imbricate, comentarii, CDATA si entitatile XML standard.
// Nu incarca DTD-uri sau entitati externe. Textul ISO-8859-1 poate fi citit deoarece
// etichetele XML si cuvintele englezesti folosite de tokenizer sunt ASCII.
namespace xmllite {
struct Node {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::string text;
    std::vector<Node> children;

    std::string attribute(const std::string& key) const {
        const auto it = attributes.find(key);
        return it == attributes.end() ? "" : it->second;
    }
    const Node* child(const std::string& key) const {
        for (const auto& node : children)
            if (node.name == key) return &node;
        return nullptr;
    }
    std::string plainText() const {
        std::string result = text;
        for (const auto& node : children) {
            result += ' ';
            result += node.plainText();
        }
        return result;
    }
};

class Parser {
    const std::string& input;
    std::size_t pos = 0;
    static constexpr unsigned maxDepth = 128;

    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error("XML invalid la octetul " + std::to_string(pos) + ": " + message);
    }
    bool starts(const std::string& value) const {
        return input.compare(pos, value.size(), value) == 0;
    }
    static bool whitespace(char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }
    void skipSpace() {
        while (pos < input.size() && whitespace(input[pos])) ++pos;
    }
    void consume(const std::string& value) {
        if (!starts(value)) fail("se astepta " + value);
        pos += value.size();
    }
    void skipUntil(const std::string& terminator) {
        const auto end = input.find(terminator, pos);
        if (end == std::string::npos) fail("lipseste " + terminator);
        pos = end + terminator.size();
    }
    std::string readName() {
        const auto begin = pos;
        while (pos < input.size()) {
            const unsigned char c = static_cast<unsigned char>(input[pos]);
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == ':' || c == '-' || c == '.')) break;
            ++pos;
        }
        if (pos == begin) fail("nume de nod sau atribut lipsa");
        return input.substr(begin, pos - begin);
    }
    static void appendCodepoint(std::string& out, std::uint32_t cp) {
        if (cp <= 0x7f) out += static_cast<char>(cp);
        else if (cp <= 0x7ff) {
            out += static_cast<char>(0xc0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3f));
        } else if (cp <= 0xffff) {
            out += static_cast<char>(0xe0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (cp & 0x3f));
        } else {
            out += static_cast<char>(0xf0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (cp & 0x3f));
        }
    }
    std::string decode(const std::string& raw) const {
        std::string result;
        for (std::size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] != '&') { result += raw[i]; continue; }
            const auto end = raw.find(';', i + 1);
            if (end == std::string::npos) fail("entitate XML neterminata");
            const auto entity = raw.substr(i + 1, end - i - 1);
            if (entity == "amp") result += '&';
            else if (entity == "lt") result += '<';
            else if (entity == "gt") result += '>';
            else if (entity == "quot") result += '"';
            else if (entity == "apos") result += '\'';
            else if (!entity.empty() && entity[0] == '#') {
                const bool hex = entity.size() > 1 && entity[1] == 'x';
                const std::size_t begin = hex ? 2 : 1;
                if (begin == entity.size()) fail("referinta numerica goala");
                std::uint32_t cp = 0;
                for (std::size_t j = begin; j < entity.size(); ++j) {
                    const char c = entity[j];
                    int digit = -1;
                    if (c >= '0' && c <= '9') digit = c - '0';
                    else if (hex && c >= 'a' && c <= 'f') digit = c - 'a' + 10;
                    else if (hex && c >= 'A' && c <= 'F') digit = c - 'A' + 10;
                    const std::uint32_t base = hex ? 16u : 10u;
                    if (digit < 0 || static_cast<std::uint32_t>(digit) >= base ||
                        cp > (0x10ffffu - static_cast<std::uint32_t>(digit)) / base)
                        fail("referinta numerica invalida");
                    cp = cp * base + static_cast<std::uint32_t>(digit);
                }
                if (!(cp == 9 || cp == 10 || cp == 13 || (cp >= 0x20 && cp <= 0xd7ff) ||
                      (cp >= 0xe000 && cp <= 0xfffd) || (cp >= 0x10000 && cp <= 0x10ffff)))
                    fail("caracter XML invalid");
                appendCodepoint(result, cp);
            } else fail("entitate necunoscuta: &" + entity + ";");
            i = end;
        }
        return result;
    }
    void skipMisc() {
        while (true) {
            skipSpace();
            if (starts("<?")) { pos += 2; skipUntil("?>"); }
            else if (starts("<!--")) { pos += 4; skipUntil("-->"); }
            else break;
        }
    }
    Node element(unsigned depth) {
        if (depth > maxDepth) fail("prea multe noduri imbricate");
        consume("<");
        if (starts("!")) fail("DTD-urile nu sunt suportate");
        Node node;
        node.name = readName();
        while (true) {
            const auto beforeSpace = pos;
            skipSpace();
            if (starts("/>")) { pos += 2; return node; }
            if (starts(">")) { ++pos; break; }
            if (beforeSpace == pos) fail("lipseste spatiul dintre atribute");
            const auto key = readName();
            skipSpace(); consume("="); skipSpace();
            if (pos >= input.size() || (input[pos] != '\'' && input[pos] != '"'))
                fail("atribut fara ghilimele");
            const char quote = input[pos++];
            const auto end = input.find(quote, pos);
            if (end == std::string::npos) fail("atribut neterminat");
            const auto raw = input.substr(pos, end - pos);
            if (raw.find('<') != std::string::npos) fail("caracter < in atribut");
            if (!node.attributes.emplace(key, decode(raw)).second) fail("atribut duplicat: " + key);
            pos = end + 1;
        }
        while (true) {
            if (pos >= input.size()) fail("nod neterminat: " + node.name);
            if (starts("</")) {
                pos += 2;
                const auto closing = readName();
                if (closing != node.name) fail("inchidere gresita pentru " + node.name);
                skipSpace(); consume(">");
                return node;
            }
            if (starts("<!--")) { pos += 4; skipUntil("-->"); }
            else if (starts("<?")) { pos += 2; skipUntil("?>"); }
            else if (starts("<![CDATA[")) {
                pos += 9;
                const auto end = input.find("]]>", pos);
                if (end == std::string::npos) fail("CDATA neterminat");
                node.text += input.substr(pos, end - pos);
                pos = end + 3;
            } else if (starts("<")) node.children.push_back(element(depth + 1));
            else {
                const auto end = input.find('<', pos);
                if (end == std::string::npos) fail("nod neterminat: " + node.name);
                node.text += decode(input.substr(pos, end - pos));
                pos = end;
            }
        }
    }
public:
    explicit Parser(const std::string& source) : input(source) {}
    Node parse() {
        if (input.compare(0, 3, "\xef\xbb\xbf") == 0) pos = 3;
        skipMisc();
        Node root = element(0);
        skipMisc();
        if (pos != input.size()) fail("date dupa nodul radacina");
        return root;
    }
};
inline Node parse(const std::string& source) { return Parser(source).parse(); }
} // namespace xmllite

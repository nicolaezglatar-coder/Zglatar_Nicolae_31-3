#include "PorterStemmer.hpp"
#include "XmlLite.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
using Frequencies = std::unordered_map<std::string, std::uint64_t>;

struct Article {
    std::string id;
    std::vector<std::string> topics;
    std::string folder;
    Frequencies frequencies;
};

// Lowercase explicit pentru ASCII: rezultatul nu depinde de setarile regionale.
std::string lowercase(std::string word) {
    for (char& c : word) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return word;
}

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Nu se poate deschide: " + path.string());
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) throw std::runtime_error("Eroare la citire: " + path.string());
    return data;
}

std::unordered_set<std::string> loadStopwords(const fs::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Nu se poate deschide lista stopwords: " + path.string());
    std::unordered_set<std::string> words;
    std::string word;
    while (in >> word) {
        if (word.compare(0, 3, "\xef\xbb\xbf") == 0) word.erase(0, 3);
        if (!word.empty()) words.insert(lowercase(word));
    }
    if (in.bad()) throw std::runtime_error("Eroare la citirea listei stopwords.");
    if (words.empty()) throw std::runtime_error("Lista stopwords este goala.");
    return words;
}

Frequencies countWords(const std::string& text,
                       const std::unordered_set<std::string>& stopwords,
                       PorterStemmer& stemmer,
                       std::unordered_map<std::string, std::string>& stemCache) {
    Frequencies result;
    std::string word;
    const auto addWord = [&]() {
        if (word.empty()) return;
        // Stopwords se elimina INAINTE de stemming, pe forma originala lowercase.
        if (stopwords.find(word) == stopwords.end()) {
            auto it = stemCache.find(word);
            if (it == stemCache.end()) it = stemCache.emplace(word, stemmer.stem(word)).first;
            if (!it->second.empty()) ++result[it->second];
        }
        word.clear();
    };
    for (unsigned char c : text) {
        if (c >= 'A' && c <= 'Z') word += static_cast<char>(c - 'A' + 'a');
        else if (c >= 'a' && c <= 'z') word += static_cast<char>(c);
        else addWord(); // Punctuatia, cifrele si celelalte caractere separa cuvintele.
    }
    addWord();
    return result;
}

std::string sourceFolder(const fs::path& file, bool allowUnlabeled) {
    for (auto p = file.parent_path(); !p.empty();) {
        const auto name = lowercase(p.filename().string());
        if (name == "training" || name == "traning") return "Training";
        if (name == "testing") return "Testing";
        const auto parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
    if (allowUnlabeled) return file.parent_path().filename().string();
    throw std::runtime_error("Articol fara folder Training/Testing: " + file.string() +
        "\nSelecteaza Reuters_34 sau foloseste --allow-unlabeled pentru setul neimpartit.");
}

Article readArticle(const fs::path& file,
                    const std::unordered_set<std::string>& stopwords,
                    PorterStemmer& stemmer,
                    std::unordered_map<std::string, std::string>& stemCache,
                    bool allowUnlabeled) {
    const auto root = xmllite::parse(readFile(file));
    if (root.name != "newsitem") throw std::runtime_error("Nodul radacina trebuie sa fie newsitem.");
    const auto* title = root.child("title");
    const auto* text = root.child("text");
    if (!title || !text) throw std::runtime_error("Lipseste nodul title sau text.");

    Article article;
    auto id = root.attribute("itemid");
    if (id.empty()) id = file.stem().string();
    if (id.find_first_of("# \t\r\n") != std::string::npos)
        throw std::runtime_error("Identificator de articol invalid.");
    article.id = "D" + id;
    article.folder = sourceFolder(file, allowUnlabeled);
    if (article.folder.empty() || article.folder.find_first_of("# \t\r\n") != std::string::npos)
        throw std::runtime_error("Numele folderului nu poate fi salvat in formatul cerut.");
    article.frequencies = countWords(title->plainText() + " " + text->plainText(),
                                      stopwords, stemmer, stemCache);

    // Categoriile vin numai din metadata/codes cu class="bip:topics:1.0".
    if (const auto* metadata = root.child("metadata")) {
        for (const auto& codes : metadata->children) {
            if (codes.name != "codes" || codes.attribute("class") != "bip:topics:1.0") continue;
            for (const auto& code : codes.children) {
                if (code.name != "code") continue;
                const auto value = code.attribute("code");
                if (value.empty()) continue;
                if (value.find_first_of("# \t\r\n") != std::string::npos)
                    throw std::runtime_error("Cod topic invalid.");
                if (std::find(article.topics.begin(), article.topics.end(), value) == article.topics.end())
                    article.topics.push_back(value);
            }
        }
    }
    return article;
}

std::vector<fs::path> findXmlFiles(const fs::path& folder) {
    if (!fs::is_directory(folder)) throw std::runtime_error("Folder inexistent: " + folder.string());
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(folder))
        if (entry.is_regular_file() && lowercase(entry.path().extension().string()) == ".xml")
            files.push_back(entry.path());
    std::sort(files.begin(), files.end()); // Acelasi input produce aceeasi ordine.
    if (files.empty()) throw std::runtime_error("Nu exista fisiere XML in folderul selectat.");
    return files;
}

void writeVectors(const fs::path& output, const std::vector<Article>& articles,
                  const std::set<std::string>& vocabulary) {
    if (!output.parent_path().empty()) fs::create_directories(output.parent_path());
    std::ofstream out(output, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Nu se poate crea rezultatul: " + output.string());
    std::unordered_map<std::string, std::size_t> index;
    std::size_t i = 0;
    for (const auto& word : vocabulary) {
        index.emplace(word, i++);
        out << "@attribute " << word << '\n';
    }
    out << "@data\n";
    for (const auto& article : articles) {
        std::vector<std::pair<std::size_t, std::uint64_t>> sparse;
        sparse.reserve(article.frequencies.size());
        for (const auto& entry : article.frequencies)
            sparse.emplace_back(index.at(entry.first), entry.second);
        std::sort(sparse.begin(), sparse.end());
        out << article.id << " #";
        for (const auto& entry : sparse) out << ' ' << entry.first << ':' << entry.second;
        out << " #";
        for (const auto& topic : article.topics) out << ' ' << topic;
        out << " # " << article.folder << '\n';
    }
    out.close();
    if (!out) throw std::runtime_error("Eroare la scrierea rezultatului.");
}

void help() {
    std::cout << "Extragerea trasaturilor Reuters - C++17\n"
              << "Utilizare: ReutersFeatures [folder_dataset] [stopwords.txt] [rezultat.txt] [--allow-unlabeled]\n"
              << "Fara argumente: data/Reuters/Reuters_34, data/stopwords.txt, output/vectori_34.txt\n"
              << "--allow-unlabeled: permite articole fara Training/Testing si pastreaza numele folderului real.\n";
}

int main(int argc, char* argv[]) {
    try {
        bool allowUnlabeled = false;
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") { help(); return 0; }
            if (arg == "--allow-unlabeled") allowUnlabeled = true;
            else if (arg.rfind("--", 0) == 0) throw std::runtime_error("Optiune necunoscuta: " + arg);
            else args.push_back(arg);
        }
        if (args.size() > 3) { help(); return 1; }
        const fs::path folder = args.size() >= 1 ? fs::path(args[0]) : fs::path("data/Reuters/Reuters_34");
        const fs::path stopPath = args.size() >= 2 ? fs::path(args[1]) : fs::path("data/stopwords.txt");
        const fs::path output = args.size() >= 3 ? fs::path(args[2]) : fs::path("output/vectori_34.txt");
        const auto outputPath = fs::weakly_canonical(fs::absolute(output));
        if (outputPath == fs::weakly_canonical(fs::absolute(stopPath)))
            throw std::runtime_error("Rezultatul nu poate suprascrie stopwords.txt.");
        const auto files = findXmlFiles(folder);
        for (const auto& file : files)
            if (fs::weakly_canonical(fs::absolute(file)) == outputPath)
                throw std::runtime_error("Rezultatul nu poate suprascrie un articol XML.");
        const auto stopwords = loadStopwords(stopPath);
        PorterStemmer stemmer;
        std::unordered_map<std::string, std::string> stemCache;
        std::unordered_set<std::string> ids;
        std::vector<Article> articles;
        std::set<std::string> vocabulary;
        std::map<std::string, std::size_t> folderCounts;
        articles.reserve(files.size());

        for (const auto& file : files) {
            Article article;
            try { article = readArticle(file, stopwords, stemmer, stemCache, allowUnlabeled); }
            catch (const std::exception& e) { throw std::runtime_error(file.string() + ": " + e.what()); }
            if (!ids.insert(article.id).second)
                throw std::runtime_error("Articol duplicat: " + article.id +
                    ". Selecteaza un singur set (Reuters_34 sau Reuters_7083).");
            for (const auto& entry : article.frequencies) vocabulary.insert(entry.first);
            ++folderCounts[article.folder];
            articles.push_back(std::move(article));
            if (articles.size() % 500 == 0) std::cout << "Procesate: " << articles.size() << '/' << files.size() << '\n';
        }
        writeVectors(output, articles, vocabulary);
        std::cout << "Articole procesate: " << articles.size() << '\n'
                  << "Termeni in dictionarul global: " << vocabulary.size() << '\n'
                  << "Stopwords distincte incarcate: " << stopwords.size() << '\n';
        for (const auto& entry : folderCounts) std::cout << entry.first << ": " << entry.second << '\n';
        std::cout << "Rezultat: " << fs::absolute(output).string() << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Eroare: " << e.what() << '\n';
        return 1;
    }
}

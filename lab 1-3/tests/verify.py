#!/usr/bin/env python3
r"""Verificare optionala cu Python 3.9+, numai biblioteca standard.

XML: ElementTree. Referinta Porter independenta: tokenizerul SQLite FTS5.
Exemplu: python tests/verify.py --exe C:\cale\ReutersFeatures.exe
"""
import argparse
from collections import Counter
from pathlib import Path
import re
import sqlite3
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def run(exe, dataset, stopwords, output, unlabeled=False, ok=True):
    command = [str(exe), str(dataset), str(stopwords), str(output)]
    if unlabeled:
        command.append("--allow-unlabeled")
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    if ok and result.returncode != 0:
        raise AssertionError(result.stdout + result.stderr)
    if not ok and result.returncode == 0:
        raise AssertionError("Se astepta un cod de eroare: " + str(dataset))
    return result


def parse_output(path):
    lines = path.read_text(encoding="utf-8").splitlines()
    separator = lines.index("@data")
    vocabulary = []
    for line in lines[:separator]:
        assert line.startswith("@attribute "), line
        word = line.removeprefix("@attribute ")
        assert re.fullmatch("[a-z]+", word), word
        vocabulary.append(word)
    assert vocabulary == sorted(set(vocabulary))
    rows = {}
    for line in lines[separator + 1:]:
        fields = [field.strip() for field in line.split("#")]
        assert len(fields) == 4, line
        doc_id, vector, topics, folder = fields
        assert doc_id not in rows, doc_id
        decoded = Counter()
        previous = -1
        for pair in vector.split():
            index, frequency = map(int, pair.split(":"))
            assert previous < index < len(vocabulary) and frequency > 0, line
            decoded[vocabulary[index]] = frequency
            previous = index
        rows[doc_id] = (decoded, topics.split(), folder)
    return vocabulary, rows


def porter_reference(words):
    # FTS5 este o implementare separata, inclusa in SQLite; nu foloseste codul C++.
    db = sqlite3.connect(":memory:")
    db.execute("CREATE VIRTUAL TABLE stems USING fts5(word, tokenize='porter ascii')")
    db.execute("CREATE VIRTUAL TABLE terms USING fts5vocab(stems, 'instance')")
    words = sorted(words)
    db.executemany("INSERT INTO stems(word) VALUES(?)", ((word,) for word in words))
    stem_map = {}
    for term, doc in db.execute("SELECT term, doc FROM terms"):
        word = words[doc - 1]
        assert word not in stem_map
        stem_map[word] = term
    assert len(stem_map) == len(words)
    db.close()
    return stem_map


def check_dataset(exe, name, temporary):
    dataset = ROOT / "data" / "Reuters" / name
    stopwords_file = ROOT / "data" / "stopwords.txt"
    output = temporary / (name + ".txt")
    run(exe, dataset, stopwords_file, output, unlabeled=name == "Reuters_7083")
    vocabulary, rows = parse_output(output)
    stopwords = set(stopwords_file.read_text().lower().split())
    originals = {}
    raw_words = set()
    for file in sorted(dataset.rglob("*")):
        if file.suffix.lower() != ".xml":
            continue
        root = ET.parse(file).getroot()
        text = " ".join(root.find("title").itertext()) + " " + " ".join(root.find("text").itertext())
        tokens = Counter(word for word in re.findall(r"[a-z]+", text.lower()) if word not in stopwords)
        raw_words.update(tokens)
        topics = list(dict.fromkeys(code.attrib["code"]
                      for code in root.findall("./metadata/codes[@class='bip:topics:1.0']/code")))
        folder = file.parent.name
        originals["D" + root.attrib["itemid"]] = (tokens, topics, folder)
    assert set(rows) == set(originals)
    stems = porter_reference(raw_words)
    expected_vocabulary = set()
    total_tokens = 0
    for doc_id, (raw_counts, topics, folder) in originals.items():
        expected = Counter()
        for word, count in raw_counts.items():
            expected[stems[word]] += count
        assert rows[doc_id] == (expected, topics, folder), (
            doc_id, rows[doc_id][0] - expected, expected - rows[doc_id][0])
        expected_vocabulary.update(expected)
        total_tokens += sum(expected.values())
    assert vocabulary == sorted(expected_vocabulary)
    example = ROOT / "examples" / ("vectori_34.txt" if name == "Reuters_34" else "vectori_7083.txt")
    assert output.read_bytes() == example.read_bytes(), "Rezultat diferit de exemplul inclus"
    # Aceleasi date trebuie sa produca exact aceiasi octeti la a doua rulare.
    repeat = temporary / (name + "_repeat.txt")
    run(exe, dataset, stopwords_file, repeat, unlabeled=name == "Reuters_7083")
    assert repeat.read_bytes() == output.read_bytes()
    return {"set": name, "articles": len(rows), "terms": len(vocabulary),
            "raw_words": len(raw_words), "retained_tokens": total_tokens,
            "folders": dict(Counter(row[2] for row in rows.values()))}


def check_edges(exe, temporary):
    training = temporary / "fixture" / "Training"
    testing = temporary / "fixture" / "Testing"
    training.mkdir(parents=True)
    testing.mkdir(parents=True)
    (training / "one.XML").write_text('''<?xml version="1.0"?>
    <!-- test --> <newsitem itemid="1"><title>CLASSIFICATION</title>
    <headline>leakheadline</headline><text><p>and clustering are</p>
    <p>learning algor&#105;thms algorithms &amp; <![CDATA[and]]></p></text>
    <copyright>leakcopyright</copyright><metadata>
    <codes class="bip:countries:1.0"><code code="COUNTRY"/></codes>
    <codes class='bip:topics:1.0'><code code='ml'/><code code='cla'/><code code='ml'/></codes>
    </metadata></newsitem>''', encoding="utf-8")
    (testing / "two.xml").write_text('''<newsitem itemid="2"><title>and is a</title>
    <text><p>THE 123 &quot;AND&quot;</p></text><metadata/></newsitem>''', encoding="utf-8")
    output = temporary / "fixture.txt"
    stopwords = ROOT / "data" / "stopwords.txt"
    run(exe, temporary / "fixture", stopwords, output)
    vocabulary, rows = parse_output(output)
    assert vocabulary == ["algorithm", "classif", "cluster", "learn"]
    assert rows["D1"] == (Counter(algorithm=2, classif=1, cluster=1, learn=1), ["ml", "cla"], "Training")
    assert rows["D2"] == (Counter(), [], "Testing")
    malformed = temporary / "malformed" / "Training"
    malformed.mkdir(parents=True)
    (malformed / "broken.xml").write_text("<newsitem><title>x</text></newsitem>")
    run(exe, malformed.parent, stopwords, temporary / "bad.txt", ok=False)
    run(exe, temporary / "nonexistent", stopwords, temporary / "missing.txt", ok=False)
    run(exe, ROOT / "data" / "Reuters" / "Reuters_34", temporary / "missing_stopwords.txt",
        temporary / "bad_stopwords.txt", ok=False)
    run(exe, ROOT / "data" / "Reuters" / "Reuters_7083", stopwords,
        temporary / "unlabeled.txt", ok=False)
    run(exe, ROOT / "data" / "Reuters", stopwords,
        temporary / "duplicates.txt", unlabeled=True, ok=False)
    return "XML entities/CDATA, boundaries, metadata, stopwords, empty vectors and 5 error cases: OK"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True, type=Path, help="Executabilul C++ deja compilat")
    args = parser.parse_args()
    exe = args.exe.resolve()
    if not exe.is_file():
        parser.error("Executabil inexistent")
    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory)
        print(check_edges(exe, temporary))
        for name in ["Reuters_34", "Reuters_7083"]:
            print(check_dataset(exe, name, temporary))
    print("TOATE VERIFICARILE AU TRECUT")


if __name__ == "__main__":
    main()

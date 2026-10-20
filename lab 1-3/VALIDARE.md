# Verificari efectuate

Data: 13 octombrie 2026.

Sursele C++ au fost compilate pe Linux cu GCC, C++17 si optiunile
`-O2 -Wall -Wextra -Wpedantic`, fara avertismente.
Fisierele de proiect Visual Studio au fost verificate ca XML valid.
Compilarea cu MSVC pe Windows nu a fost executata in acest mediu.

## Date reale

| Set | Articole | Termeni globali | Cuvinte distincte inainte de stemming | Total aparitii dupa eliminarea stopwords |
| --- | ---: | ---: | ---: | ---: |
| Reuters_34 | 34 | 1.259 | 1.605 | 4.826 |
| Reuters_7083 | 7.083 | 19.172 | 27.061 | 788.293 |

Lista furnizata contine 508 stopwords distincte.
Setul mic are 27 articole Training si 7 Testing. Setul mare nu este impartit
in aceste categorii; rezultatul sau foloseste numele folderului `Reuters_7083`.

Verificarea independenta a folosit parserul XML ElementTree din Python si
stemmerul Porter din tokenizerul SQLite FTS5. Pentru fiecare articol din
ambele seturi, s-au comparat toate frecventele, toate codurile topics si
folderul sursa cu rezultatul C++: toate au fost identice.

Dictionarele au fost verificate ca ordonate alfabetic si fara duplicate.
Indicii din vectori au fost verificati ca strict crescatori, in intervalul
dictionarului, cu frecvente strict pozitive.
O a doua rulare a fiecarui set a produs rezultate identice octet cu octet.

## Cazuri speciale

- Litere mari, stopwords si forme flexionate ale aceluiasi cuvant.
- Separarea paragrafelor, cifre, punctuatie, entitati XML numerice/standard,
  comentarii si CDATA.
- Titlul inclus in frecvente; headline, copyright si codurile countries excluse.
- Topics multiple si deduplicarea codurilor repetate.
- Articol format numai din stopwords, reprezentat prin vector rar gol.
- Erori pentru XML invalid, folder inexistent, stopwords lipsa, set fara
  Training/Testing in modul implicit si identificatori duplicati.

Verificarea poate fi repetata cu `tests/verify.py` si executabilul compilat.

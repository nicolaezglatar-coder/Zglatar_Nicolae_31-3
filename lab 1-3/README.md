# Etapa 1 - Extragerea trasaturilor, C++

Proiect de consola C++17 pentru articolele Reuters din fisierele furnizate.
Include sursele, proiectul Visual Studio, stopwords, ambele seturi Reuters si
rezultate deja generate. Nu necesita biblioteci C++ externe.

## Pornire in Visual Studio 2019 / 2022

1. Extrage complet arhiva proiectului.
2. Deschide `ReutersFeatures.sln`.
3. Selecteaza `Debug` sau `Release`, platforma `x64`.
4. Apasa **Ctrl+F5** (Start Without Debugging).
5. Rezultatul apare in `output/vectori_34.txt`.

Este necesara componenta **Desktop development with C++** din Visual Studio.
Proiectul selecteaza v142 pentru VS 2019 si v143 pentru VS 2022.
Directorul de lucru al debuggerului este folderul proiectului.
Daca rulezi executabilul dintr-un terminal, porneste din acelasi folder.

Alternativ, din **x64 Native Tools Command Prompt**, ruleaza `build_windows.bat`.

## Rulare si argumente

Fara argumente se proceseaza `data/Reuters/Reuters_34`, folosind
`data/stopwords.txt`. Exemple din folderul proiectului:

```bat
bin\x64\Debug\ReutersFeatures.exe
bin\x64\Debug\ReutersFeatures.exe "data\Reuters\Reuters_34" "data\stopwords.txt" "output\vectori_34.txt"
```

Programul accepta:

```text
ReutersFeatures [folder_dataset] [stopwords.txt] [rezultat.txt] [--allow-unlabeled]
```

Pentru setul mare:

```bat
bin\x64\Debug\ReutersFeatures.exe "data\Reuters\Reuters_7083" "data\stopwords.txt" "output\vectori_7083.txt" --allow-unlabeled
```

**Structura datelor originale:**

| Set | Numar de articole | Organizare |
| --- | ---: | --- |
| Reuters_34 | 34 | 27 Training, 7 Testing |
| Reuters_7083 | 7.083 | Toate XML-urile sunt direct in acelasi folder |

Cele 34 de articole sunt si in setul de 7.083. Selecteaza un singur set per rulare.
Programul detecteaza identificatorii duplicati si opreste rularea daca amesteci seturile.
Arhiva originala cu extensia `.zip` este de fapt RAR; datele din acest proiect
sunt deja extrase.

Pentru formatul Training/Testing cerut in tema, foloseste Reuters_34 sau o
impartire Training/Testing stabilita pentru setul mare. Cu `--allow-unlabeled`,
ultimul camp al setului mare este `Reuters_7083`, numele real al folderului sursa.
Programul nu atribuie automat o impartire care nu exista in date.

## Ce face programul

1. Gaseste recursiv fisierele `.xml` sau `.XML` si le ordoneaza.
2. Citeste numai `title` si `text` pentru continut. Include paragrafele `p`.
3. Transforma literele in lowercase si separa cuvintele pe caracterele care
   nu sunt litere englezesti `a-z`. Cifrele si punctuatia devin separatori.
4. Elimina cuvintele din lista stopwords, inainte de stemming.
5. Aplica algoritmul Porter, adaptat din fisierul C# furnizat.
6. Numara frecventele radacinilor pentru fiecare articol.
7. Construieste dictionarul global ordonat alfabetic si atribuie indici de la 0.
8. Scrie vectorii rari: numai perechile `indice:frecventa` cu frecventa nenula.
9. Extrage numai codurile din `metadata/codes` cu `class="bip:topics:1.0"`.
   Categoriile geografice si industriale nu sunt folosite ca topics.

Este un vector de frecvente brute, fara ponderare TF-IDF. Dictionarul se
construieste pe intregul set selectat, asa cum cere etapa 1.

## Formatul rezultatului

```text
@attribute algorithm
@attribute classif
@attribute cluster
@attribute learn
@attribute supervis
@attribute unsupervis
@data
D1 # 0:1 1:1 2:1 3:1 # ml cla clu # Training
D2 # 1:1 3:1 4:1 # ml cla # Training
D3 # 2:1 3:1 5:1 # ml clu # Testing
```

Exemplul este ilustrativ. La datele reale, identificatorul este `D` urmat de
atributul `itemid` din XML, de exemplu `D2330`.
Prima linie `@attribute` corespunde indicelui 0, a doua indicelui 1 etc.
Acesta este formatul text cerut in PDF; extensia folosita este `.txt`.

Toate documentele au conceptual aceeasi lungime: numarul de termeni din
dictionar. Pozitiile lipsa dintr-o linie sunt 0. Frecventele includ si titlul,
deci un cuvant din titlu si din corp se numara de doua ori.

Stemming-ul nu este lematizare: poate produce forme ca `classification -> classif`.
Algoritmul Porter nu garanteaza aceeasi forma pentru toate sinonimele sau
pluralele neregulate, cum ar fi `child` si `children`.

## Fisierele sursa

| Fisier | Rol |
| --- | --- |
| src/main.cpp | Citire articole, preprocesare, dictionar, vectori si salvare |
| src/PorterStemmer.hpp | Adaptarea C++ a clasei PorterStemmer.cs atasate |
| src/XmlLite.hpp | Cititor XML pentru structura Reuters, fara dependinte externe |
| data/stopwords.txt | Lista originala furnizata |
| examples/vectori_34.txt | Rezultatul deja generat pentru setul mic |
| examples/vectori_7083.txt | Rezultatul deja generat pentru setul mare |
| VALIDARE.md | Rezultatele verificarilor efectuate |

Parserul XML este destinat documentelor Reuters furnizate. Gestioneaza
comentarii, CDATA, atribute si entitati XML standard/numerice, dar nu incarca
DTD-uri sau entitati externe. Tokenizarea este pentru engleza ASCII, conform
datelor atasate; nu reprezinta o tokenizare generala pentru limbi cu diacritice.

## Compilare cu GCC sau CMake

Din folderul proiectului, cu GCC care suporta C++17:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic src/main.cpp -o ReutersFeatures
./ReutersFeatures
```

Sau cu CMake:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Executabilul va fi in `build/ReutersFeatures` pe Linux sau
`build/Release/ReutersFeatures.exe` pentru un generator Visual Studio.
Ruleaza-l din folderul proiectului pentru caile implicite.

## Erori

La XML invalid, lista stopwords lipsa, folder fara XML, identificatori duplicati
sau folder fara Training/Testing in modul implicit, programul afiseaza o eroare
si returneaza codul 1. XML-urile invalide nu sunt omise in tacere.
O noua rulare reusita inlocuieste fisierul rezultat selectat.

## Verificare optionala

Cu Python 3.9+ si SQLite cu suport FTS5, se poate rula:

```bat
python tests\verify.py --exe "bin\x64\Debug\ReutersFeatures.exe"
```

Verificarea foloseste ElementTree pentru XML si implementarea Porter din SQLite
pentru comparatia independenta a frecventelor. Nu sunt necesare pachete Python
externe. Python este necesar doar pentru aceasta verificare optionala.

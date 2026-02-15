# Raspored Smjena - Kalendar 📅

Desktop aplikacija za evidenciju radnih smjena sa modernim grafičkim interfejsom.

![Windows](https://img.shields.io/badge/Platform-Windows-blue)
![C++](https://img.shields.io/badge/Language-C++17-orange)
![License](https://img.shields.io/badge/License-MIT-green)

## ✨ Mogućnosti

- **Kalendarski prikaz** - pregledan mjesečni kalendar sa svim danima
- **Tri tipa smjena** - Dnevna (☀), Noćna (☾), Slobodan dan (✔)
- **Automatsko čuvanje** - podaci se čuvaju u fajlu pored exe-a
- **Statistika** - ukupan broj dnevnih, noćnih i slobodnih dana po mjesecu
- **Navigacija** - strelice za promjenu mjeseca, dugme "DANAS", scroll mišem
- **Hover efekti** - interaktivni elementi sa vizuelnim povratnim informacijama
- **Uređivanje** - lijevi klik za postavljanje, desni klik za brisanje
- **Moderan dizajn** - tamna tema sa gradijentima i zaobljenim ivicama
- **Bez zavisnosti** - koristi samo Windows API i GDI+ (ugrađeni u Windows)

## 🚀 Kako dobiti EXE fajl (GitHub Actions)

1. Napravite novi repozitorijum na GitHub-u
2. Uploadujte sve fajlove iz ovog projekta
3. Idite na **Actions** tab
4. Sačekajte da se build završi (~2-3 minuta)
5. Kliknite na završen workflow → **Artifacts**
6. Preuzmite `SmjeneKalendar-MSVC` ili `SmjeneKalendar-MinGW`
7. Raspakujte i stavite `SmjeneKalendar.exe` gdje želite
8. Pokrenite program!

## 🔧 Lokalno kompajliranje

### Sa Visual Studio (MSVC):
```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
EXE se nalazi u: `build\Release\SmjeneKalendar.exe`

### Sa MinGW:
```cmd
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
EXE se nalazi u: `build\SmjeneKalendar.exe`

### Bez CMake (MinGW direktno):
```cmd
g++ -o SmjeneKalendar.exe src/main.cpp -lgdi32 -lgdiplus -luser32 -lshell32 -lcomctl32 -lkernel32 -lole32 -mwindows -static -static-libgcc -static-libstdc++ -O2 -DUNICODE -D_UNICODE
```

## 📖 Korištenje

| Akcija | Rezultat |
|--------|----------|
| **Lijevi klik na dan** | Otvara meni za izbor smjene |
| **Desni klik na dan** | Briše postavljenu smjenu |
| **◀ / ▶ dugmad** | Prethodni / sljedeći mjesec |
| **DANAS dugme** | Vraća na trenutni mjesec |
| **Scroll mišem** | Mijenja mjesec |
| **Strelice (tastatura)** | Lijevo/desno za promjenu mjeseca |
| **Home (tastatura)** | Vraća na današnji datum |

## 💾 Čuvanje podataka

Podaci se čuvaju u fajlu `smjene_data.txt` koji se kreira u istom folderu gdje je EXE.
Format je jednostavan tekstualni fajl:
```
2026-02-15 1
2026-02-16 2
2026-02-17 3
```
Gdje: `1` = Dnevna, `2` = Noćna, `3` = Slobodan

Fajl se automatski čuva pri svakoj promjeni i učitava pri pokretanju programa.

## 📁 Struktura projekta

```
SmjeneKalendar/
├── src/
│   └── main.cpp              # Glavni izvorni kod
├── CMakeLists.txt             # Build konfiguracija
├── .github/
│   └── workflows/
│       └── build.yml          # GitHub Actions za automatski build
└── README.md
```

## ⚙️ Tehnički detalji

- **Jezik:** C++17
- **GUI:** Win32 API + GDI+ (nativni Windows)
- **Rendering:** Double-buffered sa anti-aliasing-om
- **Font:** Segoe UI
- **Min. rezolucija:** 780 x 650 px
- **Kompatibilnost:** Windows 7, 8, 10, 11

# Nora3D

Projekt zaliczeniowy z przedmiotu Zaawansowane Programowanie w C++ (4. semestr).

##  Opis projektu

Nora3D to aplikacja symulująca automat komórkowy w przestrzeni trójwymiarowej. Projekt stanowi rozwinięcie wcześniejszej wersji 2D i skupia się na poprawie wydajności, wizualizacji oraz analizie danych.

---

## Najważniejsze funkcjonalności

### Symulacja 3D

* Automat komórkowy działający na trójwymiarowej siatce
* Możliwość konfiguracji parametrów symulacji

### Wydajność

* Równoległe przetwarzanie przy użyciu Qt Concurrent
* Lepsza skalowalność dla dużych przestrzeni

### Baza danych

* Zapis wyników symulacji do SQLite (Qt SQL)
* Przechowywanie historii i parametrów
* Możliwość filtrowania i analizy danych

### Analiza danych

* Obliczanie statystyk w czasie rzeczywistym:

  * średnia liczba zarażeń
  * maksymalna liczba zarażeń
  * mediana czasu trwania
  * odchylenie standardowe

### Wizualizacja

* Dynamiczne wykresy w interfejsie Qt
* Wizualizacja przestrzeni 3D

---

## Rozwój projektu (2D -> 3D)

### Poprzednia wersja

* Automat 2D (płaska siatka)
* Obliczenia sekwencyjne (1 wątek)
* Zapis do plików tekstowych
* Prosta grafika rastrowa

### Obecna wersja

* Przestrzeń 3D
* Wielowątkowość (Qt Concurrent)
* Baza danych SQLite
* Rozbudowana analiza i wizualizacja

---

## Technologie

* C++
* Qt (Core, GUI, Widgets, 3D, SQL, Concurrent)
* SQLite

---

##  Status

Projekt w trakcie rozwoju

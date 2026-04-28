#pragma once
#include "SimulationTypes.h"

#include <QRandomGenerator>
#include <tuple> 
#include <algorithm>

class SimulationCore
{
public:
    SimulationCore() = default;

    void reset(int size);       // reset symulacji
    void step();                // krok sumulacji

    // gettery
        const Grid& grid() const { return cells; }                      // aktualny stan siatki
        int time() const { return currentTime; }                        // aktualny czas w cyklach
        int size() const { return gridSize; }                           // rozmiar siatki 
	    std::tuple<int, int, int> getCellCounts() const;                // krotka liczby danych komórek (zdrowe, zarażone, odporne)
        CellState getCellState(int r, int c) const;                     // stan komorki
        int getMaxInfectedCount() const { return maxInfectedCount; }    // maksymalna liczba zarażonych w symulacji

        // gettery ustawień
        int getInfectionDuration() const { return infectionDuration; }
        int getImmunityDuration() const { return immunityDuration; }
        int getInfectionChance() const { return infectionChance; }

    // settery
        void setCellState(int r, int c, CellState state);       // ustawienie stanu komorki
        void setParams(int infDuration, int immDuration, int infChance); // ustawienie danych zasad

private:
    // podstawowe dane
    int gridSize = 0;
    int currentTime = 0;

    // bufory
    Grid cells;         // Aktualny stan planszy
    Grid nextCells;     // bufor na kolejny stan
	TimerGrid timers;       // atualne liczniki
    TimerGrid nextTimers;   // bufor na kolejne liczniki

    // paramerty ustawien
    int infectionDuration = 6;
    int immunityDuration = 4;
    int infectionChance = 50;

	// maksymalna liczba zarażonych
    int maxInfectedCount = 0;

    // licznik zarazonych sasiadow
    int countInfectedNeighbors(int r, int c) const; 
};
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
        const Grid3D& grid() const { return cells; }                      // aktualny stan siatki
        int time() const { return currentTime; }                        // aktualny czas w cyklach
        int size() const { return gridSize; }                           // rozmiar siatki 
	    std::tuple<int, int, int, int> getCellCounts() const;                // krotka liczby danych komórek (zdrowe, zarażone, odporne)
        CellState getCellState(int x, int y, int z) const;                     // stan komorki
        int getMaxInfectedCount() const { return maxInfectedCount; }    // maksymalna liczba zarażonych w symulacji

        // gettery ustawień
        int getInfectionDuration() const { return infectionDuration; }
        int getImmunityDuration() const { return immunityDuration; }
        int getInfectionChance() const { return infectionChance; }
        int getDeathNumber() const { return deathNumber; }

    // settery
        void setCellState(int x, int y, int z, CellState state);       // ustawienie stanu komorki
        void setParams(int infDuration, int immDuration, int infChance, int dNumber); // ustawienie danych zasad

private:
    std::vector<int> infectionCounters;
    std::vector<int> nextInfectionCounters; 
    // podstawowe dane
    int gridSize = 0;
    int currentTime = 0;

    // bufory
    Grid3D cells;         // Aktualny stan planszy
    Grid3D nextCells;     // bufor na kolejny stan
	TimerGrid timers;       // atualne liczniki
    TimerGrid nextTimers;   // bufor na kolejne liczniki

    // paramerty ustawien
    int infectionDuration = 6;
    int immunityDuration = 4;
    int infectionChance = 50;
    int deathNumber = 5;

	// maksymalna liczba zarażonych
    int maxInfectedCount = 0;

    // licznik zarazonych sasiadow
    int countInfectedNeighbors(int x, int y, int z) const; 

    
};
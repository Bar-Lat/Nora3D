#include "SimulationCore.h"

// ====================================================================
// STEROWANIE I KONFIGURACJA
// ====================================================================

void SimulationCore::reset(int size)
{
	// reset rozmiaru siatki, czasu i maksymalnej liczby zarażonych
    gridSize = std::max(5, size);
    currentTime = 0;
    maxInfectedCount = 0;

    // inicjalizacja siatki obecnej i buforu stanem Healthy
    int totalCells = gridSize * gridSize * gridSize;
    cells.assign(totalCells, CellState::Healthy);
    nextCells.assign(totalCells, CellState::Healthy);

	// inicjalizacja czasu i buforu timerów zerami
    timers.assign(totalCells, 0);
    nextTimers.assign(totalCells, 0);

    // inicjalizacja liczby zarażeń danej komorki i buforu zerami
    infectionCounters.assign(totalCells, 0);
    nextInfectionCounters.assign(totalCells, 0);
}

void SimulationCore::setParams(int infDuration, int immDuration, int infChance, int dNumber)
{
    // minimalne wartosci
    infectionDuration = std::max(1, infDuration);
    immunityDuration = std::max(1, immDuration);

    // ogranicznik szansy zarazenia
    infectionChance = std::clamp(infChance, 0, 100);

    // ogranicznik smiercionosnego zarazenia
    deathNumber = std::clamp(dNumber, 1, 100);

}

// ====================================================================
// OBSŁUGA KOMÓRKI 
// ====================================================================

// pobranie stanu komórki
CellState SimulationCore::getCellState(int x, int y, int z) const
{
    if (x >= 0 && x < gridSize && y >= 0 && y < gridSize && z >= 0 && z < gridSize) {
        return cells[getIndex(x,y,z,gridSize)];
    }
    // Domyślnie zwraca Healthy poza granicami
    return CellState::Healthy;
}

// ustawienie stanu komórki
void SimulationCore::setCellState(int x, int y, int z, CellState state)
{
    if (x >= 0 && x < gridSize && y >= 0 && y < gridSize && z >= 0 && z < gridSize) {
        // Ustawienie stanu i timera
        cells[getIndex(x, y, z, gridSize)] = state;
        timers[getIndex(x, y, z, gridSize)] = (state == CellState::Infected) ? 1 : 0;
    }
}

// ====================================================================
// LOGIKA KROKU 
// ====================================================================

// krok symulacji
void SimulationCore::step() {
    currentTime++;
    auto* rng = QRandomGenerator::global();

    for (int i = 0; i < gridSize; ++i) {
        for (int j = 0; j < gridSize; ++j) {
            for (int k = 0; k < gridSize; ++k) {
                int xyz = getIndex(i, j, k, gridSize);
                CellState currentState = cells[xyz];
                CellState newState = currentState;
                int newTimer = timers[xyz];
                int newCounter = infectionCounters[xyz];

                switch (currentState) {
                case CellState::Healthy: {
                    int infectedCount = countInfectedNeighbors(i, j, k);
                    if (infectedCount > 0) {
                        for (int l = 0; l < infectedCount; ++l) {
                            if (rng->bounded(100) < infectionChance) {
                                newState = CellState::Infected;
                                newTimer = 1;
                            }
                        }
                    }
                    break;
                }
                case CellState::Infected:
                {
                    newTimer++;

                    // Sprawdzamy czy czas infekcji minął
                    if (newTimer > infectionDuration) {
                        newCounter++; // Zwiększamy licznik (komórka przeżyła pełny cykl infekcji)

                        if (newCounter >= deathNumber) {
                            // Umiera zamiast stać się odporną
                            newState = CellState::Dead;
                            newTimer = 0;
                        }
                        else {
                            // Standardowe przejście w odporność
                            newState = CellState::Immune;
                            newTimer = 1;
                        }
                    }
                    break;
                }
                case CellState::Immune: {
                    newTimer++;
                    if (newTimer > immunityDuration) {
                        newState = CellState::Healthy;
                        newTimer = 0;
                    }
                    break;
                }
                case CellState::Dead: {
                    // Komórka martwa nic nie robi
                    break;
                }
                }

                nextCells[xyz] = newState;
                nextTimers[xyz] = newTimer;
                nextInfectionCounters[xyz] = newCounter;
            }
        }
    }
    std::swap(cells, nextCells);
    std::swap(timers, nextTimers);
    std::swap(infectionCounters, nextInfectionCounters);
}

// ====================================================================
// METODY POMOCNICZE
// ====================================================================

// zliczanie zarazonych wokol komorki
int SimulationCore::countInfectedNeighbors(int x, int y, int z) const
{
    int count = 0;

    // Przeszukujemy sześcian wokół komórki
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {

                if (dx == 0 && dy == 0 && dz == 0) continue; // Pomijamy samą siebie

                int nx = x + dx;
                int ny = y + dy;
                int nz = z + dz;

                // Sprawdzamy granice (gridSize pochodzi z klasy)[cite: 3]
                if (nx >= 0 && nx < gridSize &&
                    ny >= 0 && ny < gridSize &&
                    nz >= 0 && nz < gridSize)
                {
                    if (cells[getIndex(nx, ny, nz, gridSize)] == CellState::Infected) {
                        count++;
                    }
                }
            }
        }
    }
    return count;
}

// zliczanie komórek w danym stanie
std::tuple<int, int, int, int> SimulationCore::getCellCounts() const
{
    int healthy = 0;
    int infected = 0;
    int immune = 0;
    int dead = 0;

    for (const auto& state : cells) {
        if (state == CellState::Healthy) healthy++;
        else if (state == CellState::Infected) infected++;
        else if (state == CellState::Immune) immune++;
        else if (state == CellState::Dead) dead++;
    }
    return std::make_tuple(healthy, infected, immune, dead);
}
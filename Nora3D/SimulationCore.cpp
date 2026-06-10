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
    individualDeathThresholds.assign(totalCells, 0);
}

void SimulationCore::setParams(int infDuration, int immDuration, int infChance, int dmin, int dmax, int fNum)
{
    // minimalne wartosci dla czasu
    infectionDuration = std::max(1, infDuration);
    immunityDuration = std::max(1, immDuration);

    // ogranicznik szansy zarazenia
    infectionChance = std::clamp(infChance, 0, 100);

    // Zabezpieczenie zakresu śmierci
    // Upewniamy się, że dmin nie jest większe niż dmax
    int minVal = std::clamp(dmin, 1, 100);
    int maxVal = std::clamp(dmax, 1, 100);

    if (minVal > maxVal) {
        std::swap(minVal, maxVal);
    }

    deathMin = minVal;
    deathMax = maxVal;
    filterNum = fNum;
}

// ====================================================================
// OBSŁUGA KOMÓRKI 
// ====================================================================

// pobranie stanu komórki
CellState SimulationCore::getCellState(int x, int y, int z) const
{
    if (x >= 0 && x < gridSize && y >= 0 && y < gridSize && z >= 0 && z < gridSize) {
        return cells[getIndex(x, y, z, gridSize)];
    }
    // Domyślnie zwraca Healthy poza granicami
    return CellState::Healthy;
}

// ustawienie stanu komórki
void SimulationCore::setCellState(int x, int y, int z, CellState state)
{
    if (x >= 0 && x < gridSize && y >= 0 && y < gridSize && z >= 0 && z < gridSize) {
        int xyz = getIndex(x, y, z, gridSize);

        cells[xyz] = state;
        timers[xyz] = (state == CellState::Infected) ? 1 : 0;

        if (state == CellState::Infected) {
            infectionCounters[xyz] = 1;

            individualDeathThresholds[xyz] = deathMin + QRandomGenerator::global()->bounded(deathMax - deathMin + 1);
        }
        else {
            infectionCounters[xyz] = 0;
            individualDeathThresholds[xyz] = 0;
        }
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
                    int infectedCount = countNeighborsByType(CellState::Infected, i, j, k);
                    if (infectedCount > 0) {
                        bool getsInfected = false;
                        for (int l = 0; l < infectedCount; ++l) {
                            if (rng->bounded(100) < infectionChance) {
                                getsInfected = true;
                                break;
                            }
                        }
                        if (getsInfected) {
                            newState = CellState::Infected;
                            newTimer = 1;
                            newCounter++; // Inkrementacja licznika przy zarażeniu
                            // Losowanie indywidualnego progu śmierci dla tej komórki
                            individualDeathThresholds[xyz] = deathMin + rng->bounded(deathMax - deathMin + 1);
                        }
                    }
                    break;
                }
                case CellState::Infected:
                {
                    newTimer++;
                    if (newTimer > infectionDuration) {
                        // Obliczamy sąsiadów do filtra
                        int protectedNeighbors = countNeighborsByType(CellState::Healthy, i, j, k);

                        // Jeśli filtr włączony I sąsiedzi chronią komórkę -> staje się odporna zamiast umierać
                        if (filterEnabled && protectedNeighbors > filterNum) {
                            newState = CellState::Immune;
                            newTimer = 1;
                        }
                        else {
                            // Standardowa logika śmierci
                            newCounter++;
                            if (newCounter >= individualDeathThresholds[xyz]) {
                                newState = CellState::Dead;
                                newTimer = 0;
                            }
                            else {
                                newState = CellState::Immune;
                                newTimer = 1;
                            }
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
                case CellState::Dead:
                    break;
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

// zliczanie zdrowych i odpornych wokol komorki
int SimulationCore::countNeighborsByType(CellState type, int x, int y, int z) const
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

                if (nx >= 0 && nx < gridSize &&
                    ny >= 0 && ny < gridSize &&
                    nz >= 0 && nz < gridSize)
                {
                    if (cells[getIndex(nx, ny, nz, gridSize)] == type ) {
                        count++;
                    }
                }
            }
        }
    }
    return count;
}
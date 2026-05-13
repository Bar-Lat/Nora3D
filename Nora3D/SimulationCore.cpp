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
}

void SimulationCore::setParams(int infDuration, int immDuration, int infChance)
{
    // minimalne wartosci
    infectionDuration = std::max(1, infDuration);
    immunityDuration = std::max(1, immDuration);

    // ogranicznik szansy zarazenia
    infectionChance = std::clamp(infChance, 0, 100);
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
void SimulationCore::step()
{
    currentTime++;
    auto* rng = QRandomGenerator::global();

	// iteracja po wszystkich komórkach siatki i zapis do bufora
    for (int i = 0; i < gridSize; ++i) {
        for (int j = 0; j < gridSize; ++j) {
            for (int k = 0;k < gridSize; ++k) {

                int xyz = getIndex(i, j, k, gridSize);
                CellState currentState = cells [xyz];
                CellState newState = currentState;
                int newTimer = timers[xyz];

                switch (currentState) {
                    // zdrowa -> zarażona
                case CellState::Healthy:
                {
                    int infectedCount = countInfectedNeighbors(i, j, k);
                    if (infectedCount > 0) {
                        bool getsInfected = false;

                        // szansa zarażenia zależna od zarażonych sąsiadów
                        for (int l = 0; l < infectedCount; ++l) {
                            if (rng->bounded(100) < infectionChance) {
                                getsInfected = true;
                                break;
                            }
                        }
                        // zmiana stanu i start licznika
                        if (getsInfected) {
                            newState = CellState::Infected;
                            newTimer = 1;
                        }
                    }
                    break;
                }
                // zarażona -> odporna
                case CellState::Infected:
                {
                    newTimer++;
                    if (newTimer > infectionDuration) {
                        newState = CellState::Immune;
                        newTimer = 1;
                    }
                    break;
                }
                // odporna -> zdrowa
                case CellState::Immune:
                {
                    newTimer++;
                    if (newTimer > immunityDuration) {
                        newState = CellState::Healthy;
                        newTimer = 0;
                    }
                    break;
                }
                }

                // zapis do bufora
                nextCells[xyz] = newState;
                nextTimers[xyz] = newTimer;
            }
        }
    }

	// zamiana buforów z aktualnymi danymi
    std::swap(cells, nextCells);
    std::swap(timers, nextTimers);

	// zliczenie komórek w danym stanie
    int healthy, infected, immune;
    std::tie(healthy, infected, immune) = getCellCounts();

	// maksymalna liczba zarażonych
    int currentInfected = infected;
    if (currentInfected > maxInfectedCount) {
        maxInfectedCount = currentInfected;
    }
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
std::tuple<int, int, int> SimulationCore::getCellCounts() const
{
    int healthy = 0;
    int infected = 0;
    int immune = 0;

    for (const auto& state : cells) {
        if (state == CellState::Healthy) healthy++;
        else if (state == CellState::Infected) infected++;
        else if (state == CellState::Immune) immune++;
    }
    return std::make_tuple(healthy, infected, immune);
}
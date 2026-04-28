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
    cells.assign(gridSize, std::vector<CellState>(gridSize, CellState::Healthy));
    nextCells.assign(gridSize, std::vector<CellState>(gridSize, CellState::Healthy));

	// inicjalizacja czasu i buforu timerów zerami
    timers.assign(gridSize, std::vector<int>(gridSize, 0));
    nextTimers.assign(gridSize, std::vector<int>(gridSize, 0));
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
CellState SimulationCore::getCellState(int r, int c) const
{
    if (r >= 0 && r < gridSize && c >= 0 && c < gridSize) {
        return cells[r][c];
    }
    // Domyślnie zwraca Healthy poza granicami
    return CellState::Healthy;
}

// ustawienie stanu komórki
void SimulationCore::setCellState(int r, int c, CellState state)
{
    if (r >= 0 && r < gridSize && c >= 0 && c < gridSize) {
        // Ustawienie stanu i timera
        cells[r][c] = state;
        timers[r][c] = (state == CellState::Infected) ? 1 : 0;
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

            CellState currentState = cells[i][j];
            CellState newState = currentState;
            int newTimer = timers[i][j];

            switch (currentState) {
                // zdrowa -> zarażona
            case CellState::Healthy:
            {
                int infectedCount = countInfectedNeighbors(i, j);
                if (infectedCount > 0) {
                    bool getsInfected = false;

                    // szansa zarażenia zależna od zarażonych sąsiadów
                    for (int k = 0; k < infectedCount; ++k) {
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
            nextCells[i][j] = newState;
            nextTimers[i][j] = newTimer;
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
int SimulationCore::countInfectedNeighbors(int r, int c) const
{
    int count = 0;

	// otoczenie 3x3 z pominięciem srodka
    for (int dr = -1; dr <= 1; ++dr) {
        int nr = r + dr;
		if (nr < 0 || nr >= gridSize) continue; // wyłączenie granic wierszy 

        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue; // wylaczenie srodka

            int nc = c + dc;
			if (nc >= 0 && nc < gridSize) { // wyłączenie granic kolumn
                if (cells[nr][nc] == CellState::Infected) {
                    count++;
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

    for (int i = 0; i < gridSize; ++i) {
        for (int j = 0; j < gridSize; ++j) {
            switch (cells[i][j]) {
            case CellState::Healthy:
                healthy++;
                break;
            case CellState::Infected:
                infected++;
                break;
            case CellState::Immune:
                immune++;
                break;
            }
        }
    }
    return std::make_tuple(healthy, infected, immune);
}
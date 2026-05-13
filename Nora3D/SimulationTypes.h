#pragma once
#include <vector>
#include <cstdint>

// mozliwe stany komorki
enum class CellState : uint8_t {
    Healthy, 
    Infected,
    Immune
};

// typy danych w symulacji
using Grid3D = std::vector<CellState>;
using TimerGrid = std::vector<int>;

inline int getIndex(int x, int y, int z, int gridSize) {
    return x + (y * gridSize) + (z * gridSize * gridSize);
}
#pragma once
#include <vector>
#include <cstdint>

// mozliwe stany komorki
enum class CellState : uint8_t {
    Healthy, 
    Infected,
    Immune
};

// tpypy danych w symulacji
using Grid = std::vector<std::vector<CellState>>;
using TimerGrid = std::vector<std::vector<int>>;
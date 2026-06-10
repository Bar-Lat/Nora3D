#pragma once

#include <QObject>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QVariant>
#include <QDateTime>
#include <QDebug>
#include <QByteArray>

#include "SimulationTypes.h"

struct SimulationConfiguration {
    int gridSize = 20;
    int infectionDuration = 6;
    int immunityDuration = 4;
    int infectionChance = 50;
    int deathMin = 10;
    int deathMax = 10;
    int filterNum = 1;
    bool filterEnabled = false;
};

class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    // Inicjalizacja bazy danych i tworzenie tabel
    bool init(const QString& dbName = "simulation_data.db");

    // Rejestruje nową symulację i jej konfigurację. Zwraca wygenerowane simulation_id.
    int startSimulation(int gridSize, int infDuration, int immDuration, int infChance, int dmin, int dmax, int fNum, bool filterEnabled);

    // Zapisuje stan populacji w konkretnym cyklu (do wykresów)
    void logCycle(int simulationId, int cycleNumber, int healthy, int infected, int immune, int dead);

    // Aktualizuje symulację o dane końcowe i statystyki analityczne
    void finishSimulation(int simulationId, int totalCycles, int peakInfected, int timeToPeak, int totalCases, bool herdImmunityReached);

    bool deleteSimulation(int simulationId);
    bool loadConfiguration(int simulationId, SimulationConfiguration& configuration);
    bool saveFinalGrid(int simulationId, int gridSize, const Grid3D& grid);
    bool loadFinalGrid(int simulationId, int& gridSize, Grid3D& grid);

private:
    QSqlDatabase db;
    bool createTables();
};

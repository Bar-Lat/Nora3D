#pragma once

#include <QObject>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QVariant>
#include <QDateTime>
#include <QDebug>

class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    // Inicjalizacja bazy danych i tworzenie tabel
    bool init(const QString& dbName = "simulation_data.db");

    // Rejestruje nową symulację i jej konfigurację. Zwraca wygenerowane simulation_id.
    int startSimulation(int gridSize, int infDuration, int immDuration, int infChance);

    // Zapisuje stan populacji w konkretnym cyklu (do wykresów)
    void logCycle(int simulationId, int cycleNumber, int healthy, int infected, int immune);

    // Aktualizuje symulację o dane końcowe i statystyki analityczne
    void finishSimulation(int simulationId, int totalCycles, int peakInfected, int timeToPeak, int totalCases, bool herdImmunityReached);

private:
    QSqlDatabase db;
    bool createTables();
};
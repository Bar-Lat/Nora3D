#include "DatabaseManager.h"

DatabaseManager::DatabaseManager() {
    // Dodajemy instancję bazy SQLite. 
    // Jeśli używasz wielu wątków, upewnij się, że nazwa połączenia jest unikalna, ale tutaj domyślna wystarczy.
    db = QSqlDatabase::addDatabase("QSQLITE");
}

DatabaseManager::~DatabaseManager() {
    if (db.isOpen()) {
        db.close();
    }
}

bool DatabaseManager::init(const QString& dbName) {
    db.setDatabaseName(dbName);

    if (!db.open()) {
        qCritical() << "Błąd otwarcia bazy danych:" << db.lastError().text();
        return false;
    }

    // W SQLite klucze obce są domyślnie wyłączone, musimy je włączyć
    QSqlQuery pragmaQuery;
    pragmaQuery.exec("PRAGMA foreign_keys = ON;");

    return createTables();
}

bool DatabaseManager::createTables() {
    QSqlQuery query;
    bool success = true;

    // 1. Tabela: Simulations (bez zmian)
    success &= query.exec("CREATE TABLE IF NOT EXISTS Simulations ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "total_cycles INTEGER DEFAULT 0, "
        "grid_size INTEGER)");

    // 2. Tabela: Configuration - DODANE: death_min, death_max, filter_num
    success &= query.exec("CREATE TABLE IF NOT EXISTS Configuration ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "simulation_id INTEGER, "
        "infection_duration INTEGER, "
        "immunity_duration INTEGER, "
        "infection_chance INTEGER, "
        "death_min INTEGER, "
        "death_max INTEGER, "
        "filter_num INTEGER, "
        "filter_enabled INTEGER DEFAULT 0, "
        "FOREIGN KEY(simulation_id) REFERENCES Simulations(id) ON DELETE CASCADE)");

    query.exec("ALTER TABLE Configuration ADD COLUMN death_min INTEGER");
    query.exec("ALTER TABLE Configuration ADD COLUMN death_max INTEGER");
    query.exec("ALTER TABLE Configuration ADD COLUMN filter_num INTEGER");
    query.exec("ALTER TABLE Configuration ADD COLUMN filter_enabled INTEGER DEFAULT 0");

    // 3. Tabela: SimulationLogs - DODANE: count_dead
    success &= query.exec("CREATE TABLE IF NOT EXISTS SimulationLogs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "simulation_id INTEGER, "
        "cycle_number INTEGER, "
        "count_healthy INTEGER, "
        "count_infected INTEGER, "
        "count_immune INTEGER, "
        "count_dead INTEGER, "
        "FOREIGN KEY(simulation_id) REFERENCES Simulations(id) ON DELETE CASCADE)");

    query.exec("ALTER TABLE SimulationLogs ADD COLUMN count_dead INTEGER DEFAULT 0");

    // 4. Tabela: Analytics (bez zmian w strukturze)
    success &= query.exec("CREATE TABLE IF NOT EXISTS Analytics ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "simulation_id INTEGER, "
        "peak_infected INTEGER, "
        "time_to_peak INTEGER, "
        "total_cases INTEGER, "
        "herd_immunity_reached BOOLEAN, "
        "FOREIGN KEY(simulation_id) REFERENCES Simulations(id) ON DELETE CASCADE)");

    success &= query.exec("CREATE TABLE IF NOT EXISTS FinalGrids ("
        "simulation_id INTEGER PRIMARY KEY, "
        "grid_size INTEGER NOT NULL, "
        "cell_states BLOB NOT NULL, "
        "FOREIGN KEY(simulation_id) REFERENCES Simulations(id) ON DELETE CASCADE)");

    return success;
}


int DatabaseManager::startSimulation(int gridSize, int infDuration, int immDuration, int infChance, int dmin, int dmax, int fNum, bool filterEnabled) {
    QSqlQuery query;
    db.transaction();

    query.prepare("INSERT INTO Simulations (grid_size) VALUES (?)");
    query.addBindValue(gridSize);
    query.exec();
    int simulationId = query.lastInsertId().toInt();

    query.prepare("INSERT INTO Configuration (simulation_id, infection_duration, immunity_duration, infection_chance, death_min, death_max, filter_num, filter_enabled) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(simulationId);
    query.addBindValue(infDuration);
    query.addBindValue(immDuration);
    query.addBindValue(infChance);
    query.addBindValue(dmin);
    query.addBindValue(dmax);
    query.addBindValue(fNum);
    query.addBindValue(filterEnabled ? 1 : 0);
    query.exec();

    db.commit();
    return simulationId;
}

void DatabaseManager::logCycle(int simulationId, int cycleNumber, int healthy, int infected, int immune, int dead) {
    QSqlQuery query;
    query.prepare("INSERT INTO SimulationLogs (simulation_id, cycle_number, count_healthy, count_infected, count_immune, count_dead) "
        "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(simulationId);
    query.addBindValue(cycleNumber);
    query.addBindValue(healthy);
    query.addBindValue(infected);
    query.addBindValue(immune);
    query.addBindValue(dead);
    query.exec();
}

void DatabaseManager::finishSimulation(int simulationId, int totalCycles, int peakInfected, int timeToPeak, int totalCases, bool herdImmunityReached) {
    if (simulationId < 0) return;

    QSqlQuery query;
    db.transaction();

    // Zaktualizuj całkowitą liczbę cykli w tabeli bazowej
    query.prepare("UPDATE Simulations SET total_cycles = ? WHERE id = ?");
    query.addBindValue(totalCycles);
    query.addBindValue(simulationId);
    if (!query.exec()) {
        qCritical() << "Błąd aktualizacji symulacji:" << query.lastError().text();
        db.rollback();
        return;
    }

    // Zapisz dane analityczne
    query.prepare("INSERT INTO Analytics (simulation_id, peak_infected, time_to_peak, total_cases, herd_immunity_reached) "
        "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(simulationId);
    query.addBindValue(peakInfected);
    query.addBindValue(timeToPeak);
    query.addBindValue(totalCases);
    query.addBindValue(herdImmunityReached);

    if (!query.exec()) {
        qCritical() << "Błąd zapisu statystyk analitycznych:" << query.lastError().text();
        db.rollback();
        return;
    }

    db.commit();
}

bool DatabaseManager::deleteSimulation(int simulationId) {
    if (simulationId < 0) return false;

    QSqlQuery query;
    query.prepare("DELETE FROM Simulations WHERE id = ?");
    query.addBindValue(simulationId);
    if (!query.exec()) {
        qCritical() << "Blad usuwania symulacji:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool DatabaseManager::loadConfiguration(int simulationId, SimulationConfiguration& configuration) {
    if (simulationId < 0) return false;

    QSqlQuery query;
    query.prepare("SELECT s.grid_size, c.infection_duration, c.immunity_duration, c.infection_chance, "
        "c.death_min, c.death_max, c.filter_num, COALESCE(c.filter_enabled, 0) "
        "FROM Simulations s "
        "JOIN Configuration c ON c.simulation_id = s.id "
        "WHERE s.id = ? "
        "ORDER BY c.id DESC LIMIT 1");
    query.addBindValue(simulationId);

    if (!query.exec() || !query.next()) {
        qCritical() << "Blad odczytu konfiguracji:" << query.lastError().text();
        return false;
    }

    configuration.gridSize = query.value(0).toInt();
    configuration.infectionDuration = query.value(1).toInt();
    configuration.immunityDuration = query.value(2).toInt();
    configuration.infectionChance = query.value(3).toInt();
    configuration.deathMin = query.value(4).toInt();
    configuration.deathMax = query.value(5).toInt();
    configuration.filterNum = query.value(6).toInt();
    configuration.filterEnabled = query.value(7).toBool();
    return true;
}

bool DatabaseManager::saveFinalGrid(int simulationId, int gridSize, const Grid3D& grid) {
    if (simulationId < 0 || gridSize <= 0 || grid.empty()) return false;

    QByteArray serialized;
    serialized.resize(static_cast<int>(grid.size()));
    for (int i = 0; i < static_cast<int>(grid.size()); ++i) {
        serialized[i] = static_cast<char>(grid[i]);
    }

    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO FinalGrids (simulation_id, grid_size, cell_states) "
        "VALUES (?, ?, ?)");
    query.addBindValue(simulationId);
    query.addBindValue(gridSize);
    query.addBindValue(serialized);

    if (!query.exec()) {
        qCritical() << "Blad zapisu finalnej siatki:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::loadFinalGrid(int simulationId, int& gridSize, Grid3D& grid) {
    if (simulationId < 0) return false;

    QSqlQuery query;
    query.prepare("SELECT grid_size, cell_states FROM FinalGrids WHERE simulation_id = ?");
    query.addBindValue(simulationId);

    if (!query.exec() || !query.next()) {
        gridSize = 0;
        grid.clear();
        return false;
    }

    gridSize = query.value(0).toInt();
    const QByteArray serialized = query.value(1).toByteArray();
    const int expectedCells = gridSize * gridSize * gridSize;

    if (gridSize <= 0 || serialized.size() != expectedCells) {
        qCritical() << "Nieprawidlowe dane finalnej siatki dla symulacji:" << simulationId;
        gridSize = 0;
        grid.clear();
        return false;
    }

    grid.resize(static_cast<size_t>(serialized.size()));
    for (int i = 0; i < serialized.size(); ++i) {
        const int state = static_cast<unsigned char>(serialized[i]);
        if (state < static_cast<int>(CellState::Healthy) || state > static_cast<int>(CellState::Dead)) {
            gridSize = 0;
            grid.clear();
            return false;
        }

        grid[static_cast<size_t>(i)] = static_cast<CellState>(state);
    }

    return true;
}

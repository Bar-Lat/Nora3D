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

    // 1. Tabela: Simulations
    success &= query.exec("CREATE TABLE IF NOT EXISTS Simulations ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "total_cycles INTEGER DEFAULT 0, "
        "grid_size INTEGER)");

    // 2. Tabela: Configuration
    success &= query.exec("CREATE TABLE IF NOT EXISTS Configuration ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "simulation_id INTEGER, "
        "infection_duration INTEGER, "
        "immunity_duration INTEGER, "
        "infection_chance INTEGER, "
        "FOREIGN KEY(simulation_id) REFERENCES Simulations(id) ON DELETE CASCADE)");

    // 3. Tabela: SimulationLogs (szeregi czasowe dla wykresów)
    success &= query.exec("CREATE TABLE IF NOT EXISTS SimulationLogs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "simulation_id INTEGER, "
        "cycle_number INTEGER, "
        "count_healthy INTEGER, "
        "count_infected INTEGER, "
        "count_immune INTEGER, "
        "FOREIGN KEY(simulation_id) REFERENCES Simulations(id) ON DELETE CASCADE)");

    // 4. Tabela: Analytics
    success &= query.exec("CREATE TABLE IF NOT EXISTS Analytics ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "simulation_id INTEGER, "
        "peak_infected INTEGER, "
        "time_to_peak INTEGER, "
        "total_cases INTEGER, "
        "herd_immunity_reached BOOLEAN, "
        "FOREIGN KEY(simulation_id) REFERENCES Simulations(id) ON DELETE CASCADE)");

    if (!success) {
        qCritical() << "Błąd tworzenia tabel:" << query.lastError().text();
    }

    return success;
}

int DatabaseManager::startSimulation(int gridSize, int infDuration, int immDuration, int infChance) {
    QSqlQuery query;

    // Rozpoczynamy transakcję dla bezpieczeństwa relacji
    db.transaction();

    // Zapisz wpis główny symulacji
    query.prepare("INSERT INTO Simulations (grid_size) VALUES (?)");
    query.addBindValue(gridSize);

    if (!query.exec()) {
        qCritical() << "Błąd zapisu nowej symulacji:" << query.lastError().text();
        db.rollback();
        return -1;
    }

    // Pobierz nadane ID symulacji
    int simulationId = query.lastInsertId().toInt();

    // Zapisz konfigurację powiązaną z tym ID
    query.prepare("INSERT INTO Configuration (simulation_id, infection_duration, immunity_duration, infection_chance) "
        "VALUES (?, ?, ?, ?)");
    query.addBindValue(simulationId);
    query.addBindValue(infDuration);
    query.addBindValue(immDuration);
    query.addBindValue(infChance);

    if (!query.exec()) {
        qCritical() << "Błąd zapisu konfiguracji:" << query.lastError().text();
        db.rollback();
        return -1;
    }

    db.commit();
    return simulationId;
}

void DatabaseManager::logCycle(int simulationId, int cycleNumber, int healthy, int infected, int immune) {
    if (simulationId < 0) return;

    QSqlQuery query;
    query.prepare("INSERT INTO SimulationLogs (simulation_id, cycle_number, count_healthy, count_infected, count_immune) "
        "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(simulationId);
    query.addBindValue(cycleNumber);
    query.addBindValue(healthy);
    query.addBindValue(infected);
    query.addBindValue(immune);

    if (!query.exec()) {
        qWarning() << "Błąd logowania cyklu:" << query.lastError().text();
    }
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
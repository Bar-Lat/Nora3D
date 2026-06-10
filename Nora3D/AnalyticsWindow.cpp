#include "AnalyticsWindow.h"

#include <QMessageBox>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSqlError>
#include <QWidget>

AnalyticsWindow::AnalyticsWindow(DatabaseManager* databaseManager, QWidget* parent)
    : QDialog(parent)
    , dbManager(databaseManager)
{
    setWindowTitle("Analiza symulacji");
    resize(900, 650);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QHBoxLayout* controlsLayout = new QHBoxLayout();
    simComboBox = new QComboBox(this);
    simComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    controlsLayout->addWidget(simComboBox);

    loadConfigurationButton = new QPushButton("Wczytaj konfiguracje", this);
    deleteSimulationButton = new QPushButton("Usun", this);
    controlsLayout->addWidget(loadConfigurationButton);
    controlsLayout->addWidget(deleteSimulationButton);
    layout->addLayout(controlsLayout);

    tabs = new QTabWidget(this);

    QWidget* chartTab = new QWidget(this);
    QVBoxLayout* chartLayout = new QVBoxLayout(chartTab);
    customPlot = new QCustomPlot(chartTab);
    customPlot->legend->setVisible(true);
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    chartLayout->addWidget(customPlot);
    tabs->addTab(chartTab, "Wykres");

    QWidget* finalGridTab = new QWidget(this);
    QVBoxLayout* finalGridLayout = new QVBoxLayout(finalGridTab);
    finalGridStatusLabel = new QLabel(finalGridTab);
    finalGridStatusLabel->setAlignment(Qt::AlignCenter);
    finalGridCanvas = new BoardCanvas(finalGridTab);
    finalGridCanvas->setSpacing(10);
    finalGridLayout->addWidget(finalGridStatusLabel);
    finalGridLayout->addWidget(finalGridCanvas, 1);
    tabs->addTab(finalGridTab, "Finalna siatka");

    layout->addWidget(tabs);

    connect(simComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AnalyticsWindow::onSimulationSelected);
    connect(loadConfigurationButton, &QPushButton::clicked,
        this, &AnalyticsWindow::onLoadConfigurationClicked);
    connect(deleteSimulationButton, &QPushButton::clicked,
        this, &AnalyticsWindow::onDeleteSimulationClicked);

    loadSimulationsList();
}

void AnalyticsWindow::loadSimulationsList() {
    const QSignalBlocker blocker(simComboBox);
    const int previouslySelectedId = selectedSimulationId();

    simComboBox->clear();

    QSqlQuery query("SELECT id, timestamp, total_cycles FROM Simulations ORDER BY id DESC");
    while (query.next()) {
        const int id = query.value(0).toInt();
        const QString label = QString("Symulacja #%1 - %2 (%3 cykli)")
            .arg(id)
            .arg(query.value(1).toString())
            .arg(query.value(2).toInt());
        simComboBox->addItem(label, id);
    }

    const int restoredIndex = simComboBox->findData(previouslySelectedId);
    if (restoredIndex >= 0) {
        simComboBox->setCurrentIndex(restoredIndex);
    }

    updateButtons();
    drawChart(selectedSimulationId());
    loadFinalGridPreview(selectedSimulationId());
}

void AnalyticsWindow::drawChart(int simulationId) {
    customPlot->clearGraphs();
    customPlot->clearItems();

    if (simulationId < 0) {
        customPlot->xAxis->setLabel("Cykl");
        customPlot->yAxis->setLabel("Liczba komorek");
        customPlot->xAxis->setRange(0, 1);
        customPlot->yAxis->setRange(0, 1);
        customPlot->replot();
        return;
    }

    QVector<double> cycles, healthy, infected, immune, dead;

    QSqlQuery query;
    query.prepare("SELECT cycle_number, count_healthy, count_infected, count_immune, count_dead "
        "FROM SimulationLogs WHERE simulation_id = ? ORDER BY cycle_number ASC");
    query.addBindValue(simulationId);

    if (!query.exec()) {
        QMessageBox::warning(this, "Blad wykresu", "Nie udalo sie odczytac danych wykresu.");
        customPlot->replot();
        return;
    }

    while (query.next()) {
        cycles.append(query.value(0).toDouble());
        healthy.append(query.value(1).toDouble());
        infected.append(query.value(2).toDouble());
        immune.append(query.value(3).toDouble());
        dead.append(query.value(4).toDouble());
    }

    customPlot->addGraph();
    customPlot->graph(0)->setName("Zdrowe");
    customPlot->graph(0)->setPen(QPen(QColor(46, 160, 67), 2));
    customPlot->graph(0)->setData(cycles, healthy);

    customPlot->addGraph();
    customPlot->graph(1)->setName("Zarazone");
    customPlot->graph(1)->setPen(QPen(QColor(220, 53, 69), 2));
    customPlot->graph(1)->setData(cycles, infected);

    customPlot->addGraph();
    customPlot->graph(2)->setName("Odporne");
    customPlot->graph(2)->setPen(QPen(QColor(230, 159, 0), 2));
    customPlot->graph(2)->setData(cycles, immune);

    customPlot->addGraph();
    customPlot->graph(3)->setName("Martwe");
    customPlot->graph(3)->setPen(QPen(QColor(80, 80, 80), 2));
    customPlot->graph(3)->setData(cycles, dead);

    customPlot->xAxis->setLabel("Cykl");
    customPlot->yAxis->setLabel("Liczba komorek");

    if (cycles.isEmpty()) {
        customPlot->xAxis->setRange(0, 1);
        customPlot->yAxis->setRange(0, 1);
    }
    else {
        double maxValue = 0.0;
        for (int i = 0; i < cycles.size(); ++i) {
            maxValue = qMax(maxValue, healthy[i]);
            maxValue = qMax(maxValue, infected[i]);
            maxValue = qMax(maxValue, immune[i]);
            maxValue = qMax(maxValue, dead[i]);
        }

        customPlot->xAxis->setRange(cycles.first(), qMax(cycles.last(), cycles.first() + 1.0));
        customPlot->yAxis->setRange(0, qMax(maxValue * 1.10, 1.0));
    }

    customPlot->replot();
}

void AnalyticsWindow::onSimulationSelected(int index) {
    Q_UNUSED(index);
    updateButtons();
    drawChart(selectedSimulationId());
    loadFinalGridPreview(selectedSimulationId());
}

void AnalyticsWindow::loadFinalGridPreview(int simulationId) {
    if (simulationId < 0 || dbManager == nullptr) {
        finalGridStatusLabel->setText("Brak wybranej symulacji.");
        finalGridCanvas->setGrid(Grid3D());
        return;
    }

    int gridSize = 0;
    Grid3D grid;
    if (!dbManager->loadFinalGrid(simulationId, gridSize, grid)) {
        finalGridStatusLabel->setText("Brak zapisanego finalnego stanu dla tej symulacji.");
        finalGridCanvas->setGrid(Grid3D());
        return;
    }

    finalGridStatusLabel->setText(QString("Finalna siatka: %1 x %1 x %1").arg(gridSize));
    finalGridCanvas->setGrid(grid);
}

void AnalyticsWindow::onDeleteSimulationClicked() {
    const int simulationId = selectedSimulationId();
    if (simulationId < 0 || dbManager == nullptr) return;

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        "Usun symulacje",
        QString("Usunac symulacje #%1 wraz z jej logami i analiza?").arg(simulationId));

    if (answer != QMessageBox::Yes) return;

    if (!dbManager->deleteSimulation(simulationId)) {
        QMessageBox::warning(this, "Blad usuwania", "Nie udalo sie usunac symulacji.");
        return;
    }

    loadSimulationsList();
}

void AnalyticsWindow::onLoadConfigurationClicked() {
    const int simulationId = selectedSimulationId();
    if (simulationId < 0 || dbManager == nullptr) return;

    SimulationConfiguration configuration;
    if (!dbManager->loadConfiguration(simulationId, configuration)) {
        QMessageBox::warning(this, "Blad konfiguracji", "Nie udalo sie wczytac konfiguracji symulacji.");
        return;
    }

    emit configurationLoadRequested(configuration);
    accept();
}

int AnalyticsWindow::selectedSimulationId() const {
    if (simComboBox == nullptr || simComboBox->currentIndex() < 0) return -1;
    return simComboBox->currentData().toInt();
}

void AnalyticsWindow::updateButtons() {
    const bool hasSelection = selectedSimulationId() >= 0;
    loadConfigurationButton->setEnabled(hasSelection && dbManager != nullptr);
    deleteSimulationButton->setEnabled(hasSelection && dbManager != nullptr);
}

#include "nora3d.h"
#include "AnalyticsWindow.h"

#include <QSignalBlocker>

// ====================================================================
// KONSTRUKTOR I DESTRUKTOR
// ====================================================================

nora3d::nora3d(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::Nora3DClass)
    , simulation(std::make_unique<SimulationCore>()) // inicjalizacja Modelu
    , timer(new QTimer(this))                       // inicjalizacja Timera
{
    ui->setupUi(this);
    if (!dbManager.init()) {
        QMessageBox::warning(this, "Błąd Bazy Danych", "Nie udało się zainicjalizować bazy danych SQLite.");
    }

    // Konfiguracja Widoku Planszy
    boardCanvas = new BoardCanvas(this);
    ui->boardWidget->layout()->addWidget(boardCanvas);

    // Konfiguracja kontrolek 
    ui->boardSize->setRange(5, 800);
    ui->boardSize->setValue(20);

    ui->gameTime->setRange(1, 1000);
    ui->gameTime->setValue(2);

    ui->timeInfection->setRange(1, 20);
    ui->timeInfection->setValue(6);

    ui->timeImmune->setRange(1, 20);
    ui->timeImmune->setValue(4);

    ui->chanceInfection->setRange(0, 100);
    ui->chanceInfection->setValue(50);

    ui->spacingSlider->setRange(10, 100);
    ui->spacingSlider->setValue(10);

    ui->deathSlider->setRange(1, 100);
    ui->deathSlider->setValue(10);

    ui->deathSliderMax->setRange(1, 100);
    ui->deathSliderMax->setValue(5);

    ui->deathSliderMin->setRange(1, 100);
    ui->deathSliderMin->setValue(10);

    ui->filterSlider->setRange(0, 26);
    ui->filterSlider->setValue(6);


    // polaczenia sygnal slot
    connect(ui->startStopButton, &QPushButton::clicked, this, &nora3d::onStartStopClicked);
    connect(ui->resetButton, &QPushButton::clicked, this, &nora3d::onResetClicked);

    connect(ui->gameTime, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);
    connect(ui->timeInfection, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);
    connect(ui->timeImmune, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);
    connect(ui->chanceInfection, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);

    connect(ui->boardSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &nora3d::onSizeChange);

    connect(timer, &QTimer::timeout, this, &nora3d::onTick);
    connect(boardCanvas, &BoardCanvas::cellClicked, this, &nora3d::handleCellClick);
    connect(ui->deathSliderMin, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);
    connect(ui->deathSliderMax, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);
    connect(ui->deathSlider, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);
    connect(ui->spacingSlider, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);
    connect(ui->spacingSlider, &QSlider::valueChanged,
        [this](int value)
        {
            boardCanvas->setSpacing(value);
        });
    connect(ui->showAnalyticsButton, &QPushButton::clicked, this, &nora3d::onShowAnalyticsClicked);

    connect(ui->deadRandom, &QCheckBox::toggled, this, &nora3d::onRandomDeathToggled);
    connect(ui->filterCheckbox, &QCheckBox::toggled, [this](bool checked) {
        ui->filterSlider->setEnabled(checked);
        simulation->setFilteringEnabled(checked);
        });
    connect(ui->filterSlider, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);

	// etykiety suwaków
    double cps = (double)ui->gameTime->value();
    ui->cyclesPerSecond->setText(QString::number(cps, 'f', 1) + " cykli/s");
    int interval = (ui->gameTime->value() > 0) ? (1000 / ui->gameTime->value()) : 1000;
    timer->setInterval(interval);

    ui->InfectionSeconds->setText(QString::number(ui->timeInfection->value()) + " cykli");
    ui->immuneSeconds->setText(QString::number(ui->timeImmune->value()) + " cykli");
    ui->infectionProbability->setText(QString::number(ui->chanceInfection->value()) + " %");
    ui->deathLabel->setText(QString::number(ui->deathSlider->value()));
    ui->filterLabel->setText(QString::number(ui->filterSlider->value()));



    ui->deathSliderMin->setVisible(0);
    ui->deathSliderMax->setVisible(0);
    ui->deathMinLabel->setVisible(0);
    ui->deathMaxLabel->setVisible(0);
    ui->filterSlider->setEnabled(0);


    onResetClicked();
}

// destruktor
nora3d::~nora3d()
{
    delete ui;
}

// ====================================================================
// STEROWANIE I PARAMETRY
// ====================================================================

void nora3d::onRandomDeathToggled(bool checked) {
    // Ukrywamy/pokazujemy odpowiednie kontrolki
    ui->deathSlider->setVisible(!checked);
    ui->deathLabel->setVisible(!checked);

    ui->deathSliderMin->setVisible(checked);
    ui->deathSliderMax->setVisible(checked);
    ui->deathMinLabel->setVisible(checked); 
    ui->deathMaxLabel->setVisible(checked);

    applySettings();
}

void nora3d::applySettings() {
    int minVal, maxVal;

    if (ui->deadRandom->isChecked()) {
        minVal = ui->deathSliderMin->value();
        maxVal = ui->deathSliderMax->value();
    }
    else {
        minVal = ui->deathSlider->value();
        maxVal = ui->deathSlider->value();
    }

    simulation->setParams(
        ui->timeInfection->value(),
        ui->timeImmune->value(),
        ui->chanceInfection->value(),
        minVal,
        maxVal,
        ui->filterSlider->value()
    );
}

void nora3d::onStartStopClicked()
{
    // Start i stop
    if (timer->isActive()) {
        timer->stop();
        ui->startStopButton->setText("Start");
    }
    else {
        applySettings();

        // --- ROZPOCZĘCIE NOWEJ SYMULACJI W BAZIE ---
        // Rejestrujemy symulację w bazie tylko, jeśli startujemy od zera (cykl 0)
        if (simulation->time() == 0) {
            int deathMin = ui->deathSlider->value();
            int deathMax = ui->deathSlider->value();
            if (ui->deadRandom->isChecked()) {
                deathMin = ui->deathSliderMin->value();
                deathMax = ui->deathSliderMax->value();
            }

            currentSimulationId = dbManager.startSimulation(
                simulation->size(),
                simulation->getInfectionDuration(),
                simulation->getImmunityDuration(),
                simulation->getInfectionChance(),
                deathMin,
                deathMax,
                ui->filterSlider->value(),
                ui->filterCheckbox->isChecked()
            );
            timeToPeak = 0;
            currentPeakInfected = 0;
        }

        timer->start();
        ui->startStopButton->setText("Pauza");
    }
}

void nora3d::onResetClicked()
{
    // Stop i reset parametrow symulacji 
    timer->stop();
    ui->startStopButton->setText("Start");

    applySettings();
    simulation->reset(ui->boardSize->value());

    // --- RESET ZMIENNYCH ANALITYCZNYCH ---
    currentSimulationId = -1;
    timeToPeak = 0;
    currentPeakInfected = 0;

    updateUI();
}

// przy zmianie rozmiaru planszy reset
void nora3d::onSizeChange()
{
    onResetClicked();
}

// ====================================================================
// GŁÓWNA PĘTLA SYMULACJI
// ====================================================================

void nora3d::onTick()
{
    // krok 
    simulation->step();

	// bieżacy stan komórek
    int healthyCount, infectedCount, immuneCount, deadCount;
    std::tie(healthyCount, infectedCount, immuneCount, deadCount) = simulation->getCellCounts();

    // --- LOGOWANIE CYKLU I ANALITYKA ---
    if (currentSimulationId != -1) {
        dbManager.logCycle(currentSimulationId, simulation->time(), healthyCount, infectedCount, immuneCount, deadCount);

        // Aktualizacja szczytu epidemii
        if (infectedCount > currentPeakInfected) {
            currentPeakInfected = infectedCount;
            timeToPeak = simulation->time();
        }
    }

    // koniec gdy nie ma zarażonych ani odpornych
    if (infectedCount == 0 && immuneCount == 0) {
        if (timer->isActive()) {
            // Zbieranie danych końcowych
            int timeCycles = simulation->time();
            int maxInfected = simulation->getMaxInfectedCount();
            int size = simulation->size();

            // Obliczenie całkowitej liczby przypadków: początkowa populacja - końcowi zdrowi
            int totalPopulation = size * size * size;
            int totalCases = totalPopulation - healthyCount - deadCount;

            // Odporność stadna osiągnięta, jeśli na końcu zostali jacyś zdrowi obywatele
            bool herdImmunityReached = (healthyCount > 0);

            // --- ZAPIS KOŃCOWY DO BAZY ---
            if (currentSimulationId != -1) {
                dbManager.saveFinalGrid(currentSimulationId, size, simulation->grid());
                dbManager.finishSimulation(currentSimulationId, timeCycles, maxInfected, timeToPeak, totalCases, herdImmunityReached);
            }

            // koniec i odświeżenie
            onStartStopClicked();
            updateUI();
        }
    }
    updateUI();
}

// ====================================================================
// WIDOK I INTERAKCJA
// ====================================================================

void nora3d::updateUI()
{
    int timeCycles = simulation->time();
    double timeSeconds = 0.0;

	// zamiana z cykli na sekundy
    if (timer->isActive() || timer->interval() > 0) {
        timeSeconds = (timeCycles * timer->interval()) / 1000.0;
    }

    // update cykli i czasu
    ui->label_cycles->setText(QString("Cykle: %1").arg(timeCycles));
    ui->label_time->setText(QString("Czas: %1 s").arg(timeSeconds, 0, 'f', 2));

    // update danych komórek
    int healthyCount, infectedCount, immuneCount, deadCount;
    std::tie(healthyCount, infectedCount, immuneCount, deadCount) = simulation->getCellCounts();
    ui->HealthyCellsLabel->setText(QString("%1").arg(healthyCount));
    ui->InfectedCellsLabel->setText(QString("%1").arg(infectedCount));
    ui->ImmuneCellsLabel->setText(QString("%1").arg(immuneCount));
    ui->deadCellsLabel->setText(QString("%1").arg(deadCount));

    // odświeżenie planszy
    boardCanvas->setGrid(simulation->grid());
}

void nora3d::updateSliderLabels(int value)
{
    QSlider* senderSlider = qobject_cast<QSlider*>(sender());
    if (!senderSlider) return;

    if (senderSlider == ui->gameTime) {
        double CPS = (double)value;
        ui->cyclesPerSecond->setText(QString::number(CPS, 'f', 1) + " cykli/s");
        int interval = (value > 0) ? (1000 / value) : 1000;
        timer->setInterval(interval);
        updateUI(); 
    }
    else {
        if (senderSlider == ui->timeImmune) { ui->immuneSeconds->setText(QString::number(value) + " cykli"); }
        else if (senderSlider == ui->timeInfection) { ui->InfectionSeconds->setText(QString::number(value) + " cykli"); }
        else if (senderSlider == ui->chanceInfection) { ui->infectionProbability->setText(QString::number(value) + " %"); }
        else if (senderSlider == ui->deathSlider) { ui->deathLabel->setText(QString::number(value)); }
        else if (senderSlider == ui->deathSliderMin) { ui->deathMinLabel->setText("Od: " + QString::number(value)); }
        else if (senderSlider == ui->deathSliderMax) { ui->deathMaxLabel->setText("Do: " + QString::number(value)); }
        else if (senderSlider == ui->filterSlider) { ui->filterLabel->setText( QString::number(value)); }
        else if (senderSlider == ui->spacingSlider) {
            ui->spacingLabel->setText(QString::number(value/10.0f));
            boardCanvas->update(); 
            return; 
        }
        applySettings();
        updateUI();
    }
}

void nora3d::handleCellClick(int x, int y, int z)
{
	// handler klikniecia komórki
        CellState currentState = simulation->getCellState(x,y,z);

        if (currentState == CellState::Healthy) {
            // zdrowa -> zarażona
            simulation->setCellState(x, y, z, CellState::Infected);
        }
        else{
            // zarażona / odporna -> zdrowa
            simulation->setCellState(x, y, z, CellState::Healthy);
        }
        updateUI();
    
}
void nora3d::onShowAnalyticsClicked() {
    AnalyticsWindow analyticsDialog(&dbManager, this);
    connect(&analyticsDialog, &AnalyticsWindow::configurationLoadRequested,
        this, &nora3d::loadSimulationConfiguration);
    analyticsDialog.exec();
}

void nora3d::loadSimulationConfiguration(const SimulationConfiguration& configuration) {
    timer->stop();
    ui->startStopButton->setText("Start");

    const bool randomDeath = configuration.deathMin != configuration.deathMax;

    {
        const QSignalBlocker boardSizeBlocker(ui->boardSize);
        const QSignalBlocker infectionBlocker(ui->timeInfection);
        const QSignalBlocker immuneBlocker(ui->timeImmune);
        const QSignalBlocker chanceBlocker(ui->chanceInfection);
        const QSignalBlocker deathBlocker(ui->deathSlider);
        const QSignalBlocker deathMinBlocker(ui->deathSliderMin);
        const QSignalBlocker deathMaxBlocker(ui->deathSliderMax);
        const QSignalBlocker filterBlocker(ui->filterSlider);
        const QSignalBlocker randomDeathBlocker(ui->deadRandom);
        const QSignalBlocker filterCheckBlocker(ui->filterCheckbox);

        ui->boardSize->setValue(configuration.gridSize);
        ui->timeInfection->setValue(configuration.infectionDuration);
        ui->timeImmune->setValue(configuration.immunityDuration);
        ui->chanceInfection->setValue(configuration.infectionChance);
        ui->deadRandom->setChecked(randomDeath);
        ui->deathSlider->setValue(configuration.deathMin);
        ui->deathSliderMin->setValue(configuration.deathMin);
        ui->deathSliderMax->setValue(configuration.deathMax);
        ui->filterCheckbox->setChecked(configuration.filterEnabled);
        ui->filterSlider->setValue(configuration.filterNum);
    }

    onRandomDeathToggled(randomDeath);
    ui->filterSlider->setEnabled(configuration.filterEnabled);
    simulation->setFilteringEnabled(configuration.filterEnabled);

    ui->InfectionSeconds->setText(QString::number(ui->timeInfection->value()) + " cykli");
    ui->immuneSeconds->setText(QString::number(ui->timeImmune->value()) + " cykli");
    ui->infectionProbability->setText(QString::number(ui->chanceInfection->value()) + " %");
    ui->deathLabel->setText(QString::number(ui->deathSlider->value()));
    ui->deathMinLabel->setText("Od: " + QString::number(ui->deathSliderMin->value()));
    ui->deathMaxLabel->setText("Do: " + QString::number(ui->deathSliderMax->value()));
    ui->filterLabel->setText(QString::number(ui->filterSlider->value()));

    applySettings();
    simulation->reset(ui->boardSize->value());
    currentSimulationId = -1;
    timeToPeak = 0;
    currentPeakInfected = 0;
    updateUI();
}

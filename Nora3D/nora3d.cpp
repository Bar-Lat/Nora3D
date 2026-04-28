#include "nora3d.h"

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

    // Konfiguracja Widoku Planszy
    boardCanvas = new BoardCanvas(this);
    ui->boardWidget->layout()->addWidget(boardCanvas);

    // Konfiguracja kontrolek 
    ui->boardSize->setRange(5, 800);
    ui->boardSize->setValue(20);

    ui->gameTime->setRange(1, 40);
    ui->gameTime->setValue(2);

    ui->timeInfection->setRange(1, 20);
    ui->timeInfection->setValue(6);

    ui->timeImmune->setRange(1, 20);
    ui->timeImmune->setValue(4);

    ui->chanceInfection->setRange(0, 100);
    ui->chanceInfection->setValue(50);

    ui->scaleSpinBox->setRange(1, 20);
    ui->scaleSpinBox->setValue(10); 

    // polaczenia sygnal slot
    connect(ui->startStopButton, &QPushButton::clicked, this, &nora3d::onStartStopClicked);
    connect(ui->resetButton, &QPushButton::clicked, this, &nora3d::onResetClicked);
    connect(ui->saveButton, &QPushButton::clicked, this, &nora3d::onSaveClicked);

    connect(ui->gameTime, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);
    connect(ui->timeInfection, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);
    connect(ui->timeImmune, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);
    connect(ui->chanceInfection, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);

    connect(ui->scaleSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &nora3d::onScaleChanged);
    connect(ui->boardSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &nora3d::onSizeChange);

    connect(timer, &QTimer::timeout, this, &nora3d::onTick);
    connect(boardCanvas, &BoardCanvas::cellClicked, this, &nora3d::handleCellClick);

	// etykiety suwaków
    double cps = (double)ui->gameTime->value();
    ui->cyclesPerSecond->setText(QString::number(cps, 'f', 1) + " cykli/s");
    int interval = (ui->gameTime->value() > 0) ? (1000 / ui->gameTime->value()) : 1000;
    timer->setInterval(interval);

    ui->InfectionSeconds->setText(QString::number(ui->timeInfection->value()) + " cykli");
    ui->immuneSeconds->setText(QString::number(ui->timeImmune->value()) + " cykli");
    ui->infectionProbability->setText(QString::number(ui->chanceInfection->value()) + " %");

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

void nora3d::applySettings()
{
	// przekazanie wartości do modelu symulacji 
    simulation->setParams(
        ui->timeInfection->value(),
        ui->timeImmune->value(),
        ui->chanceInfection->value()
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
    int healthyCount, infectedCount, immuneCount;
    std::tie(healthyCount, infectedCount, immuneCount) = simulation->getCellCounts();

	// koniec gdy nie ma zarażonych ani odpornych
    if (infectedCount == 0 && immuneCount == 0) {
        if (timer->isActive()) {
			// Zbieranie danych końcowych
            int timeCycles = simulation->time();
            double timeSeconds = (timeCycles * timer->interval()) / 1000.0;
            int maxInfected = simulation->getMaxInfectedCount();
            int size = simulation->size();
            int infDuration = simulation->getInfectionDuration();
            int immDuration = simulation->getImmunityDuration();
            int infChance = simulation->getInfectionChance();

            // Zapis do historii
            simulationHistory.append({
                timeCycles, timeSeconds, maxInfected,
                size, infDuration, immDuration, infChance
                });

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
    int healthyCount, infectedCount, immuneCount;
    std::tie(healthyCount, infectedCount, immuneCount) = simulation->getCellCounts();
    ui->HealthyCellsLabel->setText(QString("%1").arg(healthyCount));
    ui->InfectedCellsLabel->setText(QString("%1").arg(infectedCount));
    ui->ImmuneCellsLabel->setText(QString("%1").arg(immuneCount));

    // update historii symulacji
    QString lastTimesText = "Ostatnie symulacje:\n";
    int maxEntries = 10;

    for (int i = simulationHistory.size() - 1; i >= 0 && (simulationHistory.size() - 1 - i) < maxEntries; --i) {
        const auto& entry = simulationHistory.at(i);
        lastTimesText += QString("  - %1 cykli (%2 s)\n")
            .arg(std::get<0>(entry))
            .arg(std::get<1>(entry), 0, 'f', 2);
    }
    ui->LastTimesLabel->setText(lastTimesText);

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
        int interval = 1000 / value;
        timer->setInterval(interval);
    }
    else {
		// aktualizacja etykiet suwaków
        if (senderSlider == ui->timeImmune) { ui->immuneSeconds->setText(QString::number(value) + " cykli"); }
        else if (senderSlider == ui->timeInfection) { ui->InfectionSeconds->setText(QString::number(value) + " cykli"); }
        else if (senderSlider == ui->chanceInfection) { ui->infectionProbability->setText(QString::number(value) + " %"); }
        applySettings();
    }
    updateUI();
}

void nora3d::handleCellClick(int row, int col)
{
	// handler klikniecia komórki
        CellState currentState = simulation->getCellState(row, col);

        if (currentState == CellState::Healthy) {
            // zdrowa -> zarażona
            simulation->setCellState(row, col, CellState::Infected);
        }
        else{
            // zarażona / odporna -> zdrowa
            simulation->setCellState(row, col, CellState::Healthy);
        }
        updateUI();
    }

// skalowanie widoku planszy
void nora3d::onScaleChanged(int newScale)
{
    boardCanvas->setScaleFactor(newScale);
    updateUI();
}

// ====================================================================
// ZAPIS 
// ====================================================================

void nora3d::onSaveClicked()
{
    // okno dialogowe zapisu
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Zapisz Historię Symulacji"),
        "histora_symulacji.txt",
        tr("Plik tekstowy (*.txt)"));

    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);

		// stałe szerokości kolumn
        const int W1_NR = 4;
        const int W2_SIZE = 12;
        const int W3_TINF = 12;
        const int W4_TIMM = 12;
        const int W5_CHANCE = 14;
        const int W6_CYCLES = 10;
        const int W7_TIME = 10;
        const int W8_MAXINF = 14;

        // nagłówek i separator
        out << QString("%1 | %2 | %3 | %4 | %5 | %6 | %7 | %8\n")
            .arg("Nr.", W1_NR)
            .arg("Rozmiar", W2_SIZE)
            .arg("Czas zaraz.", W3_TINF)
            .arg("Czas odporn.", W4_TIMM)
            .arg("Szansa zaraz.", W5_CHANCE)
            .arg("Cykle", W6_CYCLES)
            .arg("Czas (s)", W7_TIME)
            .arg("Max Zarazonych", W8_MAXINF);

        out << QString("").fill('-', W1_NR) << " | "
            << QString("").fill('-', W2_SIZE) << " | "
            << QString("").fill('-', W3_TINF) << " | "
            << QString("").fill('-', W4_TIMM) << " | "
            << QString("").fill('-', W5_CHANCE) << " | "
            << QString("").fill('-', W6_CYCLES) << " | "
            << QString("").fill('-', W7_TIME) << " | "
            << QString("").fill('-', W8_MAXINF) << "\n";

		// dane symulacji
        for (int i = 0; i < simulationHistory.size(); ++i) {
            const auto& entry = simulationHistory.at(i);

            out << QString("%1 | %2 | %3 | %4 | %5 | %6 | %7 | %8\n")
                // Nr.
                .arg(i + 1, W1_NR)
                // Rozmiar
                .arg(QString("%1x%1").arg(std::get<3>(entry)), W2_SIZE)
                // Czas zarazenia
                .arg(std::get<4>(entry), W3_TINF)
                // Czas odpornosci
                .arg(std::get<5>(entry), W4_TIMM)
                // Szansa na zarazenie
                .arg(QString("%1%").arg(std::get<6>(entry)), W5_CHANCE)
                // Cykle
                .arg(std::get<0>(entry), W6_CYCLES)
                // Czas (s)
                .arg(QString::number(std::get<1>(entry), 'f', 2), W7_TIME)
                // Max zarazonych
                .arg(std::get<2>(entry), W8_MAXINF);
        }

        file.close();
    }
    else {
        QMessageBox::critical(this, tr("Błąd Zapisu"), tr("Nie można otworzyć pliku do zapisu."));
    }
}
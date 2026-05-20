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

    ui->spacingSlider->setRange(10, 100);
    ui->spacingSlider->setValue(10);

    ui->deathSlider->setRange(1, 100);
    ui->deathSlider->setValue(10);


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

    connect(ui->deathSlider, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);
    connect(ui->spacingSlider, &QSlider::valueChanged, this, &nora3d::updateSliderLabels);
    connect(ui->spacingSlider, &QSlider::valueChanged,
        [this](int value)
        {
            boardCanvas->setSpacing(value);
        });

	// etykiety suwaków
    double cps = (double)ui->gameTime->value();
    ui->cyclesPerSecond->setText(QString::number(cps, 'f', 1) + " cykli/s");
    int interval = (ui->gameTime->value() > 0) ? (1000 / ui->gameTime->value()) : 1000;
    timer->setInterval(interval);

    ui->InfectionSeconds->setText(QString::number(ui->timeInfection->value()) + " cykli");
    ui->immuneSeconds->setText(QString::number(ui->timeImmune->value()) + " cykli");
    ui->infectionProbability->setText(QString::number(ui->chanceInfection->value()) + " %");
    ui->deathLabel->setText(QString::number(ui->deathSlider->value()));

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
        ui->chanceInfection->value(),
        ui->deathSlider->value()
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
    int healthyCount, infectedCount, immuneCount, deadCount;
    std::tie(healthyCount, infectedCount, immuneCount, deadCount) = simulation->getCellCounts();

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

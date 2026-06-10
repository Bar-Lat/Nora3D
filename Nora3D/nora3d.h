#pragma once
#include <QMainWindow>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QVector>
#include <QVBoxLayout>
#include <QTextStream>

#include <tuple>
#include <memory>

#include "SimulationCore.h"
#include "BoardCanvas.h"
#include "ui_nora3d.h"
#include "DatabaseManager.h"

class nora3d : public QMainWindow {
    Q_OBJECT

public:
    explicit nora3d(QWidget* parent = nullptr);
    ~nora3d();

private:
    // komponenty i stan
    Ui::Nora3DClass* ui;
    std::unique_ptr<SimulationCore> simulation;
    BoardCanvas* boardCanvas;
    QTimer* timer;

    // --- NOWE ZMIENNE BAZY DANYCH I ANALITYKI ---
    DatabaseManager dbManager;
    int currentSimulationId = -1;
    int timeToPeak = 0;
    int currentPeakInfected = 0;

private slots:
	// sloty (reakcje na zdarzenia)
    void onStartStopClicked();
    void onResetClicked();
    void onSizeChange();
    void onTick();
    void updateSliderLabels(int value);
    void handleCellClick(int x, int y, int z);
    void onShowAnalyticsClicked();
    void loadSimulationConfiguration(const SimulationConfiguration& configuration);

    void onRandomDeathToggled(bool checked);

private:
	// metody pomocnicze
    void updateUI();
    void applySettings();
};

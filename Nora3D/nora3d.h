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


class nora3d : public QMainWindow {
    Q_OBJECT

public:
    explicit nora3d(QWidget* parent = nullptr);
    ~nora3d();

private:
	// komponenty i stan
    Ui::Nora3DClass *ui;
    std::unique_ptr<SimulationCore> simulation;
    BoardCanvas* boardCanvas;
    QTimer* timer;

	// historia wyników symulacji
    QVector<std::tuple<int, double, int, int, int, int, int>> simulationHistory;

private slots:
	// sloty (reakcje na zdarzenia)
    void onStartStopClicked();
    void onResetClicked();
    void onSizeChange();
    void onTick();
    void onSaveClicked();
    void updateSliderLabels(int value);
    void handleCellClick(int row, int col);
    void onScaleChanged(int newScale);

private:
	// metody pomocnicze
    void updateUI();
    void applySettings();
};
#pragma once
#include <QDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QSqlQuery>
#include "BoardCanvas.h"
#include "DatabaseManager.h"
#include "qcustomplot.h" // Import naszej nowej biblioteki

class AnalyticsWindow : public QDialog {
    Q_OBJECT

public:
    explicit AnalyticsWindow(DatabaseManager* databaseManager, QWidget* parent = nullptr);

signals:
    void configurationLoadRequested(const SimulationConfiguration& configuration);

private slots:
    void onSimulationSelected(int index);
    void onDeleteSimulationClicked();
    void onLoadConfigurationClicked();

private:
    void loadSimulationsList();
    void drawChart(int simulationId);
    void loadFinalGridPreview(int simulationId);
    int selectedSimulationId() const;
    void updateButtons();

    QComboBox* simComboBox;
    QPushButton* loadConfigurationButton;
    QPushButton* deleteSimulationButton;
    QTabWidget* tabs;
    QCustomPlot* customPlot;
    BoardCanvas* finalGridCanvas;
    QLabel* finalGridStatusLabel;
    DatabaseManager* dbManager;
};

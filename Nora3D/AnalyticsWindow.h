#pragma once
#include <QDialog>
#include <QComboBox>
#include <QVBoxLayout>
#include <QSqlQuery>
#include "qcustomplot.h" // Import naszej nowej biblioteki

class AnalyticsWindow : public QDialog {
    Q_OBJECT

public:
    explicit AnalyticsWindow(QWidget* parent = nullptr);

private slots:
    void onSimulationSelected(int index);

private:
    void loadSimulationsList();
    void drawChart(int simulationId);

    QComboBox* simComboBox;
    QCustomPlot* customPlot;
};
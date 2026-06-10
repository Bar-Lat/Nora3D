#include "AnalyticsWindow.h"
#include <QDateTime>

AnalyticsWindow::AnalyticsWindow(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Analiza Symulacji - QCustomPlot");
    resize(800, 600);

    QVBoxLayout* layout = new QVBoxLayout(this);
    simComboBox = new QComboBox(this);
    layout->addWidget(simComboBox);

    customPlot = new QCustomPlot(this);
    layout->addWidget(customPlot);

    connect(simComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AnalyticsWindow::onSimulationSelected);

    loadSimulationsList();
}

void AnalyticsWindow::loadSimulationsList() {
    QSqlQuery query("SELECT id, timestamp, total_cycles FROM Simulations ORDER BY id DESC");
    while (query.next()) {
        int id = query.value(0).toInt();
        QString label = QString("Symulacja #%1 - %2").arg(id).arg(query.value(1).toString());
        simComboBox->addItem(label, id);
    }
}

void AnalyticsWindow::drawChart(int simulationId) {
    customPlot->clearGraphs();

    // Tworzymy 3 serie danych
    customPlot->addGraph(); // Zdrowi
    customPlot->graph(0)->setPen(QPen(Qt::green));
    customPlot->addGraph(); // Zarażeni
    customPlot->graph(1)->setPen(QPen(Qt::red));
    customPlot->addGraph(); // Odporni
    customPlot->graph(2)->setPen(QPen(Qt::yellow));

    QVector<double> cycles, healthy, infected, immune;

    QSqlQuery query;
    query.prepare("SELECT cycle_number, count_healthy, count_infected, count_immune "
        "FROM SimulationLogs WHERE simulation_id = ? ORDER BY cycle_number ASC");
    query.addBindValue(simulationId);

    if (query.exec()) {
        while (query.next()) {
            cycles.append(query.value(0).toDouble());
            healthy.append(query.value(1).toDouble());
            infected.append(query.value(2).toDouble());
            immune.append(query.value(3).toDouble());
        }
    }

    customPlot->graph(0)->setData(cycles, healthy);
    customPlot->graph(1)->setData(cycles, infected);
    customPlot->graph(2)->setData(cycles, immune);

    customPlot->xAxis->setLabel("Cykl");
    customPlot->yAxis->setLabel("Liczba komórek");

    customPlot->rescaleAxes();
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom); // Możliwość zoomowania myszką!
    customPlot->replot();
}

void AnalyticsWindow::onSimulationSelected(int index) {
    drawChart(simComboBox->itemData(index).toInt());
}
#pragma once
#include "SimulationTypes.h"
#include <QWidget>
#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

class BoardCanvas : public QWidget
{
    Q_OBJECT

public:
	// konstruktor zabraniajacy niejawna konwersje typu
    explicit BoardCanvas(QWidget* parent = nullptr);

    // gettery i settery
    void setGrid(const Grid& newGrid);
    void setScaleFactor(int factor);
    int scaleFactor() const { return m_scaleFactor; }


signals:
    void cellClicked(int row, int col);

protected:
	// obsługa malowania i kliniecia
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    Grid grid;
    int m_scaleFactor = 1;

    // Pomocnicze metody
    QColor getColor(CellState state) const;
    QPoint cellIndexFromPos(const QPoint& pos) const;
};
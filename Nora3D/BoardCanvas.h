#pragma once
#include "SimulationTypes.h"
#include "SimulationCore.h"
#include <QWidget>
#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <QWheelEvent>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

class BoardCanvas : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
	// konstruktor zabraniajacy niejawna konwersje typu
    explicit BoardCanvas(QWidget* parent = nullptr);

    // gettery i settery
    void setGrid(const Grid3D& newGrid);
    void setScaleFactor(int factor);
    int scaleFactor() const { return m_scaleFactor; }


signals:
    void cellClicked(int x, int y, int z);

protected:
	// obsługa malowania i kliniecia
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    Grid3D grid;
    int m_scaleFactor = 1;
    int gridSize;

    // Pomocnicze metody
    QColor getColor(CellState state) const;
    QPoint cellIndexFromPos(const QPoint& pos) const;

    QOpenGLShaderProgram* program;
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    // Macierze widoku przesyłane do shaderów
    QMatrix4x4 projection;

};
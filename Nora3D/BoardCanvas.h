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
    void setSpacing(float spacing);


signals:
    void cellClicked(int x, int y, int z);

protected:
	// obsługa malowania i kliniecia
    void mousePressEvent(QMouseEvent* event) override;
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    Grid3D grid;
    int m_scaleFactor = 1;
    int gridSize = 0;

    float m_zoom = 2.5f;
    float m_rotationX = 0.0f;
    float m_rotationY = 0.0f;
    float m_spacing = 1.0f;
    QPoint m_lastMousePos;

    // Pomocnicze metody
    QColor getColor(CellState state) const;
    QPoint cellIndexFromPos(const QPoint& pos) const;

    void wheelEvent(QWheelEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

    QOpenGLShaderProgram* program;
    QOpenGLVertexArrayObject vao;
    QOpenGLVertexArrayObject vaoLines;
    QOpenGLBuffer vboLines;
    QOpenGLBuffer vbo;
    // Macierze widoku przesyłane do shaderów
    QMatrix4x4 projection;
    QMatrix4x4 getViewMatrix() const;

};
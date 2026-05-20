#ifndef BOARDCANVAS_H
#define BOARDCANVAS_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QColor>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFuture>
#include <QFutureWatcher>
#include <QMutex>
#include <vector>
#include <limits>

#include "SimulationTypes.h"  // <-- ZMIANA: użyj istniejącego pliku

// Struktura dla instanced rendering
struct InstanceData {
    QVector3D offset;
    QVector3D color;
    float padding[2]; // Wyrównanie do 32 bajtów

    InstanceData() : offset(), color(), padding{ 0.0f, 0.0f } {}
};

class BoardCanvas : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit BoardCanvas(QWidget* parent = nullptr);
    ~BoardCanvas() override;

    void setGrid(const Grid3D& newGrid);
    void setScaleFactor(int factor);
    void setSpacing(float spacing);

signals:
    void cellClicked(int x, int y, int z);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    // Metody pomocnicze
    QMatrix4x4 getViewMatrix() const;
    QColor getColor(CellState state) const;
    void updateInstanceBuffer();
    void prepareInstanceDataAsync();
    bool isInFrustum(const QVector3D& center, const QMatrix4x4& mvp) const;

    // Dane siatki
    Grid3D grid;
    int gridSize;
    int m_scaleFactor;
    float m_spacing;

    // Kamera
    float m_zoom;
    float m_rotationX;
    float m_rotationY;
    QPoint m_lastMousePos;
    QMatrix4x4 projection;

    // OpenGL obiekty
    QOpenGLShaderProgram* program;

    // VAO/VBO dla geometrii bazowej (jeden sześcian)
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    QOpenGLBuffer vboLines;

    // VAO/VBO dla instanced rendering
    QOpenGLVertexArrayObject vaoInstanced;
    QOpenGLBuffer vboInstanced;

    // VAO/VBO filled i lines
    QOpenGLVertexArrayObject vaoFilled; 
    QOpenGLVertexArrayObject vaoLines;

    // Dane instancji
    std::vector<InstanceData> instanceData;
    std::vector<InstanceData> visibleInstances;
    bool instanceBufferDirty;

    // Wielowątkowość
    QFuture<std::vector<InstanceData>> prepareFuture;
    QFutureWatcher<std::vector<InstanceData>> prepareWatcher;
    bool isPreparingData;
};

#endif // BOARDCANVAS_H
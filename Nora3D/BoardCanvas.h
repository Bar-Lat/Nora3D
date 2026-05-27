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

#include "SimulationTypes.h"  

struct InstanceData {
    QVector3D offset;
    QVector3D color;
    float padding[2];

    InstanceData() : offset(), color(), padding{ 0.0f, 0.0f } {}
};

class BoardCanvas : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    explicit BoardCanvas(QWidget* parent = nullptr);
    ~BoardCanvas();

    void setGrid(const Grid3D& newGrid);
    void setSpacing(float spacing);
    void setScaleFactor(int factor);

signals:
    void cellClicked(int x, int y, int z);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;


    

private:
    // Metody pomocnicze
    QMatrix4x4 getViewMatrix() const;
    QColor getColor(CellState state) const;
    void updateInstanceBuffer();
    void prepareInstanceDataAsync();
    bool isInFrustum(const QVector3D& center, const QMatrix4x4& mvp) const;

    // Dane symulacji
    Grid3D grid;
    int gridSize;
    std::vector<InstanceData> instanceData;
    std::vector<InstanceData> visibleInstances;

    // OpenGL
    QOpenGLShaderProgram* program;
    QOpenGLVertexArrayObject vao, vaoFilled, vaoLines, vaoInstanced;
    QOpenGLBuffer vbo, vboLines, vboInstanced;
    QMatrix4x4 projection;

    // Wielowątkowość
    QFuture<std::vector<InstanceData>> prepareFuture;
    QFutureWatcher<std::vector<InstanceData>> prepareWatcher;
    bool isPreparingData;
    bool instanceBufferDirty;

    // Sterowanie widokiem
    float m_spacing;
    float m_zoom;
    float m_rotationX;
    float m_rotationY;
    int m_scaleFactor;
    QPoint m_lastMousePos;
};

#endif // BOARDCANVAS_H
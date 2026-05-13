#include "BoardCanvas.h"
#include <QMouseEvent>
#include <QVector3D>
#include <cmath>
#include <algorithm> // Dla std::min/max

// ====================================================================
// KONSTRUKTOR I SHADERY
// ====================================================================

const char* vertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    uniform vec3 aOffset; 
    uniform vec3 aColor;  
    uniform mat4 projection;
    uniform mat4 view;
    out vec3 fragColor;
    void main() {
        fragColor = aColor;
        // Pozycja wierzchołka + offset kostki, przemnożone przez macierze
        gl_Position = projection * view * vec4(aPos + aOffset, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    in vec3 fragColor;
    out vec4 FragColor;
    uniform bool u_isOutline; // Flaga: czy rysujemy obrys
    void main() {
        if(u_isOutline) {
            FragColor = vec4(0.0, 0.0, 0.0, 1.0); // Czarny obrys
        } else {
            FragColor = vec4(fragColor, 1.0);     // Kolor komórki
        }
    }
)";

BoardCanvas::BoardCanvas(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_scaleFactor(10)
    , m_zoom(5.0f)
    , m_rotationX(-25.0f) // Lekki obrót początkowy, żeby widzieć 3D
    , m_rotationY(45.0f)
{
    setMinimumSize(400, 400);
    // Wymagane, aby mouseMoveEvent działał bez wciśniętego przycisku (opcjonalne)
    setMouseTracking(true);
}

// nowa plansza
void BoardCanvas::setGrid(const Grid3D& newGrid) {
    grid = newGrid;
    if (!grid.empty()) {
        // N = pierwiastek sześcienny z rozmiaru wektora
        gridSize = static_cast<int>(std::round(std::pow(grid.size(), 1.0 / 3.0)));
    }
    update();
}

void BoardCanvas::setScaleFactor(int factor) {
    m_scaleFactor = std::clamp(factor, 1, 100);
    update();
}

// ====================================================================
// POMOCNICZE
// ====================================================================

// Odstęp między środkami kostek
static const float SPACING = 1.5f;

QMatrix4x4 BoardCanvas::getViewMatrix() const {
    QMatrix4x4 view;
    float center = (gridSize - 1) * SPACING / 2.0f;
    float distance = gridSize * m_zoom;

    // Kamera patrzy na środek
    view.lookAt(QVector3D(center, center, distance),
        QVector3D(center, center, 0),
        QVector3D(0, 1, 0));

    // Obrót wokół środka
    view.translate(center, center, 0);
    view.rotate(m_rotationX, 1, 0, 0);
    view.rotate(m_rotationY, 0, 1, 0);
    view.translate(-center, -center, 0);

    return view;
}

// ====================================================================
// OBSŁUGA MYSZY I ZAZNACZANIA
// ====================================================================

void BoardCanvas::wheelEvent(QWheelEvent* event) {
    float delta = event->angleDelta().y() > 0 ? -0.5f : 0.5f;
    m_zoom = std::clamp(m_zoom + 0.5f * delta, 0.5f, 50.0f);
    update();
}

void BoardCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        QPoint diff = event->pos() - m_lastMousePos;
        m_rotationX += diff.y() * 0.5f;
        m_rotationY += diff.x() * 0.5f;
        update();
    }
    m_lastMousePos = event->pos();
}

void BoardCanvas::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->pos();

    if (event->button() == Qt::LeftButton) {
        if (grid.empty() || gridSize <= 0) {
            QWidget::mousePressEvent(event);
            return;
        }

        // --- RAYCASTING ---
        QMatrix4x4 view = getViewMatrix();
        QRect viewport(0, 0, width(), height());

        // Konwersja współrzędnych Qt (y-down) na OpenGL (y-up)
        float winX = event->pos().x();
        float winY = height() - event->pos().y();

        // Obliczamy punkt bliski i daleki w przestrzeni świata
        QVector3D nearPoint = QVector3D(winX, winY, 0.0f).unproject(view, projection, viewport);
        QVector3D farPoint = QVector3D(winX, winY, 1.0f).unproject(view, projection, viewport);

        QVector3D rayOrigin = nearPoint;
        QVector3D rayDir = (farPoint - nearPoint).normalized();

        float minDistance = std::numeric_limits<float>::max();
        int clickedX = -1, clickedY = -1, clickedZ = -1;

        // Promień sześcianu (połowa boku). W vertices[] mamy od -0.5 do 0.5, więc bok = 1.0.
        const float r = 0.5f;

        // Sprawdzenie przecięcia z każdym sześcianem (AABB)
        for (int x = 0; x < gridSize; ++x) {
            for (int y = 0; y < gridSize; ++y) {
                for (int z = 0; z < gridSize; ++z) {

                    QVector3D center(x * SPACING, y * SPACING, z * SPACING);
                    QVector3D bMin = center - QVector3D(r, r, r);
                    QVector3D bMax = center + QVector3D(r, r, r);

                    // --- Poprawiony algorytm Slab dla AABB ---
                    float tmin = -std::numeric_limits<float>::max();
                    float tmax = std::numeric_limits<float>::max();

                    // Oś X
                    if (std::abs(rayDir.x()) > 1e-6f) {
                        float tx1 = (bMin.x() - rayOrigin.x()) / rayDir.x();
                        float tx2 = (bMax.x() - rayOrigin.x()) / rayDir.x();
                        tmin = std::max(tmin, std::min(tx1, tx2));
                        tmax = std::min(tmax, std::max(tx1, tx2));
                    }
                    else {
                        // Promień równoległy do osi, sprawdź czy origin jest wewnątrz
                        if (rayOrigin.x() < bMin.x() || rayOrigin.x() > bMax.x()) tmax = -1;
                    }

                    // Oś Y
                    if (tmax >= 0 && std::abs(rayDir.y()) > 1e-6f) {
                        float ty1 = (bMin.y() - rayOrigin.y()) / rayDir.y();
                        float ty2 = (bMax.y() - rayOrigin.y()) / rayDir.y();
                        tmin = std::max(tmin, std::min(ty1, ty2));
                        tmax = std::min(tmax, std::max(ty1, ty2));
                    }
                    else if (std::abs(rayDir.y()) <= 1e-6f) {
                        if (rayOrigin.y() < bMin.y() || rayOrigin.y() > bMax.y()) tmax = -1;
                    }

                    // Oś Z
                    if (tmax >= 0 && std::abs(rayDir.z()) > 1e-6f) {
                        float tz1 = (bMin.z() - rayOrigin.z()) / rayDir.z();
                        float tz2 = (bMax.z() - rayOrigin.z()) / rayDir.z();
                        tmin = std::max(tmin, std::min(tz1, tz2));
                        tmax = std::min(tmax, std::max(tz1, tz2));
                    }
                    else if (std::abs(rayDir.z()) <= 1e-6f) {
                        if (rayOrigin.z() < bMin.z() || rayOrigin.z() > bMax.z()) tmax = -1;
                    }

                    // Trafienie, jeśli tmax >= tmin i tmax jest dodatnie (przed kamerą)
                    if (tmax >= tmin && tmax >= 0.0f) {
                        // Szukamy najbliższego sześcianu
                        if (tmin < minDistance) {
                            minDistance = tmin;
                            clickedX = x; clickedY = y; clickedZ = z;
                        }
                    }
                }
            }
        }

        if (clickedX != -1) {
            emit cellClicked(clickedX, clickedY, clickedZ);
        }
    }
    QWidget::mousePressEvent(event);
}

// ====================================================================
// RYSOWANIE 
// ====================================================================

void BoardCanvas::initializeGL() {
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    program = new QOpenGLShaderProgram();
    program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    program->link();

    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    // ==========================================================
    // 1. BUFOR DLA WYPEŁNIENIA (TRÓJKĄTY)
    // ==========================================================
    float solidVertices[] = {
        // Tył (z = -0.5)
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
         // Prawo (x = 0.5)
          0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
          0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
          // Przód (z = 0.5)
           0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
          -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,
          // Lewo (x = -0.5)
          -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
          -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
          // Dół (y = -0.5)
          -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,
           0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f,
           // Góra (y = 0.5)
           -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
            0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f
    };

    vao.create();
    vao.bind();

    vbo.create();
    vbo.bind();
    vbo.allocate(solidVertices, sizeof(solidVertices));

    program->enableAttributeArray(0);
    program->setAttributeBuffer(0, GL_FLOAT, 0, 3);

    vao.release();
    vbo.release();

    // ==========================================================
    // 2. BUFOR DLA KRAWĘDZI (LINIE BEZ PRZEKĄTNYCH)
    // ==========================================================
    float lineVertices[] = {
        // Przednia ściana
        -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,
        // Tylna ściana
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,
        // Krawędzie łączące przód z tyłem
        -0.5f, -0.5f,  0.5f,  -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,   0.5f, -0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f
    };

    vaoLines.create();
    vaoLines.bind();

    vboLines.create();
    vboLines.bind();
    vboLines.allocate(lineVertices, sizeof(lineVertices));

    program->enableAttributeArray(0);
    program->setAttributeBuffer(0, GL_FLOAT, 0, 3);

    vaoLines.release();
    vboLines.release();
}

void BoardCanvas::resizeGL(int w, int h) {
    if (h == 0) h = 1;
    projection.setToIdentity();
    // Perspektywa: Field of View 45st, blisko 0.1, daleko 1000
    projection.perspective(45.0f, GLfloat(w) / h, 0.1f, 1000.0f);
}

void BoardCanvas::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (grid.empty() || gridSize <= 0) return;

    program->bind();

    QMatrix4x4 view = getViewMatrix();
    program->setUniformValue("projection", projection);
    program->setUniformValue("view", view);

    // ==========================================================
    // PAS 1: Rysowanie wypełnienia (Trójkąty)
    // ==========================================================
    vao.bind();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Wymuszamy tryb pełnych wielokątów!
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    program->setUniformValue("u_isOutline", false);

    for (int x = 0; x < gridSize; ++x) {
        for (int y = 0; y < gridSize; ++y) {
            for (int z = 0; z < gridSize; ++z) {
                CellState state = grid[getIndex(x, y, z, gridSize)];
                program->setUniformValue("aOffset", QVector3D(x * SPACING, y * SPACING, z * SPACING));
                program->setUniformValue("aColor", QVector3D(getColor(state).redF(), getColor(state).greenF(), getColor(state).blueF()));
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    vao.release();

    // ==========================================================
    // PAS 2: Rysowanie krawędzi (Linie)
    // ==========================================================
    vaoLines.bind();
    program->setUniformValue("u_isOutline", true);
    glLineWidth(2.0f);

    for (int x = 0; x < gridSize; ++x) {
        for (int y = 0; y < gridSize; ++y) {
            for (int z = 0; z < gridSize; ++z) {
                program->setUniformValue("aOffset", QVector3D(x * SPACING, y * SPACING, z * SPACING));
                // UWAGA: Rysujemy używając GL_LINES zdefiniowanych w buforze vboLines
                glDrawArrays(GL_LINES, 0, 24);
            }
        }
    }
    vaoLines.release();

    program->release();
}

// ====================================================================
// KOLOROWANIE
// ====================================================================

QColor BoardCanvas::getColor(CellState state) const
{
    switch (state) {
    case CellState::Infected: return Qt::red;
    case CellState::Immune:   return Qt::yellow;
    case CellState::Healthy:  return Qt::green;
    default:                  return Qt::black;
    }
}
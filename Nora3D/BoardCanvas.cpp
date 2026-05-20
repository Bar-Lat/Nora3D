#include "BoardCanvas.h"
#include <QMouseEvent>
#include <QVector3D>
#include <QtConcurrent>
#include <cmath>
#include <algorithm>

// ====================================================================
// SHADERY Z INSTANCED RENDERING
// ====================================================================

const char* vertexShaderSourceInstanced = R"(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec3 aInstanceOffset;
    layout(location = 2) in vec3 aInstanceColor;
    
    uniform mat4 projection;
    uniform mat4 view;
    
    out vec3 fragColor;
    
    void main() {
        fragColor = aInstanceColor;
        gl_Position = projection * view * vec4(aPos + aInstanceOffset, 1.0);
    }
)";

const char* fragmentShaderSourceInstanced = R"(
    #version 330 core
    in vec3 fragColor;
    out vec4 FragColor;
    uniform bool u_isOutline;
    
    void main() {
        if(u_isOutline) {
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        } else {
            FragColor = vec4(fragColor, 1.0);
        }
    }
)";

// ====================================================================
// KONSTRUKTOR I DESTRUKTOR
// ====================================================================

BoardCanvas::BoardCanvas(QWidget* parent)
    : QOpenGLWidget(parent)
    , gridSize(0)
    , m_scaleFactor(10)
    , m_spacing(1.2f)
    , m_zoom(5.0f)
    , m_rotationX(-25.0f)
    , m_rotationY(45.0f)
    , program(nullptr)
    , vbo(QOpenGLBuffer::VertexBuffer)
    , vboLines(QOpenGLBuffer::VertexBuffer)
    , vboInstanced(QOpenGLBuffer::VertexBuffer)
    , instanceBufferDirty(true)
    , isPreparingData(false)
{
    setMinimumSize(400, 400);
    setMouseTracking(true);

    connect(&prepareWatcher, &QFutureWatcher<std::vector<InstanceData>>::finished,
        this, &BoardCanvas::updateInstanceBuffer);
}

BoardCanvas::~BoardCanvas() {
    makeCurrent();
    if (prepareFuture.isRunning()) {
        prepareFuture.waitForFinished();
    }
    vao.destroy();
    vbo.destroy();
    vboLines.destroy();
    vaoInstanced.destroy();
    vboInstanced.destroy();
    delete program;
    doneCurrent();
}

// ====================================================================
// SETTERY
// ====================================================================

void BoardCanvas::setGrid(const Grid3D& newGrid) {
    grid = newGrid;
    if (!grid.empty()) {
        gridSize = static_cast<int>(std::round(std::pow(grid.size(), 1.0 / 3.0)));
    }
    instanceBufferDirty = true;
    prepareInstanceDataAsync();
}

void BoardCanvas::setScaleFactor(int factor) {
    m_scaleFactor = std::clamp(factor, 1, 100);
    update();
}

void BoardCanvas::setSpacing(float spacing) {
    m_spacing = spacing/10.0f;
    instanceBufferDirty = true; 
    prepareInstanceDataAsync(); 
}

// ====================================================================
// FRUSTUM CULLING
// ====================================================================

bool BoardCanvas::isInFrustum(const QVector3D& center, const QMatrix4x4& mvp) const {
    QVector4D clipPos = mvp * QVector4D(center, 1.0f);

    if (std::abs(clipPos.w()) < 0.0001f) return false;

    float x = clipPos.x() / clipPos.w();
    float y = clipPos.y() / clipPos.w();
    float z = clipPos.z() / clipPos.w();

    float margin = 1.5f / clipPos.w();

    return (x >= -1.0f - margin && x <= 1.0f + margin &&
        y >= -1.0f - margin && y <= 1.0f + margin &&
        z >= -1.0f - margin && z <= 1.0f + margin);
}

// ====================================================================
// WIELOWĄTKOWE PRZYGOTOWANIE DANYCH
// ====================================================================

void BoardCanvas::prepareInstanceDataAsync() {
    if (isPreparingData || grid.empty()) return;

    isPreparingData = true;

    Grid3D gridCopy = grid;
    int size = gridSize;
    float spacing = m_spacing;

    prepareFuture = QtConcurrent::run([gridCopy, size, spacing]() -> std::vector<InstanceData> {
        std::vector<InstanceData> data;
        data.reserve(size * size * size);

        for (int z = 0; z < size; ++z) {
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    CellState state = gridCopy[getIndex(x, y, z, size)];

                    // FILTRACJA: Jeśli stan to Dead, nie dodajemy do wektora
                    if (state == CellState::Dead) {
                        continue;
                    }

                    InstanceData inst;
                    inst.offset = QVector3D(x * spacing, y * spacing, z * spacing);

                    QColor color;
                    switch (state) {
                    case CellState::Infected: color = Qt::red; break;
                    case CellState::Immune:   color = Qt::yellow; break;
                    case CellState::Healthy:  color = Qt::green; break;
                    default:                  color = Qt::black; break;
                    }

                    inst.color = QVector3D(color.redF(), color.greenF(), color.blueF());
                    inst.padding[0] = inst.padding[1] = 0.0f;

                    data.push_back(inst);
                }
            }
        }
        return data;
        });

    prepareWatcher.setFuture(prepareFuture);
}

void BoardCanvas::updateInstanceBuffer() {
    if (!prepareFuture.isFinished()) return;

    instanceData = prepareFuture.result(); // To już jest wektor bez "martwych"
    isPreparingData = false;
    instanceBufferDirty = false;

    makeCurrent();

    if (vboInstanced.isCreated()) {
        vboInstanced.bind();
        // Teraz instanceData zawiera tylko żywe komórki
        vboInstanced.allocate(instanceData.data(),
            static_cast<int>(instanceData.size() * sizeof(InstanceData)));
        vboInstanced.release();
    }

    doneCurrent();
    update();
}

// ====================================================================
// POMOCNICZE
// ====================================================================

QMatrix4x4 BoardCanvas::getViewMatrix() const {
    QMatrix4x4 view;
    float center = (gridSize - 1) * m_spacing / 2.0f;
    float distance = gridSize * m_zoom;

    view.lookAt(QVector3D(center, center, distance),
        QVector3D(center, center, 0),
        QVector3D(0, 1, 0));

    view.translate(center, center, center);
    view.rotate(m_rotationX, 1, 0, 0);
    view.rotate(m_rotationY, 0, 1, 0);
    view.translate(-center, -center, -center);

    return view;
}

// ====================================================================
// OBSŁUGA MYSZY
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

        // --- POPRAWIONY RAYCASTING ---
        QMatrix4x4 view = getViewMatrix();
        QMatrix4x4 invViewProj = (projection * view).inverted();

        // Normalizacja współrzędnych ekranu do NDC [-1, 1]
        float x = (2.0f * event->pos().x()) / width() - 1.0f;
        float y = 1.0f - (2.0f * event->pos().y()) / height();

        // Punkty w clip space
        QVector4D nearPoint(x, y, -1.0f, 1.0f);
        QVector4D farPoint(x, y, 1.0f, 1.0f);

        // Transformacja do world space
        QVector4D nearWorld = invViewProj * nearPoint;
        QVector4D farWorld = invViewProj * farPoint;

        // Perspective divide
        if (std::abs(nearWorld.w()) > 0.0001f && std::abs(farWorld.w()) > 0.0001f) {
            nearWorld /= nearWorld.w();
            farWorld /= farWorld.w();
        }

        QVector3D rayOrigin = nearWorld.toVector3D();
        QVector3D rayDir = (farWorld.toVector3D() - nearWorld.toVector3D()).normalized();

        float minDistance = std::numeric_limits<float>::max();
        int clickedX = -1, clickedY = -1, clickedZ = -1;

        const float r = 0.5f; // Promień połowy boku sześcianu

        // Sprawdzanie przecięć z każdym sześcianem
        for (int x = 0; x < gridSize; ++x) {
            for (int y = 0; y < gridSize; ++y) {
                for (int z = 0; z < gridSize; ++z) {
                    QVector3D center(x * m_spacing, y * m_spacing, z * m_spacing);
                    QVector3D bMin = center - QVector3D(r, r, r);
                    QVector3D bMax = center + QVector3D(r, r, r);

                    // Algorytm przecięcia promienia z AABB (Axis-Aligned Bounding Box)
                    float tmin = (bMin.x() - rayOrigin.x()) / rayDir.x();
                    float tmax = (bMax.x() - rayOrigin.x()) / rayDir.x();

                    if (tmin > tmax) std::swap(tmin, tmax);

                    float tymin = (bMin.y() - rayOrigin.y()) / rayDir.y();
                    float tymax = (bMax.y() - rayOrigin.y()) / rayDir.y();

                    if (tymin > tymax) std::swap(tymin, tymax);

                    if ((tmin > tymax) || (tymin > tmax)) continue;

                    if (tymin > tmin) tmin = tymin;
                    if (tymax < tmax) tmax = tymax;

                    float tzmin = (bMin.z() - rayOrigin.z()) / rayDir.z();
                    float tzmax = (bMax.z() - rayOrigin.z()) / rayDir.z();

                    if (tzmin > tzmax) std::swap(tzmin, tzmax);

                    if ((tmin > tzmax) || (tzmin > tmax)) continue;

                    if (tzmin > tmin) tmin = tzmin;
                    if (tzmax < tmax) tmax = tzmax;

                    // Trafienie - sprawdź czy jest najbliższe
                    if (tmax >= 0.0f && tmin < minDistance) {
                        minDistance = tmin >= 0.0f ? tmin : tmax;
                        clickedX = x;
                        clickedY = y;
                        clickedZ = z;
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
// INICJALIZACJA OPENGL
// ====================================================================

void BoardCanvas::initializeGL() {
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    // Opcjonalnie: jeśli sześcian wydaje się "pusty" w środku, wyłącz culling
    // glEnable(GL_CULL_FACE); 
    // glCullFace(GL_BACK);

    program = new QOpenGLShaderProgram();
    program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSourceInstanced);
    program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSourceInstanced);
    program->link();

    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    // 1. Geometria sześcianu (dla wypełnienia)
    float solidVertices[] = {
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f
    };

    vbo.create();
    vbo.bind();
    vbo.allocate(solidVertices, sizeof(solidVertices));
    vbo.release();

    // 2. Geometria linii
    float lineVertices[] = {
        -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,  -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,   0.5f, -0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f
    };

    vboLines.create();
    vboLines.bind();
    vboLines.allocate(lineVertices, sizeof(lineVertices));
    vboLines.release();

    // 3. Inicjalizacja bufora instancji
    vboInstanced.create();
    vboInstanced.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    vboInstanced.allocate(nullptr, 1000 * sizeof(InstanceData)); // Alokacja wstępna

    // Funkcja pomocnicza konfigurująca VAO
    auto setupVAO = [&](QOpenGLVertexArrayObject& vao, QOpenGLBuffer& geomBuffer) {
        vao.create();
        vao.bind();

        geomBuffer.bind();
        program->enableAttributeArray(0);
        program->setAttributeBuffer(0, GL_FLOAT, 0, 3, 0);

        vboInstanced.bind();
        program->enableAttributeArray(1);
        program->setAttributeBuffer(1, GL_FLOAT, 0, 3, sizeof(InstanceData));
        glVertexAttribDivisor(1, 1);

        program->enableAttributeArray(2);
        program->setAttributeBuffer(2, GL_FLOAT, sizeof(QVector3D), 3, sizeof(InstanceData));
        glVertexAttribDivisor(2, 1);

        vao.release();
        vboInstanced.release();
        };

    setupVAO(vaoFilled, vbo);
    setupVAO(vaoLines, vboLines);
}

void BoardCanvas::resizeGL(int w, int h) {
    if (h == 0) h = 1;
    projection.setToIdentity();
    projection.perspective(45.0f, GLfloat(w) / h, 0.1f, 1000.0f);
}

// ====================================================================
// RENDEROWANIE Z INSTANCED DRAWING
// ====================================================================

void BoardCanvas::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (instanceData.empty() || gridSize <= 0) return;

    program->bind();
    program->setUniformValue("projection", projection);
    program->setUniformValue("view", getViewMatrix());

    int instanceCount = static_cast<int>(instanceData.size());

    // Pas 1: Wypełnienie
    vaoFilled.bind();
    program->setUniformValue("u_isOutline", false);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, instanceCount);
    vaoFilled.release();

    // Pas 2: Krawędzie
    vaoLines.bind();
    glLineWidth(1.0f);
    program->setUniformValue("u_isOutline", true);
    glDrawArraysInstanced(GL_LINES, 0, 24, instanceCount);
    vaoLines.release();

    program->release();
}

QColor BoardCanvas::getColor(CellState state) const {
    switch (state) {
    case CellState::Infected: return Qt::red;
    case CellState::Immune:   return Qt::yellow;
    case CellState::Healthy:  return Qt::green;
    default:                  return Qt::black;
    }
}
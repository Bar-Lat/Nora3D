#include "BoardCanvas.h"

// ====================================================================
// KONSTRUKTOR I INTERFEJS PUBLICZNY
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
        gl_Position = projection * view * vec4(aPos + aOffset, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    in vec3 fragColor;
    out vec4 FragColor;

    void main() {
        FragColor = vec4(fragColor, 1.0);
    }
)";

BoardCanvas::BoardCanvas(QWidget* parent)
    : QOpenGLWidget(parent) 
    , m_scaleFactor(10)
{
    setMinimumSize(400, 400);
}

// nowa plansza
void BoardCanvas::setGrid(const Grid3D& newGrid)
{
    grid = newGrid;
    // Oblicz bok sześcianu: N = pierwiastek sześcienny z rozmiaru wektora
    gridSize = static_cast<int>(std::round(std::pow(grid.size(), 1.0 / 3.0)));
    update();
}

// ustawienie skali
void BoardCanvas::setScaleFactor(int factor) {
    m_scaleFactor = std::clamp(factor, 1, 100); // Skala od 1px do 100px
    update();
}

void BoardCanvas::wheelEvent(QWheelEvent* event)
{
    // Pobieramy kąt obrotu kółka
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 15; 

    // Obliczamy nowy współczynnik skali
    int newScale = m_scaleFactor + numSteps;

    // Ustawiamy nową skalę korzystając z Twojej istniejącej metody
    setScaleFactor(newScale);

    event->accept();
}

// ====================================================================
// OBSŁUGA MYSZY
// ====================================================================

// indeks komorki z pozycji myszy
QPoint BoardCanvas::cellIndexFromPos(const QPoint& pos) const
{
    // pusta plansza
    if (grid.empty()) return QPoint(-1, -1);

    int rows = static_cast<int>(grid.size());

    int cellSize = m_scaleFactor;
    int gridPixelSize = cellSize * rows;
    int xOffset = (width() - gridPixelSize) / 2;
    int yOffset = (height() - gridPixelSize) / 2;

	// obliczenie wiersza i kolumny
    int col = (pos.x() - xOffset) / cellSize;
    int row = (pos.y() - yOffset) / cellSize;

	// sprawdzenie czy w obszarze planszy
    if (row >= 0 && row < rows && col >= 0 && col < rows) {
        return QPoint(row, col);
    }
	// poza plansza
    return QPoint(-1, -1);
}

// obsluga klikniecia
void BoardCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {

        QPoint cell = cellIndexFromPos(event->pos());

        if (cell.x() != -1) {
            // wyslanie sygnalu z pozycja komorki
            emit cellClicked(cell.x(), cell.y(), 0);
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

    float vertices[] = {
        // A - back face (z = -0.5)
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,

         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,

        // B - right face (x = 0.5)
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f, -0.5f,

         0.5f,  0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,

         // C - front face (z = 0.5)
          0.5f, -0.5f,  0.5f,
         -0.5f, -0.5f,  0.5f,
          0.5f,  0.5f,  0.5f,

          0.5f,  0.5f,  0.5f,
         -0.5f, -0.5f,  0.5f,
         -0.5f,  0.5f,  0.5f,

         // D - left face (x = -0.5)
         -0.5f, -0.5f,  0.5f,
         -0.5f, -0.5f, -0.5f,
         -0.5f,  0.5f,  0.5f,

         -0.5f,  0.5f,  0.5f,
         -0.5f, -0.5f, -0.5f,
         -0.5f,  0.5f, -0.5f,

         // E - bottom face (y = -0.5)
         -0.5f, -0.5f,  0.5f,
          0.5f, -0.5f,  0.5f,
          0.5f, -0.5f, -0.5f,

          0.5f, -0.5f, -0.5f,
         -0.5f, -0.5f, -0.5f,
         -0.5f, -0.5f,  0.5f,

         // F - top face (y = 0.5)
         -0.5f,  0.5f, -0.5f,
          0.5f,  0.5f, -0.5f,
          0.5f,  0.5f,  0.5f,

          0.5f,  0.5f,  0.5f,
         -0.5f,  0.5f,  0.5f,
         -0.5f,  0.5f, -0.5f
    };

    // 2. Konfiguracja VAO (Vertex Array Object)
    vao.create();
    vao.bind();

    // 3. Konfiguracja VBO (Vertex Buffer Object)
    vbo.create();
    vbo.bind();
    vbo.allocate(vertices, sizeof(vertices));

    // 4. Powiązanie danych z atrybutem w shaderze (index 0 to zwykle position)
    program->enableAttributeArray(0);
    program->setAttributeBuffer(0, GL_FLOAT, 0, 3);

    vao.release();
    vbo.release();
}

void BoardCanvas::resizeGL(int w, int h) {
    projection.setToIdentity();
    projection.perspective(45.0f, GLfloat(w) / h, 0.01f, 100.0f);
}

void BoardCanvas::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (grid.empty()) return;

    program->bind();

    QMatrix4x4 view;
    float center = (gridSize - 1) / 2.0f; // Środek siatki
    // Odsunięcie zależy od skali i rozmiaru siatki
    float distance = gridSize * 1.5f + m_scaleFactor;

    view.lookAt(QVector3D(center, center, distance),
        QVector3D(center, center, 0),
        QVector3D(0, 1, 0));

    program->setUniformValue("projection", projection);
    program->setUniformValue("view", view);

    vao.bind();

    for (int x = 0; x < gridSize; ++x) {
        for (int y = 0; y < gridSize; ++y) {
            for (int z = 0; z < gridSize; ++z) {
                CellState state = grid[getIndex(x, y, z, gridSize)];
                if (state != CellState::Healthy) {
                    program->setUniformValue("aOffset", QVector3D(x, y, z));
                    program->setUniformValue("aColor", QVector3D(getColor(state).redF(),
                        getColor(state).greenF(),
                        getColor(state).blueF()));
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
            }
        }
    }
    vao.release();
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
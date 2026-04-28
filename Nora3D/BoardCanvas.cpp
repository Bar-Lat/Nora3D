#include "BoardCanvas.h"

// ====================================================================
// KONSTRUKTOR I INTERFEJS PUBLICZNY
// ====================================================================

BoardCanvas::BoardCanvas(QWidget* parent)
    : QWidget(parent)
    , m_scaleFactor(10) //domyslna skala
{
    // minimalny rozmiar 
    setMinimumSize(400, 400);
}

// nowa plansza
void BoardCanvas::setGrid(const Grid& newGrid)
{
    grid = newGrid;
	update(); // przeładowanie widoku
}

// ustawienie skali
void BoardCanvas::setScaleFactor(int factor)
{
	// zabezpieczenie przed zerem lub ujemna skala.
    m_scaleFactor = std::max(1, factor);
    update();
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
            emit cellClicked(cell.x(), cell.y());
        }
    }
    QWidget::mousePressEvent(event);
}

// ====================================================================
// RYSOWANIE 
// ====================================================================

void BoardCanvas::paintEvent(QPaintEvent*)
{
	// zabezpieczenie przed pusta plansza
    if (grid.empty()) return;

	// konstrukcja malarza
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false); // wylaczenie antyaliasingu

    int rows = static_cast<int>(grid.size());
    int cols = rows; // bo kwadrat

    // rozmiar komorki 
    const int cellSize = scaleFactor();

    // rozmiar siatki
    int gridPixelSize = cellSize * rows;

    // przesuniecie do wysrodkownia
    int xOffset = (width() - gridPixelSize) / 2;
    int yOffset = (height() - gridPixelSize) / 2;

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            QRect rect(xOffset + j * cellSize,
                yOffset + i * cellSize,
                cellSize, cellSize);

			// wypelnienie kolorem
            painter.fillRect(rect, getColor(grid[i][j]));
        }
    }
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
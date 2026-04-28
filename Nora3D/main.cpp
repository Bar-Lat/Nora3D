#include "nora3d.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	// arkusz stylu qt
    QString globalStyle = R"(
        QFrame {
            background-color: #333333; /* Ciemne tło */
            border-radius: 0px;       /* Zaokrąglone rogi */
            border: 1px solid #555555; /* Delikatna ramka */
        }
        QLabel {
            background-color: transparent;
            border: none; 
        }
        #HealthyCellsLabel {
            color: #00FF00; 
        }
        #InfectedCellsLabel {
            color: #FF0000; 
        }
        #ImmuneCellsLabel {
            color: #FFFF00;
        }
    )";

	// inicjalizacja aplikacji
    QApplication app(argc, argv);
    app.setStyleSheet(globalStyle);
    nora3d window;
    window.show();
    return app.exec();
}

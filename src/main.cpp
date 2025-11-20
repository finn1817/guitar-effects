#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Guitar Effects App");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("GuitarEffects");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
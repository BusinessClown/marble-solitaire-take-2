#include <QApplication>

#include "main_window.h"

// Creates the single QApplication instance, shows the main window and runs
// Qt's event loop until the last window closes.
int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  MainWindow window;
  window.show();

  return app.exec();
}

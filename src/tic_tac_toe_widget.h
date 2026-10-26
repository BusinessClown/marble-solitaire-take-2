#ifndef MARBLE_SOLITAIRE_SRC_TIC_TAC_TOE_WIDGET_H_
#define MARBLE_SOLITAIRE_SRC_TIC_TAC_TOE_WIDGET_H_

#include <QString>
#include <QWidget>

#include "tic_tac_toe_game.h"

class QMouseEvent;
class QPaintEvent;

// Draws a tic-tac-toe board and turns mouse clicks into moves.
//
// All rules live in tictactoe::Game; this class only paints that state and
// reports it through StatusChanged(). Note that paintEvent() and
// mousePressEvent() keep Qt's lowerCamelCase spelling because they override
// virtual functions declared by QWidget.
class TicTacToeWidget : public QWidget {
  Q_OBJECT

 public:
  explicit TicTacToeWidget(QWidget* parent = nullptr);

  // Clears the board and starts a new game.
  void ResetBoard(bool x_goes_first);

 signals:
  // Emitted whenever the status text changes, for example "X's turn",
  // "O wins!" or "It's a draw!".
  void StatusChanged(const QString& text);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 private:
  // Emits StatusChanged() with the game's current status text.
  void EmitStatus();

  tictactoe::Game game_;
};

#endif  // MARBLE_SOLITAIRE_SRC_TIC_TAC_TOE_WIDGET_H_

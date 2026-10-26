#include "tic_tac_toe_widget.h"

#include <QColor>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QString>

#include "tic_tac_toe_game.h"

namespace {

// Smallest size at which the board still reads clearly, in pixels.
constexpr int kMinimumSide = 300;

// Pen widths, in pixels.
constexpr int kGridPenWidth = 3;
constexpr int kMarkPenWidth = 4;
constexpr int kWinLinePenWidth = 6;

// Gap between a cell's edge and the mark drawn inside it, in pixels.
constexpr int kMarkPadding = 20;

}  // namespace

TicTacToeWidget::TicTacToeWidget(QWidget* parent) : QWidget(parent) {
  setMinimumSize(kMinimumSide, kMinimumSide);
}

void TicTacToeWidget::ResetBoard(bool x_goes_first) {
  game_.Reset(x_goes_first ? tictactoe::Player::kX : tictactoe::Player::kO);
  EmitStatus();
  update();
}

void TicTacToeWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const int board_width = width();
  const int board_height = height();
  const int cell_width = board_width / tictactoe::Game::kBoardSize;
  const int cell_height = board_height / tictactoe::Game::kBoardSize;

  // Grid lines: two vertical, two horizontal.
  painter.setPen(QPen(Qt::black, kGridPenWidth));
  for (int i = 1; i < tictactoe::Game::kBoardSize; ++i) {
    painter.drawLine(cell_width * i, 0, cell_width * i, board_height);
    painter.drawLine(0, cell_height * i, board_width, cell_height * i);
  }

  // Marks.
  for (int row = 0; row < tictactoe::Game::kBoardSize; ++row) {
    for (int col = 0; col < tictactoe::Game::kBoardSize; ++col) {
      const int left = col * cell_width;
      const int top = row * cell_height;
      const tictactoe::Cell cell = game_.CellAt(row, col);

      if (cell == tictactoe::Cell::kX) {
        painter.setPen(QPen(Qt::red, kMarkPenWidth));
        painter.drawLine(left + kMarkPadding, top + kMarkPadding,
                         left + cell_width - kMarkPadding,
                         top + cell_height - kMarkPadding);
        painter.drawLine(left + cell_width - kMarkPadding,
                         top + kMarkPadding, left + kMarkPadding,
                         top + cell_height - kMarkPadding);
      } else if (cell == tictactoe::Cell::kO) {
        painter.setPen(QPen(Qt::darkGreen, kMarkPenWidth));
        painter.drawEllipse(left + kMarkPadding, top + kMarkPadding,
                            cell_width - 2 * kMarkPadding,
                            cell_height - 2 * kMarkPadding);
      }
    }
  }

  // Line through the winning cells. Drawing from the centre of the first
  // winning cell to the centre of the last also covers the middle one,
  // because the three are collinear by construction.
  const bool has_winner =
      game_.state() == tictactoe::GameState::kXWins ||
      game_.state() == tictactoe::GameState::kOWins;
  if (has_winner) {
    const auto& line = game_.winning_line();
    const QPoint start(line.front().col * cell_width + cell_width / 2,
                       line.front().row * cell_height + cell_height / 2);
    const QPoint end(line.back().col * cell_width + cell_width / 2,
                     line.back().row * cell_height + cell_height / 2);

    painter.setPen(QPen(Qt::blue, kWinLinePenWidth));
    painter.drawLine(start, end);
  }
}

void TicTacToeWidget::mousePressEvent(QMouseEvent* event) {
  const int cell_width = width() / tictactoe::Game::kBoardSize;
  const int cell_height = height() / tictactoe::Game::kBoardSize;
  if (cell_width <= 0 || cell_height <= 0) {
    return;
  }

  const QPoint click = event->pos();
  if (!game_.PlayMove(click.y() / cell_height, click.x() / cell_width)) {
    return;  // Off the board, cell taken, or the game is already over.
  }

  EmitStatus();
  update();
}

void TicTacToeWidget::EmitStatus() {
  emit StatusChanged(QString::fromStdString(game_.StatusText()));
}

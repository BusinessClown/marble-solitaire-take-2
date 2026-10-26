#include "tic_tac_toe_game.h"

#include <array>
#include <string>

namespace tictactoe {
namespace {

// Every line that wins the game, expressed as three board positions:
// three rows, three columns and the two diagonals.
constexpr std::array<std::array<Position, Game::kBoardSize>, 8> kWinningLines =
    {{
        {{{0, 0}, {0, 1}, {0, 2}}},
        {{{1, 0}, {1, 1}, {1, 2}}},
        {{{2, 0}, {2, 1}, {2, 2}}},
        {{{0, 0}, {1, 0}, {2, 0}}},
        {{{0, 1}, {1, 1}, {2, 1}}},
        {{{0, 2}, {1, 2}, {2, 2}}},
        {{{0, 0}, {1, 1}, {2, 2}}},
        {{{0, 2}, {1, 1}, {2, 0}}},
    }};

bool IsOnBoard(int row, int col) {
  return row >= 0 && row < Game::kBoardSize && col >= 0 &&
         col < Game::kBoardSize;
}

}  // namespace

Game::Game() { Reset(Player::kX); }

void Game::Reset(Player first_player) {
  for (auto& row : board_) {
    row.fill(Cell::kEmpty);
  }
  winning_line_.fill(Position{});
  current_player_ = first_player;
  state_ = GameState::kInProgress;
}

bool Game::PlayMove(int row, int col) {
  if (is_over() || !IsOnBoard(row, col) ||
      board_[row][col] != Cell::kEmpty) {
    return false;
  }

  const Cell mark = MarkFor(current_player_);
  board_[row][col] = mark;

  if (FindWinningLine(mark)) {
    state_ = current_player_ == Player::kX ? GameState::kXWins
                                           : GameState::kOWins;
    return true;
  }

  if (IsBoardFull()) {
    state_ = GameState::kDraw;
    return true;
  }

  current_player_ =
      current_player_ == Player::kX ? Player::kO : Player::kX;
  return true;
}

Cell Game::CellAt(int row, int col) const {
  if (!IsOnBoard(row, col)) {
    return Cell::kEmpty;
  }
  return board_[row][col];
}

std::string Game::StatusText() const {
  switch (state_) {
    case GameState::kXWins:
      return "X wins!";
    case GameState::kOWins:
      return "O wins!";
    case GameState::kDraw:
      return "It's a draw!";
    case GameState::kInProgress:
      break;
  }
  return current_player_ == Player::kX ? "X's turn" : "O's turn";
}

bool Game::FindWinningLine(Cell mark) {
  for (const auto& line : kWinningLines) {
    const bool is_win =
        board_[line[0].row][line[0].col] == mark &&
        board_[line[1].row][line[1].col] == mark &&
        board_[line[2].row][line[2].col] == mark;
    if (is_win) {
      winning_line_ = line;
      return true;
    }
  }
  return false;
}

bool Game::IsBoardFull() const {
  for (const auto& row : board_) {
    for (const Cell cell : row) {
      if (cell == Cell::kEmpty) {
        return false;
      }
    }
  }
  return true;
}

Cell Game::MarkFor(Player player) {
  return player == Player::kX ? Cell::kX : Cell::kO;
}

}  // namespace tictactoe

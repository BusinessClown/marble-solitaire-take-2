#ifndef MARBLE_SOLITAIRE_SRC_TIC_TAC_TOE_GAME_H_
#define MARBLE_SOLITAIRE_SRC_TIC_TAC_TOE_GAME_H_

#include <array>
#include <string>

namespace tictactoe {

// Which of the two players is to move.
enum class Player { kX, kO };

// Contents of a single board cell.
enum class Cell { kEmpty, kX, kO };

// Overall progress of a game.
enum class GameState { kInProgress, kXWins, kOWins, kDraw };

// A board coordinate. Both members are in [0, Game::kBoardSize).
struct Position {
  int row = 0;
  int col = 0;
};

// Rules engine for a game of tic-tac-toe.
//
// This class holds no UI state and depends on nothing but the standard
// library, which is what makes it directly unit testable. The widget layer
// owns an instance, forwards clicks to PlayMove(), and paints whatever
// CellAt() reports.
//
// This class is not thread-safe; use it from a single thread (in a Qt
// application, the GUI thread).
class Game {
 public:
  // Length of one side of the board, and therefore also the number of
  // marks needed in a row to win.
  static constexpr int kBoardSize = 3;

  // Starts a game with X to move.
  Game();

  // Clears the board and hands the first move to `first_player`.
  void Reset(Player first_player);

  // Places the current player's mark on the given cell and advances the
  // game. Returns false and changes nothing if the game is already over,
  // the coordinates are off the board, or the cell is occupied.
  bool PlayMove(int row, int col);

  // Returns the contents of a cell. Returns Cell::kEmpty for coordinates
  // that are off the board.
  Cell CellAt(int row, int col) const;

  // The player to move. Meaningful only while the game is in progress.
  Player current_player() const { return current_player_; }

  GameState state() const { return state_; }

  bool is_over() const { return state_ != GameState::kInProgress; }

  // The three cells that won the game. Meaningful only when state() is
  // GameState::kXWins or GameState::kOWins.
  const std::array<Position, kBoardSize>& winning_line() const {
    return winning_line_;
  }

  // Human-readable description of state(), for example "X's turn",
  // "O wins!" or "It's a draw!".
  std::string StatusText() const;

 private:
  // Searches the board for three `mark` cells in a row. On success records
  // the three cells in winning_line_ and returns true.
  bool FindWinningLine(Cell mark);

  // Returns true when no cell is empty.
  bool IsBoardFull() const;

  // Returns the mark a player writes into the board.
  static Cell MarkFor(Player player);

  std::array<std::array<Cell, kBoardSize>, kBoardSize> board_;
  std::array<Position, kBoardSize> winning_line_;
  Player current_player_ = Player::kX;
  GameState state_ = GameState::kInProgress;
};

}  // namespace tictactoe

#endif  // MARBLE_SOLITAIRE_SRC_TIC_TAC_TOE_GAME_H_

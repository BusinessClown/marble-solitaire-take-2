#ifndef MARBLE_SOLITAIRE_SRC_MARBLE_GAME_H_
#define MARBLE_SOLITAIRE_SRC_MARBLE_GAME_H_

#include <array>
#include <vector>

namespace marble {

// Board shapes. The values must match the ids given to the tiles in
// kBoardOptions in main_window.cc.
enum class BoardShape {
    kFrench = 0,
    kJcWiegleb = 1,
    kAsymmetrical = 2,
    kEnglish = 3,
    kDiamond = 4,
    kTriangle = 5,
};

// Where the single empty hole starts. The values must match the ids given to
// the tiles in kStartPositionOptions in main_window.cc.
enum class StartPosition { kStandard = 0, kRandom = 1 };

// How much larger the board is than its standard form. The values are the
// multipliers, and must match the ids given to the tiles in kScaleOptions in
// main_window.cc.
enum class BoardScale { kStandard = 1, kDouble = 2, kTriple = 3 };

// How a finished game went, judged by the marbles left. kPerfectGame is the
// last marble sitting where the starting hole was; kWin is one marble left
// anywhere else.
enum class Outcome {
    kPerfectGame,
    kWin,
    kTwoLeft,
    kThreeLeft,
    kFourLeft,
    kFiveOrMoreLeft,
};

// State of one grid location.
enum class Spot { kOffBoard, kMarble, kEmpty };

// A board coordinate.
struct Position {
    int row = 0;
    int col = 0;
};

inline bool operator==(const Position& a, const Position& b) {
    return a.row == b.row && a.col == b.col;
}

// One jump: the marble at `from` jumps over the marble at `over`, lands in
// the empty hole `to`, and the jumped marble is removed.
struct Move {
    Position from;
    Position over;
    Position to;
};

// Board state and rules for marble solitaire.
//
// This class holds no UI state and depends only on the standard library, so
// it can be unit tested directly. It is not thread-safe.
class Game {
public:
    // Largest board dimension in grid cells (J.C. Wiegleb and Diamond at
    // BoardScale::kTriple are 25x25).
    static constexpr int kMaxBoardSize = 25;

    // Starts with a standard-size English board.
    Game();

    // Fills the scaled `shape` with marbles and empties one hole, chosen
    // according to `start`. Also clears the captured count.
    void Reset(BoardShape shape, StartPosition start, BoardScale scale);

    // Dimensions of the current board's bounding grid.
    int rows() const { return rows_; }
    int cols() const { return cols_; }

    // Number of grid columns that one spot spans. This is 1 for square-grid
    // boards and 2 for the triangle, whose rows are offset by half a spot.
    int columns_per_spot() const { return columns_per_spot_; }

    // Returns the contents of a grid location. Returns Spot::kOffBoard for
    // coordinates outside the grid.
    Spot SpotAt(int row, int col) const;

    // Returns every jump available to the marble at (row, col): over an
    // adjacent marble into the empty hole beyond it. Returns an empty vector if
    // there is no marble there or it cannot jump. Square boards allow the four
    // orthogonal directions; the triangle allows six.
    std::vector<Move> MovesFrom(int row, int col) const;

    // Returns true if the marble at (row, col) has at least one jump.
    bool IsMovable(int row, int col) const;

    // Performs `move`, removing the jumped marble. Returns false and changes
    // nothing if the move is not legal.
    bool PlayMove(const Move& move);

    // Number of marbles removed so far.
    int captured_count() const { return captured_count_; }

    // Position of the hole that was empty when the game started.
    Position start_hole() const { return start_hole_; }

    // Number of marbles currently on the board.
    int MarblesLeft() const;

    // Returns true if any marble can still jump. The game is over when this is
    // false.
    bool HasMoves() const;

    // Describes how the game went. Meaningful only once HasMoves() is false.
    Outcome FinalOutcome() const;

private:
    std::array<std::array<Spot, kMaxBoardSize>, kMaxBoardSize> spots_;
    int rows_ = 0;
    int cols_ = 0;
    int columns_per_spot_ = 1;
    int captured_count_ = 0;
    Position start_hole_;

};

}  // namespace marble

#endif  // MARBLE_SOLITAIRE_SRC_MARBLE_GAME_H_
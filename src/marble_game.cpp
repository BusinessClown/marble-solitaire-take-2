#include "marble_game.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <random>
#include <vector>
#include <array>

namespace marble {
namespace {

// Width of the arms (and of the straight faces of the French board). It does
// not change with scale; the board grows in length instead.
constexpr int kArmWidth = 3;

// Side lengths, in spots, of each shape at BoardScale::kStandard.
constexpr int kFrenchSide = 7;
constexpr int kEnglishSide = 7;
constexpr int kWieglebSide = 9;
constexpr int kDiamondSide = 9;
constexpr int kTriangleRowCount = 5;
constexpr int kAsymmetricalSide = 8;

// Standard-size asymmetrical board, scaled by nearest-neighbour sampling.
// '#' is a hole, '.' is outside the board. NOTE: 37 holes; check this against
// asymmetrical.png and adjust to taste.
constexpr const char* kAsymmetricalRows[kAsymmetricalSide] = {
    "..###...",
    "..###...",
    "..###...",
    "########",
    "########",
    "########",
    "..###...",
    "..###...",
};

struct Dimensions {
    int rows;
    int cols;
    int columns_per_spot;
};

// Distance from one end of a standard-size board to the other, multiplied by
// the scale. Side 7 becomes 13 at 2x and 19 at 3x.
int ScaledSide(int standard_side, int factor) {
    return (standard_side - 1) * factor + 1;
}

Dimensions DimensionsFor(BoardShape shape, int factor) {
    switch (shape) {
    case BoardShape::kFrench:
        return {ScaledSide(kFrenchSide, factor), ScaledSide(kFrenchSide, factor),
                1};
    case BoardShape::kJcWiegleb:
        return {ScaledSide(kWieglebSide, factor),
                ScaledSide(kWieglebSide, factor), 1};
    case BoardShape::kAsymmetrical:
        return {ScaledSide(kAsymmetricalSide, factor),
                ScaledSide(kAsymmetricalSide, factor), 1};
    case BoardShape::kEnglish:
        return {ScaledSide(kEnglishSide, factor),
                ScaledSide(kEnglishSide, factor), 1};
    case BoardShape::kDiamond:
        return {ScaledSide(kDiamondSide, factor),
                ScaledSide(kDiamondSide, factor), 1};
    case BoardShape::kTriangle: {
        // Each spot spans two columns so that rows can be offset by half a
        // spot.
        const int rows = ScaledSide(kTriangleRowCount, factor);
        return {rows, 2 * rows - 1, 2};
    }
    }
    return {0, 0, 1};
}

// An octagon: the top row is kArmWidth wide and each row below is two spots
// wider until the board reaches its full width.
bool IsFrenchHole(int row, int col, int side) {
    const int distance_from_edge = std::min(row, side - 1 - row);
    const int cut = (side - kArmWidth) / 2 - distance_from_edge;
    const int margin = std::max(0, cut);
    return col >= margin && col < side - margin;
}

// A plus sign with kArmWidth-wide arms.
bool IsCrossHole(int row, int col, int side) {
    const int low = (side - kArmWidth) / 2;
    const int high = low + kArmWidth - 1;
    return (row >= low && row <= high) || (col >= low && col <= high);
}

bool IsDiamondHole(int row, int col, int side) {
    const int center = side / 2;
    return std::abs(row - center) + std::abs(col - center) <= center;
}

// Rows are centred and `row + 1` spots wide, each spot on every second
// column.
bool IsTriangleHole(int row, int col, int rows) {
    const int apex_col = rows - 1;
    return col >= apex_col - row && col <= apex_col + row &&
           (row + col) % 2 == 0;
}

bool IsAsymmetricalHole(int row, int col, int side) {
    const int last = side - 1;
    const int source_row = (row * (kAsymmetricalSide - 1) + last / 2) / last;
    const int source_col = (col * (kAsymmetricalSide - 1) + last / 2) / last;
    return kAsymmetricalRows[source_row][source_col] == '#';
}

bool IsHole(BoardShape shape, const Dimensions& size, int row, int col) {
    switch (shape) {
    case BoardShape::kFrench:
        return IsFrenchHole(row, col, size.rows);
    case BoardShape::kJcWiegleb:
    case BoardShape::kEnglish:
        return IsCrossHole(row, col, size.rows);
    case BoardShape::kAsymmetrical:
        return IsAsymmetricalHole(row, col, size.rows);
    case BoardShape::kDiamond:
        return IsDiamondHole(row, col, size.rows);
    case BoardShape::kTriangle:
        return IsTriangleHole(row, col, size.rows);
    }
    return false;
}

// Returns the hole closest to the middle of the grid. Coordinates are doubled
// so that the middle of an even-sized grid needs no fractions.
Position CenterMostHole(const std::vector<Position>& holes,
                        const Dimensions& size) {
    Position best = holes.front();
    int best_distance = -1;
    for (const Position& hole : holes) {
        const int delta_row = 2 * hole.row - (size.rows - 1);
        const int delta_col = 2 * hole.col - (size.cols - 1);
        const int distance = delta_row * delta_row + delta_col * delta_col;
        if (best_distance < 0 || distance < best_distance) {
            best = hole;
            best_distance = distance;
        }
    }
    return best;
}

bool IsOnGrid(int row, int col, int rows, int cols) {
    return row >= 0 && row < rows && col >= 0 && col < cols;
}

// Jump directions: the offset from a marble to the marble it jumps over. The
// landing hole is twice this offset away. The triangle's spots are two
// columns wide, hence the (0, +-2) sideways step.
constexpr std::array<Position, 4> kSquareDirections = {
    {{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};
constexpr std::array<Position, 6> kPyramidDirections = {
    {{0, -2}, {0, 2}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}}};

struct DirectionSet {
    const Position* data;
    std::size_t size;
};

DirectionSet DirectionsFor(int columns_per_spot) {
    if (columns_per_spot > 1) {
        return {kPyramidDirections.data(), kPyramidDirections.size()};
    }
    return {kSquareDirections.data(), kSquareDirections.size()};
}

}  // namespace



Game::Game() {
    Reset(BoardShape::kEnglish, StartPosition::kStandard,
          BoardScale::kStandard);
}

void Game::Reset(BoardShape shape, StartPosition start, BoardScale scale) {
    const Dimensions size = DimensionsFor(shape, static_cast<int>(scale));
    rows_ = size.rows;
    cols_ = size.cols;
    columns_per_spot_ = size.columns_per_spot;
    captured_count_ = 0;

    for (auto& row : spots_) {
        row.fill(Spot::kOffBoard);
    }

    std::vector<Position> holes;
    for (int row = 0; row < rows_; ++row) {
        for (int col = 0; col < cols_; ++col) {
            if (IsHole(shape, size, row, col)) {
                spots_[row][col] = Spot::kMarble;
                holes.push_back(Position{row, col});
            }
        }
    }

    Position empty = CenterMostHole(holes, size);
    if (start == StartPosition::kRandom) {
        std::mt19937 generator{std::random_device{}()};
        std::uniform_int_distribution<std::size_t> pick(0, holes.size() - 1);
        empty = holes[pick(generator)];
    }
    start_hole_ = empty;
    spots_[empty.row][empty.col] = Spot::kEmpty;
}

Spot Game::SpotAt(int row, int col) const {
    if (!IsOnGrid(row, col, rows_, cols_)) {
        return Spot::kOffBoard;
    }
    return spots_[row][col];
}

std::vector<Move> Game::MovesFrom(int row, int col) const {
    std::vector<Move> moves;
    if (SpotAt(row, col) != Spot::kMarble) {
        return moves;
    }

    const DirectionSet directions = DirectionsFor(columns_per_spot_);
    for (std::size_t i = 0; i < directions.size; ++i) {
        const Position step = directions.data[i];
        const Position over{row + step.row, col + step.col};
        const Position to{row + 2 * step.row, col + 2 * step.col};
        if (SpotAt(over.row, over.col) == Spot::kMarble &&
            SpotAt(to.row, to.col) == Spot::kEmpty) {
            moves.push_back(Move{Position{row, col}, over, to});
        }
    }
    return moves;
}

bool Game::IsMovable(int row, int col) const {
    return !MovesFrom(row, col).empty();
}

bool Game::PlayMove(const Move& move) {
    const std::vector<Move> legal_moves =
        MovesFrom(move.from.row, move.from.col);
    const bool is_legal =
        std::any_of(legal_moves.begin(), legal_moves.end(),
                    [&move](const Move& legal) {
                        return legal.over == move.over && legal.to == move.to;
                    });
    if (!is_legal) {
        return false;
    }

    spots_[move.from.row][move.from.col] = Spot::kEmpty;
    spots_[move.over.row][move.over.col] = Spot::kEmpty;
    spots_[move.to.row][move.to.col] = Spot::kMarble;
    ++captured_count_;
    return true;
}

int Game::MarblesLeft() const {
    int count = 0;
    for (int row = 0; row < rows_; ++row) {
        for (int col = 0; col < cols_; ++col) {
            if (spots_[row][col] == Spot::kMarble) {
                ++count;
            }
        }
    }
    return count;
}

bool Game::HasMoves() const {
    for (int row = 0; row < rows_; ++row) {
        for (int col = 0; col < cols_; ++col) {
            if (IsMovable(row, col)) {
                return true;
            }
        }
    }
    return false;
}

Outcome Game::FinalOutcome() const {
    const int left = MarblesLeft();
    if (left <= 1) {
        for (int row = 0; row < rows_; ++row) {
            for (int col = 0; col < cols_; ++col) {
                if (spots_[row][col] == Spot::kMarble &&
                    Position{row, col} == start_hole_) {
                    return Outcome::kPerfectGame;
                }
            }
        }
        return Outcome::kWin;
    }
    switch (left) {
    case 2:
        return Outcome::kTwoLeft;
    case 3:
        return Outcome::kThreeLeft;
    case 4:
        return Outcome::kFourLeft;
    default:
        return Outcome::kFiveOrMoreLeft;
    }
}
}  // namespace marble
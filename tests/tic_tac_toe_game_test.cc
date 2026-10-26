#include "tic_tac_toe_game.h"

#include <array>
#include <vector>

#include <gtest/gtest.h>

namespace tictactoe {
namespace {

using Line = std::array<Position, Game::kBoardSize>;

// The eight winning lines, written out independently of the code under test.
constexpr std::array<Line, 8> kLines = {{
                                          {{{0, 0}, {0, 1}, {0, 2}}},
                                          {{{1, 0}, {1, 1}, {1, 2}}},
                                          {{{2, 0}, {2, 1}, {2, 2}}},
                                          {{{0, 0}, {1, 0}, {2, 0}}},
                                          {{{0, 1}, {1, 1}, {2, 1}}},
                                          {{{0, 2}, {1, 2}, {2, 2}}},
                                          {{{0, 0}, {1, 1}, {2, 2}}},
                                          {{{0, 2}, {1, 1}, {2, 0}}},
                                          }};

int CountCells(const Game& game, Cell kind) {
    int count = 0;
    for (int row = 0; row < Game::kBoardSize; ++row) {
        for (int col = 0; col < Game::kBoardSize; ++col) {
            if (game.CellAt(row, col) == kind) {
                ++count;
            }
        }
    }
    return count;
}

bool IsOnLine(const Line& line, int row, int col) {
    for (const Position& position : line) {
        if (position.row == row && position.col == col) {
            return true;
        }
    }
    return false;
}

// Resets the game with `player` moving first, then has `player` fill `line`
// while the opponent drops marks in cells off the line. Two stray marks can
// never make a line, so `player` wins on the fifth move.
void PlayWinningLine(Game* game, Player player, const Line& line) {
    game->Reset(player);

    std::vector<Position> off_line;
    for (int row = 0; row < Game::kBoardSize; ++row) {
        for (int col = 0; col < Game::kBoardSize; ++col) {
            if (!IsOnLine(line, row, col)) {
                off_line.push_back(Position{row, col});
            }
        }
    }

    for (int i = 0; i < Game::kBoardSize; ++i) {
        ASSERT_TRUE(game->PlayMove(line[i].row, line[i].col));
        if (i < Game::kBoardSize - 1) {
            ASSERT_TRUE(game->PlayMove(off_line[i].row, off_line[i].col));
        }
    }
}

TEST(TicTacToeGameTest, NewGameIsEmptyWithXToMove) {
    const Game game;

    EXPECT_EQ(CountCells(game, Cell::kEmpty), 9);
    EXPECT_EQ(game.current_player(), Player::kX);
    EXPECT_EQ(game.state(), GameState::kInProgress);
    EXPECT_FALSE(game.is_over());
    EXPECT_EQ(game.StatusText(), "X's turn");
}

TEST(TicTacToeGameTest, ResetGivesFirstMoveToChosenPlayer) {
    Game game;
    game.Reset(Player::kO);

    EXPECT_EQ(game.current_player(), Player::kO);
    EXPECT_EQ(game.StatusText(), "O's turn");
}

TEST(TicTacToeGameTest, PlayMoveMarksCellAndSwitchesPlayer) {
    Game game;

    ASSERT_TRUE(game.PlayMove(1, 2));
    EXPECT_EQ(game.CellAt(1, 2), Cell::kX);
    EXPECT_EQ(game.current_player(), Player::kO);
    EXPECT_EQ(game.StatusText(), "O's turn");

    ASSERT_TRUE(game.PlayMove(0, 0));
    EXPECT_EQ(game.CellAt(0, 0), Cell::kO);
    EXPECT_EQ(game.current_player(), Player::kX);
}

TEST(TicTacToeGameTest, PlayMoveRejectsOccupiedCell) {
    Game game;
    ASSERT_TRUE(game.PlayMove(1, 1));

    EXPECT_FALSE(game.PlayMove(1, 1));
    EXPECT_EQ(game.CellAt(1, 1), Cell::kX);
    EXPECT_EQ(game.current_player(), Player::kO);
}

TEST(TicTacToeGameTest, PlayMoveRejectsOffBoardCoordinates) {
    Game game;

    EXPECT_FALSE(game.PlayMove(-1, 0));
    EXPECT_FALSE(game.PlayMove(0, -1));
    EXPECT_FALSE(game.PlayMove(Game::kBoardSize, 0));
    EXPECT_FALSE(game.PlayMove(0, Game::kBoardSize));
    EXPECT_EQ(CountCells(game, Cell::kEmpty), 9);
    EXPECT_EQ(game.current_player(), Player::kX);
}

TEST(TicTacToeGameTest, CellAtOffBoardIsEmpty) {
    Game game;
    ASSERT_TRUE(game.PlayMove(0, 0));

    EXPECT_EQ(game.CellAt(-1, 0), Cell::kEmpty);
    EXPECT_EQ(game.CellAt(0, Game::kBoardSize), Cell::kEmpty);
}

TEST(TicTacToeGameTest, EveryLineWinsForX) {
    for (size_t i = 0; i < kLines.size(); ++i) {
        SCOPED_TRACE(i);
        Game game;
        ASSERT_NO_FATAL_FAILURE(PlayWinningLine(&game, Player::kX, kLines[i]));

        EXPECT_EQ(game.state(), GameState::kXWins);
        EXPECT_TRUE(game.is_over());
        EXPECT_EQ(game.StatusText(), "X wins!");
        for (int j = 0; j < Game::kBoardSize; ++j) {
            EXPECT_EQ(game.winning_line()[j].row, kLines[i][j].row);
            EXPECT_EQ(game.winning_line()[j].col, kLines[i][j].col);
        }
    }
}

TEST(TicTacToeGameTest, EveryLineWinsForO) {
    for (size_t i = 0; i < kLines.size(); ++i) {
        SCOPED_TRACE(i);
        Game game;
        ASSERT_NO_FATAL_FAILURE(PlayWinningLine(&game, Player::kO, kLines[i]));

        EXPECT_EQ(game.state(), GameState::kOWins);
        EXPECT_TRUE(game.is_over());
        EXPECT_EQ(game.StatusText(), "O wins!");
    }
}

TEST(TicTacToeGameTest, FullBoardWithoutLineIsDraw) {
    // Final board:  X O X
    //               X O O
    //               O X X
    const Position moves[] = {{0, 0}, {0, 1}, {0, 2}, {1, 1}, {1, 0},
                              {1, 2}, {2, 1}, {2, 0}, {2, 2}};
    Game game;
    for (const Position& move : moves) {
        ASSERT_TRUE(game.PlayMove(move.row, move.col));
    }

    EXPECT_EQ(game.state(), GameState::kDraw);
    EXPECT_TRUE(game.is_over());
    EXPECT_EQ(game.StatusText(), "It's a draw!");
    EXPECT_EQ(CountCells(game, Cell::kEmpty), 0);
}

TEST(TicTacToeGameTest, NoMovesAllowedOnceGameIsOver) {
    Game game;
    ASSERT_NO_FATAL_FAILURE(PlayWinningLine(&game, Player::kX, kLines[0]));
    const int empty_before = CountCells(game, Cell::kEmpty);
    ASSERT_GT(empty_before, 0);

    // Find a free cell and try to play it.
    for (int row = 0; row < Game::kBoardSize; ++row) {
        for (int col = 0; col < Game::kBoardSize; ++col) {
            if (game.CellAt(row, col) == Cell::kEmpty) {
                EXPECT_FALSE(game.PlayMove(row, col));
            }
        }
    }
    EXPECT_EQ(CountCells(game, Cell::kEmpty), empty_before);
}

TEST(TicTacToeGameTest, ResetAfterWinClearsBoardAndState) {
    Game game;
    ASSERT_NO_FATAL_FAILURE(PlayWinningLine(&game, Player::kX, kLines[0]));
    ASSERT_TRUE(game.is_over());

    game.Reset(Player::kX);

    EXPECT_FALSE(game.is_over());
    EXPECT_EQ(game.state(), GameState::kInProgress);
    EXPECT_EQ(CountCells(game, Cell::kEmpty), 9);
    EXPECT_TRUE(game.PlayMove(0, 0));
}

}  // namespace
}  // namespace tictactoe
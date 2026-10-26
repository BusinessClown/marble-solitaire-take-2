#ifndef MARBLE_SOLITAIRE_SRC_MARBLE_BOARD_WIDGET_H_
#define MARBLE_SOLITAIRE_SRC_MARBLE_BOARD_WIDGET_H_

#include <vector>

#include <QColor>
#include <QElapsedTimer>
#include <QPixmap>
#include <QPointF>
#include <QWidget>

#include "marble_game.h"

class QHideEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QSequentialAnimationGroup;
class QShowEvent;
class QTimer;
class QVariantAnimation;

// Draws a marble solitaire board using frames from a sprite sheet, lets the
// player select and jump marbles, animates each jump, and plays a confetti
// or smoke effect when the game ends.
//
// All rules live in marble::Game; this class holds only the selection,
// animation and effect state. The board is sized to leave room around it for
// a ring of captured marbles. Note that paintEvent(), mousePressEvent(),
// showEvent() and hideEvent() keep Qt's lowerCamelCase spelling because they
// override virtual functions declared by QWidget.
class MarbleBoardWidget : public QWidget {
    Q_OBJECT

public:
    explicit MarbleBoardWidget(QWidget* parent = nullptr);

    // Sets up a fresh board, cancelling any selection, animation or effect.
    void StartGame(marble::BoardShape shape, marble::StartPosition start,
                   marble::BoardScale scale);

signals:
    // Emitted once when no marble can move any more.
    void GameOver(marble::Outcome outcome);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    // Frames of the sprite sheet, numbered left to right, then top to bottom.
    enum class Sprite {
        kEmpty = 0,
        kMarble = 1,
        kMovable = 2,
        kSelected = 3,
        kJumpable = 4,
        kTarget = 5,
    };

    // Which end-of-game effect is playing.
    enum class Effect { kNone, kConfetti, kSmoke };

    // One piece of confetti or puff of smoke. Positions and sizes are in spot
    // units relative to the board's centre, so they survive a resize.
    struct Particle {
        QPointF position;
        QPointF velocity;       // Spot units per second.
        double age = 0.0;       // Seconds.
        double lifetime = 1.0;  // Seconds.
        double size = 0.1;
        double rotation = 0.0;  // Degrees.
        double spin = 0.0;      // Degrees per second.
        QColor color;
    };

    // Where the board is drawn at the current widget size.
    struct Layout {
        double spot_side = 0.0;  // Pixels.
        QPointF center;          // Pixels; centre of the board and capture ring.
    };

    Layout ComputeLayout() const;

    // Offset of a spot's centre from the board's centre, in spot units (1.0 is
    // the width of one spot).
    QPointF SpotOffset(int row, int col) const;

    QPointF SpotCenter(const Layout& layout, int row, int col) const;
    QPointF CaptureSlotCenter(const Layout& layout, int index) const;

    // Finds the board spot under `point`. Returns false if there is none.
    bool FindSpotAt(const QPointF& point, marble::Position* position) const;

    // Chooses the sprite for a spot that is not being animated.
    Sprite RestingSprite(int row, int col) const;

    // Returns the selected marble's move that lands on `position`, or nullptr.
    const marble::Move* FindSelectedMoveTo(
        const marble::Position& position) const;

    // Returns true if the selected marble can jump the marble at `position`.
    bool IsJumpedBySelection(const marble::Position& position) const;

    void DrawSprite(QPainter* painter, Sprite sprite, const QPointF& center,
                    double side) const;
    void DrawParticles(QPainter* painter, const Layout& layout) const;

    void SelectMarble(const marble::Position& position);
    void ClearSelection();

    // Precomputes where each captured marble rests around the board.
    void BuildCaptureSlots();

    // Applies `move` to the game and animates it.
    void StartMoveAnimation(marble::Move move);
    void FinishMoveAnimation();

    // Ends the game and reports the outcome if no marble can move.
    void CheckForGameOver();

    // End-of-game effects.
    void StartEffect(marble::Outcome outcome);
    void StopEffects();
    void AdvanceEffects();
    void EmitConfetti(const QPointF& origin);
    void EmitSmoke(const QPointF& origin);

    marble::Game game_;
    QPixmap sprite_sheet_;

    // Selection.
    bool has_selection_ = false;
    marble::Position selected_;
    std::vector<marble::Move> selected_moves_;

    // Capture ring, as offsets from the board centre in spot units. Slot i
    // holds the i-th captured marble.
    std::vector<QPointF> capture_slots_;
    double extent_radius_ = 1.0;  // Spot units; the ring's outer edge.

    // Jump animation: the marble slides to its landing hole, then the jumped
    // marble slides out to its capture slot.
    QSequentialAnimationGroup* move_animation_ = nullptr;
    QVariantAnimation* slide_animation_ = nullptr;
    QVariantAnimation* exit_animation_ = nullptr;
    bool is_animating_ = false;
    marble::Move animated_move_;
    double slide_progress_ = 0.0;  // 0 to 1.
    double exit_progress_ = 0.0;   // 0 to 1.

    // Game over and its effect.
    bool is_game_over_ = false;
    Effect effect_ = Effect::kNone;
    std::vector<QPointF> emitters_;  // Spot units from the board centre.
    std::vector<Particle> particles_;
    double effect_time_ = 0.0;  // Seconds since the effect started.
    double emit_carry_ = 0.0;   // Fractional particles owed to the stream.
    QTimer* effect_timer_ = nullptr;
    QElapsedTimer frame_clock_;
};

#endif  // MARBLE_SOLITAIRE_SRC_MARBLE_BOARD_WIDGET_H_
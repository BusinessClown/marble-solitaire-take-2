#include "marble_board_widget.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <QColor>
#include <QDebug>
#include <QEasingCurve>
#include <QLineF>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QRectF>
#include <QSequentialAnimationGroup>
#include <QString>
#include <QVariant>
#include <QVariantAnimation>
#include <QHideEvent>
#include <QRandomGenerator>
#include <QShowEvent>
#include <QTimer>

namespace {

// Sprite sheet: a grid of square frames, added to the .qrc file under the
// prefix "/assets". Frames are numbered left to right, then top to bottom.
constexpr char kSpriteSheetPath[] = ":/assets/marble_sprites.png";
constexpr int kSpriteFrameSide = 64;  // Source pixels per frame.
constexpr int kSpriteColumns = 4;     // Frames per row.

// Smallest size at which the board still reads clearly, in pixels.
constexpr int kMinimumSide = 300;

// Capture ring, in spot units (1.0 is the width of one board spot).
constexpr double kCapturedSide = 0.5;   // Size of a resting captured marble.
constexpr double kCapturePitch = 0.55;  // Spacing along and between rings.

constexpr double kPi = 3.14159265358979323846;

// Animation timings, in milliseconds.
constexpr int kSlideDurationMs = 350;
constexpr int kExitDurationMs = 450;

QPointF Lerp(const QPointF& from, const QPointF& to, double t) {
    return from + (to - from) * t;
}

// End-of-game effects. Rates are per second; distances are in spot units.
constexpr int kEffectFrameIntervalMs = 16;
constexpr double kMaxFrameSeconds = 0.05;  // Caps the step after a stall.

constexpr int kConfettiBurstCount = 150;
constexpr double kConfettiRate = 60.0;
constexpr double kConfettiStreamSeconds = 3.0;
constexpr double kConfettiGravity = 12.0;
constexpr double kConfettiDrag = 1.2;

constexpr double kSmokeRate = 14.0;  // Per smoking marble.

double RandomBetween(double low, double high) {
    return low + (high - low) * QRandomGenerator::global()->generateDouble();
}
}  // namespace

MarbleBoardWidget::MarbleBoardWidget(QWidget* parent)
    : QWidget(parent),
    sprite_sheet_(QString::fromUtf8(kSpriteSheetPath)) {
    setMinimumSize(kMinimumSide, kMinimumSide);
    if (sprite_sheet_.isNull()) {
        qWarning() << "Could not load sprite sheet" << kSpriteSheetPath
                   << "- drawing placeholder circles instead.";
    }

    slide_animation_ = new QVariantAnimation(this);
    slide_animation_->setStartValue(0.0);
    slide_animation_->setEndValue(1.0);
    slide_animation_->setDuration(kSlideDurationMs);
    slide_animation_->setEasingCurve(QEasingCurve::InOutQuad);
    connect(slide_animation_, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) {
                slide_progress_ = value.toDouble();
                update();
            });

    exit_animation_ = new QVariantAnimation(this);
    exit_animation_->setStartValue(0.0);
    exit_animation_->setEndValue(1.0);
    exit_animation_->setDuration(kExitDurationMs);
    exit_animation_->setEasingCurve(QEasingCurve::InOutQuad);
    connect(exit_animation_, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) {
                exit_progress_ = value.toDouble();
                update();
            });

    move_animation_ = new QSequentialAnimationGroup(this);
    move_animation_->addAnimation(slide_animation_);
    move_animation_->addAnimation(exit_animation_);
    connect(move_animation_, &QSequentialAnimationGroup::finished, this,
            [this]() { FinishMoveAnimation(); });

    effect_timer_ = new QTimer(this);
    effect_timer_->setInterval(kEffectFrameIntervalMs);
    connect(effect_timer_, &QTimer::timeout, this,
            [this]() { AdvanceEffects(); });

    BuildCaptureSlots();
}

void MarbleBoardWidget::StartGame(marble::BoardShape shape,
                                  marble::StartPosition start,
                                  marble::BoardScale scale) {
    move_animation_->stop();
    is_animating_ = false;
    is_game_over_ = false;
    slide_progress_ = 0.0;
    exit_progress_ = 0.0;
    StopEffects();
    ClearSelection();

    game_.Reset(shape, start, scale);
    BuildCaptureSlots();
    update();
    CheckForGameOver();
}

void MarbleBoardWidget::BuildCaptureSlots() {
    // The ring must clear the farthest spot from the board's centre.
    double board_radius = 0.0;
    int hole_count = 0;
    for (int row = 0; row < game_.rows(); ++row) {
        for (int col = 0; col < game_.cols(); ++col) {
            if (game_.SpotAt(row, col) == marble::Spot::kOffBoard) {
                continue;
            }
            ++hole_count;
            const QPointF offset = SpotOffset(row, col);
            board_radius = std::max(
                board_radius, std::hypot(offset.x(), offset.y()) + 0.5);
        }
    }

    // At most every marble but the last can be captured. Fill rings outward
    // from the board, each as full as its circumference allows, starting at
    // the top and running clockwise.
    const std::size_t slot_count =
        hole_count > 0 ? static_cast<std::size_t>(hole_count - 1) : 0;
    capture_slots_.clear();
    double outer_radius = board_radius;
    for (int ring = 0; capture_slots_.size() < slot_count; ++ring) {
        const double radius = board_radius + kCapturedSide + ring * kCapturePitch;
        const int capacity =
            std::max(1, static_cast<int>(2.0 * kPi * radius / kCapturePitch));
        for (int i = 0; i < capacity && capture_slots_.size() < slot_count; ++i) {
            const double angle = -kPi / 2.0 + 2.0 * kPi * i / capacity;
            capture_slots_.push_back(
                QPointF(radius * std::cos(angle), radius * std::sin(angle)));
        }
        outer_radius = radius;
    }
    extent_radius_ = outer_radius + kCapturedSide / 2.0;
}

MarbleBoardWidget::Layout MarbleBoardWidget::ComputeLayout() const {
    Layout layout;
    layout.spot_side =
        std::min(width(), height()) / (2.0 * extent_radius_);
    layout.center = QPointF(width() / 2.0, height() / 2.0);
    return layout;
}

QPointF MarbleBoardWidget::SpotOffset(int row, int col) const {
    const double columns_per_spot = game_.columns_per_spot();
    const double board_width =
        (game_.cols() + game_.columns_per_spot() - 1) / columns_per_spot;
    return QPointF(col / columns_per_spot + 0.5 - board_width / 2.0,
                   row + 0.5 - game_.rows() / 2.0);
}

QPointF MarbleBoardWidget::SpotCenter(const Layout& layout, int row,
                                      int col) const {
    return layout.center + SpotOffset(row, col) * layout.spot_side;
}

QPointF MarbleBoardWidget::CaptureSlotCenter(const Layout& layout,
                                             int index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= capture_slots_.size()) {
        return layout.center;
    }
    return layout.center + capture_slots_[index] * layout.spot_side;
}

bool MarbleBoardWidget::FindSpotAt(const QPointF& point,
                                   marble::Position* position) const {
    const Layout layout = ComputeLayout();
    const double radius = layout.spot_side / 2.0;
    for (int row = 0; row < game_.rows(); ++row) {
        for (int col = 0; col < game_.cols(); ++col) {
            if (game_.SpotAt(row, col) == marble::Spot::kOffBoard) {
                continue;
            }
            if (QLineF(SpotCenter(layout, row, col), point).length() <= radius) {
                *position = marble::Position{row, col};
                return true;
            }
        }
    }
    return false;
}

const marble::Move* MarbleBoardWidget::FindSelectedMoveTo(
    const marble::Position& position) const {
    if (!has_selection_) {
        return nullptr;
    }
    for (const marble::Move& move : selected_moves_) {
        if (move.to == position) {
            return &move;
        }
    }
    return nullptr;
}

bool MarbleBoardWidget::IsJumpedBySelection(
    const marble::Position& position) const {
    if (!has_selection_) {
        return false;
    }
    for (const marble::Move& move : selected_moves_) {
        if (move.over == position) {
            return true;
        }
    }
    return false;
}

MarbleBoardWidget::Sprite MarbleBoardWidget::RestingSprite(int row,
                                                           int col) const {
    const marble::Position here{row, col};

    // The landing hole stays empty until the jumping marble has arrived. (The
    // game has already recorded the move, so it reports a marble there.)
    if (is_animating_ && here == animated_move_.to) {
        return Sprite::kEmpty;
    }

    if (game_.SpotAt(row, col) == marble::Spot::kEmpty) {
        return FindSelectedMoveTo(here) != nullptr ? Sprite::kTarget
                                                   : Sprite::kEmpty;
    }

    // From here on the spot holds a marble.
    if (is_animating_) {
        return Sprite::kMarble;
    }
    if (has_selection_) {
        if (here == selected_) {
            return Sprite::kSelected;
        }
        return IsJumpedBySelection(here) ? Sprite::kJumpable : Sprite::kMarble;
    }
    return game_.IsMovable(row, col) ? Sprite::kMovable : Sprite::kMarble;
}

void MarbleBoardWidget::DrawSprite(QPainter* painter, Sprite sprite,
                                   const QPointF& center, double side) const {
    const QRectF target(center.x() - side / 2.0, center.y() - side / 2.0, side,
                        side);

    if (sprite_sheet_.isNull()) {
        // Placeholder so the board is usable even without the sprite sheet.
        QColor color = Qt::white;
        switch (sprite) {
        case Sprite::kEmpty:
            color = Qt::white;
            break;
        case Sprite::kMarble:
            color = Qt::blue;
            break;
        case Sprite::kMovable:
            color = Qt::green;
            break;
        case Sprite::kSelected:
            color = QColor(255, 165, 0);
            break;
        case Sprite::kJumpable:
            color = Qt::red;
            break;
        case Sprite::kTarget:
            color = Qt::yellow;
            break;
        }
        painter->setPen(QPen(Qt::black, 1));
        painter->setBrush(color);
        painter->drawEllipse(target);
        return;
    }

    const int frame = static_cast<int>(sprite);
    const QRectF source((frame % kSpriteColumns) * kSpriteFrameSide,
                        (frame / kSpriteColumns) * kSpriteFrameSide,
                        kSpriteFrameSide, kSpriteFrameSide);
    painter->drawPixmap(target, sprite_sheet_, source);
}

void MarbleBoardWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const Layout layout = ComputeLayout();
    const double captured_side = kCapturedSide * layout.spot_side;

    for (int row = 0; row < game_.rows(); ++row) {
        for (int col = 0; col < game_.cols(); ++col) {
            if (game_.SpotAt(row, col) == marble::Spot::kOffBoard) {
                continue;
            }
            DrawSprite(&painter, RestingSprite(row, col),
                       SpotCenter(layout, row, col), layout.spot_side);
        }
    }

    // Captured marbles already at rest. While a jump is animating, the newest
    // one is still travelling and is drawn below instead.
    const int resting_captured =
        game_.captured_count() - (is_animating_ ? 1 : 0);
    for (int i = 0; i < resting_captured; ++i) {
        DrawSprite(&painter, Sprite::kMarble, CaptureSlotCenter(layout, i),
                   captured_side);
    }

    DrawParticles(&painter, layout);

    if (!is_animating_) {
        return;
    }

    // The jumped marble stays put while the jumper slides over it, then slides
    // out to its slot in the ring, shrinking as it goes. The jumper is drawn
    // last so it passes over the marble it jumps.
    const QPointF from = SpotCenter(layout, animated_move_.from.row,
                                    animated_move_.from.col);
    const QPointF over = SpotCenter(layout, animated_move_.over.row,
                                    animated_move_.over.col);
    const QPointF to =
        SpotCenter(layout, animated_move_.to.row, animated_move_.to.col);
    const QPointF slot =
        CaptureSlotCenter(layout, game_.captured_count() - 1);

    const double jumped_side =
        layout.spot_side + (captured_side - layout.spot_side) * exit_progress_;
    DrawSprite(&painter, Sprite::kMarble, Lerp(over, slot, exit_progress_),
               jumped_side);
    DrawSprite(&painter, Sprite::kMarble, Lerp(from, to, slide_progress_),
               layout.spot_side);
}

void MarbleBoardWidget::mousePressEvent(QMouseEvent* event) {
    if (is_animating_ || is_game_over_ || event->button() != Qt::LeftButton) {
        return;
    }

    marble::Position clicked;
    if (!FindSpotAt(QPointF(event->pos()), &clicked)) {
        ClearSelection();
        return;
    }

    // Clicking a landing hole performs that jump.
    const marble::Move* move = FindSelectedMoveTo(clicked);
    if (move != nullptr) {
        StartMoveAnimation(*move);
        return;
    }

    // Clicking a movable marble selects it; clicking the selected marble again
    // (or anything else) clears the selection.
    const bool is_selected = has_selection_ && clicked == selected_;
    if (!is_selected && game_.IsMovable(clicked.row, clicked.col)) {
        SelectMarble(clicked);
    } else {
        ClearSelection();
    }
}

void MarbleBoardWidget::SelectMarble(const marble::Position& position) {
    selected_ = position;
    selected_moves_ = game_.MovesFrom(position.row, position.col);
    has_selection_ = true;
    update();
}

void MarbleBoardWidget::ClearSelection() {
    has_selection_ = false;
    selected_moves_.clear();
    update();
}

void MarbleBoardWidget::StartMoveAnimation(marble::Move move) {
    if (!game_.PlayMove(move)) {
        ClearSelection();
        return;
    }

    // The game now has the origin and jumped holes empty, which is what the
    // widget draws under the moving marbles.
    animated_move_ = move;
    ClearSelection();
    is_animating_ = true;
    slide_progress_ = 0.0;
    exit_progress_ = 0.0;
    move_animation_->start();
    update();
}

void MarbleBoardWidget::FinishMoveAnimation() {
    is_animating_ = false;
    slide_progress_ = 0.0;
    exit_progress_ = 0.0;
    update();
    CheckForGameOver();
}

void MarbleBoardWidget::CheckForGameOver() {
    if (game_.HasMoves()) {
        return;
    }
    is_game_over_ = true;
    const marble::Outcome outcome = game_.FinalOutcome();
    StartEffect(outcome);
    emit GameOver(outcome);
}

void MarbleBoardWidget::StartEffect(marble::Outcome outcome) {
    // Confetti comes from the one marble left; smoke from each of the two.
    emitters_.clear();
    for (int row = 0; row < game_.rows(); ++row) {
        for (int col = 0; col < game_.cols(); ++col) {
            if (game_.SpotAt(row, col) == marble::Spot::kMarble) {
                emitters_.push_back(SpotOffset(row, col));
            }
        }
    }

    switch (outcome) {
    case marble::Outcome::kPerfectGame:
    case marble::Outcome::kWin:
        effect_ = Effect::kConfetti;
        break;
    case marble::Outcome::kTwoLeft:
        effect_ = Effect::kSmoke;
        break;
    default:
        effect_ = Effect::kNone;
        return;
    }

    effect_time_ = 0.0;
    emit_carry_ = 0.0;
    particles_.clear();
    if (effect_ == Effect::kConfetti && !emitters_.empty()) {
        for (int i = 0; i < kConfettiBurstCount; ++i) {
            EmitConfetti(emitters_.front());
        }
    }
    frame_clock_.start();
    effect_timer_->start();
}

void MarbleBoardWidget::StopEffects() {
    effect_ = Effect::kNone;
    emitters_.clear();
    particles_.clear();
    effect_timer_->stop();
}

void MarbleBoardWidget::AdvanceEffects() {
    const double seconds =
        std::min(kMaxFrameSeconds, frame_clock_.restart() / 1000.0);
    effect_time_ += seconds;

    if (effect_ == Effect::kConfetti) {
        // A burst at the start, then a stream for a few seconds.
        if (effect_time_ < kConfettiStreamSeconds && !emitters_.empty()) {
            emit_carry_ += kConfettiRate * seconds;
            for (; emit_carry_ >= 1.0; emit_carry_ -= 1.0) {
                EmitConfetti(emitters_.front());
            }
        }
    } else if (effect_ == Effect::kSmoke) {
        // Smoke keeps rising until the next game starts.
        emit_carry_ += kSmokeRate * seconds;
        for (; emit_carry_ >= 1.0; emit_carry_ -= 1.0) {
            for (const QPointF& emitter : emitters_) {
                EmitSmoke(emitter);
            }
        }
    }

    for (Particle& particle : particles_) {
        particle.age += seconds;
        particle.position += particle.velocity * seconds;
        particle.rotation += particle.spin * seconds;
        if (effect_ == Effect::kConfetti) {
            particle.velocity.ry() += kConfettiGravity * seconds;
            particle.velocity *= 1.0 - kConfettiDrag * seconds;
        }
    }
    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(),
                       [](const Particle& particle) {
                           return particle.age >= particle.lifetime;
                       }),
        particles_.end());

    if (effect_ == Effect::kConfetti &&
        effect_time_ >= kConfettiStreamSeconds && particles_.empty()) {
        StopEffects();
    }
    update();
}

void MarbleBoardWidget::EmitConfetti(const QPointF& origin) {
    const double angle = RandomBetween(0.0, 2.0 * kPi);
    const double speed = RandomBetween(4.0, 12.0);

    Particle particle;
    particle.position = origin;
    // The upward bias makes the burst arc over the board before falling.
    particle.velocity =
        QPointF(std::cos(angle) * speed, std::sin(angle) * speed - 3.0);
    particle.lifetime = RandomBetween(1.5, 3.0);
    particle.size = RandomBetween(0.12, 0.22);
    particle.rotation = RandomBetween(0.0, 360.0);
    particle.spin = RandomBetween(-540.0, 540.0);
    particle.color = QColor::fromHsv(
        static_cast<int>(RandomBetween(0.0, 359.0)), 200, 255);
    particles_.push_back(particle);
}

void MarbleBoardWidget::EmitSmoke(const QPointF& origin) {
    Particle particle;
    particle.position = origin + QPointF(RandomBetween(-0.15, 0.15), 0.0);
    particle.velocity =
        QPointF(RandomBetween(-0.3, 0.3), RandomBetween(-1.4, -0.8));
    particle.lifetime = RandomBetween(1.6, 2.6);
    particle.size = 0.3;
    particle.color = QColor(110, 110, 110);
    particles_.push_back(particle);
}

void MarbleBoardWidget::DrawParticles(QPainter* painter,
                                      const Layout& layout) const {
    for (const Particle& particle : particles_) {
        const double life = particle.age / particle.lifetime;  // 0 to 1.
        const QPointF center =
            layout.center + particle.position * layout.spot_side;
        const double side = particle.size * layout.spot_side;
        QColor color = particle.color;

        if (effect_ == Effect::kSmoke) {
            // Puffs swell and fade as they rise.
            const double radius = side * (1.0 + 1.5 * life) / 2.0;
            color.setAlphaF(0.45 * (1.0 - life));
            painter->setPen(Qt::NoPen);
            painter->setBrush(color);
            painter->drawEllipse(center, radius, radius);
        } else {
            // Confetti tumbles and fades over the last 30% of its life.
            color.setAlphaF(life > 0.7 ? (1.0 - life) / 0.3 : 1.0);
            painter->save();
            painter->translate(center);
            painter->rotate(particle.rotation);
            painter->fillRect(QRectF(-side / 2.0, -side / 4.0, side, side / 2.0),
                              color);
            painter->restore();
        }
    }
}

void MarbleBoardWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (effect_ != Effect::kNone) {
        frame_clock_.restart();
        effect_timer_->start();
    }
}

void MarbleBoardWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    effect_timer_->stop();  // No point animating while another page is shown.
}
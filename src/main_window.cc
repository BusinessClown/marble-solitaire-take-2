#include "main_window.h"

#include <QButtonGroup>
#include <QDebug>
#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSequentialAnimationGroup>
#include <QShowEvent>
#include <QSize>
#include <QStackedWidget>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVector>

#include "marble_board_widget.h"
#include "marble_game.h"
#include "tic_tac_toe_widget.h"

namespace {

// One selectable tile on the marble solitaire setup page.
struct TileOption {
    const char* icon_path;
    const char* label;
    int id;
};

// Keeps the result labels' space reserved while they are empty, so the board
// does not resize when a game ends.
constexpr int kResultLabelMinimumHeight = 30;

// Board shapes the player can choose. The icons are Qt resources; add the
// PNG files to a .qrc file with the prefix "/assets" so that they are
// compiled into the binary and work on any machine.
constexpr TileOption kBoardOptions[] = {
    {":/assets/french.png", "French", 0},
    {":/assets/jcwiegleb.png", "J.C. Wiegleb", 1},
    {":/assets/asymmetrical.png", "Asymmetrical", 2},
    {":/assets/english.png", "English", 3},
    {":/assets/diamond.png", "Diamond", 4},
    {":/assets/triangle.png", "Triangle", 5},
    };

// Where the single empty hole starts.
constexpr TileOption kStartPositionOptions[] = {
    {nullptr, "Standard", 0},
    {nullptr, "Random", 1},
    };

// How many times larger the board is than its standard form. The ids are the
// multipliers; see marble::BoardScale in marble_game.h.
constexpr TileOption kScaleOptions[] = {
    {nullptr, "Standard", 1},
    {nullptr, "2x", 2},
    {nullptr, "3x", 3},
    };

constexpr int kWindowWidth = 450;
constexpr int kWindowHeight = 550;
constexpr int kBoardIconSide = 128;
constexpr int kNoIcon = 0;  // Icon size for text-only tiles.

// Intro animation timings, in milliseconds.
constexpr int kIntroFadeDurationMs = 500;
constexpr int kIntroStaggerMs = 150;

// Continuous "breathing" animation on the greeting decoration.
constexpr int kGreetingPulseDurationMs = 1600;
constexpr double kGreetingPulseMinOpacity = 0.55;

constexpr char kGreetingLabelStyleSheet[] =
    "font-size: 20px; font-weight: bold; color: #2b6cb0;";
constexpr char kGreetingDecorationStyleSheet[] = "font-size: 32px;";

constexpr char kTileStyleSheet[] =
    "QToolButton { border: 2px solid transparent; border-radius: 8px;"
    " padding: 6px; }"
    "QToolButton:checked { border: 2px solid #3399ff;"
    " background:transparent; }";

// Builds one checkable tile and registers it with `group`. A null
// `option.icon_path` produces a text-only tile and `icon_side` is ignored.
QToolButton* CreateTile(const TileOption& option, int icon_side,
                        QButtonGroup* group, QWidget* parent) {
    QToolButton* button = new QToolButton(parent);
    button->setCheckable(true);
    button->setText(QString::fromUtf8(option.label));
    button->setAutoRaise(true);
    button->setStyleSheet(QString::fromUtf8(kTileStyleSheet));

    if (option.icon_path != nullptr) {
        button->setIcon(QIcon(QString::fromUtf8(option.icon_path)));
        button->setIconSize(QSize(icon_side, icon_side));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    } else {
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    group->addButton(button, option.id);
    return button;
}

// Gives `widget` its own QGraphicsOpacityEffect pinned to fully transparent.
// Called at page-construction time so nothing flashes at full opacity
// before the intro animation gets a chance to run.
void AttachHiddenOpacityEffect(QWidget* widget) {
    auto* effect = new QGraphicsOpacityEffect(widget);
    effect->setOpacity(0.0);
    widget->setGraphicsEffect(effect);
}

// Builds a fade-in animation for `widget`, delayed by `delay_ms` before it
// starts. `widget` must already carry a QGraphicsOpacityEffect, e.g. from
// AttachHiddenOpacityEffect(). The returned animation is unparented; the
// caller is expected to hand it to a QAnimationGroup, which takes
// ownership of it.
QAbstractAnimation* BuildFadeInAnimation(QWidget* widget, int delay_ms) {
    auto* effect =
        qobject_cast<QGraphicsOpacityEffect*>(widget->graphicsEffect());
    Q_ASSERT(effect != nullptr);

    auto* fade = new QPropertyAnimation(effect, "opacity");
    fade->setDuration(kIntroFadeDurationMs);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);

    auto* sequence = new QSequentialAnimationGroup;
    sequence->addPause(delay_ms);
    sequence->addAnimation(fade);
    return sequence;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("Fun Fun Games"));
    resize(kWindowWidth, kWindowHeight);

    stacked_widget_ = new QStackedWidget(this);
    setCentralWidget(stacked_widget_);

    // The order of these calls must match the values of Page.
    stacked_widget_->addWidget(CreateMainPage());
    stacked_widget_->addWidget(CreateChooseFirstPlayerPage());
    stacked_widget_->addWidget(CreateTicTacToePage());
    stacked_widget_->addWidget(CreateMarbleBoardPage());
    stacked_widget_->addWidget(CreateMarbleGamePage());

    ShowPage(Page::kMain);

    qDebug() << "MainWindow constructed. Qt version:" << qVersion();
}

MainWindow::~MainWindow() = default;

QWidget* MainWindow::CreateMainPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);

    greeting_decoration_label_ = new QLabel(tr("🎲 🎮 ✨"), page);
    greeting_decoration_label_->setAlignment(Qt::AlignCenter);
    greeting_decoration_label_->setStyleSheet(
        QString::fromUtf8(kGreetingDecorationStyleSheet));

    greeting_label_ = new QLabel(tr("Welcome to Fun Fun Games!"), page);
    greeting_label_->setAlignment(Qt::AlignCenter);
    greeting_label_->setStyleSheet(
        QString::fromUtf8(kGreetingLabelStyleSheet));

    play_tic_tac_toe_button_ = new QPushButton(tr("Play Tic-Tac-Toe"), page);
    play_tic_tac_toe_button_->setStyleSheet("font-size: 14px; padding: 8px;");

    play_marble_button_ =
        new QPushButton(tr("Start Marble Solitaire Game"), page);
    play_marble_button_->setStyleSheet("font-size: 14px; padding: 8px;");

    // Each of these starts fully transparent; PlayIntroAnimation() fades
    // them in once the window is first shown.
    AttachHiddenOpacityEffect(greeting_decoration_label_);
    AttachHiddenOpacityEffect(greeting_label_);
    AttachHiddenOpacityEffect(play_tic_tac_toe_button_);
    AttachHiddenOpacityEffect(play_marble_button_);

    layout->addStretch();
    layout->addWidget(greeting_decoration_label_);
    layout->addWidget(greeting_label_);
    layout->addSpacing(20);
    layout->addWidget(play_tic_tac_toe_button_);
    layout->addWidget(play_marble_button_);

    connect(play_tic_tac_toe_button_, &QPushButton::clicked, this,
            &MainWindow::ShowChooseFirstPlayerPage);
    connect(play_marble_button_, &QPushButton::clicked, this,
            &MainWindow::ShowMarbleBoardPage);

    return page;
}

QWidget* MainWindow::CreateChooseFirstPlayerPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);

    QLabel* title = new QLabel(tr("Who goes first?"), page);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 22px; font-weight: bold;");

    choose_x_button_ = new QPushButton(tr("X goes first"), page);
    choose_o_button_ = new QPushButton(tr("O goes first"), page);
    choose_x_button_->setStyleSheet("font-size: 16px; padding: 12px;");
    choose_o_button_->setStyleSheet("font-size: 16px; padding: 12px;");

    // The stretches above and below centre the content vertically.
    layout->addStretch();
    layout->addWidget(title);
    layout->addSpacing(20);
    layout->addWidget(choose_x_button_);
    layout->addWidget(choose_o_button_);
    layout->addStretch();

    connect(choose_x_button_, &QPushButton::clicked, this,
            &MainWindow::StartGameWithX);
    connect(choose_o_button_, &QPushButton::clicked, this,
            &MainWindow::StartGameWithO);

    return page;
}

QWidget* MainWindow::CreateTicTacToePage() {
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);

    QLabel* title = new QLabel(tr("Tic-Tac-Toe"), page);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 20px; font-weight: bold;");

    tic_tac_toe_ = new TicTacToeWidget(page);

    game_status_label_ = new QLabel(tr("X's turn"), page);
    game_status_label_->setAlignment(Qt::AlignCenter);
    game_status_label_->setStyleSheet(
        "font-size: 16px; font-weight: bold; padding: 6px;");

    game_back_button_ = new QPushButton(tr("Back"), page);
    game_reset_button_ = new QPushButton(tr("Reset Board"), page);

    QHBoxLayout* button_layout = new QHBoxLayout;
    button_layout->addWidget(game_back_button_);
    button_layout->addWidget(game_reset_button_);

    layout->addWidget(title);
    layout->addWidget(tic_tac_toe_);
    layout->addWidget(game_status_label_);
    layout->addLayout(button_layout);

    connect(game_back_button_, &QPushButton::clicked, this,
            &MainWindow::ShowMainPage);

    // A mid-game reset always gives the first move back to X.
    connect(game_reset_button_, &QPushButton::clicked, this,
            [this]() { tic_tac_toe_->ResetBoard(true); });

    connect(tic_tac_toe_, &TicTacToeWidget::StatusChanged, game_status_label_,
            &QLabel::setText);

    return page;
}

QWidget* MainWindow::CreateMarbleBoardPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);

    QLabel* title = new QLabel(tr("Board and Position Selection"), page);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 22px; font-weight: bold;");

    QLabel* board_label = new QLabel(tr("Choose a board:"), page);
    board_label->setAlignment(Qt::AlignCenter);

    board_group_ = new QButtonGroup(page);
    board_group_->setExclusive(true);

    QHBoxLayout* board_layout = new QHBoxLayout;
    for (const TileOption& option : kBoardOptions) {
        board_layout->addWidget(
            CreateTile(option, kBoardIconSide, board_group_, page));
    }
    board_group_->button(0)->setChecked(true);

    QLabel* start_label = new QLabel(tr("Starting position:"), page);
    start_label->setAlignment(Qt::AlignCenter);

    start_position_group_ = new QButtonGroup(page);
    start_position_group_->setExclusive(true);

    QHBoxLayout* start_layout = new QHBoxLayout;
    for (const TileOption& option : kStartPositionOptions) {
        start_layout->addWidget(
            CreateTile(option, kNoIcon, start_position_group_, page));
    }
    start_position_group_->button(0)->setChecked(true);

    QLabel* scale_label = new QLabel(tr("Board size:"), page);
    scale_label->setAlignment(Qt::AlignCenter);

    scale_group_ = new QButtonGroup(page);
    scale_group_->setExclusive(true);

    QHBoxLayout* scale_layout = new QHBoxLayout;
    for (const TileOption& option : kScaleOptions) {
        scale_layout->addWidget(
            CreateTile(option, kNoIcon, scale_group_, page));
    }
    scale_group_->button(1)->setChecked(true);

    marble_back_button_ = new QPushButton(tr("Back"), page);
    QHBoxLayout* button_layout = new QHBoxLayout;
    button_layout->addWidget(marble_back_button_);

    marble_start_button_ = new QPushButton(tr("Start Game"), page);
    button_layout->addWidget(marble_start_button_);

    layout->addWidget(title);
    layout->addSpacing(10);
    layout->addWidget(board_label);
    layout->addLayout(board_layout);
    layout->addSpacing(20);
    layout->addWidget(start_label);
    layout->addLayout(start_layout);
    layout->addSpacing(20);
    layout->addSpacing(20);
    layout->addWidget(scale_label);
    layout->addLayout(scale_layout);
    layout->addLayout(button_layout);
    layout->addStretch();

    connect(marble_back_button_, &QPushButton::clicked, this,
            &MainWindow::ShowMainPage);

    connect(marble_start_button_, &QPushButton::clicked, this,
            &MainWindow::ShowMainPage);

    connect(marble_start_button_, &QPushButton::clicked, this,
            &MainWindow::StartMarbleGame);

    return page;
}

QWidget* MainWindow::CreateMarbleGamePage() {
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);

    QLabel* title = new QLabel(tr("Marble Solitaire"), page);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 20px; font-weight: bold;");

    marble_board_ = new MarbleBoardWidget;

    marble_board_ = new MarbleBoardWidget(page);

    marble_game_back_button_ = new QPushButton(tr("Back"), page);

    marble_result_headline_label_ = new QLabel(page);
    marble_result_headline_label_->setAlignment(Qt::AlignCenter);
    marble_result_headline_label_->setStyleSheet(
        "font-size: 20px; font-weight: bold;");
    marble_result_headline_label_->setMinimumHeight(kResultLabelMinimumHeight);

    marble_result_detail_label_ = new QLabel(page);
    marble_result_detail_label_->setAlignment(Qt::AlignCenter);
    marble_result_detail_label_->setStyleSheet("font-size: 14px;");
    marble_result_detail_label_->setMinimumHeight(kResultLabelMinimumHeight);

    layout->addWidget(title);
    layout->addWidget(marble_board_, 1);
    layout->addWidget(marble_board_, 1);
    layout->addWidget(marble_result_headline_label_);
    layout->addWidget(marble_result_detail_label_);
    layout->addWidget(marble_game_back_button_);
    layout->addWidget(marble_game_back_button_);

    // Back returns to the setup page so a different board can be chosen.
    connect(marble_game_back_button_, &QPushButton::clicked, this,
            &MainWindow::ShowMarbleBoardPage);

    connect(marble_board_, &MarbleBoardWidget::GameOver, this,
            &MainWindow::ShowMarbleResult);

    return page;

}

void MainWindow::StartMarbleGame() {
    marble_result_headline_label_->clear();
    marble_result_detail_label_->clear();

    // The button group ids are the enum values; see marble_game.h.
    marble_board_->StartGame(
        static_cast<marble::BoardShape>(board_group_->checkedId()),
        static_cast<marble::StartPosition>(start_position_group_->checkedId()),
        static_cast<marble::BoardScale>(scale_group_->checkedId()));
    ShowPage(Page::kMarbleGame);
}

void MainWindow::ShowMarbleResult(marble::Outcome outcome) {
    QString headline;
    QString detail;
    switch (outcome) {
    case marble::Outcome::kPerfectGame:
        headline = tr("Perfect Game");
        detail = tr("You are an absolute Genius");
        break;
    case marble::Outcome::kWin:
        headline = tr("You Won");
        detail = tr("You sure are Brilliant");
        break;
    case marble::Outcome::kTwoLeft:
        headline = tr("Close But No Cigar");
        detail = tr("You're pretty smart");
        break;
    case marble::Outcome::kThreeLeft:
        headline = tr("Almost Had it");
        detail = tr("You gonna give up here?");
        break;
    case marble::Outcome::kFourLeft:
        headline = tr("Were you even trying?");
        detail = tr("Come on try next time");
        break;
    case marble::Outcome::kFiveOrMoreLeft:
        headline = tr("Is your name Jack?");
        detail = tr("Why because your @$$ at this!");
        break;
    }
    marble_result_headline_label_->setText(headline);
    marble_result_detail_label_->setText(detail);
}

void MainWindow::ShowPage(Page page) {
    stacked_widget_->setCurrentIndex(static_cast<int>(page));
    qDebug() << "Switched to page" << static_cast<int>(page);
}

void MainWindow::ShowMainPage() { ShowPage(Page::kMain); }

void MainWindow::ShowChooseFirstPlayerPage() {
    ShowPage(Page::kChooseFirstPlayer);
}

void MainWindow::ShowMarbleBoardPage() { ShowPage(Page::kMarbleBoard); }

void MainWindow::StartGameWithX() {
    tic_tac_toe_->ResetBoard(true);
    ShowPage(Page::kTicTacToe);
}

void MainWindow::StartGameWithO() {
    tic_tac_toe_->ResetBoard(false);
    ShowPage(Page::kTicTacToe);
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (!has_played_intro_animation_) {
        has_played_intro_animation_ = true;
        PlayIntroAnimation();
    }
}

void MainWindow::PlayIntroAnimation() {
    // Each widget fades in kIntroStaggerMs after the one before it, so the
    // page builds itself up piece by piece instead of popping in all at
    // once.
    auto* group = new QParallelAnimationGroup(this);
    group->addAnimation(
        BuildFadeInAnimation(greeting_decoration_label_, 0));
    group->addAnimation(
        BuildFadeInAnimation(greeting_label_, kIntroStaggerMs));
    group->addAnimation(
        BuildFadeInAnimation(play_tic_tac_toe_button_, kIntroStaggerMs * 2));
    group->addAnimation(
        BuildFadeInAnimation(play_marble_button_, kIntroStaggerMs * 3));

    // Once every widget has finished fading in, start the decoration's
    // continuous pulse.
    connect(group, &QParallelAnimationGroup::finished, this,
            &MainWindow::StartGreetingPulse);

    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::StartGreetingPulse() {
    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(
        greeting_decoration_label_->graphicsEffect());
    if (effect == nullptr) {
        return;
    }

    auto* pulse = new QPropertyAnimation(effect, "opacity", this);
    pulse->setDuration(kGreetingPulseDurationMs);
    pulse->setStartValue(1.0);
    pulse->setKeyValueAt(0.5, kGreetingPulseMinOpacity);
    pulse->setEndValue(1.0);
    pulse->setEasingCurve(QEasingCurve::InOutSine);
    pulse->setLoopCount(-1);  // Repeats for as long as the window is open.
    pulse->start(QAbstractAnimation::DeleteWhenStopped);
}


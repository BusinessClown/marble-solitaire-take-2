#ifndef MARBLE_SOLITAIRE_SRC_MAIN_WINDOW_H_
#define MARBLE_SOLITAIRE_SRC_MAIN_WINDOW_H_

#include <QMainWindow>
#include "marble_game.h"

class QButtonGroup;
class QLabel;
class QPushButton;
class QShowEvent;
class QStackedWidget;
class QWidget;
class MarbleBoardWidget;

class TicTacToeWidget;

// Top-level application window.
//
// The window owns a QStackedWidget holding one page per screen; see Page for
// the order in which they are added. Every child widget is parented, so Qt
// destroys them along with the window and no manual cleanup is needed.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    // Plays the one-time intro animation the first time the window becomes
    // visible. Qt can call showEvent() again later (e.g. after minimizing),
    // so has_played_intro_animation_ guards against replaying it.
    void showEvent(QShowEvent* event) override;

private:
    // Index of each page inside stacked_widget_, in the order the pages are
    // added by the constructor.
    enum class Page {
        kMain = 0,
        kChooseFirstPlayer = 1,
        kTicTacToe = 2,
        kMarbleBoard = 3,
        kMarbleGame = 4,
    };

private slots:
    // Navigation.
    void ShowMainPage();
    void ShowChooseFirstPlayerPage();
    void ShowMarbleBoardPage();

    // "Who goes first?" page.
    void StartGameWithX();
    void StartGameWithO();

    void StartMarbleGame();

    void ShowMarbleResult(marble::Outcome outcome);


private:
    // Page builders. Each is called once from the constructor and returns a
    // widget that the stacked widget takes ownership of.
    QWidget* CreateMainPage();
    QWidget* CreateChooseFirstPlayerPage();
    QWidget* CreateTicTacToePage();
    QWidget* CreateMarbleBoardPage();
    QWidget* CreateMarbleGamePage();

    // Shows one page and logs the transition.
    void ShowPage(Page page);

    // Fades in the greeting, decoration and both buttons on the main page in
    // a staggered sequence. Called once from showEvent().
    void PlayIntroAnimation();

    // Starts the looping "breathing" opacity animation on
    // greeting_decoration_label_. Called once the intro animation finishes.
    void StartGreetingPulse();

    QStackedWidget* stacked_widget_ = nullptr;

    // Main page.
    QLabel* greeting_label_ = nullptr;
    QLabel* greeting_decoration_label_ = nullptr;
    QPushButton* play_tic_tac_toe_button_ = nullptr;
    QPushButton* play_marble_button_ = nullptr;

    // True once PlayIntroAnimation() has run, so showEvent() does not
    // replay it on subsequent show events.
    bool has_played_intro_animation_ = false;

    // "Who goes first?" page.
    QPushButton* choose_x_button_ = nullptr;
    QPushButton* choose_o_button_ = nullptr;

    // Tic-tac-toe page.
    TicTacToeWidget* tic_tac_toe_ = nullptr;
    QLabel* game_status_label_ = nullptr;
    QPushButton* game_back_button_ = nullptr;
    QPushButton* game_reset_button_ = nullptr;

    // Marble solitaire setup page.
    QButtonGroup* board_group_ = nullptr;
    QButtonGroup* start_position_group_ = nullptr;
    QPushButton* marble_back_button_ = nullptr;
    QPushButton* marble_start_button_ = nullptr;
    QButtonGroup* scale_group_ = nullptr;

    // Marble solitaire game page.
    MarbleBoardWidget* marble_board_ = nullptr;
    QPushButton* marble_game_back_button_ = nullptr;
    QLabel* marble_result_headline_label_ = nullptr;
    QLabel* marble_result_detail_label_ = nullptr;
};

#endif  // MARBLE_SOLITAIRE_SRC_MAIN_WINDOW_H_
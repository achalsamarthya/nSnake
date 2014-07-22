#include <Game/Game.hpp>
#include <Config/Globals.hpp>
#include <Misc/Utils.hpp>
#include <Interface/LayoutGame.hpp>
#include <Flow/InputManager.hpp>
#include <Game/BoardParser.hpp>

#include <stdlib.h>

// Options of the Pause Menu
enum NamesToEasilyIdentifyTheMenuItemsInsteadOfRawNumbers
{
	RESUME, QUIT_MENU, QUIT_GAME
};

Game::Game():
	layout(NULL),
	gameOver(false),
	score(NULL),
	highScore(NULL),
	isPaused(false),
	showPauseMenu(false),
	showHelp(false),
	pauseMenu(NULL),
	player(NULL),
	board(NULL),
	fruits(NULL)
{ }
Game::~Game()
{
	SAFE_DELETE(this->layout);
	SAFE_DELETE(this->score);
	SAFE_DELETE(this->pauseMenu);
	SAFE_DELETE(this->player);
	SAFE_DELETE(this->board);
	SAFE_DELETE(this->fruits);
}
void Game::start()
{
	// Cleaning things from the previous game (if any)
	SAFE_DELETE(this->layout);
	SAFE_DELETE(this->score);
	SAFE_DELETE(this->pauseMenu);
	SAFE_DELETE(this->player);
	SAFE_DELETE(this->board);
	SAFE_DELETE(this->fruits);

	this->userAskedToQuit     = false;
	this->userAskedToGoToMenu = false;
	this->gameOver            = false;

	// The interface
	this->layout = new LayoutGame(this, 80, 24);

	// Initializing the player and it's attributes
	this->score = new Score();
	this->score->level = Globals::Game::starting_level;

	// Creating the menu and adding each item
	this->pauseMenu = new Menu(1,
	                           1,
	                           this->layout->pause->getW() - 2,
	                           this->layout->pause->getH() - 2);

	MenuItem* item;

	item = new MenuItem("Resume", RESUME);
	this->pauseMenu->add(item);

	this->pauseMenu->addBlank();

	item = new MenuItem("Quit to Main Menu", QUIT_MENU);
	this->pauseMenu->add(item);

	item = new MenuItem("Quit Game", QUIT_GAME);
	this->pauseMenu->add(item);

	// the player!
	this->player = new Player(2, 2);

	// Defaults to large
	int boardw = 78;
	int boardh = 21;

	if (Globals::Game::board_size == Globals::Game::SMALL)
	{
		boardw = 40;
		boardh = 10;
	}
	else if (Globals::Game::board_size == Globals::Game::MEDIUM)
	{
		boardw = 55;
		boardh = 14;
	}

	this->board = BoardParser::load("tmp/level01.nsnake");

	if (! this->board)
	{
		// If something wrong happens when loading the
		// level, silently falls back to a default one
		//
		// TODO improve on this error handling - show
		//      a dialog, perhaps?

		this->board = new Board(boardw,
		                        boardh,
		                        ((Globals::Game::teleport) ?
		                         Board::TELEPORT :
		                         Board::SOLID));
	}

	if (Globals::Game::random_walls)
		this->board->randomlyFillExceptBy(this->player->getX(),
		                                  this->player->getY());

	// fruits beibeh
	this->fruits = new FruitManager(Globals::Game::fruits_at_once);
	this->fruits->update(this->player, this->board);

	// Starting timers
	this->timerSnake.start();
	this->timer.start();
}
void Game::handleInput()
{
	if (InputManager::noKeyPressed())
		return;

	// The only two absolute inputs are to quit and pause.
	// Others depend if the game is paused or not.
	if (InputManager::isPressed("quit"))
	{
		this->userAskedToQuit = true;
	}
	else if (InputManager::isPressed("pause"))
	{
		(this->isPaused) ?
			this->pause(false) :
			this->pause(true);

		return;
	}
	else if (InputManager::isPressed('\n') ||
	         InputManager::isPressed(KEY_ENTER))
	{
		if (! this->isPaused)
		{
			this->pause(true);
			return;
			// This needs to be here otherwise
			// ENTER goes to the menu and immediately
			// unpauses the game.
		}
	}
	else if (InputManager::isPressed("help"))
	{
		// Toggling Pause and Help window
		if (this->isPaused)
		{
			this->showHelp = false;
			this->timer.unpause();
			this->timerSnake.unpause();
		}
		else
		{
			this->showHelp = true;
			this->timer.pause();
			this->timerSnake.pause();
		}
	}

	// Other keys are not used when paused.
	if (this->isPaused || this->showHelp)
	{
		this->pauseMenu->handleInput();
		return;
	}

	if (InputManager::isPressed("left"))
	{
		this->player->move(Player::LEFT);
	}
	else if (InputManager::isPressed("right"))
	{
		this->player->move(Player::RIGHT);
	}
	else if (InputManager::isPressed("up"))
	{
		this->player->move(Player::UP);
	}
	else if (InputManager::isPressed("down"))
	{
		this->player->move(Player::DOWN);
	}
}
void Game::update()
{
	if (this->gameOver)
		return;

	// If we're paused, only handle the menu.
	if (this->isPaused)
	{
		if (this->pauseMenu->willQuit())
		{
			int option = this->pauseMenu->currentID();

			switch(option)
			{
			case RESUME:
				this->pause(false);
				break;

			case QUIT_MENU:
				this->userAskedToGoToMenu = true;
				break;

			case QUIT_GAME:
				this->userAskedToQuit = true;
				break;
			}
			this->pauseMenu->reset();
		}
		return;
	}

	// Forcing Snake to move if enough time has passed
	// (time based on current level)
	this->timerSnake.pause();
	int delta = this->getDelay(this->score->level);

	if (this->timerSnake.delta_ms() >= delta)
	{
		// Checking if on the previous frame
		// the Snake died.
		if (! this->player->isAlive())
		{
			this->gameOver = true;

			if (this->score->points > Globals::Game::highScore.points)
			{
				Globals::Game::highScore.points = this->score->points;
				Globals::Game::highScore.level = this->score->level;
			}
		}
		else
		{
			// Actually move the player
			this->player->update(this->board);

			while (this->fruits->eatenFruit(this->player))
			{
				this->player->increase();

				// Score formula is kinda random and
				// scattered all over this file.
				// TODO: Center it all on the Score class.
				this->score->points += this->score->level * 2;
			}

			this->fruits->update(this->player, this->board);
		}
		this->timerSnake.start();
	}
	else
		this->timerSnake.unpause();
}
void Game::draw()
{
	this->layout->draw(this->pauseMenu);
}
bool Game::isOver()
{
	return (this->gameOver);
}
bool Game::willQuit()
{
	return this->userAskedToQuit;
}
bool Game::willReturnToMenu()
{
	return this->userAskedToGoToMenu;
}
int Game::getDelay(int level)
{
	// returning delay in milliseconds
	if (level < 1) return 800;

	switch (level)
	{
	case 1:  return 800;
	case 2:  return 600;
	case 3:  return 500;
	case 4:  return 300;
	case 5:  return 200;
	case 6:  return 150;
	case 7:  return 125;
	case 8:  return 100;
	case 9:  return 80;
	case 10: return 50;
	}
	return 50;
}
void Game::pause(bool option)
{
	if (option)
	{
		if (this->isPaused)
			return;

		this->isPaused = true;
		this->showPauseMenu = true;
		this->timer.pause();
		this->timerSnake.pause();
	}
	else
	{
		if (! this->isPaused)
			return;

		this->isPaused = false;
		this->showPauseMenu = false;
		this->timer.unpause();
		this->timerSnake.unpause();
	}
}


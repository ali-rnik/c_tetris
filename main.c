#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>

#define CTRL_C 3

enum Direction {
	LEFT,
	RIGHT,
	DOWN,
	ROTATE,
};

struct Coardinate {
	int x;
	int y;
};

struct Piece {
	struct Coardinate center;
	struct Coardinate tile[4];
	int tiles_count;
};

struct Player {
	char name[20];
	int current_score;
	int last_score;
	int top_score;
};

struct Game {
	struct Piece pieces[5];
	struct Player player;
	char board[15][15];
	int height;
	int width;
	int speed;
	int pieces_count;

	int board_mid;
};

void start();
void piece_rotate();
int piece_move(enum Direction dir);
int piece_insert();
void piece_delete();
void piece_install();
int check();
void merge_and_score();
int roll_dice(int pieces_count);
void* game_engine();
void board_show();

int sig = 65;
struct Piece cur_piece;
int game_finished = 0;
pthread_mutex_t mutex_ge;
struct Piece tree, corn;
struct Game game;
struct Player player;

int main()
{
	strcpy(player.name, "default_player");
	player.current_score = 0;

	tree.tiles_count = 3;
	tree.center.x = 0, tree.center.y = 0;
	tree.tile[0].x = -1 + tree.center.x, tree.tile[0].y = 0 + tree.center.y;
	tree.tile[1].x = 0 + tree.center.x, tree.tile[1].y = 1 + tree.center.y;
	tree.tile[2].x = 1 + tree.center.x, tree.tile[2].y = 0 + tree.center.y;

	corn.tiles_count = 2;
	corn.center.x = 0, corn.center.y = 0;
	corn.tile[0].x = 0 + corn.center.x, corn.tile[0].y = 1 + corn.center.y;
	corn.tile[1].x = 1 + corn.center.x, corn.tile[1].y = 0 + corn.center.y;

	game.pieces[0] = tree;
	game.pieces[1] = corn;
	game.pieces_count = 2;
	game.height = 15;
	game.width = 10;
	game.speed = 1;
	for (int i = 0; i < game.height; i++) {
		for (int j = 0; j < game.width; j++) {
			game.board[i][j] = '-';
		}
	}

	start(game);
}

void start()
{
	game.board_mid = game.width/2 - 1;

	pthread_t ge_thread;
	pthread_create(&ge_thread, NULL, game_engine, NULL);

	static struct termios oldt, newt;
	tcgetattr( STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON);
	tcsetattr( STDIN_FILENO, TCSANOW, &newt);

	while ((sig = getchar()) != CTRL_C) {
		if (sig == 'h') {
			piece_move(LEFT);
			board_show();
		}
		else if (sig == 'l') {
			piece_move(RIGHT);
			board_show();
		}
		else if (sig == 'j') {
			piece_move(ROTATE);
			board_show();
		}
	}

	tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
	pthread_join(ge_thread, NULL);
}

int piece_insert()
{
	int piece_num = roll_dice(game.pieces_count);
	cur_piece = game.pieces[piece_num];

	cur_piece.center.x = game.board_mid;

	if (!check(game))
		return 0;

	piece_install();
	return 1;
}

void piece_install()
{
	game.board[cur_piece.center.y][cur_piece.center.x] = '*';
	for (int i = 0; i < cur_piece.tiles_count; i++) {
		game.board[cur_piece.tile[i].y + cur_piece.center.y]
			[cur_piece.tile[i].x + cur_piece.center.x] = '*';
	}
}

void piece_delete()
{
	game.board[cur_piece.center.y][cur_piece.center.x] = '-';
	for (int i = 0; i < cur_piece.tiles_count; i++) {
		game.board[cur_piece.tile[i].y + cur_piece.center.y]
			[cur_piece.tile[i].x + cur_piece.center.x] = '-';
	}
}

int roll_dice(int pieces_count)
{
	srand(time(NULL));
	int random = rand() % pieces_count;
	printf("%d", random);
	return random;
}

int check()
{
	int x = cur_piece.center.x;
	int y = cur_piece.center.y;

	if (y > game.height-1 || y < 0 || x > game.width-1 || x < 0)
		return 0;
	if (game.board[y][x] == '*')
		return 0;

	for (int i = 0; i < cur_piece.tiles_count; i++) {
		int ytile = cur_piece.tile[i].y;
		int xtile = cur_piece.tile[i].x;

		if (y + ytile > game.height-1 || y + ytile < 0 || 
			x + xtile > game.width-1 || x + xtile < 0)
			return 0;
		if (game.board[cur_piece.tile[i].y + y]
			[cur_piece.tile[i].x + x] == '*')
			return 0;
	}

	return 1;
}

void merge_and_score()
{
	for (int i = 0; i < game.width; i++)
		if (game.board[game.height-1][i] == '-')
			return;

	player.current_score += 2;
	for (int i = game.height-2; i >= 0; i--) {
		for (int j = 0; j < game.width; j++) {
			game.board[i+1][j] = game.board[i][j];
		}
	}
}

void* game_engine()
{
	while (1) {
		pthread_mutex_lock(&mutex_ge);
		if (!piece_insert(game)) {
			printf(":(\n");
			exit(0);
		}

		board_show();
		pthread_mutex_unlock(&mutex_ge);

		while (1) {
			sleep(1);
			pthread_mutex_lock(&mutex_ge);
			if (!piece_move(DOWN)) {
				merge_and_score();
				board_show();
				pthread_mutex_unlock(&mutex_ge);
				break;
			}
			board_show();
			pthread_mutex_unlock(&mutex_ge);
		}
	}
}

void board_show()
{
	printf("\e[1;1H\e[2J");
	printf("player score: %d\n", player.current_score);
	printf("player name: %s\n\n", player.name);
	for (int i = 0; i < game.height; i++) {
		for (int j = 0; j < game.width; j++) {
			printf("%c ", game.board[i][j]);
		}
		printf("\n");
	}
}

int piece_move(enum Direction dir)
{
	piece_delete();

	if (dir == DOWN)
		cur_piece.center.y += 1;
	if (dir == LEFT)
		cur_piece.center.x -= 1;
	if (dir == RIGHT)
		cur_piece.center.x += 1;
	if (dir == ROTATE) {
		
	}

	if (!check()) {
		if (dir == DOWN)
			cur_piece.center.y -= 1;
		if (dir == LEFT)
			cur_piece.center.x += 1;
		if (dir == RIGHT)
			cur_piece.center.x -= 1;
		piece_install();
		return 0;
	}

	piece_install();

	return 1;
}








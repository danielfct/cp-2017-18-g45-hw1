//
//  othello.c
//
//  Created by Bernardo Ferreira and Jo�o Louren�o on 22/09/17.
//  Copyright (c) 2017 DI-FCT-UNL. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h>
#include <cilk/cilk_api.h>
#include <cilk/cilk.h>

#define RED "\x1B[31m"
#define BLUE "\x1B[34m"
#define RESET "\x1B[0m"
#define TOPLEFT "\x1B[0;0H"
#define CLEAR "\x1B[2J"

#define E '-'
#define R 'R'
#define B 'B'

typedef struct
{
	int i;
	int j;
	char color;
	int right;
	int left;
	int up;
	int down;
	int up_right;
	int up_left;
	int down_right;
	int down_left;
	int heuristic;
} move;

enum print_mode
{
	normal,
	silent,
	colorize,
	timer
};

enum print_mode print_mode = normal;
int anim_mode = 0;
int board_size = 8;
int delay = 0;
char *threads = "1";

char **board;
move **available_moves;

struct timeval start_time, end_time;

void init_board()
{
	board = malloc(board_size * sizeof(char *));
	for (int i = 0; i < board_size; i++)
	{ // malloc vai ser executado em serie sempre
		board[i] = malloc(board_size * sizeof(char));
		memset(board[i], E, board_size);
	}
	board[board_size / 2 - 1][board_size / 2 - 1] = R;
	board[board_size / 2][board_size / 2 - 1] = B;
	board[board_size / 2 - 1][board_size / 2] = B;
	board[board_size / 2][board_size / 2] = R;
}

void init_available_moves()
{
	available_moves = malloc(board_size * sizeof(move));
	for (int i = 0; i < board_size; i++)
	{ // malloc vai ser sempre executado em serie
		available_moves[i] = malloc(board_size * sizeof(move));
	}
}

void init_move(move *m, int i, int j, char color)
{
	m->i = i;
	m->j = j;
	m->color = color;
	m->right = 0;
	m->left = 0;
	m->up = 0;
	m->down = 0;
	m->up_right = 0;
	m->up_left = 0;
	m->down_right = 0;
	m->down_left = 0;
	m->heuristic = 0;
}

bool valid_move(int i, int j)
{
	return i >= 0 && i < board_size && j >= 0 && j < board_size;
}

char opponent(char turn)
{
	switch (turn)
	{
	case R:
		return B;
	case B:
		return R;
	default:
		return -1;
	}
}

// Go through game board by lines to calculate points for both players
// pre-condition: game is finished
void get_score(int *player_red, int *player_blue)
{

	int score_red = 0, score_blue = 0;
	int i, j;
	for (i = 0; i < board_size; i++)
		for (j = 0; j < board_size; j++)
		{
			if (board[i][j] == R)
				score_red++;
			else
				score_blue++;
		}
	*player_red = score_red;
	*player_blue = score_blue;
}

long calc_time_elapsed()
{
	long secs_used = end_time.tv_sec - start_time.tv_sec;
	long millis_used = secs_used * 1000 + (end_time.tv_usec - start_time.tv_usec) / 1000;

	return millis_used;
}

void print_board()
{
	if (print_mode == silent || print_mode == timer)
		return;
	if (anim_mode)
		printf(TOPLEFT);

	for (int i = 0; i < board_size; i++)
	{
		for (int j = 0; j < board_size; j++)
		{
			const char c = board[i][j];
			if (print_mode == normal)
				printf("%c ", c);
			else if (print_mode == colorize)
			{
				if (c == R)
					printf("%s%c %s", RED, R, RESET);
				else if (c == B)
					printf("%s%c %s", BLUE, B, RESET);
				else
					printf("%s%c %s", "", E, "");
			}
		}
		printf("\n");
	}

	for (int i = 0; i < 2 * board_size; i++)
	{
		printf("=");
	}
	printf("\n");
}

void print_scores()
{
	int r, b;
	get_score(&r, &b);
	printf("score - red:%i blue:%i\n", r, b);
}

void print_timer()
{
	long elapsed = calc_time_elapsed();
	int r, b;
	get_score(&r, &b);
	printf("%d\t%s\t%lu\t%d\t%d\n", board_size, threads, elapsed, r, b);
}

void free_board()
{
	for (int i = 0; i < board_size; i++)
		free(board[i]);
	free(board);
}

void free_available_moves()
{
	for (int i = 0; i < board_size; i++)
		free(available_moves[i]);
	free(available_moves);
}

void finish_game()
{
	if (print_mode == timer)
		print_timer();
	else
	{
		if (print_mode != silent)
			print_board();
		print_scores();
	}

	free_board();
	free_available_moves();
}

void flip_direction(move *m, int inc_i, int inc_j)
{
	int i = m->i + inc_i;
	int j = m->j + inc_j;
	while (board[i][j] != m->color)
	{
		board[i][j] = m->color;
		i += inc_i;
		j += inc_j;
	}
}

void flip_board(move *m)
{
	board[m->i][m->j] = m->color;
	if (m->right)
		flip_direction(m, 1, 0); //right
	if (m->left)
		flip_direction(m, -1, 0); //left
	if (m->up)
		flip_direction(m, 0, -1); //up
	if (m->down)
		flip_direction(m, 0, 1); //down
	if (m->up_right)
		flip_direction(m, 1, -1); //up right
	if (m->up_left)
		flip_direction(m, -1, -1); //up left
	if (m->down_right)
		flip_direction(m, 1, 1); //down right
	if (m->down_left)
		flip_direction(m, -1, 1); //down left
}

int get_direction_heuristic(move *m, char opp, int inc_i, int inc_j)
{
	int heuristic = 0;
	int i = m->i + inc_i;
	int j = m->j + inc_j;
	char curr = opp;

	while (valid_move(i, j))
	{
		curr = board[i][j];
		if (curr != opp)
			break;
		heuristic++;
		i += inc_i;
		j += inc_j;
	}
	if (curr == m->color)
		return heuristic;
	else
		return 0;
}

void get_move(move *m)
{
	if (board[m->i][m->j] != E)
		return;
	char opp = opponent(m->color);
	int heuristic;

	heuristic = get_direction_heuristic(m, opp, 1, 0); //right
	if (heuristic > 0)
	{
		m->heuristic += heuristic;
		m->right = 1;
	}
	heuristic = get_direction_heuristic(m, opp, -1, 0); //left
	if (heuristic > 0)
	{
		m->heuristic += heuristic;
		m->left = 1;
	}
	heuristic = get_direction_heuristic(m, opp, 0, -1); //up
	if (heuristic > 0)
	{
		m->heuristic += heuristic;
		m->up = 1;
	}
	heuristic = get_direction_heuristic(m, opp, 0, 1); //down
	if (heuristic > 0)
	{
		m->heuristic += heuristic;
		m->down = 1;
	}
	heuristic = get_direction_heuristic(m, opp, 1, -1); //up right
	if (heuristic > 0)
	{
		m->heuristic += heuristic;
		m->up_right = 1;
	}
	heuristic = get_direction_heuristic(m, opp, -1, -1); //up left
	if (heuristic > 0)
	{
		m->heuristic += heuristic;
		m->up_left = 1;
	}
	heuristic = get_direction_heuristic(m, opp, 1, 1); //down right
	if (heuristic > 0)
	{
		m->heuristic += heuristic;
		m->down_right = 1;
	}
	heuristic = get_direction_heuristic(m, opp, -1, 1); //down left
	if (heuristic > 0)
	{
		m->heuristic += heuristic;
		m->down_left = 1;
	}
}

//finds the best move inside an array of moves
move getBestMoveInArray(move *m)
{
	move bestMove = m[0];
	for (int i = 1; i < board_size; i++)
	{
		if (m[i].heuristic > bestMove.heuristic)
			bestMove = m[i];
	}
	return bestMove;
}
//finds the best move inside an matrix of moves by finding the best on each array and comparing them
move getBestMove(move **m)
{
	move *_m = malloc(sizeof(move) * board_size);
	cilk_for(int i = 0; i < board_size; i++)
	{
		_m[i] = getBestMoveInArray(m[i]);
	}

	return getBestMoveInArray(_m);
}

//PLEASE PARALELIZE THIS FUNCTION
int make_move(char color)
{
	move best_move;
	best_move.heuristic = 0;

	//obter todos os moves
	cilk_for(int i = 0; i < board_size; i++)
	{
		for (int j = 0; j < board_size; j++)
		{
			init_move(&available_moves[i][j], i, j, color);
			get_move(&available_moves[i][j]);
		}
	}

	//ver qual o melhor move
	best_move = getBestMove(available_moves);

	if (best_move.heuristic > 0)
	{
		flip_board(&best_move);
		return true; //made a move
	}
	else
		return false; //no move to make
}

void help(const char *prog_name)
{
	printf("Usage: %s [-s] [-c] [-t] [-a] [-d <MILI_SECS>] [-b <BOARD_SIZE>] [-n <N_THREADS>]\n", prog_name);
	exit(1);
}

void set_prog_flags(int argc, char *argv[])
{
	char ch;
	while ((ch = getopt(argc, argv, "scatd:b:n:")) != -1)
	{
		switch (ch)
		{
		case 's':
			print_mode = silent;
			break;
		case 'a':
			anim_mode = 1;
			printf(CLEAR);
			break;
		case 'c':
			if (print_mode != silent)
				print_mode = colorize;
			break;
		case 'd':
			delay = atoi(optarg);
			if (delay < 0)
			{
				printf("Minimum delay is 0.\n");
				help(argv[0]);
			}
			break;
		case 'b':
			board_size = atoi(optarg);
			if (board_size < 4)
			{
				printf("Minimum board size is 4.\n");
				help(argv[0]);
			}
			break;
		case 'n':
			threads = optarg;
			if (atoi(threads) < 1)
			{
				printf("Minimum threads is 1.\n");
				help(argv[0]);
			}

			break;
		case 't':
			print_mode = timer;
			break;
		case '?':
		default:
			help(argv[0]);
		}
	}
}

int main(int argc, char *argv[])
{
	// start timer
	gettimeofday(&start_time, NULL);

	// set all program flags
	set_prog_flags(argc, argv);

	//set number of threads
	if (__cilkrts_set_param("nworkers", threads) != 0)
	{
		printf("Failed to set worker count.\n");
		exit(1);
	}

	// initialize necessary arrays
	init_board();
	init_available_moves();

	// main game loop which makes alternate moves between player red and player blue
	// the game stops when no more moves can be done for both players
	// red player has the first move
	bool can_move_r = true, can_move_b = true;
	for (char turn = R; can_move_r || can_move_b; turn = opponent(turn))
	{
		print_board();
		bool can_move = make_move(turn);
		if (turn == R)
		{
			can_move_r = can_move;
		}
		else
		{
			can_move_b = can_move;
		}
		usleep(delay * 1000);
	}

	// stop timer
	gettimeofday(&end_time, NULL);

	// print game score and free all allocated memory
	finish_game();
}

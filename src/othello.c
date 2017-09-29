//
//  othello.c
//
//  Created by Bernardo Ferreira and João Lourenço on 22/09/17.
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

#define RED     "\x1B[31m"
#define BLU     "\x1B[34m"
#define RESET   "\x1B[0m"
#define TOPLEFT "\x1B[0;0H"
#define CLEAR   "\x1B[2J"

#define E '-'
#define R 'R'
#define B 'B'

typedef struct {
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

enum print_mode { normal, silent, colorize, timer };

enum print_mode print_mode = normal;
int anim_mode = 0;
int board_size = 8;
int delay = 0;
int threads = 1;

char** board;

struct timeval start_time, end_time;


void init_board() {
	board = malloc(board_size * sizeof(char*));
	for (int i = 0; i < board_size; i++) {// malloc vai ser executado em serie sempre
		board[i] = malloc(board_size * sizeof(char));
		for (int j = 0; j < board_size; j++) //como e apenas uma atribuicao nao vale a pena
			board[i][j] = E;
	}
	board[board_size/2 - 1][board_size/2 - 1] = R;
	board[board_size/2][board_size/2 - 1] = B;
	board[board_size/2 - 1][board_size/2] = B;
	board[board_size/2][board_size/2] = R;
}

void init_move(move* m, int i, int j, char color) {
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

bool valid_move(int i, int j) {
	return i >= 0 && i < board_size && j >= 0 && j < board_size;
}

char opponent(char turn) {
	switch (turn) {
	case R:
		return B;
	case B:
		return R;
	default:
		return -1;
	}
}

// percorre o tabuleiro por linhas para calcular a pontua��o do jogador da dada cor
int score(char color) {
	//	int res = 0;
	//	for (int i = 0; i < board_size; i++) {
	//		for (int j = 0; j < board_size; j++) {
	//			if (board[i][j] == color)
	//				res++;
	//		}
	//	}
	//	return res;

	// TODO: n�o era melhor percorrer o tabuleiro apenas 1x e calcular logo a pontua��o de ambos os jogadores?
	int res = 0;
	if (board[0:board_size][0:board_size] == color)
		res++;
	return res;
}

long timeElapsed(struct timeval start, struct timeval end) {
	long secs_used = end.tv_sec - start.tv_sec; //avoid overflow by subtracting first
	long micros_used = ((secs_used * 1000000) + end.tv_usec) - (start.tv_usec);

	return micros_used;
}


void print_board() {
	if (anim_mode) {
		printf(TOPLEFT);
	}
	for (int i = 0; i < board_size; i++) {
		for (int j = 0; j < board_size; j++) {
			const char c = board[i][j];
			if (print_mode == normal)
				printf("%c ",c);
			else if (print_mode == colorize) {
				if (c == R)
					printf("%s%c %s", RED, R, RESET);
				else if (c == B)
					printf("%s%c %s", BLU, B, RESET);
				else
					printf("%s%c %s", "", E, "");
			}
		}
		printf("\n");
	}
	for (int i = 0; i < 2*board_size; i++)
		printf("=");
	printf("\n");
}

void print_scores() {
	int r =	score(R);
	int b =	score(B);
	printf("score - red:%i blue:%i\n", r, b);
}

void print_timer() {
	long elapsed = timeElapsed(start_time, end_time);
	int w =	score(R);
	int b =	score(B);
	printf("board size:%d\t num threads:%d\t time(ms):%lu\t red:%i\t blue:%i\n", board_size, threads, elapsed, w, b);
}

void free_board() {
	// cilk_for (int i = 0; i < board_size; i++)
	// 	free(board[i]);
	free(board[0:board_size]);
	free(board);
}

void finish_game() {
	if (print_mode == timer) {
		print_timer();
	}
	else {
		if (print_mode != silent) {
			print_board();
		}
		print_scores();
	}

	free_board();
}

void flip_direction(move* m, int inc_i, int inc_j) {
	int i = m->i + inc_i;
	int j = m->j + inc_j;
	while (board[i][j] != m->color) {
		board[i][j] = m->color;
		i += inc_i;
		j += inc_j;
	}
}

void flip_board(move* m) {
	board[m->i][m->j] = m->color;
	if (m->right)
		flip_direction(m,1,0); //right
	if (m->left)
		flip_direction(m,-1,0); //left
	if (m->up)
		flip_direction(m,0,-1); //up
	if (m->down)
		flip_direction(m,0,1); //down
	if (m->up_right)
		flip_direction(m,1,-1); //up right
	if (m->up_left)
		flip_direction(m,-1,-1); //up left
	if (m->down_right)
		flip_direction(m,1,1); //down right
	if (m->down_left)
		flip_direction(m,-1,1); //down left
}

int get_direction_heuristic(move* m, char opp, int inc_i, int inc_j) {
	int heuristic = 0;
	int i = m->i + inc_i;
	int j = m->j + inc_j;
	char curr = opp;

	while (valid_move(i,j)) {
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

void get_move(move* m) {
	if (board[m->i][m->j] != E)
		return;
	char opp = opponent(m->color);
	int heuristic;

	heuristic = get_direction_heuristic(m, opp, 1, 0); //right
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->right = 1;
	}
	heuristic = get_direction_heuristic(m, opp, -1, 0); //left
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->left = 1;
	}
	heuristic = get_direction_heuristic(m, opp, 0, -1); //up
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->up = 1;
	}
	heuristic = get_direction_heuristic(m, opp, 0, 1); //down
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->down = 1;
	}
	heuristic = get_direction_heuristic(m, opp, 1, -1); //up right
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->up_right = 1;
	}
	heuristic = get_direction_heuristic(m, opp, -1, -1); //up left
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->up_left = 1;
	}
	heuristic = get_direction_heuristic(m, opp, 1, 1); //down right
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->down_right = 1;
	}
	heuristic = get_direction_heuristic(m, opp, -1, 1); //down left
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->down_left = 1;
	}
}



move getBestMoveInArray(move* m, int i, int j){
	printf("%i	%i\n", i, j);	
	if(i>j)
		return m[i];

	int mid = (i+j)/2;
	move x, y;
	x = /*cilk_spawn*/ getBestMoveInArray(m, i, mid);
	y = getBestMoveInArray(m, mid+1, j);
	//cilk_sync;

	if(x.heuristic > y.heuristic)
		return x;
	return y;
}

move getBestMove(move** m){
	move* _m = malloc(sizeof(move) * board_size); 
	int i;
	/*cilk_*/for(i = 0; i < board_size; i++){
		_m[i] = getBestMoveInArray(m[i], 0, board_size-1);
	}
	
	return getBestMoveInArray(_m, 0, board_size-1);
}



//PLEASE PARALELIZE THIS FUNCTION
int make_move(char color) {
	int i, j;
	move best_move;
	move** m;
	best_move.heuristic = 0;

	//alocar o array de moves
	m = malloc(board_size * sizeof(move));
	for (int i = 0; i < board_size; i++) {// malloc vai ser executado em serie sempre
		m[i] = malloc(board_size * sizeof(move));
	}
	
	//obter todos os moves
	cilk_for (i = 0; i < board_size; i++) {
		cilk_for (j = 0; j < board_size; j++) {
			init_move(&m[i][j],i,j,color);
			get_move(&m[i][j]);
		}
	}

	//ver qual o melhor move
	best_move = getBestMove(m);
	/* for (i = 0; i < board_size; i++){
		for (j = 0; j < board_size; j++){
	 		if(m[i][j].heuristic > best_move.heuristic) 
				best_move = m[i][j];
		}
	} */

	printf("Best move: %i\n",best_move.heuristic);

	if (best_move.heuristic > 0) {
		flip_board(&best_move);
		return true;	//made a move
	} else 
		return false;	//no move to make
}




void help(const char* prog_name) {
	printf ("Usage: %s [-s] [-c] [-t] [-a] [-d <MILI_SECA>] [-b <BOARD_ZISE>] [-n <N_THREADS>]\n", prog_name);
	exit (1);
}

void get_flags(int argc, char * argv[]) {
	char ch;
	while ((ch = getopt(argc, argv, "scatd:b:n:")) != -1) {
		switch (ch) {
		case 's':
			print_mode = silent;
			break;
		case 'a':
			anim_mode = 1;
			break;
		case 'c':
			if (print_mode != silent)
				print_mode = colorize;
			break;
		case 'd':
			delay = atoi(optarg);
			if (delay < 0){
				printf("Minimum delay is 0.\n");
				help(argv[0]);
			}
			break;
		case 'b':
			board_size = atoi(optarg);
			if (board_size < 4) {
				printf("Minimum board size is 4.\n");
				help(argv[0]);
			}
			break;
		case 'n':
			threads = atoi(optarg);
			if (threads < 1) {
				printf("Minimum threads is 1.\n");
				help(argv[0]);
			}

			if (0!= __cilkrts_set_param("nworkers",optarg))
			   printf("Failed to set worker count\n");
			
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


int main (int argc, char * argv[]) {

    gettimeofday(&start_time, NULL);

	get_flags(argc, argv);
	// argc -= optind;
	// argv += optind;

	init_board();
	int cant_move_r = false, cant_move_b = false;
	char turn = R;

	if (anim_mode) {
		printf(CLEAR);
	}
	while (!cant_move_r && !cant_move_b) {
		if (print_mode != silent && print_mode != timer)
			print_board();
		int cant_move = !make_move(turn);
		if (cant_move) {
			if (turn == R)
				cant_move_r = true;
			else
				cant_move_b = true;
		} else {
			if (turn == R)
				cant_move_r = false;
			else
				cant_move_b = false;
		}
		turn = opponent(turn);
		usleep(delay * 1000);
	}

    gettimeofday(&end_time, NULL);

	finish_game();
}

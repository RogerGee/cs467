/* tictactoe.c - tic-tac-toe reinforcement learning program */
#include <dstructs/treemap.h>
#include <dstructs/dynarray.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* constants */
enum board_state
{
    board_won,
    board_complete,
    board_incomplete
};
enum move_result
{
    move_win,
    move_draw,
    move_good,
    move_bad
};
#define X 'X' /* X goes first: this is the human player */
#define O 'O' /* O goes second: this is the computer player */
#define EMPTY '.'

/* represent a decision made while playing the game */
struct decision
{
    int pos; /* position in board config to move */
    int worth; /* part of an overall probability distribution */
};
static struct decision* decision_new(int pos,int worth);
static void decision_init(struct decision* decision,int pos,int worth);

/* represent the game board */
typedef char gameboard[9];
static void gameboard_init(gameboard board);
static enum board_state gameboard_get_state(gameboard board,char player);
static enum move_result gameboard_would_move(gameboard board,char player,int pos);
static int gameboard_count_empty(gameboard board);
static void gameboard_print(gameboard board,int indentLevel);
static const gameboard EMPTY_BOARD = {EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY};

/* represent turn knowledge */
struct turn_node
{
    /* current state of the board; the opposing player played last to get
       to the current board state; it is our move from this state */
    gameboard board;

    /* list of decision structures (actions) */
    int sum; /* cache sum of their worth */
    int lastMove; /* hold onto last decision made */
    struct dynamic_array actions; /* of 'struct decision' */
};
static struct turn_node* turn_node_new(const gameboard board);
static struct turn_node* turn_node_new_fromnone();
static void turn_node_free(struct turn_node* node);
static struct decision* turn_node_make_move(struct turn_node* node);
static void turn_node_mark_good(struct turn_node* node);
static void turn_node_mark_bad(struct turn_node* node);
/*static void turn_node_cap(struct turn_node* node);*/
static void turn_node_print(struct turn_node* node);

/* encapsulate turn knowledge */
struct knowledge
{
    struct turn_node* first;
    struct turn_node* node;
    struct treemap* reactions;
};
static struct knowledge* knowledge_new(char player);
static void knowledge_free(struct knowledge* knowledge);

/* misc functions */
static int compar_board(const gameboard left,const gameboard right);

/* top-level operations */
static struct turn_node* get_move_reaction(struct treemap* reactions,const gameboard newboard);
static struct knowledge* aquire_knowledge();

int main(int argc,const char* argv[])
{
    bool won;
    size_t iter;
    gameboard board;
    struct dynamic_array moves;
    struct knowledge* knowledge;
    srand(time(NULL));

    knowledge = aquire_knowledge();
    dynamic_array_init(&moves);

    /* play against player */
    do {
        gameboard_init(board);
        while (true) {
            int x, y;
            struct turn_node* node;
            struct decision* decision;
            enum board_state state;
            fputs("your turn: ",stdout);
            while (scanf("%d %d",&x,&y) != 2)
                fputs("bad input, try again: ",stdout);
            x += y*3;
            if (x>=9 || board[x] != EMPTY) {
                puts("you cannot play there!");
                continue;
            }
            board[x] = X;
            state = gameboard_get_state(board,X);
            if (state == board_complete) {
                printf("it's a draw\n");
                won = true;
                break;
            }
            else if (state == board_won) {
                printf("you won!\n");
                won = false;
                break;
            }
            node = get_move_reaction(knowledge->reactions,board);
            dynamic_array_pushback(&moves,node);
            decision = turn_node_make_move(node);
            turn_node_print(node);
            printf("computer decision: {%d, %d} %d%%\n",decision->pos%3,decision->pos/3,(int)((float)decision->worth / node->sum * 100));
            board[decision->pos] = O;
            state = gameboard_get_state(board,O);
            if (state == board_complete) {
                printf("it's a draw\n");
                won = true;
                break;
            }
            else if (state == board_won) {
                printf("you lost!\n");
                won = true;
                break;
            }
            gameboard_print(board,0);
            putchar('\n');
            puts("--------------------");
        }
        gameboard_print(board,0);
        puts("\n--------------------");

        for (iter = 0;iter < moves.da_top;++iter) {
            if (won)
                turn_node_mark_good((struct turn_node*)moves.da_data[iter]);
            else
                turn_node_mark_bad((struct turn_node*)moves.da_data[iter]);
        }
        dynamic_array_clear(&moves);

        fputs("play again (y/n)? ",stdout);
        while (true) {
            char c;
            c = fgetc(stdin);
            if ( isspace(c) )
                continue;
            if (c == 'y')
                break;
            else
                goto done;
        }
    } while (true);
done:

    dynamic_array_delete(&moves);
    knowledge_free(knowledge);
    return 0;
    (void)argc;
    (void)argv;
}

/* 'struct decision' */
struct decision* decision_new(int pos,int worth)
{
    struct decision* decision;
    decision = malloc(sizeof(struct decision));
    decision_init(decision,pos,worth);
    return decision;
}
void decision_init(struct decision* decision,int pos,int worth)
{
    decision->pos = pos;
    decision->worth = worth;
}

/* 'gameboard' */
void gameboard_init(gameboard board)
{
    short i;
    for (i = 0;i < 9;++i)
        board[i] = EMPTY;
}
enum board_state gameboard_get_state(gameboard board,char player)
{
    int i;
    bool complete = true;
    for (i = 0;i < 9;++i) {
        if (board[i] == EMPTY) {
            complete = false;
            break;
        }
    }
    for (i = 0;i < 3;++i) {
        int j, k;
        bool success = true;
        /* horizontile */
        for (j = i*3,k = 0;k < 3;++j,++k) {
            if (board[j] != player) {
                success = false;
                break;
            }
        }
        if (success)
            return board_won;
        success = true;
        /* vertical */
        for (j = i,k = 0;k < 3;j+=3,++k) {
            if (board[j] != player) {
                success = false;
                break;
            }
        }
        if (success)
            return board_won;
    }
    /* diagonals */
    if (board[4]==player && ((board[0]==player && board[8]==player) || (board[2]==player && board[6]==player)))
        return board_won;
    return complete ? board_complete : board_incomplete;
}
enum move_result gameboard_would_move(gameboard board,char player,int pos)
{
    char config[9];
    enum board_state state;
    if (board[pos] != EMPTY)
        return move_bad;
    memcpy(config,board,9);
    config[pos] = player;
    state = gameboard_get_state(config,player);
    return state==board_won ? move_win : state==board_complete ? move_draw : move_good;
}
int gameboard_count_empty(gameboard board)
{
    int i;
    int c = 0;
    for (i = 0;i < 9;++i)
        if (board[i] == EMPTY)
            ++c;
    return c;
}
void gameboard_print(gameboard board,int indentLevel)
{
    int i, j, k, l;
    k = 0;
    for (i = 0;i < indentLevel;++i)
        putchar(' ');
    for (i = 0;i < 3;++i) {
        for (j = 0;j < 3;++j,++k)
            putchar(board[k]);
        if (i+1 < 3) {
            putchar('\n');
            for (l = 0;l < indentLevel;++l)
                putchar(' ');
        }
    }
}

/* 'struct turn_node' */
struct turn_node* turn_node_new(const gameboard board)
{
    int i, win;
    struct turn_node* node;
    node = malloc(sizeof(struct turn_node));
    memcpy(node->board,board,sizeof(gameboard));
    /* generate all possible decisions that we ourselves could make; if a
       winning move is possible, then choose it above all others; also if
       a winning move for the player is available then block it */
    node->sum = 0;
    node->lastMove = -1;
    dynamic_array_init_ex(&node->actions,9);
    win = -1; /* no winning move initially */
    for (i = 0;i < 9;++i) {
        if (node->board[i] == EMPTY) {
            struct decision* ndecision = decision_new(i,100);
            node->sum += 100;
            dynamic_array_pushback(&node->actions,ndecision);
            if (gameboard_would_move(node->board,O,ndecision->pos) == move_win)
                win = (int)node->actions.da_top - 1;
            else if (win == -1 && gameboard_would_move(node->board,X,ndecision->pos) == move_win)
                win = (int)node->actions.da_top - 1;
        }
    }
    if (win > -1) { /* make a winning move certain */
        for (i = 0;i < (int)node->actions.da_top;++i)
            if (win != i)
                ((struct decision*)node->actions.da_data[i])->worth = 0;
        ((struct decision*)node->actions.da_data[win])->worth = node->sum;
    }
    return node;
}
struct turn_node* turn_node_new_fromnone()
{
    struct turn_node* node;
    node = malloc(sizeof(struct turn_node));
    gameboard_init(node->board);
    dynamic_array_init_ex(&node->actions,9);
    node->sum = 0;
    node->lastMove = -1;
    return node;
}
void turn_node_free(struct turn_node* node)
{
    dynamic_array_delete_ex(&node->actions,free);
    free(node);
}
struct decision* turn_node_make_move(struct turn_node* node)
{
    /* randomly pick a move to make; the distribution will cause
       some moves to have greater probabilities of being chosen */
    int i = 1, j = 0;
    int r = rand() % node->sum + 1;
    while (true) {
        i += ((struct decision*)node->actions.da_data[j])->worth;
        if (r < i)
            break;
        ++j;
    }
    node->lastMove = j;
    return (struct decision*) node->actions.da_data[j];
}
void turn_node_mark_good(struct turn_node* node)
{
    /* the last move contributed to a win, so increase its probability */
    int i, cnt, take;
    struct decision* decision;
    decision = (struct decision*)node->actions.da_data[node->lastMove];
    take = ((int)node->actions.da_top-1) + 100;
    decision->worth += take;
    if (decision->worth > node->sum || decision->worth < 0)
        decision->worth = node->sum;
    i = 0; cnt = 1;
    while (cnt < (int)node->actions.da_top && take > 0) {
        if (i != node->lastMove) {
            decision = (struct decision*)node->actions.da_data[i];
            if (decision->worth <= 0)
                ++cnt;
            else {
                --decision->worth;
                --take;
            }
        }
        if (++i >= (int)node->actions.da_top)
            i = 0;
    }
}
void turn_node_mark_bad(struct turn_node* node)
{
    /* the last move contributed to a loss, so decrease its probability */
    int i, give;
    struct decision* decision;
    if (node->actions.da_top > 1) {
        decision = (struct decision*)node->actions.da_data[node->lastMove];
        give = ((int)node->actions.da_top-1) + 50;
        decision->worth -= give;
        if (decision->worth < 0) {
            give += decision->worth;
            decision->worth = 0;
        }
        i = 0;
        while (give > 0) {
            if (i != node->lastMove) {
                decision = (struct decision*)node->actions.da_data[i];
                ++decision->worth;
                --give;
            }
            if (++i >= (int)node->actions.da_top)
                i = 0;
        }
    }
}
/*void turn_node_cap(struct turn_node* node)
{
    size_t iter, high;
    struct decision** decisions;
    if (node->actions.da_top > 0) {
        decisions = (struct decision**)node->actions.da_data;
        high = 0;
        for (iter = 1;iter < node->actions.da_top;++iter)
            if (decisions[iter]->worth > decisions[high]->worth)
                high = iter;
        while (iter < node->actions.da_top) {
            if (iter != high && (float)decisions[high]->worth / node->sum - (float)decisions[iter]->worth/node->sum >= 0.80) {
                decisions[high]->worth += decisions[iter]->worth;
                decisions[iter]->worth = 0;
                iter = 0;
            }
            else
                ++iter;
        }
    }
}*/
void turn_node_print(struct turn_node* node)
{
    size_t iter;
    static struct decision* decision;
    /* gameboard_print(node->board,0); */
    /* putchar('\n'); */
    if (node->actions.da_top == 0)
        puts("NO CONTEXT");
    else {
        for (iter = 0;iter < node->actions.da_top;++iter) {
            decision = (struct decision*) node->actions.da_data[iter];
            printf("{%d,%d}: %d\n",decision->pos%3,decision->pos/3,decision->worth);
        }
    }
}

/* 'struct knowledge' */
struct knowledge* knowledge_new(char player)
{
    struct knowledge* knowledge;
    knowledge = malloc(sizeof(struct knowledge));
    knowledge->reactions = treemap_new((key_comparator)compar_board,(destructor)turn_node_free);
    if (player == X)
        knowledge->first = turn_node_new(EMPTY_BOARD);
    else
        knowledge->first = turn_node_new_fromnone();
    knowledge->node = knowledge->first;
    return knowledge;
}
void knowledge_free(struct knowledge* knowledge)
{
    turn_node_free(knowledge->first);
    treemap_free(knowledge->reactions);
    free(knowledge);
}


/* misc functions */
int compar_board(const gameboard left,const gameboard right)
{
    size_t i;
    for (i = 0;i < sizeof(gameboard);++i)
        if (left[i] < right[i])
            return -1;
        else if (left[i] > right[i])
            return 1;
    return 0;
}

/* top-level operations */
struct turn_node* get_move_reaction(struct treemap* reactions,const gameboard newboard)
{
    /* get turn node corresponding with specified board configuration */
    struct turn_node* nturn = NULL;
    nturn = treemap_lookup(reactions,newboard);
    /* if the node didn't preexist, then we create a new one and insert it */
    if (nturn == NULL) {
        /* create a new node for our next move and insert it into the treemap */
        nturn = turn_node_new(newboard);
        treemap_insert(reactions,nturn);
    }
    return nturn;
}

static char self_play_recursive(gameboard board,struct knowledge* active,struct knowledge* inactive,char turn)
{
    struct turn_node* node;
    static enum board_state state;
    static struct decision* decision;

    /* the active player gets to make a move */
    node = active->node; /* copy active player state */
    decision = turn_node_make_move(active->node);
    board[decision->pos] = turn;

    /* base case: the game is over (won or drawn) */
    state = gameboard_get_state(board,turn);
    if (state==board_won || state==board_complete) {
        turn_node_mark_good(node);
        return turn;
    }

    /* recursive case: the inactive player gets to go */
    inactive->node = get_move_reaction(inactive->reactions,board);
    if (self_play_recursive(board,inactive,active,turn==X?O:X) == turn) {
        turn_node_mark_good(node);
        return turn;
    }
    turn_node_mark_bad(node);
    return turn == O ? X : O;
}

struct knowledge* aquire_knowledge()
{
    int i;
    struct knowledge* us, *them;
    us = knowledge_new(O);
    them = knowledge_new(X);

    for (i = 0;i < 1000000;++i) {
        gameboard board;
        gameboard_init(board);
        self_play_recursive(board,them,us,X);
    }

    knowledge_free(them);
    return us;
}

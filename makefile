################################################################################
# Makefile for cs467 projects ##################################################
################################################################################

ifeq ($(MAKECMDGOALS),debug)
MAKE_DEBUG = yes
endif

# set options according to goal
ifdef MAKE_DEBUG
PROJECT1 = knapsack-debug
PROJECT2 = ga-knapsack-debug
PROJECT3 = ga-graph-color-debug
PROJECT4 = tictactoe-debug

BUILD = gcc -g -Wall -Werror -Wextra -Wshadow -pedantic-errors -Wfatal-errors -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function -std=gnu99
else
PROJECT1 = knapsack
PROJECT2 = ga-knapsack
PROJECT3 = ga-graph-color
PROJECT4 = tictactoe

BUILD = gcc -s -O3 -Wall -Werror -Wextra -Wshadow -pedantic-errors -Wfatal-errors -Wno-unused-function -std=gnu99
endif

# rules

all: $(PROJECT1) $(PROJECT2) $(PROJECT3) $(PROJECT4)
debug: $(PROJECT1) $(PROJECT2) $(PROJECT3) $(PROJECT4)

$(PROJECT1): knapsack2.c
	$(BUILD) -o$(PROJECT1) -DFEAT_LINUX_TINFO knapsack2.c -ltinfo -lm
$(PROJECT2): ga-knapsack.c
	$(BUILD) -o$(PROJECT2) ga-knapsack.c -lm
$(PROJECT3): ga-graph-color.c
	$(BUILD) -o$(PROJECT3) ga-graph-color.c -ldstructs
$(PROJECT4): tictactoe.c
	$(BUILD) -o$(PROJECT4) tictactoe.c -ldstructs

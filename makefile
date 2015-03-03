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

BUILD = gcc -g -Wall -Werror -Wextra -Wshadow -pedantic-errors -Wfatal-errors -Wno-unused-variable -Wno-unused-parameter -std=gnu99
else
PROJECT1 = knapsack
PROJECT2 = ga-knapsack

BUILD = gcc -s -O3 -Wall -Werror -Wextra -Wshadow -pedantic-errors -Wfatal-errors -Wno-unused-function -std=gnu99
endif
MACRO = -DFEAT_LINUX_TINFO
LIB = -ltinfo -lm

# rules

all: $(PROJECT1) $(PROJECT2)
debug: $(PROJECT1) $(PROJECT2)

$(PROJECT1): knapsack2.c
	$(BUILD) -o$(PROJECT1) $(MACRO) knapsack2.c $(LIB)
$(PROJECT2): ga-knapsack.c
	$(BUILD) -o$(PROJECT2) $(MACRO) ga-knapsack.c $(LIB)

/* knapsack.c - solve the knapsack problem using exhaustive search */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

struct k_item
{
    int cost;
    int value;
    char* name;
};
static struct k_item* k_item_new(int cost,int value,const char* name);
static void k_item_free(struct k_item* item);

struct k_sack
{
    size_t itemCap, itemSz;
    struct k_item** items;
};
static struct k_sack* k_sack_new();
static struct k_sack* k_sack_copy(struct k_sack* sack);
static void k_sack_free(struct k_sack* sack);
static void k_sack_add_item(struct k_sack* sack,struct k_item* item);
static void k_sack_sum_up(struct k_sack* sack,int* cost,int* value);

struct k_solution
{
    int limit; /* cost limit */

    /* best info */
    int bestCost;
    int bestValue;
    struct k_sack* bestSack;
};
static struct k_solution* k_solution_new();
static void k_solution_free(struct k_solution* set);
static int k_solution_check_sack(struct k_solution* set,struct k_sack* sack);

static struct k_solution* solution;
static void knapsack(FILE* file,const char* filename);
static void knapsack_candidates_recursive(struct k_item** item,struct k_sack* sack);

int main(int argc,const char* argv[])
{
    if (argc > 1) {
        int i;
        for (i = 1;i < argc;++i) {
            FILE* fin = fopen(argv[i],"r");
            if (fin == NULL)
                fprintf(stderr,"Cannot open '%s': %s\n",argv[i],strerror(errno));
            else {
                knapsack(fin,argv[i]);
                fclose(fin);
            }
        }
    }
    else
        knapsack(stdin,"stdin");
    return 0;
}

/* k_item */
struct k_item* k_item_new(int cost,int value,const char* name)
{
    size_t len;
    struct k_item* item;
    item = malloc(sizeof(struct k_item));
    item->cost = cost;
    item->value = value;
    len = strlen(name);
    item->name = malloc(len+1);
    strcpy(item->name,name);
    return item;
}
void k_item_free(struct k_item* item)
{
    free(item->name);
    free(item);
}

/* k_sack */
struct k_sack* k_sack_new()
{
    struct k_sack* sack;
    sack = malloc(sizeof(struct k_sack));
    sack->itemCap = 4;
    sack->itemSz = 0;
    sack->items = malloc(sizeof(struct k_item*) * sack->itemCap);
    return sack;
}
struct k_sack* k_sack_copy(struct k_sack* sack)
{
    size_t i;
    struct k_sack* copy;
    copy = malloc(sizeof(struct k_sack));
    copy->itemCap = sack->itemCap;
    copy->itemSz = sack->itemSz;
    copy->items = malloc(sizeof(struct k_item*) * copy->itemCap);
    for (i = 0;i < copy->itemSz;++i)
        copy->items[i] = sack->items[i];
    return copy;
}
void k_sack_free(struct k_sack* sack)
{
    free(sack->items);
    free(sack);
}
void k_sack_add_item(struct k_sack* sack,struct k_item* item)
{
    if (sack->itemSz >= sack->itemCap) {
        struct k_item** newblock;
        sack->itemCap <<= 2;
        newblock = realloc(sack->items,sizeof(struct k_item*)*sack->itemCap);
        if (newblock == NULL) {
            fprintf(stderr,"memory exception: fail realloc()\n");
            exit(EXIT_FAILURE);
        }
        sack->items = newblock;
    }
    sack->items[sack->itemSz++] = item;
}
void k_sack_sum_up(struct k_sack* sack,int* cost,int* value)
{
    size_t i;
    *cost = 0;
    *value = 0;
    for (i = 0;i < sack->itemSz;++i) {
        *cost += sack->items[i]->cost;
        *value += sack->items[i]->value;
    }
}

/* k_solution */
struct k_solution* k_solution_new()
{
    struct k_solution* sol;
    sol = malloc(sizeof(struct k_solution));
    sol->bestSack = NULL;
    return sol;
}
void k_solution_free(struct k_solution* sol)
{
    if (sol->bestSack != NULL)
        k_sack_free(sol->bestSack);
    free(sol);
}
int k_solution_check_sack(struct k_solution* sol,struct k_sack* sack)
{
    /* see if the sack is better than anything so far */
    int cost, value;
    k_sack_sum_up(sack,&cost,&value);
    if (cost<=sol->limit && (sol->bestSack==NULL || value>sol->bestValue)) {
        if (sol->bestSack != NULL)
            k_sack_free(sol->bestSack);
        sol->bestCost = cost;
        sol->bestValue = value;
        sol->bestSack = sack;
        return 1;
    }
    return 0;
}

static int readline(FILE* fin,char* buf,size_t cap)
{
    size_t s = --cap;
    while (cap > 0) {
        size_t len;
        if (fgets(buf,cap+1,fin) == NULL)
            return feof(fin) && cap<s;
        len = strlen(buf);
        if (buf[len-1] == '\n') {
            buf[len-1] = 0;
            break;
        }
        buf += len;
        cap -= len;
    }
    return 1;
}
static char* commasep(char** s)
{
    char* i, *start;
    while ( isspace(**s) )
        ++*s;
    start = *s;
    while (**s && **s!=',')
        ++*s;
    i = *s-1;
    while ( isspace(*i) )
        *(i--) = 0;
    **s = 0;
    ++*s;
    return start;
}
static int itemcompar(const struct k_item** left,const struct k_item** right)
{
    return strcmp((*left)->name,(*right)->name);
}
void knapsack(FILE* fin,const char* filename)
{
    size_t i;
    char linebuf[4097];
    size_t itemCap, itemSz;
    struct k_item** items;
    /* read in knapsack limit */
    solution = k_solution_new(); /* GLOBAL */
    readline(fin,linebuf,sizeof(linebuf));
    if (sscanf(linebuf,"%d",&solution->limit) != 1) {
        fprintf(stderr,"format error in file '%s': <cost-limit> was not an integer\n",filename);
        k_solution_free(solution);
        return;
    }
    /* read in set of possible items */
    itemCap = 4;
    itemSz = 0;
    items = malloc(sizeof(struct k_item*) * itemCap);
    while (1) {
        if (itemSz >= itemCap) {
            struct k_item** newblock;
            itemCap <<= 2;
            newblock = realloc(items,sizeof(struct k_item*) * itemCap);
            if (newblock == NULL) {
                fprintf(stderr,"memory exception: fail realloc()\n");
                exit(EXIT_FAILURE);
            }
            items = newblock;
        }
        if ( readline(fin,linebuf,sizeof(linebuf)) ) {
            char* s, *name;
            int cost, value;
            s = linebuf;
            name = commasep(&s);
            if (sscanf(commasep(&s),"%d",&cost)!=1 || sscanf(commasep(&s),"%d",&value)!=1) {
                fprintf(stderr,"format error in file '%s': bad item format for item %d\n",filename,(int)itemSz+1);
                k_solution_free(solution);
                free(items);
                return;
            }
            items[itemSz++] = k_item_new(cost,value,name);
        }
        else {
            items[itemSz] = NULL;
            break;
        }
    }
    if (itemSz <= 0) {
        fprintf(stderr,"empty item set in file '%s'\n",filename);
        k_solution_free(solution);
        free(items);
        return;
    }
    /* find candidate solutions to the knapsack problem; the k_solution will
       find the best sack as it generates them */
    knapsack_candidates_recursive(items,k_sack_new());
    if (solution->bestSack == NULL)
        /* this shouldn't happen */
        printf("no solution!!! [%s]\n",filename);
    else {
        if (solution->bestSack->itemSz == 0)
            printf("empty set [%s]\n",filename);
        else {
            qsort(solution->bestSack->items,solution->bestSack->itemSz,
                sizeof(struct k_item*),(int (*)(const void*,const void*))itemcompar);
            printf("best knapsack has cost=%d and value=%d [%s]\nitems:",solution->bestCost,solution->bestValue,filename);
            printf("\t%s",solution->bestSack->items[0]->name);
            for (i = 1;i < solution->bestSack->itemSz;++i)
                printf(i%10==0 ? ",\n\t%s" : ", %s",solution->bestSack->items[i]->name);
            putchar('\n');
        }
    }
    k_solution_free(solution);
    for (i = 0;i < itemSz;++i)
        k_item_free(items[i]);
    free(items);
}
void knapsack_candidates_recursive(struct k_item** item,struct k_sack* sack)
{
    struct k_sack* right;
    if (*item == NULL) {
        /* sack is a leaf sack; check it to see if it is a better
           solution; if not, then delete it */
        if (!k_solution_check_sack(solution,sack))
            k_sack_free(sack);
        return;
    }
    right = k_sack_copy(sack);
    /* generate subtree that does not contain the current item */
    knapsack_candidates_recursive(item+1,sack);
    /* generate the subtree that contains the current item */
    k_sack_add_item(right,*item);
    knapsack_candidates_recursive(item+1,right);
}

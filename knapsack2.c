/* knapsack.c - solve the knapsack problem using exhaustive search;
   version2: add lower and upper bound computations */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <curses.h>
#include <term.h>
#include <unistd.h>

/* terminal codes */
char TERM_BOLD[32] = "";
char TERM_SGR0[32] = "";
char TERM_SETF[32] = "";
char TERM_SETD[32] = "";

struct k_item
{
    int cost;
    int value;
    char* name;
};
static struct k_item* k_item_new(int cost,int value,const char* name);
static void k_item_free(struct k_item* item);

/* item comparisons (by reference-to-reference) */
static int itemcompar_name(const struct k_item** left,const struct k_item** right);
static int itemcompar_value(const struct k_item** left,const struct k_item** right);
static int itemcompar_cost(const struct k_item** left,const struct k_item** right);
static int itemcompar_ratio(const struct k_item** left,const struct k_item** right);

struct k_sack
{
    int cost;
    int value;
    size_t itemCap, itemSz;
    struct k_item** items;
};
static struct k_sack* k_sack_new();
static struct k_sack* k_sack_copy(struct k_sack* sack);
static void k_sack_free(struct k_sack* sack);
static void k_sack_add_item(struct k_sack* sack,struct k_item* item);

struct k_partial_sack
{
    int cost;
    double value; /* needs fractional component */
    /* items[0] are complete items; items[1] are partial */
    size_t itemCap[2], itemSz[2];
    struct k_item** items[2];
};
static struct k_partial_sack* k_partial_sack_new();
static void k_partial_sack_free(struct k_partial_sack* psack);
static void k_partial_sack_add_item(struct k_partial_sack* psack,struct k_item* item,int cost);
static void k_partial_sack_print(struct k_partial_sack* psack,const char* title);

struct k_solution
{
    size_t sackCounter; /* number of sacks considered before solution */
    struct k_sack* sack; /* solution knapsack */
};
static struct k_solution* globlSolution;
static struct k_solution* k_solution_new();
static void k_solution_free(struct k_solution* sol);
static int k_solution_check_sack(struct k_solution* sol,struct k_sack* sack);
static void k_solution_print(struct k_solution* sol,const char* title);

struct k_info
{
    int limit; /* total cost that must not be exceeded */
    int lowerValueBound; /* value of best greedy knapsack */
    int upperValueBound; /* value of best partial knapsack */
};
static struct k_info globlInfo;
static void k_info_init();
static inline void k_info_update_lower_value_bound(struct k_solution* sol);

/* solution functionality */
static void knapsack(FILE* file,const char* filename);
static void knapsack_bruteforce_recursive(struct k_item** item,struct k_sack* sack);
static void knapsack_optimized1_recursive(struct k_item** item,struct k_sack* sack);
static int knapsack_optimized2_recursive(struct k_item** item,struct k_sack* sack);
static struct k_solution* greedy_highest_value(struct k_item** items,size_t cnt);
static struct k_solution* greedy_lowest_cost(struct k_item** items,size_t cnt);
static struct k_solution* greedy_highest_ratio(struct k_item** items,size_t cnt);
static struct k_partial_sack* partial_knapsack(struct k_item** items,size_t cnt);

static const char* programName;
int main(int argc,const char* argv[])
{
    programName = argv[0];
    /* setup terminal information if stdout is a terminal */
    if (isatty(STDOUT_FILENO)) {
        const char* term = getenv("TERM");
        if (term!=NULL && setupterm(term,1,NULL)!=ERR) {
            strncpy(TERM_BOLD,tigetstr("bold"),sizeof(TERM_BOLD));
            strncpy(TERM_SGR0,tigetstr("sgr0"),sizeof(TERM_SGR0));
            strncpy(TERM_SETF,tiparm(tigetstr("setaf"),4),sizeof(TERM_SETF));
            strncpy(TERM_SETD,tiparm(tigetstr("setaf"),9),sizeof(TERM_SETD));
        }
    }
    /* perform knapsack computations on instance input; if no file names
       were supplied, read from standard input */
    if (argc > 1) {
        int i;
        for (i = 1;i < argc;++i) {
            FILE* fin = fopen(argv[i],"r");
            if (fin == NULL)
                fprintf(stderr,"%s: cannot open '%s': %s\n",argv[0],argv[i],strerror(errno));
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
    if (item == NULL) {
        fprintf(stderr,"%s: memory exception: fail malloc()\n",programName);
        exit(EXIT_FAILURE);
    }
    item->cost = cost;
    item->value = value;
    len = strlen(name);
    item->name = malloc(len+1);
    if (item->name == NULL) {
        fprintf(stderr,"%s: memory exception: fail malloc()\n",programName);
        exit(EXIT_FAILURE);
    }
    strcpy(item->name,name);
    return item;
}
void k_item_free(struct k_item* item)
{
    free(item->name);
    free(item);
}

int itemcompar_name(const struct k_item** left,const struct k_item** right)
{
    return strcmp((*left)->name,(*right)->name);
}
int itemcompar_value(const struct k_item** left,const struct k_item** right)
{
    return (*right)->value - (*left)->value;
}
int itemcompar_cost(const struct k_item** left,const struct k_item** right)
{
    return (*left)->cost - (*right)->cost;
}
int itemcompar_ratio(const struct k_item** left,const struct k_item** right)
{
    double ratio[2];
    ratio[0] = (double)(*right)->value / (*right)->cost;
    ratio[1] = (double)(*left)->value / (*left)->cost;
    if (ratio[0] < ratio[1])
        return -1;
    else if (ratio[0] > ratio[1])
        return 1;
    return 0;
}

/* k_sack */
struct k_sack* k_sack_new()
{
    struct k_sack* sack;
    sack = malloc(sizeof(struct k_sack));
    if (sack == NULL) {
        fprintf(stderr,"%s: memory exception: fail malloc()\n",programName);
        exit(EXIT_FAILURE);
    }
    sack->cost = sack->value = 0;
    sack->itemCap = 4;
    sack->itemSz = 0;
    sack->items = malloc(sizeof(struct k_item*) * sack->itemCap);
    if (sack->items == NULL) {
        fprintf(stderr,"%s: memory exception: fail malloc()\n",programName);
        exit(EXIT_FAILURE);
    }
    return sack;
}
struct k_sack* k_sack_copy(struct k_sack* sack)
{
    size_t i;
    struct k_sack* copy;
    copy = malloc(sizeof(struct k_sack));
    if (copy == NULL) {
        fprintf(stderr,"%s: memory exception: fail malloc()\n",programName);
        exit(EXIT_FAILURE);
    }
    copy->cost = sack->cost;
    copy->value = sack->value;
    copy->itemCap = sack->itemCap;
    copy->itemSz = sack->itemSz;
    copy->items = malloc(sizeof(struct k_item*) * copy->itemCap);
    if (copy->items == NULL) {
        fprintf(stderr,"%s: memory exception: fail malloc()\n",programName);
        exit(EXIT_FAILURE);
    }
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
            fprintf(stderr,"%s: memory exception: fail realloc()\n",programName);
            exit(EXIT_FAILURE);
        }
        sack->items = newblock;
    }
    sack->items[sack->itemSz++] = item;
    sack->cost += item->cost;
    sack->value += item->value;
}

/* k_partial_sack */
struct k_partial_sack* k_partial_sack_new()
{
    int i;
    struct k_partial_sack* psack;
    psack = malloc(sizeof(struct k_partial_sack));
    if (psack == NULL) {
        fprintf(stderr,"%s: memory exception: fail malloc()\n",programName);
        exit(EXIT_FAILURE);
    }
    psack->cost = 0;
    psack->value = 0.0;
    for (i = 0;i < 2;++i) {
        psack->itemCap[i] = 4;
        psack->itemSz[i] = 0;
        psack->items[i] = malloc(sizeof(struct k_item*) * psack->itemCap[i]);
        if (psack->items[i] == NULL) {
            fprintf(stderr,"%s: memory exception: fail malloc()\n",programName);
            exit(EXIT_FAILURE);
        }
    }
    return psack;
}
void k_partial_sack_free(struct k_partial_sack* psack)
{
    int i;
    for (i = 0;i < 2;++i)
        free(psack->items[i]);
    free(psack);
}
void k_partial_sack_add_item(struct k_partial_sack* psack,struct k_item* item,int cost)
{
    int index;
    double value = item->value;
    value *= (double)cost / item->cost;
    index = value < item->value ? 1 : 0;
    if (psack->itemSz[index] >= psack->itemCap[index]) {
        struct k_item** newblock;
        psack->itemCap[index] <<= 2;
        newblock = realloc(psack->items[index],sizeof(struct k_item*)*psack->itemCap[index]);
        if (newblock == NULL) {
            fprintf(stderr,"%s: memory exception: fail realloc()\n",programName);
            exit(EXIT_FAILURE);
        }
        psack->items[index] = newblock;
    }
    psack->items[index][psack->itemSz[index]++] = item;
    psack->cost += cost;
    psack->value += value;
}
void k_partial_sack_print(struct k_partial_sack* psack,const char* title)
{
    if (psack->itemSz[0]==0 && psack->itemSz[1]==0)
        printf("\t[%s%s%s%s%s] solution: empty set\n",TERM_SETF,TERM_BOLD,title,TERM_SGR0,TERM_SETD);
    else {
        size_t i;
        for (i = 0;i < 2;++i)
            qsort(psack->items[i],psack->itemSz[i],sizeof(struct k_item*),(int (*)(const void*,const void*))itemcompar_name);
        printf("\t[%s%s%s%s%s] solution: cost=%s%d%s, value=%s%f%s",TERM_SETF,TERM_BOLD,title,TERM_SGR0,TERM_SETD,TERM_SETF,psack->cost,
            TERM_SETD,TERM_SETF,psack->value,TERM_SETD);
        if (psack->itemSz[0] > 0) {
            printf("\n\twhole-items:\t%s%s",TERM_BOLD,psack->items[0][0]->name);
            for (i = 1;i < psack->itemSz[0];++i)
                printf(i%10==0 ? ",\n\t%s" : ", %s",psack->items[0][i]->name);
            printf("%s\n",TERM_SGR0);
        }
        if (psack->itemSz[1] > 0) {
            printf("\tpartial-items:\t%s%s",TERM_BOLD,psack->items[1][0]->name);
            for (i = 1;i < psack->itemSz[1];++i)
                printf(i%10==0 ? ",\n\t%s" : ", %s",psack->items[1][i]->name);
            printf("%s\n",TERM_SGR0);
        }
    }
}

/* k_solution */
struct k_solution* k_solution_new()
{
    struct k_solution* sol;
    sol = malloc(sizeof(struct k_solution));
    if (sol == NULL) {
        fprintf(stderr,"%s: memory exception: fail malloc()\n",programName);
        exit(EXIT_FAILURE);
    }
    sol->sackCounter = 0;
    sol->sack = NULL;
    return sol;
}
void k_solution_free(struct k_solution* sol)
{
    if (sol->sack != NULL)
        k_sack_free(sol->sack);
    free(sol);
}
int k_solution_check_sack(struct k_solution* sol,struct k_sack* sack)
{
    /* see if the sack is better than anything so far */
    if (sack->cost<=globlInfo.limit && (sol->sack==NULL || sack->value>sol->sack->value)) {
        if (sol->sack != NULL)
            k_sack_free(sol->sack);
        sol->sack = sack;
        return 1;
    }
    return 0;
}
void k_solution_print(struct k_solution* sol,const char* title)
{
    if (sol->sack==NULL || sol->sack->itemSz==0)
        printf("\t[%s%s%s%s%s] solution: empty set\n",TERM_SETF,TERM_BOLD,title,TERM_SGR0,TERM_SETD);
    else {
        size_t i;
        qsort(sol->sack->items,sol->sack->itemSz,sizeof(struct k_item*),(int (*)(const void*,const void*))itemcompar_name);
        printf("\t[%s%s%s%s%s] solution: cost=%s%d%s, value=%s%d%s",TERM_SETF,TERM_BOLD,title,TERM_SGR0,TERM_SETD,TERM_SETF,sol->sack->cost,
            TERM_SETD,TERM_SETF,sol->sack->value,TERM_SETD);
        if (sol->sackCounter > 0)
            printf(", sack-count=%s%zu%s",TERM_SETF,sol->sackCounter,TERM_SETD);
        printf("\n\titems:\t%s%s",TERM_BOLD,sol->sack->items[0]->name);
        for (i = 1;i < sol->sack->itemSz;++i)
            printf(i%10==0 ? ",\n\t%s" : ", %s",sol->sack->items[i]->name);
        printf("%s\n",TERM_SGR0);
    }
}

/* k_info functionality */
void k_info_init()
{
    globlInfo.lowerValueBound = 0;
    globlInfo.upperValueBound = 0;
}
void k_info_update_lower_value_bound(struct k_solution* sol)
{
    if (sol->sack->value > globlInfo.lowerValueBound)
        globlInfo.lowerValueBound = sol->sack->value;
}

/* knapsack functionality and utilities */
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
void knapsack(FILE* fin,const char* filename)
{
    size_t i;
    char linebuf[4097];
    size_t itemCap, itemSz;
    struct k_item** items;
    struct k_solution* solution;
    struct k_partial_sack* partial;
    k_info_init();
    /* read in knapsack limit */
    readline(fin,linebuf,sizeof(linebuf));
    if (sscanf(linebuf,"%d",&globlInfo.limit) != 1) {
        fprintf(stderr,"%s: format error in file '%s': <cost-limit> field was not an integer\n",programName,filename);
        return;
    }
    /* read in set of possible items */
    itemCap = 4;
    itemSz = 0;
    items = malloc(sizeof(struct k_item*) * itemCap);
    if (items == NULL) {
        fprintf(stderr,"%s: memory exception: fail malloc()\n",programName);
        exit(EXIT_FAILURE);
    }
    while (1) {
        if (itemSz >= itemCap) {
            struct k_item** newblock;
            itemCap <<= 2;
            newblock = realloc(items,sizeof(struct k_item*) * itemCap);
            if (newblock == NULL) {
                fprintf(stderr,"%s: memory exception: fail realloc()\n",programName);
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
                fprintf(stderr,"%s: format error in file '%s': bad item format for item %d\n",programName,filename,(int)itemSz+1);
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
        fprintf(stderr,"%s: empty item set in file '%s'\n",programName,filename);
        free(items);
        return;
    }
    printf("[%s%s%s%s%s] with item-count=%s%zu%s and cost-limit=%s%d%s\n",TERM_SETF,TERM_BOLD,filename,TERM_SGR0,TERM_SETD,
        TERM_SETF,itemSz,TERM_SETD,TERM_SETF,globlInfo.limit,TERM_SETD);
    /* produce alternate solutions (greedy and partial); use these to
       compute lower and upper bounds on the best sack */
    solution = greedy_highest_value(items,itemSz);
    k_solution_print(solution,"greedy/highest value");
    k_info_update_lower_value_bound(solution);
    k_solution_free(solution);
    solution = greedy_lowest_cost(items,itemSz);
    k_solution_print(solution,"greedy/lowest cost");
    k_info_update_lower_value_bound(solution);
    k_solution_free(solution);
    solution = greedy_highest_ratio(items,itemSz);
    k_solution_print(solution,"greedy/highest ratio");
    k_info_update_lower_value_bound(solution);
    k_solution_free(solution);
    partial = partial_knapsack(items,itemSz);
    k_partial_sack_print(partial,"partial knapsack");
    k_partial_sack_free(partial);
    /* do an exhaustive search that optimizes out sub-trees that exceed cost limit */
    globlSolution = k_solution_new();
    knapsack_optimized1_recursive(items,k_sack_new());
    k_solution_print(globlSolution,"optimized1");
    k_solution_free(globlSolution);
    /* do an exhaustive search that optimizes out sub-trees whose value does not reach
       the lower value bound; this optimization is performed in addition to the one above */
    globlSolution = k_solution_new();
    knapsack_optimized2_recursive(items,k_sack_new());
    k_solution_print(globlSolution,"optimized2");
    k_solution_free(globlSolution);
    /* do a brute-force exhaustive search that explores all of the candidate
       solutions; the k_solution will find the best sack as it generates them */
    globlSolution = k_solution_new();
    knapsack_bruteforce_recursive(items,k_sack_new());
    k_solution_print(globlSolution,"brute force");
    k_solution_free(globlSolution);
    for (i = 0;i < itemSz;++i)
        k_item_free(items[i]);
    free(items);
}
void knapsack_bruteforce_recursive(struct k_item** item,struct k_sack* sack)
{
    struct k_sack* right;
    if (*item == NULL) {
        ++globlSolution->sackCounter;
        /* sack is a leaf sack; check it to see if it is a better
           solution; if not, then delete it */
        if (!k_solution_check_sack(globlSolution,sack))
            k_sack_free(sack);
        return;
    }
    right = k_sack_copy(sack);
    /* generate subtree that does not contain the current item */
    knapsack_bruteforce_recursive(item+1,sack);
    /* generate the subtree that contains the current item */
    k_sack_add_item(right,*item);
    knapsack_bruteforce_recursive(item+1,right);
}
void knapsack_optimized1_recursive(struct k_item** item,struct k_sack* sack)
{
    struct k_sack* right;
    if (*item == NULL) {
        ++globlSolution->sackCounter;
        if (!k_solution_check_sack(globlSolution,sack))
            k_sack_free(sack);
        return;
    }
    right = k_sack_copy(sack);
    k_sack_add_item(right,*item);
    if (right->cost <= globlInfo.limit)
        knapsack_optimized1_recursive(item+1,right);
    else
        k_sack_free(right);
    knapsack_optimized1_recursive(item+1,sack);
}
int knapsack_optimized2_recursive(struct k_item** item,struct k_sack* sack)
{
    struct k_sack* right;
    if (*item == NULL) {
        ++globlSolution->sackCounter;
        if (sack->value < globlInfo.lowerValueBound) {
            k_sack_free(sack);
            return 0;
        }
        if (!k_solution_check_sack(globlSolution,sack))
            k_sack_free(sack);
        return 1;
    }
    right = k_sack_copy(sack);
    k_sack_add_item(right,*item);
    if (right->cost <= globlInfo.limit) {
        if (!knapsack_optimized2_recursive(item+1,right)) {
            k_sack_free(sack);
            return 0;
        }
    }
    else
        k_sack_free(right);
    knapsack_optimized2_recursive(item+1,sack);
    return 1;
}
struct k_solution* greedy_highest_value(struct k_item** items,size_t cnt)
{
    size_t iter;
    int leftover;
    struct k_solution* solution;
    qsort(items,cnt,sizeof(struct k_item*),(int (*)(const void*,const void*))itemcompar_value);
    solution = k_solution_new();
    solution->sack = k_sack_new();
    leftover = globlInfo.limit;
    for (iter = 0;iter < cnt;++iter) {
        if (items[iter]->cost <= leftover) {
            k_sack_add_item(solution->sack,items[iter]);
            leftover -= items[iter]->cost;
        }
    }
    return solution;
}
struct k_solution* greedy_lowest_cost(struct k_item** items,size_t cnt)
{
    size_t iter;
    int leftover;
    struct k_solution* solution;
    qsort(items,cnt,sizeof(struct k_item*),(int (*)(const void*,const void*))itemcompar_cost);
    solution = k_solution_new();
    solution->sack = k_sack_new();
    leftover = globlInfo.limit;
    for (iter = 0;iter < cnt;++iter) {
        if (items[iter]->cost <= leftover) {
            k_sack_add_item(solution->sack,items[iter]);
            leftover -= items[iter]->cost;
        }
        else
            /* all the remaining items could never fit */
            break;
    }
    return solution;
}
struct k_solution* greedy_highest_ratio(struct k_item** items,size_t cnt)
{
    size_t iter;
    int leftover;
    struct k_solution* solution;
    qsort(items,cnt,sizeof(struct k_item*),(int (*)(const void*,const void*))itemcompar_ratio);
    solution = k_solution_new();
    solution->sack = k_sack_new();
    leftover = globlInfo.limit;
    for (iter = 0;iter < cnt;++iter) {
        if (items[iter]->cost <= leftover) {
            k_sack_add_item(solution->sack,items[iter]);
            leftover -= items[iter]->cost;
        }
    }
    return solution;
}
struct k_partial_sack* partial_knapsack(struct k_item** items,size_t cnt)
{
    size_t iter;
    int leftover;
    struct k_partial_sack* sack;
    qsort(items,cnt,sizeof(struct k_item*),(int (*)(const void*,const void*))itemcompar_ratio);
    sack = k_partial_sack_new();
    leftover = globlInfo.limit;
    for (iter = 0;iter < cnt;++iter) {
        int cost = items[iter]->cost > leftover ? leftover : items[iter]->cost;
        k_partial_sack_add_item(sack,items[iter],cost);
        if ((leftover-=cost) <= 0)
            break;
    }
    return sack;
}

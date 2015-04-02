#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

/* CONSTANTS ------------------------- */

enum gak_constants
{
    GAK_POPULATION_LIMIT = 100,
    GAK_UNINITIALIZED_VALUE = -1
};

/* TYPEDEFS -------------------------- */

typedef int (*compar_func)(const void* left,const void* right);
typedef void (*crossover_func)(const uint8_t* bitsParentA,const uint8_t* bitsParentB,uint8_t* bitsOffspring,size_t bits);

/* DATA STRUCTURES ------------------- */
struct gak_instance;

/* represent a knapsack item */
struct gak_item
{
    int cost;
    int value;
    char* label;
};
struct gak_item* gak_item_new(int cost,int value,const char* label);
void gak_item_free(struct gak_item* item);

/* represent a candidate solution */
struct gak_candidate
{
    /* fitness value to rank the candidate; a higher value indicates a
       higher fitness for the candidate solution */
    int fitness;

    /* sum of value and cost */
    int value, cost;

    /* this is a bitstring where bits 0..n-1 correspond to items 1..n; its
       length (in bytes) is implied by the number of items in the instance */
    uint8_t* bits;
};
struct gak_candidate* gak_candidate_new_random(struct gak_instance* inst);
struct gak_candidate* gak_candidate_new_crossover(struct gak_instance* inst,struct gak_candidate** parents,crossover_func func);
void gak_candidate_free(struct gak_candidate* cand);
void gak_candidate_mutate(struct gak_candidate* cand,struct gak_instance* inst,size_t numBits);
void gak_candidate_print(struct gak_candidate* cand,struct gak_instance* inst);
bool gak_candidate_compare(struct gak_candidate* candA,struct gak_candidate* candB,struct gak_instance* inst);

/* represent a set of candidates (population) */
struct gak_population
{
    /* list of population members is sorted from highest fitness
       value to lowest fitness value */
    struct gak_candidate* members[GAK_POPULATION_LIMIT];
};
struct gak_population* gak_population_new_random(struct gak_instance* inst);
void gak_population_free(struct gak_population* popl);
bool gak_population_breed_frombest(struct gak_population* popl,struct gak_instance* inst,crossover_func func);
bool gak_population_breed_tophalf(struct gak_population* popl,struct gak_instance* inst,crossover_func func);
bool gak_population_breed_threshold(struct gak_population* popl,struct gak_instance* inst,crossover_func func);
bool gak_population_breed_weighted(struct gak_population* popl,struct gak_instance* inst,crossover_func func);
bool gak_population_check_homogenous(struct gak_population* popl,struct gak_instance* inst);
void gak_population_cataclysmic_mutation(struct gak_population* popl,struct gak_instance* inst);
void gak_population_print(struct gak_population* popl,struct gak_instance* inst);

/* represent a knapsack problem instance */
struct gak_instance
{
    int costLimit;

    size_t itemSz, itemCap;
    struct gak_item** items;

    size_t bitcnt;
    size_t bytecnt;

    bool nonZeroSol;
};
struct gak_instance* gak_instance_new(FILE* fin,const char* filename);
void gak_instance_free(struct gak_instance* inst);
void gak_instance_apply_metrics(struct gak_instance* inst,struct gak_candidate* cand); /* apply fitness, sum value and sum cost to candidate */

/* CROSSOVER OPERATORS --------------- */

static void crossover_random(const uint8_t* bitsParentA,const uint8_t* bitsParentB,uint8_t* bitsChild,size_t bits);
static void crossover_alternate(const uint8_t* bitsParentA,const uint8_t* bitsParentB,uint8_t* bitsChild,size_t bits);

/* HELPERS --------------------------- */

static void gak_error(bool useErrno,const char* format, ...);
static void gak_fatal_error(bool useErrno,const char* format, ...);
static int readline(FILE* fin,char* buf,size_t cap);
static char* commasep(char** s);
static int gak_candidate_compar_byfitness(const struct gak_candidate** left,const struct gak_candidate** right);
static void zero_last_bits(uint8_t* byte,size_t lastBits);

/* GLOBALS --------------------------- */

static const char* programName;

/* MAIN FUNCTIONS -------------------- */

static void ga_knapsack(FILE* file,const char* filename);

int main(int argc,const char* argv[])
{
    programName = argv[0];
    /* seed puesdo-random number generator */
    srand(time(NULL));
    /* process command-line arguments */
    if (argc > 1) {
        int i;
        for (i = 1;i < argc;++i) {
            FILE* fin;
            fin = fopen(argv[i],"r");
            if (fin == NULL)
                gak_error(true,"cannot open '%s'",argv[i]);
            else {
                ga_knapsack(fin,argv[i]);
                fclose(fin);
            }
        }
    }
    else
        ga_knapsack(stdin,"stdin");
    return 0;
}

/* INLINE HELPERS -------------------- */

static inline int min(int a,int b)
{
    return a<b ? a : b;
}

/* IMPLEMENTATION -------------------- */

/* gak_item */
struct gak_item* gak_item_new(int cost,int value,const char* label)
{
    struct gak_item* item;
    item = malloc(sizeof(struct gak_item));
    if (item == NULL)
        gak_fatal_error(false,"memory allocation failure");
    item->cost = cost;
    item->value = value;
    item->label = malloc(strlen(label)+1);
    if (item->label == NULL)
        gak_fatal_error(false,"memory allocation failure");
    strcpy(item->label,label);
    return item;
}
void gak_item_free(struct gak_item* item)
{
    free(item->label);
    free(item);
}

/* gak_candidate */
struct gak_candidate* gak_candidate_new_random(struct gak_instance* inst)
{
    size_t i;
    struct gak_candidate* cand;
    cand = malloc(sizeof(struct gak_candidate));
    if (cand == NULL)
        gak_fatal_error(false,"memory allocation failure");
    cand->fitness = GAK_UNINITIALIZED_VALUE;
    cand->value = GAK_UNINITIALIZED_VALUE;
    cand->cost = GAK_UNINITIALIZED_VALUE;
    cand->bits = malloc(inst->bytecnt);
    if (cand->bits == NULL)
        gak_fatal_error(false,"memory allocation failure");
    memset(cand->bits,0,inst->bytecnt);
    for (i = 0;i < inst->bytecnt;++i) {
        size_t j;
        uint8_t* p;
        uint32_t r;
        r = (uint32_t)rand();
        p = (uint8_t*)&r;
        for (j = 0;j < sizeof(uint32_t);++j)
            cand->bits[i] = *p++;
    }
    /* make sure the unused bits on the last byte are zeroed out */
    zero_last_bits(cand->bits+i-1,inst->bitcnt % inst->itemSz);
    gak_instance_apply_metrics(inst,cand);
    return cand;
}
struct gak_candidate* gak_candidate_new_crossover(struct gak_instance* inst,struct gak_candidate** parents,crossover_func func)
{
    struct gak_candidate* cand;
    cand = malloc(sizeof(struct gak_candidate));
    if (cand == NULL)
        gak_fatal_error(false,"memory allocation failure");
    cand->fitness = GAK_UNINITIALIZED_VALUE;
    cand->value = GAK_UNINITIALIZED_VALUE;
    cand->cost = GAK_UNINITIALIZED_VALUE;
    cand->bits = malloc(inst->bytecnt);
    memset(cand->bits,0,inst->bytecnt);
    /* set child's genes and compile metrics */
    func(parents[0]->bits,parents[1]->bits,cand->bits,inst->itemSz);
    /* make sure the unused bits on the last byte are zeroed out */
    zero_last_bits(cand->bits+inst->bytecnt-1,inst->bitcnt % inst->itemSz);
    gak_instance_apply_metrics(inst,cand);
    return cand;
}
void gak_candidate_free(struct gak_candidate* cand)
{
    free(cand->bits);
    free(cand);
}
void gak_candidate_mutate(struct gak_candidate* cand,struct gak_instance* inst,size_t numBits)
{
    size_t iter, t, u;
    for (iter = 0;iter < numBits;++iter) {
        t = rand() % inst->itemSz;
        u = t % 8;
        t /= 8;
        cand->bits[t] ^= cand->bits[t] & (0x1<<u);
    }
}
void gak_candidate_print(struct gak_candidate* cand,struct gak_instance* inst)
{
    size_t iter, m, n;
    printf("[cost]%*d [value]%*d [fitness]%*d:",4,cand->cost,4,cand->value,4,cand->fitness);
    m = n = 0;
    for (iter = 0;iter < inst->itemSz;++iter) {
        if (cand->bits[n] & (0x1<<m)) {
            putchar(' ');
            fputs(inst->items[iter]->label,stdout);
        }
        ++m;
        if (m >= 8) {
            m = 0;
            ++n;
        }
    }
    putchar('\n');
}
bool gak_candidate_compare(struct gak_candidate* candA,struct gak_candidate* candB,struct gak_instance* inst)
{
    size_t i;
    for (i = 0;i < inst->bytecnt;++i)
        if (candA->bits[i] != candB->bits[i])
            return false;
    return true;
}

/* gak_population */
struct gak_population* gak_population_new_random(struct gak_instance* inst)
{
    /* create new population with randomized members */
    size_t i;
    struct gak_population* popl;
    popl = malloc(sizeof(struct gak_population));
    if (popl == NULL)
        gak_fatal_error(false,"memory allocation failure");
    /* generate random candidates; compute their fitness as well */
    for (i = 0;i < GAK_POPULATION_LIMIT;++i)
        popl->members[i] = gak_candidate_new_random(inst);
    /* sort the candidates by fitness ranking */
    qsort(popl->members,GAK_POPULATION_LIMIT,sizeof(struct gak_candidate*),(compar_func)gak_candidate_compar_byfitness);
    return popl;
}
void gak_population_free(struct gak_population* popl)
{
    size_t i;
    for (i = 0;i < GAK_POPULATION_LIMIT;++i)
        gak_candidate_free(popl->members[i]);
    free(popl);
}
static bool gak_population_breed(struct gak_population* popl,struct gak_candidate* offspring,struct gak_instance* inst,size_t threshold)
{
    size_t iter;
    iter = 0;
    while (iter<GAK_POPULATION_LIMIT && offspring->fitness<popl->members[iter]->fitness)
        ++iter;
    if (iter >= GAK_POPULATION_LIMIT)
        /* offspring wasn't better than any of the population members */
        gak_candidate_free(offspring);
    else {
        size_t i, last;
        /* delete the last population member and bump the rest down */
        last = GAK_POPULATION_LIMIT-1;
        gak_candidate_free(popl->members[last]);
        for (i = last;i > iter;--i)
            popl->members[i] = popl->members[i-1];
        popl->members[iter] = offspring;
        /* the offspring has a chance for mutation */
        if (rand() % 47 == 0)
            gak_candidate_mutate(offspring,inst,inst->bitcnt);
    }
    return iter <= threshold;
}
bool gak_population_breed_frombest(struct gak_population* popl,struct gak_instance* inst,crossover_func func)
{
    struct gak_candidate* offspring;
    /* generate an offspring using the specified combining function; use
       the best two population candidates so far as parents */
    offspring = gak_candidate_new_crossover(inst,popl->members,func);
    return gak_population_breed(popl,offspring,inst,1);
}
bool gak_population_breed_tophalf(struct gak_population* popl,struct gak_instance* inst,crossover_func func)
{
    int r[2];
    struct gak_candidate* parents[2];
    struct gak_candidate* offspring;
    /* randomly select parents from top 50 candidates */
    do {
        r[0] = rand() % 50;
        r[1] = rand() % 50;
    } while (r[0] == r[1]);
    parents[0] = popl->members[r[0]];
    parents[1] = popl->members[r[1]];
    offspring = gak_candidate_new_crossover(inst,parents,func);
    return gak_population_breed(popl,offspring,inst,1);
}
bool gak_population_breed_threshold(struct gak_population* popl,struct gak_instance* inst,crossover_func func)
{
    int r[2];
    int fit;
    double sigma;
    size_t i, threshold;
    struct gak_candidate* parents[2];
    struct gak_candidate* offspring;
    /* let the most fit candidate be the mean of the population augmented with values
       which form an additive inverse with every population value except the most fit;
       find the standard deviation of this population; establish a threshold such that
       the parents will appear at most 1 standard deviation away from the most fit */
    sigma = 0.0;
    for (i = 1;i < GAK_POPULATION_LIMIT;++i) {
        double diff;
        diff = popl->members[i]->fitness - popl->members[0]->fitness;
        sigma += 2.0 * diff * diff; /* multiply by two to account for the augmented population */
    }
    sigma /= GAK_POPULATION_LIMIT*2;
    sigma = sqrt(sigma);
    fit = round(popl->members[0]->fitness - sigma);
    /* find threshold index */
    threshold = 2; /* need to include at least 2 candidates */
    while (threshold<GAK_POPULATION_LIMIT && popl->members[threshold]->fitness>=fit)
        ++threshold;
    /* randomly select parents from candidates above the threshold */
    do {
        r[0] = rand() % threshold;
        r[1] = rand() % threshold;
    } while (r[0] == r[1]);
    parents[0] = popl->members[r[0]];
    parents[1] = popl->members[r[1]];
    offspring = gak_candidate_new_crossover(inst,parents,func);
    return gak_population_breed(popl,offspring,inst,1);
}
bool gak_population_breed_weighted(struct gak_population* popl,struct gak_instance* inst,crossover_func func)
{
    int r[2];
    struct gak_candidate* parents[2];
    struct gak_candidate* offspring;
    do {
        int i;
        for (i = 0;i < 2;++i) {
            int a, b;
            a = rand() % GAK_POPULATION_LIMIT;
            b = rand() % GAK_POPULATION_LIMIT;
            r[i] = min(a,b);
        }
    } while (r[0] == r[1]);
    parents[0] = popl->members[r[0]];
    parents[1] = popl->members[r[1]];
    offspring = gak_candidate_new_crossover(inst,parents,func);
    return gak_population_breed(popl,offspring,inst,1);
}
bool gak_population_check_homogenous(struct gak_population* popl,struct gak_instance* inst)
{
    size_t iter;
    for (iter = 1;iter < GAK_POPULATION_LIMIT;++iter)
        if ( !gak_candidate_compare(popl->members[0],popl->members[iter],inst) )
            return false;
    return true;
}
void gak_population_cataclysmic_mutation(struct gak_population* popl,struct gak_instance* inst)
{
    /* keep only the first member intact; randomly influence a random number of genes of each
       remaining member */
    size_t iter, i, t;
    for (iter = 1;iter < GAK_POPULATION_LIMIT/4;++iter) {
        /* use genes from the best candidate */
        for (i = 0;i < inst->bytecnt;++i)
          popl->members[iter]->bits[i] = popl->members[0]->bits[i];
        for (i = 0;i < inst->itemSz/2;++i) {
            size_t u;
            /* generate random position */
            t = rand() % inst->itemSz;
            u = t % 8;
            t /= 8;
            /* toggle the bit at the random position */
            popl->members[iter]->bits[t] ^= (popl->members[iter]->bits[t] & (0x1<<u));
        }
        /* we need to recompute the metrics for the candidate */
        gak_instance_apply_metrics(inst,popl->members[iter]);
    }
    for (;iter < GAK_POPULATION_LIMIT;++iter) {
        for (i = 0;i < inst->bytecnt;++i) {
            uint8_t* p;
            uint32_t r;
            r = (uint32_t)rand();
            p = (uint8_t*)&r;
            for (t = 0;t < sizeof(uint32_t);++t)
                popl->members[iter]->bits[t] = *p++;
        }
        /* make sure the unused bits on the last byte are zeroed out */
        zero_last_bits(popl->members[iter]->bits+i-1,inst->bitcnt % inst->itemSz);
        gak_instance_apply_metrics(inst,popl->members[iter]);
    }
    /* we have to resort the population after making all those changes */
    qsort(popl->members,GAK_POPULATION_LIMIT,sizeof(struct gak_candidate*),(compar_func)gak_candidate_compar_byfitness);
}
void gak_population_print(struct gak_population* popl,struct gak_instance* inst)
{
    size_t iter;
    for (iter = 0;iter < GAK_POPULATION_LIMIT;++iter)
        gak_candidate_print(popl->members[iter],inst);
}

/* gak_instance */
struct gak_instance* gak_instance_new(FILE* fin,const char* filename)
{
    struct gak_instance* inst;
    char linebuf[4097];
    inst = malloc(sizeof(struct gak_instance));
    if (inst == NULL)
        gak_fatal_error(false,"memory allocation failure");
    /* read in knapsack cost limit */
    readline(fin,linebuf,sizeof(linebuf));
    if (sscanf(linebuf,"%d",&inst->costLimit) != 1) {
        gak_error(false,"format error in file '%s': expected numeric cost limit",filename);
        free(inst);
        return NULL;
    }
    /* read items */
    inst->itemSz = 0;
    inst->itemCap = 8;
    inst->items = malloc(sizeof(struct gak_item*) * inst->itemCap);
    inst->nonZeroSol = false; /* is there at least one item that can fit in a sack? */
    if (inst->items == NULL)
        gak_fatal_error(false,"memory allocation failure");
    while (true) {
        /* check reallocation on dynamic array */
        if (inst->itemSz >= inst->itemCap) {
            size_t cap;
            struct gak_item** block;
            cap = inst->itemCap << 1;
            block = realloc(inst->items,sizeof(struct gak_item*) * cap);
            if (block == NULL)
                gak_fatal_error(false,"memory allocation failure");
            inst->items = block;
            inst->itemCap = cap;
        }
        /* read item line input */
        if ( readline(fin,linebuf,sizeof(linebuf)) ) {
            char* s, *name;
            int cost, value;
            s = linebuf;
            name = commasep(&s);
            if (sscanf(commasep(&s),"%d",&cost) != 1) {
                gak_error(false,"format error in file '%s': expected integer value for item cost",filename);
                gak_instance_free(inst);
                return NULL;
            }
            if (sscanf(commasep(&s),"%d",&value) != 1) {
                gak_error(false,"format error in file '%s': expected integer value for item value",filename);
                gak_instance_free(inst);
                return NULL;
            }
            if (cost < inst->costLimit)
                inst->nonZeroSol = true;
            inst->items[inst->itemSz++] = gak_item_new(cost,value,name);
        }
        else {
            inst->items[inst->itemSz] = NULL;
            break;
        }
    }
    if (inst->itemSz == 0) {
        gak_error(false,"file '%s' has an empty item set",filename);
        gak_instance_free(inst);
        return NULL;
    }
    /* compute the number of bytes needed to represent
       the candidates' item bitstring */
    inst->bytecnt = inst->itemSz/8;
    inst->bytecnt += inst->itemSz%8>0 ? 1 : 0;
    inst->bitcnt = inst->bytecnt * 8;
    return inst;
}
void gak_instance_free(struct gak_instance* inst)
{
    size_t iter;
    for (iter = 0;iter < inst->itemSz;++iter)
        gak_item_free(inst->items[iter]);
    free(inst->items);
    free(inst);
}
void gak_instance_apply_metrics(struct gak_instance* inst,struct gak_candidate* cand)
{
    size_t iter, off = 0, pos = 0;
    cand->value = cand->cost = 0;
    for (iter = 0;iter < inst->itemSz;++iter) {
        if ((cand->bits[pos]>>off) & 0x01) {
            cand->value += inst->items[iter]->value;
            cand->cost += inst->items[iter]->cost;
        }
        /* compute next bit position */
        if (++off >= 8) {
            ++pos;
            off = 0;
        }
    }
    if (cand->cost > inst->costLimit)
        cand->fitness = 0;
    else
        cand->fitness = cand->value;
}

/* helpers */
void gak_error_generic(bool useErrno,const char* format,va_list vargs)
{
    char errmsg[4096];
    size_t sz;
    sz = sizeof(errmsg);
    vsnprintf(errmsg,sz,format,vargs);
    /* optionally apply errno message */
    if (useErrno) {
        size_t len = strlen(errmsg);
        sz -= len;
        snprintf(errmsg+len,sz,": %s",strerror(errno));
    }
    fprintf(stderr,"%s: %s\n",programName,errmsg);
}
void gak_error(bool useErrno,const char* format, ...)
{
    va_list vargs;
    va_start(vargs,format);
    gak_error_generic(useErrno,format,vargs);
    va_end(vargs);
}
void gak_fatal_error(bool useErrno,const char* format, ...)
{
    va_list vargs;
    va_start(vargs,format);
    gak_error_generic(useErrno,format,vargs);
    va_end(vargs);
    exit(1);
}
int readline(FILE* fin,char* buf,size_t cap)
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
char* commasep(char** s)
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
int gak_candidate_compar_byfitness(const struct gak_candidate** left,const struct gak_candidate** right)
{
    return (*right)->fitness - (*left)->fitness;
}
void zero_last_bits(uint8_t* byte,size_t lastBits)
{
    uint8_t pattern = 0xff;
    while (lastBits > 0) {
        pattern &= ~(0x01 << (8-lastBits));
        --lastBits;
    }
    *byte &= pattern;
}

void crossover_random(const uint8_t* bitsParentA,const uint8_t* bitsParentB,uint8_t* bitsChild,size_t bits)
{
    /* choose a random cross-over point */
    size_t i, t, pnt;
    pnt = rand() % bits;
    while (bits > 0) {
        t = bits>8 ? 8 : bits;
        for (i = 0;i < t;++i)
            *bitsChild |= (bits-i>=pnt ? *bitsParentA : *bitsParentB) & (0x1<<i);
        bits -= t;
        if (t == 8) {
            ++bitsParentA;
            ++bitsParentB;
            ++bitsChild;
        }
    }
}
void crossover_alternate(const uint8_t* bitsParentA,const uint8_t* bitsParentB,uint8_t* bitsChild,size_t bits)
{
    size_t a, b;
    size_t ai, bi;
    size_t aj, bj;
    bool toggle = false;
    a = 0, b = 0;
    ai = 0, bi = 0;
    aj = 0, bj = 0;
    while (ai<bits && bi<bits) {
        size_t* t[3];
        const uint8_t** parent;
        if (toggle) {
            t[0] = &a;
            t[1] = &ai;
            t[2] = &aj;
            parent = &bitsParentA;
        }
        else {
            t[0] = &b;
            t[1] = &bi;
            t[2] = &bj;
            parent = &bitsParentB;
        }
        /* find non-zero bit in parent (if any) */
        while (*t[1] < bits && (**parent & (0x1 << *t[0])) == 0) {
            ++(*t[0]);
            ++(*t[1]);
            if (*t[0] >= 8) {
                ++(*parent);
                *t[0] = 0;
                ++(*t[2]);
            }
        }
        if (*t[1] < bits) {
            bitsChild[*t[2]] |= 0x1 << *t[0];
            ++(*t[0]);
            ++(*t[1]);
            if (*t[0] >= 8) {
                ++(*parent);
                *t[0] = 0;
                ++(*t[2]);
            }
        }
        toggle = !toggle;
    }
}


/* main program operation */
void ga_knapsack(FILE* fin,const char* filename)
{
    static crossover_func crossoverFunctions[] = {crossover_alternate,crossover_random};
    int fit;
    size_t cnt, cycles;
    size_t totalCycles, totalMutations;
    struct gak_instance* inst;
    struct gak_population* popl;
    inst = gak_instance_new(fin,filename);
    if (inst == NULL)
        /* instance could not be initialized */
        return;
    popl = gak_population_new_random(inst);
    /* produce a homogenous population and then perform cataclysmic mutation; perform
       the mutation process while there is still change; if no change occurs for more
       than 3 mutations then we finally quit */
    cycles = 0;
    totalCycles = 0;
    totalMutations = 0;
    do {
        /* breed in the population until a homogenous population is found */
        gak_population_breed_weighted(popl,inst,crossoverFunctions[rand()%2]);
        ++cycles;
    } while (cycles<1000000 && !gak_population_check_homogenous(popl,inst));
    totalCycles += cycles;
    fit = popl->members[0]->fitness;
    cnt = 3;
    do {
        /* cataclysmic mutation */
        gak_population_cataclysmic_mutation(popl,inst);
        ++totalMutations;
        cycles = 0;
        do {
            gak_population_breed_weighted(popl,inst,crossoverFunctions[rand()%2]);
            ++cycles;
        } while (cycles<1000000 && !gak_population_check_homogenous(popl,inst));
        totalCycles += cycles;
        --cnt;
        if (popl->members[0]->fitness > fit) {
            fit = popl->members[0]->fitness;
            cnt = 3;
        }
    } while (cnt > 0);
    gak_candidate_print(popl->members[0],inst);
    printf("[total cycles]   %*zu\n[total mutations]%*zu\n",6,totalCycles,6,totalMutations);
    gak_population_free(popl);
    gak_instance_free(inst);
}

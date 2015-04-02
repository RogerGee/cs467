#include <dstructs/treemap.h>
#include <dstructs/stack.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

/* constants */
enum constants
{
    SOLUTION = 0x7fffffff,
    POPULATION_MAX = 1000,
};

/* typedefs */
typedef int (*compar_func)(const void* left,const void* right);

/* we will attempt a 4-coloring of the graph; in this way we assume
   graph input will represent planar graphs */
enum color
{
    red,
    blue,
    green,
    yellow,
    top_color
};

/* 'node_map' data type: this is the representation of the graph; we assume
   the input has specified all bi-directional adjacencies */
struct node_map
{
    char** names; /* node 'm' is named 'names[m]' */
    int** map; /* for all n until 'sizes[m]', node 'm' has adjacency 'map[m][n]' */
    size_t* sizes;
    size_t cnt, size;
};
static bool node_map_init_fromfile(struct node_map* map,FILE* file,const char* name);
static void node_map_delete(struct node_map* map);

/* 'graph' data type: stores  */
struct graph
{
    int fitness; /* cached */
    const struct node_map* map; /* referenced by all graphs */
    enum color* colors; /* unique for each graph */
};
static struct graph* graph_new_random(const struct node_map* map);
static struct graph* graph_new_offspring(const struct graph* parent);
static void graph_free(struct graph* graph);
static int graph_fitness(const struct graph* graph);
static void graph_print(const struct graph* graph);

/* 'population' data type */
struct population
{
    const struct node_map* map;
    struct graph* popl[POPULATION_MAX];
};
static void population_init(struct population* popl,const struct node_map* map);
static void population_delete(struct population* popl);
static bool population_cycle(struct population* popl);

/* functions */
void ga_graph_color(FILE* file,const char* name);

/* globals */
static const char* PROGRAM;

int main(int argc,const char* argv[])
{
    PROGRAM = argv[0];
    srand(time(NULL));
    if (argc == 1)
        ga_graph_color(stdin,"stdin");
    else {
        int i;
        for (i = 1;i < argc;++i) {
            FILE* file;
            file = fopen(argv[i],"r");
            if (file == NULL)
                fprintf(stderr,"%s: could not open file '%s': %s\n",argv[0],argv[i],strerror(errno));
            else {
                ga_graph_color(file,argv[i]);
                fclose(file);
            }
        }
    }
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
static char* findsep(char** s,char sep,bool strict)
{
    char* i, *start;
    while ( isspace(**s) ) /* leading whitespace */
        ++*s;
    start = *s; /* copy the beginning */
    while (**s && **s!=sep)
        ++*s;
    if (strict && **s!=sep)
        return NULL;
    i = *s-1;
    while ( isspace(*i) )
        *(i--) = 0;
    **s = 0;
    ++*s;
    return *start==0 ? NULL : start;
}
struct namekey
{
    char* name;
    int index;
};
static void namekey_free(struct namekey* key)
{
    free(key->name);
    free(key);
}

struct namekey_assign
{
    char* name;
    size_t nodex, adjex;
};
static int namekeycmp(struct namekey* a,struct namekey* b)
{
    return strcmp(a->name,b->name);
}
static void nameassign_free(struct namekey_assign* assign)
{
    free(assign->name);
    free(assign);
}

static size_t reallocate(void** ptr,size_t elemSz,size_t size)
{
    void* pnew;
    size_t nsize;
    nsize = size << 1;
    pnew = realloc(*ptr,nsize*elemSz);
    if (pnew == NULL) {
        fprintf(stderr,"%s: memory allocation failure\n",PROGRAM);
        exit(EXIT_FAILURE);
    }
    *ptr = pnew;
    return nsize;
}

/* node map */
bool node_map_init_fromfile(struct node_map* map,FILE* file,const char* name)
{
    /* read graph representation from file; format:
         node: adj1, adj2, ...
         ...
     */
    char linebuf[4097];
    struct namekey* key;
    struct namekey_assign* assign;
    struct stack stk; /* stack of late bindings to perform */
    struct treemap namemap; /* map of names to indeces */
    stack_init(&stk,(destructor)nameassign_free);
    treemap_init(&namemap,(key_comparator)namekeycmp,(destructor)namekey_free);
    map->cnt = 0;
    map->size = 8;
    map->names = malloc(map->size * sizeof(char*));
    map->map = malloc(map->size * sizeof(int*));
    map->sizes = malloc(map->size * sizeof(size_t));
    while (1) {
        char* item, *it;
        size_t sz = 8; /* allocation size for adjacencies */
        if ( !readline(file,linebuf,sizeof(linebuf)) )
            break;
        if (map->cnt >= map->size) { /* reallocate */
            reallocate((void**)&map->map,sizeof(int*),map->size);
            reallocate((void**)&map->names,sizeof(char*),map->size);
            map->size = reallocate((void**)&map->sizes,sizeof(size_t),map->size);
        }
        it = linebuf;
        /* get and assign node name */
        if ((item = findsep(&it,':',true)) == NULL) {
            fprintf(stderr,"%s: in file '%s': expected <node name>: <adjcencies> ...\n",PROGRAM,name);
            node_map_delete(map);
            treemap_delete(&namemap);
            stack_delete(&stk);
            return false;
        }
        map->names[map->cnt] = malloc(strlen(item)+1);
        strcpy(map->names[map->cnt],item);
        /* see if name exists in namemap; if so, then see if it has been assigned
           an index within the node_map; if so, then the name is a multiple, otherwise
           we officially bind it to a node_map index */
        if ((key = treemap_lookup(&namemap,map->names+map->cnt)) != NULL) {
            if (key->index != -1) {
                fprintf(stderr,"%s: file '%s' contains node name multiplicity\n",PROGRAM,name);
                free(map->names[map->cnt]);
                node_map_delete(map);
                treemap_delete(&namemap);
                stack_delete(&stk);
                return false;
            }
        }
        else {
            key = malloc(sizeof(struct namekey));
            key->name = malloc(strlen(map->names[map->cnt])+1);
            strcpy(key->name,map->names[map->cnt]);
            treemap_insert(&namemap,key);
        }
        key->index = map->cnt++;
        map->map[key->index] = malloc(sizeof(int) * sz);
        map->sizes[key->index] = 0;
        /* retrieve the adjacency list */
        while ((item = findsep(&it,',',false)) != NULL) {
            struct namekey* k;
            if (map->sizes[key->index] >= sz) /* reallocation */
                sz = reallocate((void**)&map->map[key->index],sizeof(int),sz);
            /* lookup item */
            k = treemap_lookup(&namemap,&item);
            if (k == NULL) {
                /* perform a late binding; we have to store indeces since the
                   array could be reallocated later */
                assign = malloc(sizeof(struct namekey_assign));
                assign->name = malloc(strlen(item)+1);
                strcpy(assign->name,item);
                assign->nodex = key->index;
                assign->adjex = map->sizes[key->index];
                stack_push(&stk,assign);
            }
            else
                map->map[key->index][map->sizes[key->index]] = k->index;
            ++map->sizes[key->index];
        }
    }
    /* perform late bindings */
    if ( !stack_is_empty(&stk) ) {
        do {
            assign = stack_top(&stk);
            key = treemap_lookup(&namemap,assign);
            if (key == NULL) {
                fprintf(stderr,"%s: in file '%s': adjacency '%s' does not map to an existing node\n",PROGRAM,name,assign->name);
                node_map_delete(map);
                treemap_delete(&namemap);
                stack_delete(&stk);
                return false;
            }
            map->map[assign->nodex][assign->adjex] = key->index;
        } while ( stack_pop(&stk) );
    }
    treemap_delete(&namemap);
    stack_delete(&stk);
    return true;
}
void node_map_delete(struct node_map* map)
{
    size_t i;
    for (i = 0;i < map->cnt;++i) {
        free(map->names[i]);
        free(map->map[i]);
    }
    free(map->names);
    free(map->map);
    free(map->sizes);
}

/* graph */
struct graph* graph_new_random(const struct node_map* map)
{
    size_t i;
    struct graph* graph;
    graph = malloc(sizeof(struct graph));
    graph->map = map;
    graph->colors = malloc(sizeof(enum color) * map->cnt);
    for (i = 0;i < map->cnt;++i)
        graph->colors[i] = (enum color) (rand() % top_color);
    graph->fitness = graph_fitness(graph);
    return graph;
}
struct graph* graph_new_offspring(const struct graph* parent)
{
    size_t i;
    struct graph* offspring;
    offspring = malloc(sizeof(struct graph));
    offspring->map = parent->map;
    offspring->colors = malloc(sizeof(enum color) * offspring->map->cnt);
    for (i = 0;i < offspring->map->cnt;++i) {
        size_t j;
        /* select colors from the parent among random nodes not adjacent to 
           the node under consideration */
        do {
            int index;
            index = parent->map->map[i][rand() % parent->map->sizes[i]];
            for (j = 0;j < parent->map->sizes[i];++j)
                if (parent->map->map[i][j] == index)
                    break;
            if (j < parent->map->sizes[i]) {
                offspring->colors[i] = parent->colors[index];
                break;
            }
        } while (true);
    }
    offspring->fitness = graph_fitness(offspring);
    return offspring;
}
void graph_free(struct graph* graph)
{
    free(graph->colors);
    free(graph);
}
int graph_fitness(const struct graph* graph)
{
    int fit = 0;
    size_t iter;
    bool bad = false;
    /* the graph receives a "point" for each adjacency it has without like colors */
    for (iter = 0;iter < graph->map->cnt;++iter) {
        size_t jter;
        for (jter = 0;jter < graph->map->sizes[iter];++jter) {
            if (graph->colors[graph->map->map[iter][jter]] != graph->colors[iter])
                ++fit;
            else
                bad = true;
        }
    }
    return !bad ? SOLUTION : fit;
}
void graph_print(const struct graph* graph)
{
    static const char* color_names[] = {
        "r", "b", "g", "y"
    };
    size_t i;
    if (graph->fitness == SOLUTION)
        printf("sol ");
    else
        printf("%*d ",4,graph->fitness);
    printf("{%s[%s]",graph->map->names[0],color_names[graph->colors[0]]);
    for (i = 1;i < graph->map->cnt;++i)
        printf(", %s[%s]",graph->map->names[i],color_names[graph->colors[i]]);
    puts("}");
}

static int fitness_compar(const struct graph* left,const struct graph* right)
{
    return left->fitness - right->fitness;
}

/* population */
void population_init(struct population* popl,const struct node_map* map)
{
    size_t i;
    popl->map = map;
    for (i = 0;i < POPULATION_MAX;++i)
        popl->popl[i] = graph_new_random(map);
    qsort(popl->popl,POPULATION_MAX,sizeof(struct graph*),(compar_func)fitness_compar);
}
void population_delete(struct population* popl)
{
    size_t i;
    for (i = 0;i < POPULATION_MAX;++i)
        graph_free(popl->popl[i]);
}
bool population_cycle(struct population* popl)
{
    int r, a;
    size_t ins;
    struct graph* parent, *child;
    /* choose parent from among population; weight by fitness */
    r = rand() % POPULATION_MAX;
    a = rand() % POPULATION_MAX;
    if (a < r)
        r = a;
    parent = popl->popl[r];
    /* generate child and insert into population */
    child = graph_new_offspring(parent);
    ins = 0;
    while (ins < POPULATION_MAX && child->fitness < popl->popl[ins]->fitness)
        ++ins;
    if (ins >= POPULATION_MAX)
        graph_free(child); /* wasn't any better */
    else {
        size_t i, last;
        /* delete the last population member and bump the rest down */
        last = POPULATION_MAX - 1;
        graph_free(popl->popl[last]);
        for (i = last;i > ins;--i)
            popl->popl[i] = popl->popl[i-1];
        popl->popl[ins] = child;
    }
    return popl->popl[0]->fitness == SOLUTION;
}

void ga_graph_color(FILE* file,const char* name)
{
    size_t iterations;
    struct node_map map;
    struct population pop;
    if ( !node_map_init_fromfile(&map,file,name) )
        return;
    population_init(&pop,&map);

    iterations = 0;
    while (true) {
        ++iterations;
        if (population_cycle(&pop)) {
            graph_print(pop.popl[0]);
            printf("%zu cycles\n",iterations);
            break;
        }
        if (iterations % 1000000 == 0)
            graph_print(pop.popl[0]);
    }

    population_delete(&pop);
    node_map_delete(&map);
}

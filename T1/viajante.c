#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <float.h>

#include "viajante.h"

typedef struct{
    int *z;
    int n;
    double **m;
    int nperm;
    double res;
} Args;

void *thread_func(void *ptr){
    Args *a = (Args *)ptr;
    a->res = viajante(a->z, a->n, a->m, a->nperm);
    return NULL;
}

double viajante_par(int z[], int n, double **m, int nperm, int p) {
    pthread_t pid[p];
    Args args[p];
  
    for (int k = 0; k < p; k++){
        args[k].z = malloc((n+1) * sizeof(int));
        args[k].n = n;
        args[k].m = m;
        args[k].nperm = nperm/p;

        pthread_create(&pid[k], NULL, thread_func, &args[k]);
    }
    
    double min = DBL_MAX;
    for (int k = 0; k < p; k++){
        pthread_join(pid[k], NULL);
        if (args[k].res < min){
            min = args[k].res;
            for (int j= 0; j<=n; j++){
                z[j]= args[k].z[j];
            }   
        }
        free(args[k].z);   
    }
    return min;
}

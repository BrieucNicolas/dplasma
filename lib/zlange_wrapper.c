/*
 * Copyright (c) 2011-2012 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 *
 */
#include "dague_internal.h"
#include <plasma.h>
#include <core_blas.h>
#include "dplasma.h"
#include "dplasma/lib/dplasmatypes.h"
#include "dplasma/lib/dplasmaaux.h"
#include "data_dist/matrix/two_dim_rectangle_cyclic.h"

#include "zlange_max_cyclic.h"
#include "zlange_inf_cyclic.h"
#include "zlange_one_cyclic.h"
#include "zlange_frb_cyclic.h"

static inline int dague_imax(int a, int b) { return (a >= b) ? a : b; };

dague_handle_t* dplasma_zlange_New( PLASMA_enum ntype,
                                    int P, int Q,
                                    tiled_matrix_desc_t *A,
                                    double *result )
{
    int m, n, mb, nb, elt;
    two_dim_block_cyclic_t *Wcol;
    two_dim_block_cyclic_t *Welt;
    dague_handle_t *dague_zlange = NULL;

    /* Create the workspace */
    Wcol = (two_dim_block_cyclic_t*)malloc(sizeof(two_dim_block_cyclic_t));
    Welt = (two_dim_block_cyclic_t*)malloc(sizeof(two_dim_block_cyclic_t));

    /* Warning: Pb with smb/snb when mt/nt lower than P/Q */
    switch( ntype ) {
    case PlasmaFrobeniusNorm:
        mb = 2;
        nb = 1;
        m  = A->mt * mb;
        n  = Q;
        elt = 2;
        break;
    case PlasmaInfNorm:
        mb = A->mb;
        nb = 1;
        m  = A->mt * mb;
        n  = Q;
        elt = 1;
        break;
    case PlasmaOneNorm:
        mb = 1;
        nb = A->nb;
        m  = P;
        n  = dague_imax(A->nt, Q) * nb;
        elt = 1;
        break;
    case PlasmaMaxNorm:
    default:
        mb = 1;
        nb = 1;
        m  = A->mt;
        n  = Q;
        elt = 1;
    }

    PASTE_CODE_INIT_AND_ALLOCATE_MATRIX(
        (*Wcol), two_dim_block_cyclic,
        (Wcol, matrix_RealDouble, matrix_Tile,
         A->super.nodes, A->super.cores, A->super.myrank,
         mb,  nb,   /* Dimesions of the tile                */
         m,   n,    /* Dimensions of the matrix             */
         0,   0,    /* Starting points (not important here) */
         m,   n,    /* Dimensions of the submatrix          */
         1, 1, P));

    PASTE_CODE_INIT_AND_ALLOCATE_MATRIX(
        (*Welt), two_dim_block_cyclic,
        (Welt, matrix_RealDouble, matrix_Tile,
         A->super.nodes, A->super.cores, A->super.myrank,
         elt,   1,  /* Dimesions of the tile                */
         elt*P, Q,  /* Dimensions of the matrix             */
         0,     0,  /* Starting points (not important here) */
         elt*P, Q,  /* Dimensions of the submatrix          */
         1, 1, P));

    /* Create the DAG */
    switch( ntype ) {
    case PlasmaFrobeniusNorm:
        dague_zlange = (dague_handle_t*)dague_zlange_frb_cyclic_new(
            P, Q, (dague_ddesc_t*)A, (dague_ddesc_t*)Wcol, (dague_ddesc_t*)Welt, result);
        break;
    case PlasmaInfNorm:
        dague_zlange = (dague_handle_t*)dague_zlange_inf_cyclic_new(
            P, Q, (dague_ddesc_t*)A, (dague_ddesc_t*)Wcol, (dague_ddesc_t*)Welt, result);
        break;
    case PlasmaOneNorm:
        dague_zlange = (dague_handle_t*)dague_zlange_one_cyclic_new(
            P, Q, (dague_ddesc_t*)A, (dague_ddesc_t*)Wcol, (dague_ddesc_t*)Welt, result);
        break;
    case PlasmaMaxNorm:
    default:
        dague_zlange = (dague_handle_t*)dague_zlange_max_cyclic_new(
            P, Q, (dague_ddesc_t*)A, (dague_ddesc_t*)Wcol, (dague_ddesc_t*)Welt, result);
    }

    /* Set the datatypes */
    dplasma_add2arena_tile(((dague_zlange_inf_cyclic_handle_t*)dague_zlange)->arenas[DAGUE_zlange_inf_cyclic_DEFAULT_ARENA],
                           A->mb*A->nb*sizeof(dague_complex64_t),
                           DAGUE_ARENA_ALIGNMENT_SSE,
                           MPI_DOUBLE_COMPLEX, A->mb);
    dplasma_add2arena_rectangle(((dague_zlange_inf_cyclic_handle_t*)dague_zlange)->arenas[DAGUE_zlange_inf_cyclic_COL_ARENA],
                                mb * nb * sizeof(double), DAGUE_ARENA_ALIGNMENT_SSE,
                                MPI_DOUBLE, mb, nb, -1);
    dplasma_add2arena_rectangle(((dague_zlange_inf_cyclic_handle_t*)dague_zlange)->arenas[DAGUE_zlange_inf_cyclic_ELT_ARENA],
                                elt * sizeof(double), DAGUE_ARENA_ALIGNMENT_SSE,
                                MPI_DOUBLE, elt, 1, -1);

    return (dague_handle_t*)dague_zlange;
}

void
dplasma_zlange_Destruct( dague_handle_t *o )
{
    dague_zlange_inf_cyclic_handle_t *dague_zlange = (dague_zlange_inf_cyclic_handle_t *)o;
    two_dim_block_cyclic_t *Wcol, *Welt;

    Wcol = (two_dim_block_cyclic_t*)(dague_zlange->Wcol);
    dague_data_free( Wcol->mat );
    dague_ddesc_destroy( dague_zlange->Wcol );
    free( dague_zlange->Wcol );

    Welt = (two_dim_block_cyclic_t*)(dague_zlange->Welt);
    dague_data_free( Welt->mat );
    dague_ddesc_destroy( dague_zlange->Welt );
    free( dague_zlange->Welt );

    dplasma_datatype_undefine_type( &(dague_zlange->arenas[DAGUE_zlange_inf_cyclic_DEFAULT_ARENA]->opaque_dtt) );
    dplasma_datatype_undefine_type( &(dague_zlange->arenas[DAGUE_zlange_inf_cyclic_COL_ARENA]->opaque_dtt) );
    dplasma_datatype_undefine_type( &(dague_zlange->arenas[DAGUE_zlange_inf_cyclic_ELT_ARENA]->opaque_dtt) );

    DAGUE_INTERNAL_HANDLE_DESTRUCT(o);
}

double dplasma_zlange( dague_context_t *dague,
                       PLASMA_enum ntype,
                       tiled_matrix_desc_t *A)
{
    double result;
    dague_handle_t *dague_zlange = NULL;

    int P = ((two_dim_block_cyclic_t*)A)->grid.rows;
    int Q = ((two_dim_block_cyclic_t*)A)->grid.cols;

    dague_zlange = dplasma_zlange_New(ntype, P, Q, A, &result);

    if ( dague_zlange != NULL )
    {
        dague_enqueue( dague, (dague_handle_t*)dague_zlange);
        dplasma_progress(dague);
        dplasma_zlange_Destruct( dague_zlange );
    }

    return result;
}


/**@file   graph.c
 * @brief  grpha data for Chance-constrained Program with Random Right-hand Side
 * @author Wei Lv
 * @author Wei-Kun Chen
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include "graph.h"


/** Judge the relationship of dominance */
SCIP_Bool judgeDominant(
	SCIP*                   scip,               /**< SCIP data structure */
	GRAPH*                  graph,              /**< pointer to store graph */
	SCIP_Real**             randomRhs,          /**< the random right-hand side matrix */
	int                     dimension,          /**< the dimension of random vector */
	int 							i,						  /**< scenario i */
	int							j,						  /**< scenario j */
	SCIP_Bool*					isGeq,				  /**< is the relationship greater than or equal to? */
	SCIP_Bool*					isLeq					  /**< is the relationship less than or equal to? */
)
{
	for( int t = 0; t < dimension; t++ )
	{
		if( !SCIPisGE(scip, randomRhs[i][t], randomRhs[j][t]) )
		{
			(*isGeq) = FALSE;
			break;
		}
	}
	for( int t = 0; t < dimension; t++ )
	{
		if( !SCIPisLE(scip, randomRhs[i][t], randomRhs[j][t]) )
		{
			(*isLeq) = FALSE;
			break;
		}
	}

	return TRUE;
}

/** store the graph data */
SCIP_Bool storeGraphData(
	SCIP*                   scip,               /**< SCIP data structure */
	GRAPH*                  graph,              /**< pointer to store graph */
	int 							i,						  /**< scenario i */
	int							j,						  /**< scenario j */
	SCIP_Bool					isGeq,				  /**< is the relationship greater than or equal to? */
	SCIP_Bool					isLeq					  /**< is the relationship less than or equal to? */
)
{
	if( isGeq == TRUE )
	{
		graph->edges[graph->nedges].tail = i;
		graph->edges[graph->nedges].head = j;
		graph->nedges++;
		graph->edgeTH[i][j] = TRUE;

		graph->vOut[i][graph->vOutSize[i]] = j;
		graph->vIn[j][graph->vInSize[j]] = i;
		graph->vOutSize[i]++;
		graph->vInSize[j]++;

		assert( isLeq == FALSE );
	}

	if( isLeq == TRUE )
	{
		graph->edges[graph->nedges].tail = j;
		graph->edges[graph->nedges].head = i;
		graph->nedges++;
		graph->edgeTH[j][i] = TRUE;

		graph->vOut[j][graph->vOutSize[j]] = i;
		graph->vIn[i][graph->vInSize[i]] = j;
		graph->vOutSize[j]++;
		graph->vInSize[i]++;
	}

	return TRUE;
}

/** create a graph */
SCIP_Bool createGraph(
	SCIP*                   scip,               /**< SCIP data structure */
	GRAPH*                  graph,              /**< pointer to store graph */
	SCIP_Real**             randomRhs,          /**< the random right-hand side matrix */
	int*                    basicVarzInd,       /**< the set of indexes for scenarios with right-side hand greater than the lower bound */
	int                     basicNScenario,     /**< the number of scenarios with right-side hand greater than the lower bound */
	int                     nScenarion,         /**< the number of scenarios */
	int                     dimension           /**< the dimension of random vector */
)
{
	SCIP_Bool isGeq;
	SCIP_Bool isLeq;

	/* allocate buffer memory for storing graph data */
	SCIP_CALL( SCIPallocBufferArray(scip, &(graph->edges),  		nScenarion*nScenarion) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(graph->edgeTH),  	nScenarion) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(graph->vIn),   		nScenarion) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(graph->vOut),  		nScenarion) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(graph->vInSize),		nScenarion) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(graph->vOutSize),	nScenarion) );
	for( int i = 0; i < nScenarion; i++ )
	{
		SCIP_CALL( SCIPallocBufferArray(scip, &(graph->edgeTH[i]), 	 nScenarion) );
		SCIP_CALL( SCIPallocBufferArray(scip, &(graph->vIn[i]),    	 nScenarion) );
		SCIP_CALL( SCIPallocBufferArray(scip, &(graph->vOut[i]),   	 nScenarion) );
	}

	graph->nedges = 0;
	if( basicNScenario == 0 )
	{
		for( int i = 0; i < nScenarion; i++ )
		{
			graph->vInSize[i]  = 0;
			graph->vOutSize[i] = 0;
		}
		/* caculate the (basic) domination pair (Ruszczynski's condition) */
		for( int i = 0; i < nScenarion; i++ )
		{
			for( int j = i+1; j < nScenarion; j++ )
			{
				isGeq   = TRUE;
				isLeq   = TRUE;
				graph->edgeTH[i][j]   = FALSE;
				graph->edgeTH[j][i]   = FALSE;
				judgeDominant(scip, graph, randomRhs, dimension, i, j, &isGeq, &isLeq);
				storeGraphData(scip, graph, i, j, isGeq, isLeq);
			}
		}
	}
	else
	{
		int basicI;
		int basicJ;
		for( int i = 0; i < basicNScenario; i++ )
		{
			graph->vInSize[i]  = 0;
			graph->vOutSize[i] = 0;
		}
		/* caculate the (basic) domination pair (an improved domination condition) */
		for( int i = 0; i < basicNScenario; i++ )
		{
			basicI = basicVarzInd[i];
			for( int j = i+1; j < basicNScenario; j++ )
			{
				isGeq	= TRUE;
				isLeq = TRUE;
				graph->edgeTH[i][j] = FALSE;
				graph->edgeTH[j][i] = FALSE;
				basicJ = basicVarzInd[j];
				judgeDominant(scip, graph, randomRhs, dimension, basicI, basicJ, &isGeq, &isLeq);
				storeGraphData(scip, graph, i, j, isGeq, isLeq);
			}
		}
	}

	SCIPinfoMessage(scip, NULL, "NumBasicDomIneqs: %d\n", graph->nedges);

	return TRUE;
}

/** find the dominance inequalities */
SCIP_Bool findDomIneqs(
	SCIP*                   scip,               /**< SCIP data structure */
	SCIP_Bool					isGraphProp,		  /**< is it a domain propagation? */
	GRAPH*                  graph,              /**< pointer to store graph */
	SCIP_Real**             randomRhs,          /**< the random right-hand side matrix */
	int*                    basicVarzInd,       /**< the set of indexes for scenarios with right-side hand greater than the lower bound */
	int                     basicNScenario,     /**< the number of scenarios with right-side hand greater than the lower bound */
	int                     nScenarion,         /**< the number of scenarios */
	int                     dimension           /**< the dimension of random vector */
)
{
   int       nVIn;
   int       nVOut;
	SCIP_Bool flag;

   assert(nScenarion > 0);
   assert(dimension  > 0);

   /* create a graph */
	createGraph(scip, graph, randomRhs, basicVarzInd, basicNScenario, nScenarion, dimension);

   /* redundancy detection for dominance inequalities:
      edge (i, j) in A is redundant if exist s in V, s neq i, j such that (i, s) in A and (s, j) in A
   */
	graph->nPreCons = 0;
   for( int i = 0; i < graph->nedges; i++ )
   {
      flag = TRUE;
      /* determining whether a bidirectional edge exists between two points */
      assert(graph->edgeTH[graph->edges[i].head][graph->edges[i].tail] == FALSE);
      nVIn = graph->vInSize[graph->edges[i].head];
      for( int s = 0; s < nVIn; s++ )
      {
			if( graph->vIn[graph->edges[i].head][s] != graph->edges[i].tail )
         {
            nVOut = graph->vInSize[graph->vIn[graph->edges[i].head][s]];
            for( int j = 0; j < nVOut; j++ )
            {
               if( graph->vIn[graph->vIn[graph->edges[i].head][s]][j] == graph->edges[i].tail )
               {
                  flag = FALSE;
						graph->edgeTH[graph->edges[i].tail][graph->edges[i].head] = FALSE;
                  break;
               }
            }
         }
         if( flag == FALSE )
         {
            break;
         }
      }
      if( flag == TRUE )
      {
         graph->nPreCons++;
      }
   }
   SCIPinfoMessage(scip, NULL, "NumNonReDomIneqs: %d\n", graph->nPreCons);

   return TRUE;
}

/** free a graph */
SCIP_Bool freeGraph(
	SCIP*                   scip,               /**< SCIP data structure */
   GRAPH*					   graph,              /**< graph data structure */
   int                     nScenarion          /**< the number of scenarios */
   )
{
	assert(graph != NULL);

	/* free memory of arrays */
	for ( int i = 0; i < nScenarion; i++ )
	{
		SCIPfreeBufferArray(scip, &(graph->edgeTH[i]));
		SCIPfreeBufferArray(scip, &(graph->vIn[i]));
		SCIPfreeBufferArray(scip, &(graph->vOut[i]));
	}
	SCIPfreeBufferArray(scip, &(graph->edgeTH));
	SCIPfreeBufferArray(scip, &(graph->vIn));
	SCIPfreeBufferArray(scip, &(graph->vOut));
	SCIPfreeBufferArray(scip, &(graph->vInSize));
	SCIPfreeBufferArray(scip, &(graph->vOutSize));
	SCIPfreeBufferArray(scip, &(graph->edges));

	return TRUE;
}




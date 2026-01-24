
/**@file   graph.h
 * @brief  grpha data for Chance-constrained Program with Random Right-hand Side
 * @author Wei Lv
 * @author Wei-Kun Chen
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __CCP_GRAPH__
#define __CCP_GRAPH__

#include "scip/scip.h"

/** an edge in the graph */
typedef struct GraphEdge
{
   int tail;                                   /**< tail of this edge */
   int head;                                   /**< head of this edge */
}GRAPHEDGE;

/** directed graph */
typedef struct Graph
{
   int                      nedges;            /**< number of edges */
   int                      nPreCons;          /**< number of precedence constraints */
   SCIP_Bool**              edgeTH;            /**< store the edges existing in the graph (tail-head) */
   int**                    vIn;               /**< a matrix that stores the incoming  arc's tail of all vertices */
   int**                    vOut;              /**< a matrix that stores the outcoming arc's head of all vertices */
   int*                     vInSize;           /**< a vecotr that stores the number of the incoming  arc's tail for a vertice */
   int*                     vOutSize;          /**< a vecotr that stores the number of the outcoming arc's head for a vertice */
   GRAPHEDGE*               edges;             /**< the edges of the graph */
}GRAPH;


/** Judge the relationship of dominance */
SCIP_Bool judgeDominant(
	SCIP*                   scip,               /**< SCIP data structure */
	GRAPH*                  graph,              /**< pointer to store graph */
	SCIP_Real**             randomRhs,          /**< the random right-hand side matrix */
	int                     dimension,          /**< the dimension of random vector */
	int 							i,						  /**< scenario i */
	int							j,						  /**< scenario j */
	SCIP_Bool*					isGeq,				  /**< is the relationship greater than or equal to ? */
	SCIP_Bool*					isLeq					  /**< is the relationship less than or equal to ? */
);

SCIP_Bool storeGraphData(
	SCIP*                   scip,               /**< SCIP data structure */
	GRAPH*                  graph,              /**< pointer to store graph */
	int 							i,						  /**< scenario i */
	int							j,						  /**< scenario j */
	SCIP_Bool					isGeq,				  /**< is the relationship greater than or equal to ? */
	SCIP_Bool					isLeq					  /**< is the relationship less than or equal to ? */
);

/** create a graph */
SCIP_Bool createGraph(
	SCIP*                   scip,               /**< SCIP data structure */
	GRAPH*                  graph,              /**< pointer to store graph */
	SCIP_Real**             randomRhs,          /**< the random right-hand side matrix */
	int*                    basicVarzInd,       /**< the set of indexes for scenarios with right-side hand greater than the lower bound */
	int                     basicNScenario,     /**< the number of scenarios with right-side hand greater than the lower bound */
	int                     nScenarion,         /**< the number of scenarios */
	int                     dimension           /**< the dimension of random vector */
);

/** get the dominance inequalities*/
SCIP_Bool findDomIneqs(
   SCIP*                   scip,               /**< SCIP data structure */
	SCIP_Bool					isGraphProp,		  /**< is it a domain propagation ? */
   GRAPH*                  graph,              /**< pointer to store graph */
   SCIP_Real**             randomRhs,          /**< the random right-hand side matrix */
   int*                    basicVarzInd,       /**< the set of indexes for scenarios with right-side hand greater than the lower bound */
   int                     basicNScenario,     /**< the number of scenarios with right-side hand greater than the lower bound */
   int                     nScenarion,         /**< the number of scenarios */
   int                     dimension           /**< the dimension of random vector */
);

/** free graph data */
SCIP_Bool freeGraph(
	SCIP*                   scip,               /**< SCIP data structure */
   GRAPH*						graph,              /**< raph data structure */
   int                     nScenarion          /**< the number of scenarios */
);

#endif /* __CCP_GRAPH__ */

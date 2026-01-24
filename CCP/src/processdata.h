/**@file   processdata.h
 * @brief  process data for Chance-constrained Program with Random Right-hand Side
 * @author Wei Lv
 * @author Wei-Kun Chen
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __CCP_PROCESSDATA__
#define __CCP_PROCESSDATA__

#include "scip/scip.h"


SCIP_Bool sortRhs(
	SCIP*               	scip,             	   /**< SCIP data structure */
	SCIP_Real 		      epsilon,                /**< confidence parameter */
	SCIP_Bool		      isEqualProbability,     /**< should equal probability be used in the problem? */
	int	        			nScenario,		  		   /**< pointer to store the number of scenarios on exit */
	int						dimension,				   /**< pointer to store the dimension of random vector on exit */
	SCIP_Real**  			randomRhs,				   /**< pointer to store the random right-hand side matrix on exit */
	SCIP_Real*** 			sortTransRhs,			   /**< pointer to store sorted transpose of random right-hand side matrix on exit */
	int***					sortTransRhsInd,		   /**< pointer to store the index of sorted transpose of random right-hand side matrix on exit */
	SCIP_Real*	 			proba,					   /**< pointer to store the scenario probability vector on exit */
	int**						KI							   /**< pointer to store the index vector of lower bound for variables v on exit  */
);

/** strengthen right hand side */
SCIP_Bool strengthenedRhs(
   SCIP*                 scip,                  /**< SCIP data structure */
   SCIP_Real***          stRhs,                 /**< pointer to store the strengthened random right-hand side matrix on exit */
   SCIP_Real**           randomRhs,             /**< the random right-hand side matrix */
   int**                 sortTransRhsInd,       /**< the index of sorted transpose of random right-hand side matrix  */
   int*                  KI,                    /**< the index vector of lower bound for variables v */
   int                   nScenario,             /**< the number of scenarios */
   int                   dimension              /**< the dimension of random vector */
);

/* aggregation scenario, revise right-hand side vector and probability simultaneously */
SCIP_Bool aggScenario(
	SCIP*               	scip,             	   /**< SCIP data structure */
	SCIP_Real 		      epsilon,                /**< confidence parameter */
	SCIP_Real**  			randomRhs,				   /**< random right-hand side matrix */
	SCIP_Real***  			aggRhs,				   	/**< pointer to store the right-hand side matrix after aggregation on exit */
	SCIP_Real** 			aggProba,					/**< pointer to store the scenario probability after aggregation on exit */
	SCIP_Real*	 			proba,					   /**< scenario probability vector */
	int*        			nAggScenario,		  		/**< pointer to store the number of scenarios after aggregation on exit */
	int                  nScenario,              /**< number of scenarios */
	int						dimension				   /**< dimension of random vector  */
);

/** find the basic scenarios that the strengthened random right-side hand is greater than or equal to the lower bound */
SCIP_Bool findBasicScenes(
	SCIP*                 scip,                  /**< SCIP data structure */
   SCIP_Real**           stRhs,                 /**< pointer to store the strengthened random right-hand side matrix on exit */
   SCIP_Real**           randomRhs,             /**< the random right-hand side matrix */
	int**                 sortTransRhsInd,       /**< the index of sorted transpose of random right-hand side matrix  */
   int**						 basicVarzInd,				/**< the index of variable z cooresponding to basic scenarios */
   int*						 basicNScenario,			/**< the number of essential scenarios */
	int*                  KI,                    /**< the index vector of lower bound for variables v */
	int                   nScenario,             /**< the number of scenarios */
   int                   dimension              /**< the dimension of random vector */
);

/** free process data arrays */
SCIP_Bool freeProcessData(
	SCIP*                scip,               		/**< SCIP data structure */
	int*						KI,							/**< index vector of lower bound for variables v */
	SCIP_Real** 			sortTransRhs,			   /**< sorted transpose of random right-hand side matrix */
	int**						sortTransRhsInd,		   /**< index of sorted transpose of random right-hand side matrix */
	int*                 aggKI,                  /**< the index vector of lower bound for variables v */
   SCIP_Real** 			sortAggRhs,			   	/**< sorted transpose of random right-hand side matrix after aggregation */
	int**						sortAggRhsInd,		   	/**< index of sorted transpose of random right-hand side matrix after aggregation */
	SCIP_Real* 				aggProba,					/**< scenario probability after aggregation */
	int                  dimension              	/**< the dimension of random vector */
);

/** get problem name  */
SCIP_Bool getProblemName(
	SCIP*                scip,               		/**< SCIP data structure */
   char*           		cpyfilename,           	/**< input filename */
   char*                name           	  		/**< output problemname */
);

/** compute random right-hand side matrix (lot-sizing problem) */
SCIP_Bool sumRhs(
	SCIP*                scip,               		/**< SCIP data structure */
	SCIP_Real***			rhsData,						/**< right-hand side matrix data */
	SCIP_Real***			randomRhs,					/**< the random right-hand side matrix */
	int                  nScenario,             	/**< the number of scenarios */
   int                  dimension               /**< the dimension of random vector */
);

/** calculate the true dominance ratio */
SCIP_Bool dominanceRatio(
	SCIP*                scip,               		/**< SCIP data structure */
	SCIP_Real**				randomRhs,					/**< the random right-hand side matrix */
	int                  nScenario,             	/**< the number of scenarios */
   int                  dimension               /**< the dimension of random vector */
);

#endif /* __CCP_PROCESSDATA__ */

/**@file   processdata.h
 * @brief  process data for Chance-constrained Program with Random Right-hand Side
 * @author Wei Lv
 * @author Wei-Kun Chen
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include "processdata.h"
#include "reader_ccrp.h"


/** compare values of coefficients */
static
SCIP_DECL_SORTINDCOMP(compareCoefs)
{
   SCIP_Real coef1;
   SCIP_Real coef2;
   SCIP_Real* coefs = (SCIP_Real*) dataptr;

   coef1 = coefs[ind1];
   coef2 = coefs[ind2];
   if( coef1 < coef2 )
	{
   	return -1;
	}
	if( coef2 < coef1 )
	{
   	return 1;
	}

   return 0;
}

/** sort right-hand vector and then compute the index vector of lower bound for each dimension */
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
)
{
	SCIP_Real sum;
	SCIP_Real k;

	assert( randomRhs != NULL );
	assert( sortTransRhs != NULL );
	assert( sortTransRhsInd != NULL );

	/* allocate buffer memory for transpose of the random right-hand side matrix and its index*/
	SCIP_CALL( SCIPallocBufferArray(scip, sortTransRhs, dimension) );
	SCIP_CALL( SCIPallocBufferArray(scip, sortTransRhsInd, dimension) );
	for( int i = 0; i < dimension; i++ )
	{
		SCIP_CALL( SCIPallocBufferArray(scip, &((*sortTransRhs)[i]), nScenario) );
		SCIP_CALL( SCIPallocBufferArray(scip, &((*sortTransRhsInd)[i]), nScenario) );
	}
	for( int s = 0; s < nScenario; s++ )
   {
      for( int i = 0; i < dimension; i++ )
      {
			(*sortTransRhsInd)[i][s] = s;
         (*sortTransRhs)[i][s] = randomRhs[s][i];
      }
   }
	for( int i = 0; i < dimension; i++ )
	{
		SCIPsortDownInd((*sortTransRhsInd)[i], compareCoefs, (void*) (*sortTransRhs)[i], nScenario);
		SCIPsortDownReal((*sortTransRhs)[i], nScenario);
	}

	/* allocate buffer memory for index vector of lower bound for variables v */
	SCIP_CALL( SCIPallocBufferArray(scip, KI, dimension) );
   for( int i = 0; i < dimension; i++ )
   {
      sum = 0;
      for( int s = 0; s < nScenario; s++ )
      {
        sum += proba[(*sortTransRhsInd)[i][s]];
        	if( SCIPisGT(scip, sum, epsilon) )
        	{
				(*KI)[i] = s;
				break;
        	}
      }
      if( isEqualProbability == TRUE )
      {
			k = epsilon * nScenario;
			k = SCIPfloor(scip, k);
			assert( SCIPisEQ(scip, (*KI)[i], k) );
      }
   }

	return TRUE;
}

/** strengthen right hand side */
SCIP_Bool strengthenedRhs(
   SCIP*                 scip,                     /**< SCIP data structure */
   SCIP_Real***          stRhs,                    /**< pointer to store the strengthened random right-hand side matrix on exit */
   SCIP_Real**           randomRhs,                /**< the random right-hand side matrix */
   int**                 sortTransRhsInd,          /**< the index of sorted transpose of random right-hand side matrix  */
   int*                  KI,                       /**< the index vector of lower bound for variables v */
   int                   nScenario,                /**< the number of scenarios */
   int                   dimension                 /**< the dimension of random vector */
   )
{
   SCIP_Bool flag;

   assert( randomRhs != NULL );
   assert( sortTransRhsInd != NULL );
   assert( KI != NULL );

   /* allocate buffer memory for storing the strengthened random right-hand side matrix */
	SCIP_CALL( SCIPallocBufferArray(scip, stRhs, nScenario) );
	for( int s = 0; s < nScenario; s++ )
	{
		SCIP_CALL( SCIPallocBufferArray(scip, &((*stRhs)[s]), dimension) );
	}

   /* details of the computation steps */
   for( int s = 0; s < nScenario; s++ )
   {
      for( int i = 0; i < dimension; i++ )
      {
         flag = SCIPisLT(scip, randomRhs[s][i], randomRhs[sortTransRhsInd[i][KI[i]]][i]);
         (*stRhs)[s][i] = flag ? randomRhs[sortTransRhsInd[i][KI[i]]][i] : randomRhs[s][i];
      }
   }

   return TRUE;
}


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
)
{

	SCIP_Bool mark;
	SCIP_Real sum;
	SCIP_Bool* flag;
	SCIP_Real* tempProb;

	/* allocate buffer memory */
	SCIP_CALL( SCIPallocBufferArray(scip, &flag, nScenario) );
	SCIP_CALL( SCIPallocBufferArray(scip, &tempProb, nScenario) );

	SCIP_CALL( SCIPallocBufferArray(scip, aggRhs, nScenario) );
	for( int s = 0; s < nScenario; s++ )
	{
		SCIP_CALL( SCIPallocBufferArray(scip, &((*aggRhs)[s]), dimension) );
	}
   SCIP_CALL( SCIPallocBufferArray(scip, aggProba, nScenario) );

	for( int s = 0; s < nScenario; s++ )
	{
		flag[s] = FALSE;
		tempProb[s] = proba[s];
	}

	for( int i = 0; i < nScenario; i++ )
	{
		if( flag[i] == TRUE )
			continue;
		for( int j = i+1; j < nScenario; j++ )
		{
			if( flag[j] == TRUE )
				continue;
			mark = TRUE;
			for( int t = 0; t < dimension; t++ )
			{
				if( !SCIPisEQ(scip, randomRhs[i][t], randomRhs[j][t]) )
				{
					mark = FALSE;
					break;
				}
			}
			if( mark == TRUE )
			{
				flag[j] = TRUE;
				tempProb[i] += proba[j];
			}
		}
	}

	sum = 0;
	(*nAggScenario) = 0;
	for( int s = 0; s < nScenario; s++ )
	{
		if( flag[s] == FALSE )
		{
			(*aggProba)[*nAggScenario] = tempProb[s];
			sum += tempProb[s];
			for( int t = 0; t < dimension; t++ )
			{
				(*aggRhs)[*nAggScenario][t]=randomRhs[s][t];
			}
			(*nAggScenario)++;
		}
	}
	assert( SCIPisEQ(scip, sum, 1.0) );
	SCIPinfoMessage(scip, NULL, "NumAggScenario: %d \n", (*nAggScenario));

	SCIPfreeBufferArray(scip, &flag);
	SCIPfreeBufferArray(scip, &tempProb);
	return TRUE;
}


/** find the basic scenarios that the strengthened random right-side hand is greater than or equal to the lower bound */
SCIP_Bool findBasicScenes(
	SCIP*                 scip,                  /**< SCIP data structure */
   SCIP_Real**           stRhs,                 /**< pointer to store the strengthened random right-hand side matrix on exit */
   SCIP_Real**           randomRhs,             /**< the random right-hand side matrix */
	int**                 sortTransRhsInd,       /**< the index of sorted transpose of random right-hand side matrix  */
   int**						 basicVarzInd,				/**< the index of variable z corresponding to basic scenarios */
   int*						 basicNScenario,			/**< the number of essential scenarios */
	int*                  KI,                    /**< the index vector of lower bound for variables v */
	int                   nScenario,             /**< the number of scenarios */
   int                   dimension              /**< the dimension of random vector */
)
{
	/* allocate buffer memory for storing the index vector of variable z corresponding to basic scenarios */
	SCIP_CALL( SCIPallocBufferArray(scip, basicVarzInd, nScenario) );

	*basicNScenario = 0;
	for(int s = 0; s < nScenario; s++ )
	{
		for( int i = 0; i < dimension; i++ )
		{
			if( SCIPisGT(scip, stRhs[s][i], stRhs[sortTransRhsInd[i][KI[i]]][i]) )
			{
				(*basicVarzInd)[*basicNScenario] = s;
				(*basicNScenario)++;
				break;
			}
		}
	}

	/* output data information */
	SCIPinfoMessage(scip, NULL, "BasicNumScenario: %d \n", *basicNScenario);

 	return TRUE;
}

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
)
{
	/* free memory of arrays */
	SCIPfreeBufferArray(scip, &KI);
	SCIPfreeBufferArray(scip, &aggKI);
   SCIPfreeBufferArray(scip, &aggProba);
	for( int i = 0; i < dimension; i++ )
	{
		SCIPfreeBufferArray(scip, &sortTransRhs[i]);
		SCIPfreeBufferArray(scip, &sortTransRhsInd[i]);
		SCIPfreeBufferArray(scip, &sortAggRhs[i]);
		SCIPfreeBufferArray(scip, &sortAggRhsInd[i]);
	}
	SCIPfreeBufferArray(scip, &sortTransRhs);
	SCIPfreeBufferArray(scip, &sortTransRhsInd);
	SCIPfreeBufferArray(scip, &sortAggRhs);
	SCIPfreeBufferArray(scip, &sortAggRhsInd);

	return TRUE;
}

/** get problem name  */
SCIP_Bool getProblemName(
	SCIP*                scip,               		/**< SCIP data structure */
   char*           		cpyfilename,           	/**< input filename */
   char*                name           	  		/**< output problemname */
   )
{
	int   ntokens;
	char* token;
	char* nexttoken;

	ntokens = 0;
	token = SCIPstrtok(cpyfilename, "/", &nexttoken);
	while( token != NULL )
	{
		if( ntokens == 1 )
		{
			strcpy(name, token);
		}
		token = SCIPstrtok(NULL, ".", &nexttoken);
		ntokens ++;
	}

   return TRUE;
}

/** compute random right-hand side matrix (CCLS problem) */
SCIP_Bool sumRhs(
	SCIP*                scip,               		/**< SCIP data structure */
	SCIP_Real***			rhsData,						/**< right-hand side matrix data */
	SCIP_Real***			randomRhs,					/**< the random right-hand side matrix */
	int                  nScenario,             	/**< the number of scenarios */
   int                  dimension               /**< the dimension of random vector */
	)
{

	SCIP_Real sum;

	/* allocate buffer memory for storing random right-hand side matrix */
	SCIP_CALL( SCIPallocBufferArray(scip, randomRhs, nScenario) );
	for( int i = 0; i < nScenario; i++ )
	{
		SCIP_CALL( SCIPallocBufferArray(scip, &((*randomRhs)[i]), dimension) );
	}

   for( int s = 0; s < nScenario; s++ )
   {
      sum = 0;
      for( int i = 0; i < dimension; i++ )
      {
         (*randomRhs)[s][i] = sum + (*rhsData)[s][i];
         sum = (*randomRhs)[s][i];
      }
   }

	return TRUE;
}

/** calculate the true dominance ratio */
SCIP_Bool dominanceRatio(
	SCIP*                scip,               		/**< SCIP data structure */
	SCIP_Real**				randomRhs,					/**< the random right-hand side matrix */
	int                  nScenario,             	/**< the number of scenarios */
   int                  dimension               /**< the dimension of random vector */
	)
{
	int 		 allNEdges;
	int 		 allNReEdges;
	SCIP_Bool isGeq;
	SCIP_Bool isLeq;

	allNEdges     = 0;
	allNReEdges   = 0;
	for( int i = 0; i < nScenario; i++ )
	{
		for( int j = i+1; j < nScenario; j++ )
		{
			isGeq	= TRUE;
			isLeq	= TRUE;
			for( int t = 0; t < dimension; t++ )
			{
				if( !SCIPisGE(scip, randomRhs[i][t], randomRhs[j][t]) )
				{
					isGeq = FALSE;
					break;
				}
			}
			for( int t = 0; t < dimension; t++ )
			{
				if( !SCIPisLE(scip, randomRhs[i][t], randomRhs[j][t]) )
				{
					isLeq = FALSE;
					break;
				}
			}
			if( isGeq == TRUE )
			{
				allNEdges ++;
				if( isLeq == TRUE )
				{
					allNEdges ++;
					allNReEdges++;
				}
			}
			if( isGeq == FALSE && isLeq == TRUE )
			{
				allNEdges ++;
			}
		}
	}

	SCIPinfoMessage(scip, NULL, "NumEdge:        %d\n", allNEdges);
	SCIPinfoMessage(scip, NULL, "NumBiEdge: 	%d\n", allNReEdges);
	SCIPinfoMessage(scip, NULL, "NumDomIneqs: 	%d\n", allNEdges-allNReEdges);
	SCIPinfoMessage(scip, NULL, "DominanceRatio: %.4f\n", (allNEdges-allNReEdges)/(SCIP_Real)((nScenario*(nScenario-1))/2));

	return TRUE;
}

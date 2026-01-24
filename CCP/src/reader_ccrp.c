/**@file   reader_ccrp.c
 * @brief  Chance-constrained version of the resource planning (CCRP) file reader
 * @author Wei Lv
 * @author Wei-Kun Chen
 *
 * This file implements the reader/parser used to read CCRP problems.
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include "reader_ccrp.h"

/**@name Reader properties
 *
 * @{
 */

#define READER_NAME 							"ccrpreader"
#define READER_DESC 							"file reader for the chance-constrained version of the resource planning problem"
#define READER_EXTENSION 					"ccrp"

#define DEFAULT_EPSILON                0.10     /**< default confidence parameter epsilon is 0.10 */
#define DEFAULT_iSEQUALPROBABILITY     TRUE     /**< by default, all scenarios have an equal probability */
#define DEFAULT_BnC_MIX_DI             FALSE    /**< by default, do not add dominance inequalities (DI) */
#define DEFAULT_BnC_MIX_sDI            FALSE    /**< by default, do not use the stronger version of dominance inequalities (sDI) */
#define DEFAULT_DB       					FALSE    /**< by default, do not use dominance-based branching (DB) */
#define DEFAULT_DB_OPF       				FALSE    /**< by default, do not use overlap-oriented node pruning and variable fixing (OPF) */


struct SCIP_ReaderData
{
	SCIP_Real epsilon;      							/**< confidence parameter epsilon */
   SCIP_Bool isEqualProbability; 					/**< TRUE if all scenarios are assumed to have equal probability */
	SCIP_Bool BnC_MIX_DI;								/**< B&C+MIX+DI: B&C+MIX, i.e., solving formulation (MILP) using the B&C algorithm with the mixing cuts, with the dominance inequalities */
	SCIP_Bool BnC_MIX_sDI;								/**< B&C+MIX+sDI: solving formulation (MILP) with the stronger version of dominance inequalities*/
	SCIP_Bool DB;											/**< DB: solving formulation (MILP) using the dominance-based branching where the mixing cuts were also implemented */
	SCIP_Bool DB_OPF;										/**< DB+OPF: solving formulation (MILP) using the dominance-based branching 
																		with the overlap-oriented node pruning and variable fixing techniques where the mixing cuts were also implemented */
};

/** creates the reader data */
static
SCIP_RETCODE readerdataCreate(
   SCIP*                 scip,               	/**< SCIP data structure */
   SCIP_READERDATA**     readerdata          	/**< pointer to store the reader data */
   )
{
   assert(scip != NULL);
   assert(readerdata != NULL);

   SCIP_CALL( SCIPallocBlockMemory(scip, readerdata) );

   return SCIP_OKAY;
}


/** frees the reader data */
static
SCIP_RETCODE readerdataFree(
   SCIP*                 scip,               	/**< SCIP data structure */
   SCIP_READERDATA**     readerdata          	/**< pointer to store the reader data */
   )
{
   assert(scip != NULL);
   assert(readerdata != NULL);
   assert(*readerdata != NULL);

   SCIPfreeBlockMemory(scip, readerdata);

   return SCIP_OKAY;
}


/**@name Callback methods */

/** destructor of reader to free user data (called when SCIP is exiting)*/
static
SCIP_DECL_READERFREE(readerFreeCCRP)
{
   SCIP_READERDATA* readerdata;

   assert(scip != NULL);
   assert(reader != NULL);

   readerdata = SCIPreaderGetData(reader);

   SCIP_CALL( readerdataFree(scip, &readerdata) );

   return SCIP_OKAY;
}


/* ----------------- auxiliary functions ------------------------ */

/** read data from file */
static
SCIP_RETCODE readFileCCRP(
	SCIP*               	scip,             		/**< SCIP data structure */
	SCIP_READERDATA* 		readerdata,					/**< reader data strucure */
	const char*				filename,         		/**< name of file to read */
	int*        			nScenario,		  			/**< pointer to store the number of scenarios */
	int*						dimension,					/**< pointer to store the dimension of random vector */
	SCIP_Real***  			randomRhs,					/**< pointer to store the random right-hand side matrix */
	int*                	nResource,        		/**< pointer to store the number of resources */
	SCIP_Real** 			cost,							/**< pointer to store the cost vector */
	SCIP_Real***			mu,							/**< pointer to store the service rate matrix */
   SCIP_Real** 			proba 					   /**< pointer to store the scenario probability vector */
	)
{
	SCIP_FILE* file;
	char  buffer[SCIP_MAXSTRLEN];
	int 	i;
	int 	j;
	int   ntokens;
	char* token;
	char* nexttoken;
	SCIP_Real value;
	SCIP_Bool flag;

	assert( dimension != NULL );
   assert( nScenario != NULL );
	assert( randomRhs != NULL );
	assert( nResource != NULL );
	assert( cost != NULL );
	assert( mu != NULL );

	/* open file */
	file = SCIPfopen(filename, "r");
	if ( file == NULL )
	{
		SCIPerrorMessage("Could not open file <%s>.\n", filename);
		SCIPprintSysError(filename);
		return SCIP_NOFILE;
	}
	else
	{
		/* read the number of scenarios */
		char* name = (char*) filename;
		ntokens = 0;
		token = SCIPstrtok(name, "-", &nexttoken);
		while ( token != NULL )
		{
			if ( ntokens == 2 )
			{
				*nScenario = atoi(token);
				break;
			}
			else
			{
				ntokens ++;
				token = SCIPstrtok(NULL, "-", &nexttoken);
			}
		}
	}
	assert(SCIPisGT(scip, *nScenario, 0));

	/* read the number of resources */
	if ( !SCIPfeof(file) )
	{
		/* get next line */
		if( SCIPfgets(buffer, (int)sizeof(buffer), file) == NULL )
		{
			return SCIP_READERROR;
		}
		sscanf(buffer, "%d\n", nResource);
	}
	assert(SCIPisGT(scip, *nResource, 0));

	/* read the dimension of random vector */
	if ( !SCIPfeof(file) )
	{
		/* get next line */
		if ( SCIPfgets(buffer, (int)sizeof(buffer), file) == NULL )
		{
			return SCIP_READERROR;
		}
		sscanf(buffer, "%d\n", dimension);
	}
	assert(SCIPisGT(scip, *dimension, 0));

	/* allocate buffer memory for storing the service rate matrix */
	SCIP_CALL( SCIPallocBufferArray(scip, mu, *nResource) );
	for ( i = 0; i < *nResource; i++ )
	{
		SCIP_CALL( SCIPallocBufferArray(scip, &((*mu)[i]), *dimension) );
	}

	i = 0;
	j = 0;
	ntokens = 0;
	/* read the service rate matrix */
	while ( !SCIPfeof(file) )
	{
		value = 0.0;
		flag = FALSE;
		/* get next line */
		if ( SCIPfgets(buffer, (int)sizeof(buffer), file) == NULL )
		{
			return SCIP_READERROR;
		}
		/* dividing strings by spaces */
		token = SCIPstrtok(buffer, " ", &nexttoken);
		while ( token != NULL )
		{
			if ( *token != '[' && *token != ',' && *token != ' ' && *token != ']' )
			{
				value = atof(token);
         	if( value == -1 )
            	(*mu)[ntokens][i] = 0.00;
         	else
            	(*mu)[ntokens][i] = value;
				ntokens ++;
			}
			else if ( *token == ']' )
			{
				j++;
				if( j < *dimension )
         	{
					i++;
					ntokens = 0;
				}
				else
				{
					flag = TRUE;
            	break;
				}
			}
			token = SCIPstrtok(NULL, " ", &nexttoken);
		}
		if ( flag == TRUE )
		{
			break;
		}
	}

	/* allocate buffer memory for storing the cost vector */
	SCIP_CALL( SCIPallocBufferArray(scip, cost, *nResource) );

	ntokens = 0;
	/* read the cost vector */
   while ( !SCIPfeof(file) )
   {
		flag = FALSE;
		/* get next line */
		if ( SCIPfgets(buffer, (int)sizeof(buffer), file) == NULL )
		{
			return SCIP_READERROR;
		}
		/* dividing strings by spaces */
		token = SCIPstrtok(buffer, " ", &nexttoken);
		while ( token != NULL )
		{
			if ( *token != '[' && *token != ',' && *token != ' ' && *token != ']' )
			{
				(*cost)[ntokens] = atof(token);
				ntokens ++;
			}
			else if ( *token == ']')
			{
				flag = TRUE;
				break;
			}
			token = SCIPstrtok(NULL, " ", &nexttoken);
		}
		if ( flag == TRUE )
		{
			break;
		}
   }

	/* allocate buffer memory for storing scenario probability and random right-hand side matrix */
	SCIP_CALL( SCIPallocBufferArray(scip, proba, *nScenario) );
	SCIP_CALL( SCIPallocBufferArray(scip, randomRhs, *nScenario) );
	for ( i = 0; i < *nScenario; i++ )
	{
		SCIP_CALL( SCIPallocBufferArray(scip, &((*randomRhs)[i]), *dimension) );
	}

	/* read the random right-hand side matrix */
	i = 0;
	j = 0;
	ntokens = 0;
	while ( !SCIPfeof(file) )
   {
		flag = FALSE;
		/* get next line */
		if ( SCIPfgets(buffer, (int)sizeof(buffer), file) == NULL )
		{
			return SCIP_READERROR;
		}
		/* dividing strings by spaces */
		token = SCIPstrtok(buffer, " ", &nexttoken);
		while ( token != NULL )
		{
			if ( *token != 'r' && *token != '[' && *token != ',' && *token != '.' && *token != ']' )
			{
				(*randomRhs)[i][ntokens] = REALABS(atof(token));
         	assert( SCIPisGE(scip, (*randomRhs)[i][ntokens], 0.0) );
				ntokens++;
			}
			else if ( *token == ']' )
			{
				if ( j < *nScenario-1)
				{
					ntokens = 0;
					i++;
					j++;
				}
				else
				{
					flag = TRUE;
					break;
				}
			}
			token = SCIPstrtok(NULL, " ", &nexttoken);
		}
		if ( flag == TRUE )
		{
			break;
		}
   }
	(void) SCIPfclose(file);

	/* equal probability case */
	if ( readerdata->isEqualProbability == TRUE )
   {
      for ( int s = 0; s < *nScenario; s++ )
      {
			(*proba)[s] = 1.0/(SCIP_Real) (*nScenario);
		}
   }

	/* output parameters and check data*/
	SCIPinfoMessage(scip, NULL, "NumResource: \t%d\n", *nResource);
	SCIPinfoMessage(scip, NULL, "Dimension: \t%d\n", 	*dimension);
	SCIPinfoMessage(scip, NULL, "NumScenario: \t%d\n", *nScenario);
	SCIPinfoMessage(scip, NULL, "ConfidenceParameter: \t%.2f\n", readerdata->epsilon);
	SCIPinfoMessage(scip, NULL, "IsEqualProbability: \t%d\n", 	 readerdata->isEqualProbability);
   SCIPinfoMessage(scip, NULL, "BnC+MIX+DI: \t%d\n", 	 		 readerdata->BnC_MIX_DI);
   SCIPinfoMessage(scip, NULL, "BnC+MIX+sDI: \t%d\n", 	 readerdata->BnC_MIX_sDI);
	SCIPinfoMessage(scip, NULL, "DB: \t%d\n",  	 readerdata->DB);
   SCIPinfoMessage(scip, NULL, "DB+OPF: \t%d\n", 	 readerdata->DB_OPF);
		
	return SCIP_OKAY;
}


/** release variable */
static
SCIP_RETCODE releaseVars(
	SCIP*                scip,               		/**< SCIP data structure */
	SCIP_VAR** 				varx,							/**< varx represents quantity of resource */
	SCIP_VAR*** 			vary,							/**< vary represents the amount of resources allocated to customers */
	SCIP_VAR** 				varz,							/**< scenario variable */
	SCIP_VAR** 				varv,							/**< introducing variable */
	int        				nScenario,		  			/**< the number of scenarios */
	int						dimension,					/**< the dimension of random vector */
	int                	nResource	        		/**< the number of resources */
)
{
	for ( int i = 0; i < nResource; i++ )
	{
		SCIP_CALL( SCIPreleaseVar(scip, &(varx[i])) );
	}
	SCIPfreeBufferArray(scip, &varx);
	for ( int i = 0; i < nResource; i++ )
	{
		for ( int j = 0; j < dimension; j++ )
		{
			SCIP_CALL( SCIPreleaseVar(scip, &(vary[i][j])) );
		}
		SCIPfreeBufferArray(scip, &(vary[i]));
	}
	SCIPfreeBufferArray(scip, &vary);
	for ( int i = 0; i < nScenario; i++ )
	{
		SCIP_CALL( SCIPreleaseVar(scip, &(varz[i])) );
	}
	SCIPfreeBufferArray(scip, &varz);
	for ( int i = 0; i < dimension; i++ )
	{
		SCIP_CALL( SCIPreleaseVar(scip, &(varv[i])) );
	}
	SCIPfreeBufferArray(scip, &varv);

	return SCIP_OKAY;
}


/** problem reading method of reader */
static
SCIP_DECL_READERREAD(readerReadCCRP)
{/*lint --e{715}*/
	SCIP_Real 		epsilon;
	SCIP_Bool		isEqualProbability;
   int        		nScenario;
	int				dimension;
	SCIP_Real**  	randomRhs;
	int            nResource;
	SCIP_Real* 		cost;
	SCIP_Real**		mu;
	char*			   cpyfilename;
	char 			   name[SCIP_MAXSTRLEN];
	SCIP_Real** 	sortTransRhs;
	int** 			sortTransRhsInd;
	SCIP_Real*     proba;
	int*			   KI;
	SCIP_VAR** 		varx;
	SCIP_VAR*** 	vary;
	SCIP_VAR** 		varz;
	SCIP_VAR** 		varv;
	SCIP_CONS* 		cons;
	SCIP_Real 		val;
	SCIP_Real 		recordTime;
	SCIP_Real**  	stRhs;
	GRAPH          graph;
	int 				nPreCons;

	SCIP_Real**    aggRhs;
	SCIP_Real*     aggProba;
	int*           aggKI;
	int            nAggScenario;
	SCIP_Real** 	sortAggRhs;
	int** 			sortAggRhsInd;
	int* 				basicAggVarzInd;
	int  				basicNAggScenario;

	SCIP_READERDATA* readerdata;
	readerdata = SCIPreaderGetData(reader);

	assert( scip != NULL );
   assert( result != NULL );

	*result = SCIP_DIDNOTRUN;

	epsilon = readerdata->epsilon;
	isEqualProbability = readerdata->isEqualProbability;

	SCIP_CALL( SCIPallocBufferArray(scip, &cpyfilename, SCIP_MAXSTRLEN) );
	strcpy(cpyfilename, filename);
	SCIPinfoMessage(scip, NULL, "File name: \t%s\n", filename);

	/* generate problem name from filename */
   SCIP_CALL( getProblemName(scip, cpyfilename, name) );
	SCIPinfoMessage(scip, NULL, "Problem name:\t%s\n", name);

	/* read problem */
	SCIP_CALL( readFileCCRP(scip, readerdata, filename, &nScenario, &dimension, &randomRhs, &nResource, &cost, &mu, &proba) );

	/* sort right-hand vector and compute the index vector of lower bound for each dimension */
	sortRhs(scip, epsilon, isEqualProbability, nScenario, dimension, randomRhs, &sortTransRhs, &sortTransRhsInd, proba, &KI);

	/* strengthen right hand side */
   strengthenedRhs(scip, &stRhs, randomRhs, sortTransRhsInd, KI, nScenario, dimension);

	/* aggregation scenario, revise right-hand side vector and probability simultaneously */
	SCIPinfoMessage(scip, NULL, "Dominance and Aggregation Statistics......\n");
	if( readerdata->BnC_MIX_sDI || readerdata->DB || readerdata->DB_OPF )
	{
		/* calculate the true dominance ratio */
		dominanceRatio(scip, stRhs, nScenario, dimension);
		aggScenario(scip, epsilon, stRhs, &aggRhs, &aggProba, proba, &nAggScenario, nScenario, dimension);
	}
	else
	{
		/* calculate the true dominance ratio */
		dominanceRatio(scip, randomRhs, nScenario, dimension);
		aggScenario(scip, epsilon, randomRhs, &aggRhs, &aggProba, proba, &nAggScenario, nScenario, dimension);
	}

	/* sort right-hand vector and	compute the index vector of lower bound for each dimension after aggregation */
	sortRhs(scip, epsilon, FALSE, nAggScenario, dimension, aggRhs, &sortAggRhs, &sortAggRhsInd, aggProba, &aggKI);
	for( int i = 0; i < dimension; i++ )
	{
		assert(SCIPisEQ(scip, randomRhs[sortTransRhsInd[i][KI[i]]][i], aggRhs[sortAggRhsInd[i][aggKI[i]]][i]));
	}

	/* find the basic scenarios that the strengthened random right-side hand is greater than or equal to the lower bound after aggregation */
	findBasicScenes(scip, aggRhs, randomRhs, sortAggRhsInd, &basicAggVarzInd, &basicNAggScenario, aggKI, nAggScenario, dimension);

	/* create problem */
   SCIP_CALL( SCIPcreateProbBasic(scip, name) );

	/* set objective sense */
   SCIP_CALL( SCIPsetObjsense(scip, SCIP_OBJSENSE_MINIMIZE) );

	/* generate variables */
	/* variables x (quantity of resource) */
   SCIP_CALL( SCIPallocBufferArray(scip, &varx, nResource) );
	for( int i = 0; i < nResource; i++ )
   {
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "x#%d", i);

      /* create the SCIP_VAR object */
      SCIP_CALL( SCIPcreateVar(scip,
               &(varx[i]),              // returns new index
               name,   						 // name
               0.0,                     // lower bound
               SCIPinfinity(scip) ,     // upper bound
               cost[i],                 // objective
               SCIP_VARTYPE_CONTINUOUS, // variable type
               TRUE,                    // initial
               FALSE,                   // forget the rest ...
               NULL, NULL, NULL, NULL, NULL) );  /*lint !e732 !e747*/

      /* add the SCIP_VAR object to the scip problem */
      SCIP_CALL( SCIPaddVar(scip, varx[i]) );
   }

	/* variables y (the amount of resources allocated to customers ) */
	SCIP_CALL( SCIPallocBufferArray(scip, &vary, nResource) );
   for( int i = 0; i < nResource; i++ )
   {
		SCIP_CALL( SCIPallocBufferArray(scip, &(vary[i]), dimension) );
      for( int j = 0; j < dimension; j++ )
      {
			(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "y#%d#%d", i, j);

         /* create the SCIP_VAR object */
         SCIP_CALL( SCIPcreateVar(scip,
                  &(vary[i][j]),           // returns new index
                  name,   						 // name
                  0.0,                     // lower bound
                  SCIPinfinity(scip),      // upper bound
                  0.0,                     // objective
                  SCIP_VARTYPE_CONTINUOUS, // variable type
                  TRUE,                    // initial
                  FALSE,                   // forget the rest ...
                  NULL, NULL, NULL, NULL, NULL) );  /*lint !e732 !e747*/

         /* add the SCIP_VAR object to the scip problem */
         SCIP_CALL( SCIPaddVar(scip, vary[i][j]) );
      }
   }

	/* variables z (scenario variable) */
   SCIP_CALL( SCIPallocBufferArray(scip, &varz, nAggScenario) );
   for( int i = 0; i < nAggScenario; i++ )
   {
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "z#%d", i);

      /* create the SCIP_VAR object */
      SCIP_CALL( SCIPcreateVar(scip,
               &(varz[i]),              // returns new index
               name,   						 // name
               0.0,                     // lower bound
               1.0,                     // upper bound
               0.0,                     // objective
               SCIP_VARTYPE_BINARY,     // variable type
               TRUE,                    // initial
               FALSE,                   // forget the rest ...
               NULL, NULL, NULL, NULL, NULL) );  /*lint !e732 !e747*/

      /* add the SCIP_VAR object to the scip problem */
      SCIP_CALL( SCIPaddVar(scip, varz[i]) );
   }

	/* variable v (introducing variable) */
   SCIP_CALL( SCIPallocBufferArray(scip, &varv, dimension) );
   for( int i = 0; i < dimension; i++ )
   {
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "v#%d", i);

      /* create the SCIP_VAR object */
      SCIP_CALL( SCIPcreateVar(scip,
               &(varv[i]),              // returns new index
               name,            			 // name
               0.0,                     // lower bound
               SCIPinfinity(scip),      // upper bound
               0.0,                     // objective
               SCIP_VARTYPE_CONTINUOUS, // variable type
               TRUE,                    // initial
               FALSE,                   // forget the rest ...
               NULL, NULL, NULL, NULL, NULL) );  /*lint !e732 !e747*/

      /* add the SCIP_VAR object to the scip problem */
      SCIP_CALL( SCIPaddVar(scip, varv[i]) );
   }

	/* generate resource planning problem constraints */
	/* adds the equality constraints */
	for( int j = 0; j < dimension; j++ )
	{
		cons = NULL;
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "equality_%d", j);

		/* we first create an empty equality constraint and then add the variables */
		SCIP_CALL( SCIPcreateConsBasicLinear(
				scip,
				&cons,   /* pointer to hold the created constraint */
				name,    /* name of constraint */
				0,       /* number of nonzeros in the constraint */
				NULL,    /* array with variables of constraint entries */
				NULL,    /* array with coefficients of constraint entries */
				0.0,     /* left hand side of constraint */
				0.0) );  /* right hand side of constraint */

		/* add the vars belonging to field in this row to the constraint */
		/* var: v[i] val: 1 */
		SCIP_CALL( SCIPaddCoefLinear(scip, cons, varv[j], 1.0) );

		/* var: y[i][j] val: -mu[i][j] */
		for( int i = 0; i < nResource; i++ )
		{
			SCIP_CALL( SCIPaddCoefLinear(scip, cons, vary[i][j], -mu[i][j]) );
		}
		/* add the constraint to scip */
		SCIP_CALL( SCIPaddCons(scip, cons) );
		/* release this constraint */
		SCIP_CALL( SCIPreleaseCons(scip, &cons) );
	}

	/* adds strengthened linearization constraints */
   for( int i = 0; i < dimension; i++ )
   {
      for( int s = 0; s < aggKI[i]; s++ )
      {
         cons = NULL;
			(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "strengLinear_%d_%d", i, s);

         /* we first create an empty inequality constraint and then add the variables */
         SCIP_CALL( SCIPcreateConsBasicLinear(
                  scip,
                  &cons,                   					/* pointer to hold the created constraint */
                  name,  											/* name of constraint */
                  0,                      					/* number of nonzeros in the constraint */
                  NULL,                   					/* array with variables of constraint entries */
                  NULL,                   					/* array with coefficients of constraint entries */
                  aggRhs[sortAggRhsInd[i][s]][i],   		/* left hand side of constraint */
                  SCIPinfinity(scip)));   					/* right hand side of constraint */

         /* add the vars belonging to field in this row to the constraint */
         /* var: v[i] val: 1 */
         SCIP_CALL( SCIPaddCoefLinear(scip, cons, varv[i], 1.0) );

         /* var: z[s]  val: rhs[s][i]-rhs[k+1][i] */
         val = aggRhs[sortAggRhsInd[i][s]][i]-aggRhs[sortAggRhsInd[i][aggKI[i]]][i];
         SCIP_CALL( SCIPaddCoefLinear(scip, cons, varz[sortAggRhsInd[i][s]], val) );

         /* add the constraint to scip */
         SCIP_CALL( SCIPaddCons(scip, cons) );
			/* release this constraint */
			SCIP_CALL( SCIPreleaseCons(scip, &cons) );
      }
   }

	/* knapsack constraint */
   cons = NULL;
	(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "knapsack");
   /* we first create an empty equality constraint and then add the variables */
   SCIP_CALL( SCIPcreateConsBasicLinear(
            scip,
            &cons,                   	/* pointer to hold the created constraint */
            name,  							/* name of constraint */
            0,                      	/* number of nonzeros in the constraint */
            NULL,                   	/* array with variables of constraint entries */
            NULL,                   	/* array with coefficients of constraint entries */
            -SCIPinfinity(scip),    	/* left hand side of constraint */
            readerdata->epsilon) );    /* right hand side of constraint */

   /* add the vars belonging to field in this row to the constraint */
   /* var: z[i] val: 1 */
   for( int i = 0; i < nAggScenario; i++ )
	{
   	SCIP_CALL( SCIPaddCoefLinear(scip, cons, varz[i], aggProba[i]) );
	}
   /* add the constraint to scip */
   SCIP_CALL( SCIPaddCons(scip, cons) );
	/* release this constraint */
	SCIP_CALL( SCIPreleaseCons(scip, &cons) );

	/* adds the resources constraints*/
	for( int i = 0; i < nResource; i++ )
   {
      cons = NULL;
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "resources_%d", i);

      /* we first create an empty equality constraint and then add the variables */
      SCIP_CALL( SCIPcreateConsBasicLinear(
               scip,
               &cons,                  /* pointer to hold the created constraint */
               name,  						/* name of constraint */
               0,                      /* number of nonzeros in the constraint */
               NULL,                   /* array with variables of constraint entries */
               NULL,                   /* array with coefficients of constraint entries */
               -SCIPinfinity(scip),    /* left hand side of constraint */
               0.0) );                 /* right hand side of constraint */

      /* add the vars belonging to field in this row to the constraint */
      /* var: x[i] val: -1 */
      SCIP_CALL( SCIPaddCoefLinear(scip, cons, varx[i], -1.0) );

      /* var: y[j][i] val: 1 */
      for( int j = 0; j < dimension; j++ )
		{
         SCIP_CALL( SCIPaddCoefLinear(scip, cons, vary[i][j], 1.0) );
		}
      /* add the constraint to scip */
      SCIP_CALL( SCIPaddCons(scip, cons) );
		/* release this constraint */
		SCIP_CALL( SCIPreleaseCons(scip, &cons) );
   }

	/* dominance inequalities */
	if( readerdata->BnC_MIX_DI == TRUE )
	{
		clock_t GraphTimeStart = clock();
		SCIPinfoMessage(scip, NULL, "Dominance Inequalities Statistics......\n");
		if( readerdata->BnC_MIX_sDI == FALSE )
		{
			/* find all non-redundant dominance inequalities in existing work */
			findDomIneqs(scip, FALSE, &graph, aggRhs, NULL, 0, nAggScenario, dimension);
		}
		else
		{
			/* find all non-redundant dominance inequalities under our improved domination condition */
			findDomIneqs(scip, FALSE, &graph, aggRhs, basicAggVarzInd, basicNAggScenario, nAggScenario, dimension);
		}
		recordTime = (clock()-GraphTimeStart)/(SCIP_Real) CLOCKS_PER_SEC;
		SCIPinfoMessage(scip, NULL, "Duration: %.4f s \n\n", recordTime);

		/* add dominance inequalities */
		nPreCons = 0;
		for( int i = 0; i < graph.nedges; i++ )
   	{
      	if( graph.edgeTH[graph.edges[i].tail][graph.edges[i].head] == TRUE )
      	{
				cons = NULL;
				(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "precons_%d", nPreCons);
				nPreCons++;
				/* we first create an empty equality constraint and then add the variables */
				SCIP_CALL( SCIPcreateConsBasicLinear(
							scip,
							&cons,                  /* pointer to hold the created constraint */
							name,                   /* name of constraint */
							0,                      /* number of nonzeros in the constraint */
							NULL,                   /* array with variables of constraint entries */
							NULL,                   /* array with coefficients of constraint entries */
							0.0,                    /* left hand side of constraint */
							SCIPinfinity(scip)) );  /* right hand side of constraint */
				if( readerdata->BnC_MIX_sDI == FALSE )
				{
					/* add the vars belonging to field in this row to the constraint */
					/* var: z[tail] val: 1 */
					SCIP_CALL( SCIPaddCoefLinear(scip, cons, varz[graph.edges[i].tail], 1.0) );
					/* var: z[head] val: -1 */
					SCIP_CALL( SCIPaddCoefLinear(scip, cons, varz[graph.edges[i].head], -1.0) );
				}
				else
				{
					/* add the vars belonging to field in this row to the constraint */
					/* var: z[tail] val: 1 */
					SCIP_CALL( SCIPaddCoefLinear(scip, cons, varz[basicAggVarzInd[graph.edges[i].tail]], 1.0) );
					/* var: z[head] val: -1 */
					SCIP_CALL( SCIPaddCoefLinear(scip, cons, varz[basicAggVarzInd[graph.edges[i].head]], -1.0) );
				}
				/* add the constraint to scip */
				SCIP_CALL( SCIPaddCons(scip, cons) );
				/* release this constraint */
				SCIP_CALL( SCIPreleaseCons(scip, &cons) );
			}
		}
	}

	if( readerdata->DB == TRUE || readerdata->DB_OPF == TRUE )
	{
		clock_t TimeStart = clock();
		SCIPinfoMessage(scip, NULL, "Propagator Statistics......\n");

		/* find all non-redundant dominance inequalities under our improved domination condition */
		findDomIneqs(scip, TRUE, &graph, aggRhs, basicAggVarzInd, basicNAggScenario, nAggScenario, dimension);

		recordTime = (clock()-TimeStart)/(SCIP_Real) CLOCKS_PER_SEC;
		SCIPinfoMessage(scip, NULL, "Duration: %.4f s \n\n", recordTime);
	}

	/* special constraint structrue to use dominance inequalities by propagator */
	cons = NULL;
	SCIP_CALL( SCIPcreateConsCCP(scip, &cons, "chance", readerdata->DB, readerdata->DB_OPF,
				varz, varv, nAggScenario, dimension, aggProba, epsilon, aggKI, basicAggVarzInd, basicNAggScenario, aggRhs, stRhs,
				sortAggRhs, sortAggRhsInd, graph.vIn, graph.vInSize, graph.vOut, graph.vOutSize, graph.edgeTH) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

	/* free memory of defaul data arrays */
	SCIPfreeBufferArray(scip, &cpyfilename);
   SCIPfreeBufferArray(scip, &proba);
	SCIPfreeBufferArray(scip, &cost);
	for( int i = 0; i < nResource; i++ )
	{
		SCIPfreeBufferArray(scip, &mu[i]);
	}
	SCIPfreeBufferArray(scip, &mu);
	for( int i = 0; i < nScenario; i++ )
	{
	   SCIPfreeBufferArray(scip, &randomRhs[i]);
		SCIPfreeBufferArray(scip, &stRhs[i]);
		SCIPfreeBufferArray(scip, &aggRhs[i]);
	}
	SCIPfreeBufferArray(scip, &randomRhs);
	SCIPfreeBufferArray(scip, &stRhs);
	SCIPfreeBufferArray(scip, &aggRhs);
	SCIPfreeBufferArray(scip, &basicAggVarzInd);

	/* free memory of process data arrays */
  	freeProcessData(scip, KI, sortTransRhs, sortTransRhsInd, aggKI, sortAggRhs, sortAggRhsInd, aggProba, dimension);
	if( readerdata->BnC_MIX_DI == TRUE || readerdata->DB == TRUE || readerdata->DB_OPF )
   {
      /* free memory of graph data arrays */
	   freeGraph(scip, &graph, nAggScenario);
   }
	/*	release all variables */
	SCIP_CALL( releaseVars(scip, varx, vary, varz, varv, nAggScenario, dimension, nResource) );

	/* write LP file */
	// SCIP_CALL( SCIPwriteOrigProblem(scip, "ccrp.lp", "lp", FALSE) );

	*result = SCIP_SUCCESS;

	return SCIP_OKAY;
}

/**@} */


/**@name Interface methods
 *
 * @{
 */

/** includes the ccrp file reader in SCIP */
SCIP_RETCODE SCIPincludeReaderCCRP(
    SCIP *scip 											/**< SCIP data structure */
)
{
	SCIP_READERDATA *readerdata;
	SCIP_READER *reader;

	/* create ccrp reader data */
	readerdata = NULL;

	SCIP_CALL( readerdataCreate(scip, &readerdata) );
	assert( readerdata != NULL );

	/* include ccrp reader */
	SCIP_CALL( SCIPincludeReaderBasic(scip, &reader, READER_NAME, READER_DESC, READER_EXTENSION, readerdata) );
	assert(reader != NULL);

	SCIP_CALL( SCIPsetReaderFree(scip, reader, readerFreeCCRP) );
	SCIP_CALL( SCIPsetReaderRead(scip, reader, readerReadCCRP) );

	SCIP_CALL( SCIPaddRealParam(scip,
         	"reading/" READER_NAME "/epsilon", "confidence parameter",
         	&readerdata->epsilon, FALSE, DEFAULT_EPSILON, 0.0, 1.0, NULL, NULL) );

	SCIP_CALL( SCIPaddBoolParam(scip,
				"reading/" READER_NAME "/probability", "All scenarios have an equal probability",
				&readerdata->isEqualProbability, FALSE, DEFAULT_iSEQUALPROBABILITY, NULL, NULL) );

	SCIP_CALL( SCIPaddBoolParam(scip,
				"reading/" READER_NAME "/BnC_MIX_DI", "solving formulation (MILP) using the B&C algorithm with the mixing cuts and the dominance inequalities",
				&readerdata->BnC_MIX_DI, FALSE, DEFAULT_BnC_MIX_DI, NULL, NULL) );

	SCIP_CALL( SCIPaddBoolParam(scip,
            "reading/" READER_NAME "/BnC_MIX_sDI", "solving formulation (MILP) using the B&C algorithm with the mixing cuts and the stronger version of dominance inequalities",
            &readerdata->BnC_MIX_sDI, FALSE, DEFAULT_BnC_MIX_sDI, NULL, NULL) );

	SCIP_CALL( SCIPaddBoolParam(scip,
			"reading/" READER_NAME "/DB", "solving formulation (MILP) using the dominance-based branching where the mixing cuts were also implemented",
			&readerdata->DB, FALSE, DEFAULT_DB, NULL, NULL) );

	SCIP_CALL( SCIPaddBoolParam(scip,
		"reading/" READER_NAME "/DB_OPF", " solving formulation (MILP) using the dominance-based branching with the overlap-oriented node pruning and variable fixing techniques where the mixing cuts were also implemented",
		&readerdata->DB_OPF, FALSE, DEFAULT_DB_OPF, NULL, NULL) );

   return SCIP_OKAY;
}

/**@} */

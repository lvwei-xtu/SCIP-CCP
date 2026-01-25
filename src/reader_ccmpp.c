/**@file   reader_ccmpp.c
 * @brief  Chance-constrained versions of the multiperiod power planning (CCMPP) file reader
 * @author Wei Lv
 * @author Wei-Kun Chen
 *
 * This file implements the reader/parser used to read CCMPP problems.
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include "reader_ccmpp.h"

/**@name Reader properties
 *
 * @{
 */

#define READER_NAME 							"ccmppreader"
#define READER_DESC 							"file reader for the chance-constrained version of the multiperiod power planning problem"
#define READER_EXTENSION 					"ccmpp"

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
SCIP_DECL_READERFREE(readerFreeCCMPP)
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
SCIP_RETCODE readFileCCMPP(
	SCIP*               	scip,             		/**< SCIP data structure */
	SCIP_READERDATA* 		readerdata,					/**< reader data strucure */
	const char*				filename,         		/**< name of file to read */
	int*        			nScenario,		  			/**< pointer to store the number of scenarios */
	int*						dimension,					/**< pointer to store the dimension of random vector */
	SCIP_Real***  			randomRhs,					/**< pointer to store the random right-hand side matrix */
	SCIP_Real**			   coalCost,					/**< pointer to store the service rate matrix */
	SCIP_Real** 			nuclearCost,				/**< pointer to store the coal cost vector  */
	SCIP_Real** 			eResources,					/**< pointer to store the electric capacity vector from existing resources */
   SCIP_Real** 			proba 					   /**< pointer to store the scenario probability vector */
	)
{
	SCIP_FILE* file;
	char  buffer[SCIP_MAXSTRLEN];
	int 	i;
	int 	j;
	int 	s;
	int   ntokens;
	char* token;
	char* nexttoken;
	SCIP_Bool flag;

	assert( dimension != NULL );
   assert( nScenario != NULL );
	assert( randomRhs != NULL );
	assert( coalCost != NULL );
	assert( nuclearCost != NULL );
	assert( eResources != NULL );
	assert( proba != NULL );

	/* open file */
	file = SCIPfopen(filename, "r");
	if( file == NULL )
	{
		SCIPerrorMessage("Could not open file <%s>.\n", filename);
		SCIPprintSysError(filename);
		return SCIP_NOFILE;
	}

	/* read the dimension of random vector */
	if( !SCIPfeof(file) )
	{
		/* get next line */
		if( SCIPfgets(buffer, (int)sizeof(buffer), file) == NULL )
		{
			return SCIP_READERROR;
		}
		sscanf(buffer, "%d\n", dimension);
	}
	assert(SCIPisGT(scip, *dimension, 0));

   /* read the number of scenarios */
	if( !SCIPfeof(file) )
	{
		/* get next line */
		if( SCIPfgets(buffer, (int)sizeof(buffer), file) == NULL )
		{
			return SCIP_READERROR;
		}
		sscanf(buffer, "%d\n", nScenario);
	}
	assert(SCIPisGT(scip, *nScenario, 0));

	/* allocate buffer memory for storing coal cost, nuclear cost and existing resources */
	SCIP_CALL( SCIPallocBufferArray(scip, coalCost, *dimension) );
	SCIP_CALL( SCIPallocBufferArray(scip, nuclearCost, *dimension) );
	SCIP_CALL( SCIPallocBufferArray(scip, eResources, *dimension) );

	ntokens = 0;
	/* read the coal cost vector */
   while( !SCIPfeof(file) )
   {
		flag = FALSE;
		/* get next line */
		if( SCIPfgets(buffer, (int)sizeof(buffer), file) == NULL )
		{
			return SCIP_READERROR;
		}
		/* dividing strings by spaces */
		token = SCIPstrtok(buffer, " ", &nexttoken);
		while( token != NULL )
		{
			if( *token != '[' && *token != ']' )
			{
				(*coalCost)[ntokens] = atof(token);
				ntokens ++;
			}
			else if( *token == ']')
			{
				flag = TRUE;
				break;
			}
			token = SCIPstrtok(NULL, " ", &nexttoken);
		}
		if( flag == TRUE )
		{
			break;
		}
   }

   ntokens = 0;
	/* read the nuclear cost vector */
   while( !SCIPfeof(file) )
   {
		flag = FALSE;
		/* get next line */
		if( SCIPfgets(buffer, (int)sizeof(buffer), file) != NULL )
		{
			/* skip empty lines */
			if(buffer[0] == '\n')
				continue;

			/* dividing strings by spaces */
			token = SCIPstrtok(buffer, " ", &nexttoken);
			while( token != NULL )
			{
				if( *token != '[' && *token != ']' )
				{
					(*nuclearCost)[ntokens] = atof(token);
					ntokens ++;
				}
				else if( *token == ']')
				{
					flag = TRUE;
					break;
				}
				token = SCIPstrtok(NULL, " ", &nexttoken);
			}
			if( flag == TRUE )
			{
				break;
			}
		}
   }

	ntokens = 0;
	/* read electric capacity from existing resources */
   while( !SCIPfeof(file) )
   {
		flag = FALSE;
		/* get next line */
		if( SCIPfgets(buffer, (int)sizeof(buffer), file) != NULL )
		{
			/* skip empty lines */
			if(buffer[0] == '\n')
				continue;

			/* dividing strings by spaces */
			token = SCIPstrtok(buffer, " ", &nexttoken);
			while( token != NULL )
			{
				if( *token != '[' && *token != ']' )
				{
					(*eResources)[ntokens] = atof(token);
					ntokens ++;
				}
				else if( *token == ']')
				{
					flag = TRUE;
					break;
				}
				token = SCIPstrtok(NULL, " ", &nexttoken);
			}
			if( flag == TRUE )
			{
				break;
			}
		}
   }

	/* allocate buffer memory for storing scenario probability and random right-hand side matrix */
	SCIP_CALL( SCIPallocBufferArray(scip, proba, *nScenario) );
	SCIP_CALL( SCIPallocBufferArray(scip, randomRhs, *nScenario) );
	for( i = 0; i < *nScenario; i++ )
	{
		SCIP_CALL( SCIPallocBufferArray(scip, &((*randomRhs)[i]), *dimension) );
	}
	/* read the random right-hand side matrix and secnario probability */
	s = 0;
	while( !SCIPfeof(file) )
   {
		j = 0;
		ntokens = 0;
		flag = FALSE;
		/* get next line */
		if( SCIPfgets(buffer, (int)sizeof(buffer), file) != NULL )
		{
			/* skip empty lines */
			if (buffer[0] == '\n')
			 	continue;
			/* dividing strings by \n and spaces */
			token = strtok(buffer, "\n");
			token = SCIPstrtok(token, " ", &nexttoken);
			while( token != NULL )
			{
				if( *token != '[' && flag == FALSE )
				{
					assert( SCIPisGE(scip, atof(token), 0.0) );
					(*proba)[s] = atof(token);
					flag = TRUE;
				}
				else if(*token != '[' && *token != ']' && flag == TRUE )
				{
					(*randomRhs)[s][j] = REALABS(atof(token));
					j++;
				}
				token = SCIPstrtok(NULL, " ", &nexttoken);
				ntokens++;
			}
			s++;
			if( s == *nScenario )
				break;
		}
   }

   /* upate right-hand side (d^i_t - e_t)*/
	for( s = 0; s < *nScenario; s++ )
   {
      for( i = 0; i < *dimension; i++ )
      {
        (*randomRhs)[s][i] = (*randomRhs)[s][i]-(*eResources)[i];
      }
   }
	(void) SCIPfclose(file);
	
	/* equal probability case */
	if( readerdata->isEqualProbability == TRUE )
   {
      for( s = 0; s < *nScenario; s++ )
      {
			(*proba)[s] = 1.0/(SCIP_Real) (*nScenario);
		}
   }

	/* output parameters*/
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
	SCIP_VAR** 				varx,							/**< power capacity of coal plants */
	SCIP_VAR**  			vary,							/**< power capacity of nuclear plants */
	SCIP_VAR** 				varz,							/**< scenario variable */
	SCIP_VAR** 				varu,							/**< total coal power capacity */
	SCIP_VAR** 				varv,							/**< total nuclear power capacity  */
	SCIP_VAR** 				varw,							/**< introducing variable */
	int        				nScenario,		  			/**< the number of scenarios */
	int						dimension					/**< the dimension of random vector */
)
{
	for ( int i = 0; i < dimension; i++ )
	{
		SCIP_CALL( SCIPreleaseVar(scip, &(varx[i])) );
		SCIP_CALL( SCIPreleaseVar(scip, &(vary[i])) );
		SCIP_CALL( SCIPreleaseVar(scip, &(varu[i])) );
		SCIP_CALL( SCIPreleaseVar(scip, &(varv[i])) );
		SCIP_CALL( SCIPreleaseVar(scip, &(varw[i])) );
	}
	SCIPfreeBufferArray(scip, &varx);
	SCIPfreeBufferArray(scip, &vary);
	SCIPfreeBufferArray(scip, &varu);
	SCIPfreeBufferArray(scip, &varv);
	SCIPfreeBufferArray(scip, &varw);

	for ( int i = 0; i < nScenario; i++ )
	{
		SCIP_CALL( SCIPreleaseVar(scip, &(varz[i])) );
	}
	SCIPfreeBufferArray(scip, &varz);

	return SCIP_OKAY;
}


/** problem reading method of reader */
static
SCIP_DECL_READERREAD(readerReadCCMPP)
{/*lint --e{715}*/
	SCIP_Real 		epsilon;
	SCIP_Bool		isEqualProbability;
   int        		nScenario;
	int				dimension;
	SCIP_Real**  	randomRhs;
   SCIP_Real*     coalCost;
   SCIP_Real*     nuclearCost;
   SCIP_Real*     eResources;
	char*			   cpyfilename;
	char 			   name[SCIP_MAXSTRLEN];
	SCIP_Real** 	sortTransRhs;
	int** 			sortTransRhsInd;
	SCIP_Real*     proba;
	int*			   KI;
	SCIP_VAR** 		varx;
	SCIP_VAR** 		vary;
	SCIP_VAR** 		varu;
	SCIP_VAR** 		varv;
	SCIP_VAR** 		varw;
	SCIP_VAR** 		varz;
	SCIP_CONS* 		cons;
	SCIP_Real 		val;
	SCIP_Real 		recordTime;
	SCIP_Real**  	stRhs;
	GRAPH          graph;
	int 				nPreCons;
	int    			Tc;
   int    			Tn;
   SCIP_Real 		f;

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
	SCIP_CALL( readFileCCMPP(scip, readerdata, filename, &nScenario, &dimension, &randomRhs, &coalCost, &nuclearCost, &eResources, &proba) );

	/* sort right-hand vector and	compute the index vector of lower bound for each dimension */
	sortRhs(scip, epsilon, isEqualProbability, nScenario, dimension, randomRhs, &sortTransRhs, &sortTransRhsInd, proba, &KI);

	/* strengthen right hand side */
   strengthenedRhs(scip, &stRhs, randomRhs, sortTransRhsInd, KI, nScenario, dimension);

	/* aggregation scenario, revise right-hand side vector and probability simultaneously */
	SCIPinfoMessage(scip, NULL, "Dominance and Aggregation Statistics......\n");
	if( readerdata->BnC_MIX_sDI || readerdata->DB || readerdata->DB_OPF)
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
   SCIP_CALL( SCIPallocBufferArray(scip, &varx, dimension) );
	for( int i = 0; i < dimension; i++ )
   {
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "x#%d", i);

      /* create the SCIP_VAR object */
      SCIP_CALL( SCIPcreateVar(scip,
               &(varx[i]),              // returns new index
               name,   						 // name
               0.0,                     // lower bound
               SCIPinfinity(scip) ,     // upper bound
               coalCost[i],             // objective
               SCIP_VARTYPE_CONTINUOUS, // variable type
               TRUE,                    // initial
               FALSE,                   // forget the rest ...
               NULL, NULL, NULL, NULL, NULL) );  /*lint !e732 !e747*/

      /* add the SCIP_VAR object to the scip problem */
      SCIP_CALL( SCIPaddVar(scip, varx[i]) );
   }

	/* variables y (the amount of resources allocated to customers ) */
	SCIP_CALL( SCIPallocBufferArray(scip, &vary, dimension) );
   for( int i = 0; i < dimension; i++ )
   {
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "y#%d", i);

		/* create the SCIP_VAR object */
		SCIP_CALL( SCIPcreateVar(scip,
					&(vary[i]),           	 // returns new index
					name,   						 // name
					0.0,                     // lower bound
					SCIPinfinity(scip),      // upper bound
					nuclearCost[i],          // objective
					SCIP_VARTYPE_CONTINUOUS, // variable type
					TRUE,                    // initial
					FALSE,                   // forget the rest ...
					NULL, NULL, NULL, NULL, NULL) );  /*lint !e732 !e747*/

		/* add the SCIP_VAR object to the scip problem */
		SCIP_CALL( SCIPaddVar(scip, vary[i]) );
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

	/* variable u */
   SCIP_CALL( SCIPallocBufferArray(scip, &varu, dimension) );
   for( int i = 0; i < dimension; i++ )
   {
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "u#%d", i);

      /* create the SCIP_VAR object */
      SCIP_CALL( SCIPcreateVar(scip,
               &(varu[i]),              // returns new index
               name,            			 // name
               0.0,                     // lower bound
               SCIPinfinity(scip),      // upper bound
               0.0,                     // objective
               SCIP_VARTYPE_CONTINUOUS, // variable type
               TRUE,                    // initial
               FALSE,                   // forget the rest ...
               NULL, NULL, NULL, NULL, NULL) );  /*lint !e732 !e747*/

      /* add the SCIP_VAR object to the scip problem */
      SCIP_CALL( SCIPaddVar(scip, varu[i]) );
   }

	/* variable v */
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

	/* variable w */
   SCIP_CALL( SCIPallocBufferArray(scip, &varw, dimension) );
   for( int i = 0; i < dimension; i++ )
   {
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "w#%d", i);

      /* create the SCIP_VAR object */
      SCIP_CALL( SCIPcreateVar(scip,
               &(varw[i]),              // returns new index
               name,            			 // name
               0.0,                     // lower bound
               SCIPinfinity(scip),      // upper bound
               0.0,                     // objective
               SCIP_VARTYPE_CONTINUOUS, // variable type
               TRUE,                    // initial
               FALSE,                   // forget the rest ...
               NULL, NULL, NULL, NULL, NULL) );  /*lint !e732 !e747*/

      /* add the SCIP_VAR object to the scip problem */
      SCIP_CALL( SCIPaddVar(scip, varw[i]) );
   }

   /* parameter for constraints */
	Tc = 15;
   Tn = 10;
  	f = 0.2;

	/* equality constraint (coal)*/
   for( int t = 0; t < dimension; t++ )
	{
		int i;
      cons = NULL;
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "coal_%d", t);

		/* we first create an empty equality constraint and then add the variables */
      SCIP_CALL( SCIPcreateConsBasicLinear(
               scip,
               &cons,                     /* pointer to hold the created constraint */
               name,     						/* name of constraint */
               0,                         /* number of nonzeros in the constraint */
               NULL,                      /* array with variables of constraint entries */
               NULL,                      /* array with coefficients of constraint entries */
               0.0,                       /* left hand side of constraint */
               0.0) );                    /* right hand side of constraint */

      /* add the vars belonging to field in this row to the constraint */
      /* var:u val:1 */
      SCIP_CALL( SCIPaddCoefLinear(scip, cons, varu[t], 1.0) );

      /* var:x val:-1 */
      i = t-Tc+1 > 0 ? t-Tc+1 : 0;
      for( ; i <= t; i++ )
		{
         SCIP_CALL( SCIPaddCoefLinear(scip, cons, varx[i], -1) );
		}

      /* add the constraint to scip */
      SCIP_CALL( SCIPaddCons(scip, cons) );
		/* release this constraint */
		SCIP_CALL( SCIPreleaseCons(scip, &cons) );
	}

	/* Equality constraint (nuclear)*/
   for( int t = 0; t < dimension; t++ )
   {
		int i;
      cons = NULL;
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "nuclear_%d", t);

      /* we first create an empty equality constraint and then add the variables */
      SCIP_CALL( SCIPcreateConsBasicLinear(
               scip,
               &cons,                     /* pointer to hold the created constraint */
               name,     						/* name of constraint */
               0,                         /* number of nonzeros in the constraint */
               NULL,                      /* array with variables of constraint entries */
               NULL,                      /* array with coefficients of constraint entries */
               0.0,                       /* left hand side of constraint */
               0.0) );                    /* right hand side of constraint */

      /* add the vars belonging to field in this row to the constraint */
      /* var:v val:1 */
      SCIP_CALL( SCIPaddCoefLinear(scip, cons, varv[t], 1.0) );

      /* var:x val:-1 */
      i = t-Tn +1 > 0 ? t-Tn +1 : 0;
      for( ; i <= t; i++ )
		{
         SCIP_CALL( SCIPaddCoefLinear(scip, cons, vary[i], -1) );
		}
      /* add the constraint to scip */
      SCIP_CALL( SCIPaddCons(scip, cons) );
		/* release this constraint */
		SCIP_CALL( SCIPreleaseCons(scip, &cons) );
   }

	/* equality constraint (w_t = u_t + v_t)*/
   for( int t = 0; t < dimension; t++ )
   {
      cons = NULL;
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "agg_%d", t);

      /* we first create an empty equality constraint and then add the variables */
      SCIP_CALL( SCIPcreateConsBasicLinear(
               scip,
               &cons,                     /* pointer to hold the created constraint */
               name,     						/* name of constraint */
               0,                         /* number of nonzeros in the constraint */
               NULL,                      /* array with variables of constraint entries */
               NULL,                      /* array with coefficients of constraint entries */
               0.0,                       /* left hand side of constraint */
               0.0) );                    /* right hand side of constraint */

      /* add the vars belonging to field in this row to the constraint */
      /* var:u val:-1 */
      SCIP_CALL( SCIPaddCoefLinear(scip, cons, varu[t], -1.0) );
      /* var:v val:-1 */
      SCIP_CALL( SCIPaddCoefLinear(scip, cons, varv[t], -1.0) );
      /* var:w val:1 */
      SCIP_CALL( SCIPaddCoefLinear(scip, cons, varw[t],  1.0) );

      /* add the constraint to scip */
      SCIP_CALL( SCIPaddCons(scip, cons) );
      /* release this constraint */
		SCIP_CALL( SCIPreleaseCons(scip, &cons) );
   }

	/* equation enforces the regulatory limit on nuclear capacity is satisfied */
   for( int t = 0; t < dimension; t++ )
   {
      cons = NULL;
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "capacity_%d", t);

      /* we first create an empty equality constraint and then add the variables */
      SCIP_CALL( SCIPcreateConsBasicLinear(
               scip,
               &cons,                     /* pointer to hold the created constraint */
               name,     						/* name of constraint */
               0,                         /* number of nonzeros in the constraint */
               NULL,                      /* array with variables of constraint entries */
               NULL,                      /* array with coefficients of constraint entries */
               -SCIPinfinity(scip),    	/* left hand side of constraint */
               f*eResources[t]) );        /* right hand side of constraint */

      /* add the vars belonging to field in this row to the constraint */
      /* var:u val:f */
      SCIP_CALL( SCIPaddCoefLinear(scip, cons, varu[t], -f) );

      /* var:v val:1-f */
      SCIP_CALL( SCIPaddCoefLinear(scip, cons, varv[t], 1-f) );

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
                  aggRhs[sortAggRhsInd[i][s]][i],        /* left hand side of constraint */
                  SCIPinfinity(scip)));   					/* right hand side of constraint */

         /* add the vars belonging to field in this row to the constraint */
         /* var: v[i] val: 1 */
         SCIP_CALL( SCIPaddCoefLinear(scip, cons, varw[i], 1.0) );

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
		findDomIneqs(scip, TRUE, &graph, aggRhs, basicAggVarzInd, basicNAggScenario, nAggScenario, dimension );

		recordTime = (clock()-TimeStart)/(SCIP_Real) CLOCKS_PER_SEC;
		SCIPinfoMessage(scip, NULL, "Duration: %.4f s \n\n", recordTime);
	}

	/* special constraint structrue to use dominance inequalities by propagator */
	cons = NULL;
	SCIP_CALL( SCIPcreateConsCCP(scip, &cons, "chance", readerdata->DB, readerdata->DB_OPF,
			varz, varw, nAggScenario, dimension, aggProba, epsilon, aggKI, basicAggVarzInd, basicNAggScenario, aggRhs, stRhs,
			sortAggRhs, sortAggRhsInd, graph.vIn, graph.vInSize, graph.vOut, graph.vOutSize, graph.edgeTH) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

	/* free memory of defaul data arrays */
	SCIPfreeBufferArray(scip, &cpyfilename);
   SCIPfreeBufferArray(scip, &proba);
	SCIPfreeBufferArray(scip, &coalCost);
	SCIPfreeBufferArray(scip, &nuclearCost);
	SCIPfreeBufferArray(scip, &eResources);
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
	if( readerdata->BnC_MIX_DI || readerdata->DB || readerdata->DB_OPF)
   {
      /* free memory of graph data arrays */
	   freeGraph(scip, &graph, nAggScenario);
   }
	/*	release all variables */
	SCIP_CALL( releaseVars(scip, varx, vary, varz, varu, varv, varw, nAggScenario, dimension) );

	// SCIP_CALL( SCIPwriteOrigProblem(scip, "ccmpp.lp", "lp", FALSE) );

	*result = SCIP_SUCCESS;

	return SCIP_OKAY;
}

/**@} */


/**@name Interface methods
 *
 * @{
 */

/** includes the ccmpp file reader in SCIP */
SCIP_RETCODE SCIPincludeReaderCCMPP(
    SCIP *scip 											/**< SCIP data structure */
)
{
	SCIP_READERDATA *readerdata;
	SCIP_READER *reader;

	/* create ccmpp reader data */
	readerdata = NULL;

	SCIP_CALL( readerdataCreate(scip, &readerdata) );
	assert( readerdata != NULL );

	/* include ccmpp reader */
	SCIP_CALL( SCIPincludeReaderBasic(scip, &reader, READER_NAME, READER_DESC, READER_EXTENSION, readerdata) );
	assert(reader != NULL);

	SCIP_CALL( SCIPsetReaderFree(scip, reader, readerFreeCCMPP) );
	SCIP_CALL( SCIPsetReaderRead(scip, reader, readerReadCCMPP) );

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

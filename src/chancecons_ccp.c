/**@file   chancecons_ccp.c
 * @brief  constraint handler for Chance-constrained Program with Random Right-hand Side
 * @author Wei Lv
 * @author Wei-Kun Chen
 *
 * We handle the following system of linear constraints:
 * v_j \geq \xi_{ij}*(1 - z_{i}), \forall i \in [n], j \in [m],
 * \sum_{i=1}^{n} p_{i}z_{i} \leq 1-\epsilon,
 * v \in \mathbb{R}^{m}_{+}, z \in \{0,1\}^{n}.
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#include "chancecons_ccp.h"

/* constraint handler properties */
#define CONSHDLR_NAME              "chancecons"
#define CONSHDLR_DESC              "chance constraint handler"
#define CONSHDLR_ENFOPRIORITY      0                          /**< priority of the constraint handler for constraint enforcing */
#define CONSHDLR_CHECKPRIORITY     999999                     /**< priority of the constraint handler for checking feasibility */
#define CONSHDLR_PROPFREQ          -1                         /**< frequency for propagating domains; zero means only preprocessing propagation */
#define CONSHDLR_EAGERFREQ         1                          /**< frequency for using all instead of only the useful constraints in separation,
                                                               *  propagation and enforcement, -1 for no eager evaluations, 0 for first only */
#define CONSHDLR_DELAYPROP         FALSE                      /**< should propagation method be delayed, if other propagators found reductions? */
#define CONSHDLR_NEEDSCONS         TRUE                       /**< should the constraint handler be skipped, if no constraints are available? */
#define consEnfolpCCP              NULL                       /**< constraint enforcing method of constraint handler for LP solutions */
#define consEnfopsCCP              NULL                       /**< constraint enforcing method of constraint handler for pseudo solutions */
#define consCheckCCP               NULL                       /**< feasibility check method of constraint handler for integral solutions */
#define consLockCCP                NULL                       /**< variable rounding lock method of constraint handler */
#define CONSHDLR_PROP_TIMING       SCIP_PROPTIMING_BEFORELP   /**< propagation execution timing flags call propagator before LP is solved */

#define CONSHDLR_SEPAFREQ          10                         /**< frequency for separating cuts; zero means to separate only in the root node */
#define CONSHDLR_SEPAPRIORITY      100                        /**< priority of the constraint handler for separation */
#define CONSHDLR_DELAYSEPA         FALSE                      /**< should separation method be delayed, if other separators found cuts? */

#define CONSHDLR_MAXPREROUNDS      0                          /**< maximal number of presolving rounds the constraint handler participates in (-1: no limit, 0: no entry; 1: one entry only) */
#define CONSHDLR_PRESOLTIMING      SCIP_PRESOLTIMING_ALWAYS   /**< timing of the presolving method (fast, medium, or exhaustive) */

#define DEFAULT_ISCLOSEPROBING     FALSE                      /**< should probing be turned off in domain propagation? */
#define DEFAULT_TIGHTCONTVARBOUND  TRUE                       /**< should the bounds of the continue variable v be updated in the domain propagation? */
#define DEFAULT_BASE_DB_EOPF       FALSE                      /**< should the exact approach for implementing overlap-oriented node pruning and variable fixing be used in domain propagation? */

/*
 * Data structures
*/

/** constraint data for chance constraints */
struct SCIP_ConsData
{
   SCIP_Bool                  BASE_DB;                        /**< should the dominance-based branching be used to solve the problem? */
   SCIP_Bool                  BASE_DB_OPF;                    /**< should the the dominance-based branching with overlap-oriented node pruning and variable fixing be used to solve the problem? */
   SCIP_VAR**                 varz;                           /**< scenario variable z */
   SCIP_VAR**                 varv;                           /**< introducing variable v */
   int                        nScenario;                      /**< number of scenarios */
   int                        dimension;                      /**< dimension of random vector */
   SCIP_Real*                 proba;                          /**< scenario probability vector */
   SCIP_Real                  epsilon;                        /**< confidence parameter */
   int*                       KI;                             /**< index vector of lower bound for variables v */
   int*                       basicVarzInd;                   /**< index of variable z corresponding to basic scenarios */
   int                        basicNScenario;                 /**< number of essential scenarios */
   SCIP_Real**                randomRhs;                      /**< random right-hand side matrix */
   SCIP_Real**                stRhs;                          /**< strengthened random right-hand side matrix */
   SCIP_Real**                sortTransRhs;                   /**< sorted transpose of random right-hand side matrix */
   int**                      sortTransRhsInd;                /**< index of sorted transpose of random right-hand side matrix */
   int**                      vIn;                            /**< matrix that stores the incoming  arc's tail of all vertices */
   int*                       vInSize;                        /**< vecotr that stores the number of the incoming  arc's tail for a vertice */
   int**                      vOut;                           /**< matrix that stores the outcoming arc's head of all vertices */
   int*                       vOutSize;                       /**< vecotr that stores the number of the outcoming arc's head for a vertice */
   SCIP_Bool**                edgeTH;                         /**< the edges existing in the graph (tail-head) */
   SCIP_Longint               npropagations;                  /**< statistic the number of propagation round */
   SCIP_Longint               cutoffNKC;                      /**< number of infeasible case one */
   SCIP_Longint               fixNOneVars;                    /**< number of variable fix to one in polynomial reduction algorithm */
   SCIP_Longint               fixNZeroVars;                   /**< number of variable fix to zero in polynomial reduction algorithm */
   int*                       MiBeg;                          /**< array that specifies the nonzero elements number of right-hand side vector being added */
   int*                       roundMiBeg;                     /**< array that specifies the nonzero elements number of right-hand side vector being added
                                                               * in each round of polynomial-time branch scheme */
   int**                      MiInd;                          /**< array that specifies the nonzero elements index of right-hand side vector being added */
   SCIP_Real**                MiVal;                          /**< array that specifies the nonzero elements value of right-hand side vector being added */
   SCIP_Real*                 nodeLb;                         /**< lower bound of continuous variable v of in child node */
   SCIP_Longint               tightNConVarLb;                 /**< tighten times of node lower bound for continuous variable v */
   SCIP_Real                  tightNConVarLbTime;             /**< time taken to tighten the lower bound of the continue variable */
   SCIP_Longint               maxRoundPRA;                    /**< the maximum round of polynomial reduction algorithm */
   SCIP_Real                  exactTime;                      /**< time of exact branching algorithm */
   SCIP_Longint               fixNOneVarsExact;               /**< number of variable fix to one in exact algorithm */
   SCIP_Longint               fixNZeroVarsExact;              /**< number of variable fix to zero in exact algorithm */
   SCIP_Longint               cutoffTimesExact;               /**< number of cutoff in exact algorithm */
   SCIP_Longint               cutoffTimesAOPF;                /**< number of cutoff in approximation algorithm */
};
/**@name brancing schemes
 *
 * @{
 */


/** constraint handler data */
struct SCIP_ConshdlrData
{
   SCIP_Bool   isCloseProbing;                                /**< should probing be turned off in domain propagation? */
   SCIP_Bool   tightContVarBound;                             /**< should the bounds of the continue variable v be updated in the domain propagation? */
   SCIP_Bool   BASE_DB_EOPF;                                  /**< should the exact approach for implementing overlap-oriented node pruning and variable fixing be used in domain propagation? */
};


/** creates constraint handler data for chance constraint handler */
static
SCIP_RETCODE conshdlrdataCreate(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSHDLRDATA**   conshdlrdata                         /**< pointer to store the constraint handler data */
   )
{
   assert(scip != NULL);
   assert(conshdlrdata != NULL);

   SCIP_CALL( SCIPallocBlockMemory(scip, conshdlrdata) );

   return SCIP_OKAY;
}


/** frees constraint handler data for chance constraint handler */
static
void conshdlrdataFree(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSHDLRDATA**   conshdlrdata                         /**< pointer to the constraint handler data */
   )
{
   assert(scip != NULL);
   assert(conshdlrdata != NULL);
   assert(*conshdlrdata != NULL);

   SCIPfreeBlockMemory(scip, conshdlrdata);
}


/** destructor of constraint handler to free constraint handler data (called when SCIP is exiting) */
static
SCIP_DECL_CONSFREE(consFreeCCP)
{  /*lint --e{715}*/
   SCIP_CONSHDLRDATA* conshdlrdata;

   assert(scip != NULL);
   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);

   /* free constraint handler data */
   conshdlrdata = SCIPconshdlrGetData(conshdlr);
   assert(conshdlrdata != NULL);

   conshdlrdataFree(scip, &conshdlrdata);

   return SCIP_OKAY;
}


/** create constraint data */
static
SCIP_RETCODE consdataCreate(
   SCIP*                      scip,                           /**< SCIP data structure */
   SCIP_CONSDATA**            consdata,                       /**< pointer to store the constraint data */
   SCIP_CONSHDLR*             conshdlr,                       /**< constraint handler data */
   SCIP_Bool                  BASE_DB,                        /**< should the dominance-based branching be used to solve the problem? */
   SCIP_Bool                  BASE_DB_OPF,                    /**< should the dominance-based branching with overlap-oriented node pruning and variable fixing be used to solve the problem? */
   SCIP_VAR**                 varz,                           /**< scenario variable z */
   SCIP_VAR**                 varv,                           /**< introducing variable v */
   int                        nScenario,                      /**< number of scenarios */
   int                        dimension,                      /**< dimension of random vector */
   SCIP_Real*                 proba,                          /**< scenario probability vector */
   SCIP_Real                  epsilon,                        /**< confidence parameter */
   int*                       KI,                             /**< index vector of lower bound for variables v */
   int*                       basicVarzInd,                   /**< index of variable z corresponding to basic scenarios */
   int                        basicNScenario,                 /**< number of essential scenarios */
   SCIP_Real**                randomRhs,                      /**< random right-hand side matrix */
   SCIP_Real**                stRhs,                          /**< strengthened random right-hand side matrix */
   SCIP_Real**                sortTransRhs,                   /**< sorted transpose of random right-hand side matrix */
   int**                      sortTransRhsInd,                /**< index of sorted transpose of random right-hand side matrix */
   int**                      vIn,                            /**< matrix that stores the incoming  arc's tail of all vertices */
   int*                       vInSize,                        /**< vecotr that stores the number of the incoming  arc's tail for a vertice */
   int**                      vOut,                           /**< matrix that stores the outcoming arc's head of all vertices */
   int*                       vOutSize,                       /**< vecotr that stores the number of the outcoming arc's head for a vertice */
   SCIP_Bool**                edgeTH                          /**< the edges existing in the graph (tail-head) */
   )
{

   assert( scip != NULL );
   assert( consdata != NULL );
   assert( conshdlr != NULL );

   SCIP_CALL( SCIPallocBlockMemory(scip, consdata) );

   /* initialization statistic data */
   (*consdata)->npropagations      = 0;
   (*consdata)->cutoffNKC          = 0;
   (*consdata)->fixNOneVars        = 0;
   (*consdata)->fixNZeroVars       = 0;
   (*consdata)->tightNConVarLb     = 0;
   (*consdata)->tightNConVarLbTime = 0;
   (*consdata)->maxRoundPRA        = 0;
   (*consdata)->exactTime          = 0;
   (*consdata)->fixNOneVarsExact   = 0;
   (*consdata)->fixNZeroVarsExact  = 0;
   (*consdata)->cutoffTimesExact   = 0;
   (*consdata)->cutoffTimesAOPF    = 0;

   /* initialization input data */
   (*consdata)->BASE_DB          = BASE_DB;
   (*consdata)->BASE_DB_OPF      = BASE_DB_OPF;
   (*consdata)->nScenario        = nScenario;
   (*consdata)->dimension        = dimension;
   (*consdata)->basicNScenario   = basicNScenario;
   (*consdata)->epsilon          = epsilon;

   /* allocate memory */
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->varz),  nScenario ) );
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->proba), nScenario) );
   for( int i = 0; i < nScenario; i++ )
   {
      assert( varz[i] != NULL );
      (*consdata)->varz[i]  = varz[i];
      (*consdata)->proba[i] = proba[i];
   }
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->varv),  dimension ) );
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->KI),  dimension) );
   for( int i = 0; i < dimension; i++ )
   {
      assert( varv[i] != NULL );
      (*consdata)->varv[i] = varv[i];
      (*consdata)->KI[i]   = KI[i];
   }
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->basicVarzInd), basicNScenario) );
   for( int i = 0; i < basicNScenario; i++ )
   {
      (*consdata)->basicVarzInd[i] = basicVarzInd[i];
   }
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->sortTransRhs),    dimension) );
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->sortTransRhsInd), dimension) );
   for( int i = 0; i < dimension; i++ )
   {
      SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->sortTransRhs[i]), nScenario) );
      SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->sortTransRhsInd[i]), nScenario) );
      for( int s = 0; s < nScenario; s++ )
      {
         (*consdata)->sortTransRhs[i][s]    = sortTransRhs[i][s];
         (*consdata)->sortTransRhsInd[i][s] = sortTransRhsInd[i][s];
      }
   }
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->stRhs), nScenario) );
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->randomRhs), nScenario) );
   for( int i = 0; i < nScenario; i++ )
   {
      SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->stRhs[i] ), dimension) );
      SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->randomRhs[i] ), dimension) );
      for( int j = 0; j < dimension; j++ )
      {
         (*consdata)->stRhs[i][j] = stRhs[i][j];
         (*consdata)->randomRhs[i][j] = randomRhs[i][j];
      }
   }
   if( BASE_DB || BASE_DB_OPF )
   {
      SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->vOut),    basicNScenario) );
      SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->vIn),     basicNScenario) );
      SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->edgeTH),  basicNScenario) );
      for( int t = 0; t < basicNScenario; t++ )
      {
         SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->vIn[t]),  vInSize[t]) );
         SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->vOut[t]), vOutSize[t]) );
         SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->edgeTH[t]),  basicNScenario) );
         for( int s = 0; s < vInSize[t]; s++ )
         {
            (*consdata)->vIn[t][s] = vIn[t][s];
         }
         for( int s = 0; s < vOutSize[t]; s++ )
         {
            (*consdata)->vOut[t][s] = vOut[t][s];
         }
         for( int s = 0; s < basicNScenario; s++ )
         {
            if(t != s)
            {
               (*consdata)->edgeTH[t][s] = edgeTH[t][s];
            }
         }
      }
      SCIP_CALL (SCIPallocBlockMemoryArray(scip, &( (*consdata)->vInSize),  basicNScenario) );
      SCIP_CALL( SCIPallocBlockMemoryArray(scip, &( (*consdata)->vOutSize), basicNScenario) );
      for( int t = 0; t < basicNScenario; t++ )
      {
         (*consdata)->vOutSize[t]  = vOutSize[t];
         (*consdata)->vInSize[t]   = vInSize[t];
      }
   }

   if( BASE_DB_OPF == TRUE )
   {
      SCIP_CALL(SCIPallocBlockMemoryArray(scip, &((*consdata)->MiBeg), basicNScenario));
      SCIP_CALL(SCIPallocBlockMemoryArray(scip, &((*consdata)->roundMiBeg), basicNScenario));
      SCIP_CALL(SCIPallocBlockMemoryArray(scip, &((*consdata)->MiVal), basicNScenario));
      SCIP_CALL(SCIPallocBlockMemoryArray(scip, &((*consdata)->MiInd), basicNScenario));
      for( int s = 0; s < basicNScenario; s++ )
      {
         SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->MiVal[s]), basicNScenario));
         SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->MiInd[s]), basicNScenario));
      }
      SCIP_CALL( SCIPallocBlockMemoryArray(scip, &((*consdata)->nodeLb), dimension) );
   }

   if( SCIPisTransformed(scip) )
   {
      SCIP_CALL( SCIPgetTransformedVars(scip, nScenario, (*consdata)->varz, (*consdata)->varz) );
      SCIP_CALL( SCIPgetTransformedVars(scip, dimension, (*consdata)->varv, (*consdata)->varv) );
   }
   return SCIP_OKAY;
}


/** frees specific constraint data */
static
SCIP_DECL_CONSDELETE(consDeleteCCP)
{
   int i;
   int nScenario;
   int basicNScenario;
   int dimension;

   assert( conshdlr  != NULL );
   assert( strcmp( SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME ) == 0 );
   assert( consdata  != NULL );
   assert( *consdata != NULL );

   nScenario      = (*consdata)->nScenario;
   basicNScenario = (*consdata)->basicNScenario;
   dimension      = (*consdata)->dimension;

   if( (*consdata)->BASE_DB || (*consdata)->BASE_DB_OPF ) 
   {
      for( i = 0; i < basicNScenario; ++i )
      {
         SCIPfreeBlockMemoryArray(scip, &((*consdata)->vIn[i]),  (*consdata)->vInSize[i]);
         SCIPfreeBlockMemoryArray(scip, &((*consdata)->vOut[i]), (*consdata)->vOutSize[i]);
         SCIPfreeBlockMemoryArray(scip, &((*consdata)->edgeTH[i]), basicNScenario);
      }
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->vIn),      basicNScenario);
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->vOut),     basicNScenario);
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->edgeTH),   basicNScenario);
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->vOutSize), basicNScenario);
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->vInSize),  basicNScenario);
   }

   if( (*consdata)->BASE_DB_OPF == TRUE )
   {
      for( i = 0; i < basicNScenario; ++i )
      {
         SCIPfreeBlockMemoryArray(scip, &((*consdata)->MiVal[i]), basicNScenario);
         SCIPfreeBlockMemoryArray(scip, &((*consdata)->MiInd[i]), basicNScenario);
      }
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->MiVal), basicNScenario);
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->MiInd), basicNScenario);
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->MiBeg), basicNScenario);
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->roundMiBeg), basicNScenario);

      SCIPfreeBlockMemoryArray(scip, &((*consdata)->nodeLb), dimension);
   }

   for( i = 0; i < nScenario; ++i )
   {
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->randomRhs[i]), dimension);
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->stRhs[i]),     dimension);
   }
   SCIPfreeBlockMemoryArray(scip, &((*consdata)->randomRhs), nScenario);
   SCIPfreeBlockMemoryArray(scip, &((*consdata)->stRhs),     nScenario);
   SCIPfreeBlockMemoryArray(scip, &((*consdata)->varz),      nScenario);
   SCIPfreeBlockMemoryArray(scip, &((*consdata)->varv),      dimension);
   SCIPfreeBlockMemoryArray(scip, &((*consdata)->basicVarzInd), basicNScenario);
   SCIPfreeBlockMemoryArray(scip, &((*consdata)->proba),     nScenario);
   SCIPfreeBlockMemoryArray(scip, &((*consdata)->KI),        dimension);
   for( i = 0; i < dimension; ++i )
   {
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->sortTransRhs[i]),    nScenario);
      SCIPfreeBlockMemoryArray(scip, &((*consdata)->sortTransRhsInd[i]), nScenario);
   }
   SCIPfreeBlockMemoryArray(scip, &((*consdata)->sortTransRhs),    dimension);
   SCIPfreeBlockMemoryArray(scip, &((*consdata)->sortTransRhsInd), dimension);

   SCIPfreeBlockMemory(scip, consdata);
   return SCIP_OKAY;
}


/** transforms constraint data into data belonging to the transformed problem */
static
SCIP_DECL_CONSTRANS(consTransCCP)
{
   char s[SCIP_MAXSTRLEN];
   SCIP_CONSDATA* consdata;
   SCIP_CONSDATA* sourcedata;

   assert( scip != NULL );
   assert( conshdlr != NULL );
   assert( strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0 );
   assert( SCIPgetStage(scip) == SCIP_STAGE_TRANSFORMING );
   assert( sourcecons != NULL );
   assert( targetcons != NULL );

   SCIPdebugMsg(scip, "transforming chance constraint <%s>.\n", SCIPconsGetName(sourcecons) );

   /* get data of original constraint */
   sourcedata = SCIPconsGetData(sourcecons);
   assert(sourcedata != NULL);

   /* create constraint data for target constraint */
   SCIP_CALL( consdataCreate(scip, &consdata, conshdlr, sourcedata->BASE_DB, sourcedata->BASE_DB_OPF,
               sourcedata->varz, sourcedata->varv, sourcedata->nScenario, sourcedata->dimension, sourcedata->proba,
               sourcedata->epsilon, sourcedata->KI, sourcedata->basicVarzInd, sourcedata->basicNScenario, sourcedata->randomRhs,
               sourcedata->stRhs, sourcedata->sortTransRhs, sourcedata->sortTransRhsInd, sourcedata->vIn, sourcedata->vInSize,
               sourcedata->vOut, sourcedata->vOutSize, sourcedata->edgeTH) );

   /* create constraint */
   (void) SCIPsnprintf(s, SCIP_MAXSTRLEN, "t_%s", SCIPconsGetName(sourcecons));

   SCIP_CALL( SCIPcreateCons(scip, targetcons, s, conshdlr, consdata,
            SCIPconsIsInitial(sourcecons), SCIPconsIsSeparated(sourcecons),
            SCIPconsIsEnforced(sourcecons), SCIPconsIsChecked(sourcecons),
            SCIPconsIsPropagated(sourcecons), SCIPconsIsLocal(sourcecons),
            SCIPconsIsModifiable(sourcecons), SCIPconsIsDynamic(sourcecons),
            SCIPconsIsRemovable(sourcecons), SCIPconsIsStickingAtNode(sourcecons)) );

   return SCIP_OKAY;
}


/** copy method for constraint handler plugins (called when SCIP copies plugins) */
static
SCIP_DECL_CONSHDLRCOPY(conshdlrCopyCCP)
{
   assert( scip != NULL );
   assert( conshdlr != NULL );
   assert( strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0 );
   assert( valid != NULL );

   /* call inclusion method of constraint handler */
   SCIP_CALL( SCIPincludeConshdlrCCP(scip) );

   *valid = TRUE;

   return SCIP_OKAY;
}


/** constraint copying method of constraint handler */
static
SCIP_DECL_CONSCOPY(consCopyCCP)
{
   SCIP_CONSDATA* sourcedata;
   SCIP_VAR**     sourcevarz;
   SCIP_VAR**     sourcevarv;
   SCIP_Bool      BASE_DB;
   SCIP_Bool      BASE_DB_OPF;
   SCIP_VAR**     varz;
   SCIP_VAR**     varv;
   int            nScenario;
   int            dimension;
   SCIP_Real*     proba;
   SCIP_Real      epsilon;
   int*           KI;
   int*           basicVarzInd;
   int            basicNScenario;
   SCIP_Real**    randomRhs;
   SCIP_Real**    stRhs;
   SCIP_Real**    sortTransRhs;
   int**          sortTransRhsInd;
   int**          vIn;
   int*           vInSize;
   int**          vOut;
   int*           vOutSize;
   SCIP_Bool**    edgeTH;

   assert( scip != 0 );
   assert( varmap != 0 );
   assert( sourcescip != 0 );
   assert( sourcecons != 0 );
   assert( sourceconshdlr != 0 );
   assert( SCIPisTransformed(sourcescip) );
   assert( strcmp(SCIPconshdlrGetName(sourceconshdlr), CONSHDLR_NAME) == 0 );

   *valid = TRUE;
   SCIPdebugMsg(scip, "Copying method for chance constraint handler.\n");
   sourcedata = SCIPconsGetData(sourcecons);
   assert( sourcedata != NULL );

   BASE_DB            = sourcedata->BASE_DB;
   BASE_DB_OPF        = sourcedata->BASE_DB_OPF;
   sourcevarz         = sourcedata->varz;
   sourcevarv         = sourcedata->varv;
   nScenario          = sourcedata->nScenario;
   dimension          = sourcedata->dimension;
   proba              = sourcedata->proba;
   epsilon            = sourcedata->epsilon;
   KI                 = sourcedata->KI;
   basicVarzInd       = sourcedata->basicVarzInd;
   basicNScenario     = sourcedata->basicNScenario;
   randomRhs          = sourcedata->randomRhs;
   stRhs              = sourcedata->stRhs;
   sortTransRhs       = sourcedata->sortTransRhs;
   sortTransRhsInd    = sourcedata->sortTransRhsInd;
   vIn                = sourcedata->vIn;
   vInSize            = sourcedata->vInSize;
   vOut               = sourcedata->vOut;
   vOutSize           = sourcedata->vOutSize;
   edgeTH             = sourcedata->edgeTH;

   assert( sourcevarz != NULL );
   assert( sourcevarv != NULL );

   SCIP_CALL( SCIPallocBufferArray(scip, &varz, nScenario) );
   BMSclearMemoryArray(varz, nScenario);
   SCIP_CALL( SCIPallocBufferArray(scip, &varv, dimension) );
   BMSclearMemoryArray(varv, dimension);

   for( int i = 0; i < nScenario && *valid; i++ )
   {
      SCIP_CALL( SCIPgetVarCopy(sourcescip, scip, sourcevarz[i], &varz[i], varmap, consmap, global, valid) );
      assert( !(*valid) || varz[i] != NULL );
   }
   for( int i = 0; i < dimension && *valid; i++ )
   {
      SCIP_CALL( SCIPgetVarCopy(sourcescip, scip, sourcevarv[i], &varv[i], varmap, consmap, global, valid) );
      assert( !(*valid) || varv[i] != NULL );
   }
   if( *valid )
   {
      if( name == 0 )
      {
         name = SCIPconsGetName(sourcecons);
      }
      /* create copied constraint */
      SCIP_CALL( SCIPcreateConsCCP(scip, cons, name, BASE_DB, BASE_DB_OPF, varz, varv,
               nScenario, dimension, proba, epsilon, KI, basicVarzInd, basicNScenario, randomRhs, stRhs,
					sortTransRhs, sortTransRhsInd, vIn, vInSize, vOut, vOutSize, edgeTH) );
   }

   /* free memory in reverse order */
   SCIPfreeBufferArray(scip, &varz);
   SCIPfreeBufferArray(scip, &varv);

   return SCIP_OKAY;
}


/** initial queue (graph) */
static
SCIP_RETCODE graphDivideVar(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,                            /**< constraint data */
   Queue*                Queueone,                            /**< queue that store the index of varible fixing to one */
   Queue*                Queuezero,                           /**< queue that store the index of varible fixing to zero */
   Queue*                Queuefree,                           /**< queue that store the index of free varible */
   char**                visitedOne,                          /**< whether varible is fixed to one */
   char**                visitedZero,                         /**< whether varible is fixed to zero */
   int*                  basicVarzInd,                        /**< index of variable z corresponding to basic scenarios */
   int                   basicNScenario                       /**< number of scenarios with right-side hand greater than the lower bound */
   )
{
   SCIP_VAR* var;
   InitQueue(scip, Queueone);
   InitQueue(scip, Queuezero);
   InitQueue(scip, Queuefree);
   assert(Queueone->front ==Queueone->rear );
   assert(Queuezero->front==Queuezero->rear);
   assert(Queuefree->front==Queuefree->rear);

   for( int s = 0; s < basicNScenario; s++ )
   {
      var = consdata->varz[basicVarzInd[s]];
      if( SCIPvarGetLbLocal(var) > 0.5 )
      {
         EnQueue(scip, Queueone, basicNScenario, s);
         (*visitedOne)[s]  = 'G';
         (*visitedZero)[s] = 'W';
      }
      else if( SCIPvarGetUbLocal(var) < 0.5 )
      {
         EnQueue(scip, Queuezero, basicNScenario, s);
         (*visitedOne)[s]  = 'W';
         (*visitedZero)[s] = 'G';
      }
      else
      {
         assert(SCIPvarGetLbLocal(var) < 0.5);
         EnQueue(scip, Queuefree, basicNScenario, s);
         (*visitedOne)[s]  = 'W';
         (*visitedZero)[s] = 'W';
      }
   }
   assert((Queueone->size+Queuezero->size+Queuefree->size) == basicNScenario);

   return SCIP_OKAY;
}


/** check whether the knapsack constraint is feasible */
static
SCIP_RETCODE checkKnapCons(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,                            /**< constraint data */
   Queue*                Queueone,                            /**< queue that store the index of varible fixing to one */
   SCIP_Bool*            cutoff                               /**< pointer to store if a cutoff was detected */
   )
{
   int ind;
   SCIP_Real  residuals;

   residuals = consdata->epsilon;
   for( int i = 0; i < Queueone->size; i++ )
   {
      ind = consdata->basicVarzInd[Queueone->base[i]];
      residuals -= consdata->proba[ind];
   }

   /* infeasible case one */
   if( SCIPisLT(scip, residuals, 0.0) )
   {
      (*cutoff) = TRUE;
      if( !SCIPinProbing(scip) )
      {
         consdata->cutoffNKC++;
      }
   }

   return SCIP_OKAY;
}


/** check whether or not the knapsack can handle the scenario */
static
SCIP_RETCODE varFixtoZeroByKnapCons(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,                            /**< constraint data */
   int*                  basicVarzInd,                        /**< set of indexes for scenarios with right-side hand greater than the lower bound */
   Queue*                Queueone,                            /**< queue that store the index of varible fixing to one */
   Queue*                Queuefree,                           /**< queue that store the index of free varible */
   SCIP_Bool*            isFixVar,                            /**< whether a variable is fixed */
   int*                  nfixedvars,                          /**< pointer to store the number of fixed variables */
   SCIP_Bool*            cutoff                               /**< pointer to store if a cutoff was detected */
   )
{
   SCIP_Bool fixed;
   SCIP_Bool infeasible;

   int ind;
   SCIP_VAR* var;
   SCIP_Real  residuals;

   residuals = consdata->epsilon;
   for( int i = 0; i < Queueone->size; i++ )
   {
      ind = basicVarzInd[Queueone->base[i]];
      residuals -= consdata->proba[ind];
   }

   for( int i = 0; i < Queuefree->size; i++ )
   {
      ind = basicVarzInd[Queuefree->base[i]];
      if( SCIPisGT(scip, consdata->proba[ind], residuals) )
      {
         var = consdata->varz[ind];
         if( SCIPvarGetUbLocal(var) < 0.5 )
         {
            continue;
         }
         else
         {
            (*isFixVar) = TRUE;
            SCIP_CALL( SCIPfixVar(scip, var, 0.0, &infeasible, &fixed) );
            if( infeasible )
            {
               if( !SCIPinProbing(scip) )
               {
                  consdata->cutoffTimesAOPF++;
               }
               assert(SCIPvarGetLbLocal(var) > 0.5);
               SCIPdebugMsg(scip, "-> cutoff\n");
               (*cutoff) = TRUE;
               break;
            }
            else
            {
               if( !SCIPinProbing(scip) )
               {
                  consdata->fixNZeroVars++;
               }
               assert(fixed);
               (*nfixedvars)++;
            }
         }
      }
   }

   return SCIP_OKAY;
}


/** variable fixing by graph (reverse) breadth-first search based on the dominance inequalities using queue--local bound fixed at 0(1) BFS(RBFS) */
static
SCIP_RETCODE varFixByGraphBFS(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,                            /**< constraint data */
   int*                  basicVarzInd,                        /**< pointer to store the number of fixed variables */
   Queue*                Queueone,                            /**< queue that store the index of varible fixing to one */
   Queue*                Queuezero,                           /**< queue that store the index of varible fixing to zero */
   char**                visitedOne,                          /**< whether varible is fixed to one */
   char**                visitedZero,                         /**< whether varible is fixed to zero */
   int*                  nfixedvars,                          /**< pointer to store the number of fixed variables */
   SCIP_Bool*            cutoff                               /**< pointer to store if a cutoff was detected */
   )
{
   SCIP_Bool fixed;
   SCIP_Bool infeasible;

   int tempNode;
   int tail;
   int head;

   /* reverse breadth-first search */
   while( !QueueEmpty(Queueone) && !(*cutoff) )
   {
      tempNode = DeQueue(Queueone, consdata->basicNScenario);
      for( int j = 0; j < consdata->vInSize[tempNode]; j++ )
      {
         tail = consdata->vIn[tempNode][j];
         if( (*visitedOne)[tail] == 'W' && consdata->edgeTH[tail][tempNode] == TRUE )
         {
            (*visitedOne)[tail] = 'G';
            EnQueue(scip, Queueone, consdata->basicNScenario, tail);

            /* fix the variable LocalLB for Ind = tail to one */
            if( SCIPvarGetLbLocal(consdata->varz[basicVarzInd[tail]]) < 0.5 )
            {
               SCIP_CALL( SCIPfixVar(scip, consdata->varz[basicVarzInd[tail]], 1.0, &infeasible, &fixed) );
               if( infeasible )
               {
                  assert(SCIPvarGetUbLocal(consdata->varz[basicVarzInd[tail]]) < 0.5);
                  SCIPdebugMsg(scip, "-> cutoff\n");
                  (*cutoff) = TRUE;
                  break;
               }
               else
               {
                  if( !SCIPinProbing(scip) )
                  {
                     consdata->fixNOneVars++;
                  }
                  assert(fixed);
                  (*nfixedvars)++;
               }
            }
         }
      }
      (*visitedOne)[tempNode] = 'B';
   }

   /* breadth-first search */
   while( !QueueEmpty(Queuezero) && !(*cutoff) )
   {
      tempNode = DeQueue(Queuezero, consdata->basicNScenario);
      for( int j = 0; j < consdata->vOutSize[tempNode]; j++ )
      {
         head = consdata->vOut[tempNode][j];
         if( (*visitedZero)[head] == 'W' && consdata->edgeTH[tempNode][head] == TRUE )
         {
            (*visitedZero)[head] = 'G';
            EnQueue(scip, Queuezero, consdata->basicNScenario, head);

            /* fix the variable LocalLB for Ind = head to zero */
            if( SCIPvarGetUbLocal(consdata->varz[basicVarzInd[head]]) > 0.5 )
            {
               SCIP_CALL( SCIPfixVar(scip, consdata->varz[basicVarzInd[head]], 0.0, &infeasible, &fixed) );

               if( infeasible )
               {
                  assert(SCIPvarGetLbLocal(consdata->varz[basicVarzInd[head]]) > 0.5);
                  SCIPdebugMsg(scip, "-> cutoff\n");
                  (*cutoff) = TRUE;
                  break;
               }
               else
               {
                  if( !SCIPinProbing(scip) )
                  {
                     consdata->fixNZeroVars++;
                  }
                  assert(fixed);
                  (*nfixedvars)++;
               }
            }
         }
      }
      (*visitedZero)[tempNode] = 'B';
   }

   return SCIP_OKAY;
}


/** Dominance-based branching (graph-based implementation) */
static
SCIP_RETCODE dominanceBranch(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,                            /**< constraint data */
   int*                  basicVarzInd,                        /**< the index of variable z corresponding to basic scenarios */
   int*                  nfixedvars,                          /**< pointer to store the number of fixed variables */
   SCIP_Bool*            cutoff                               /**< pointer to store if a cutoff was detected */
   )
{
   int   basicNScenario;
   char* visitedOne;
   char* visitedZero;
   Queue Queueone;
   Queue Queuezero;
   Queue Queuefree;
   SCIP_Bool isFixVar;

   assert(scip != NULL);
   assert(consdata != NULL);
   assert(nfixedvars != NULL);

   isFixVar  = FALSE;
   basicNScenario = consdata->basicNScenario;
   /* allocate buffer memory */
	SCIP_CALL( SCIPallocBufferArray(scip, &(Queueone.base),  basicNScenario) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(Queuezero.base), basicNScenario) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(Queuefree.base), basicNScenario) );
   SCIP_CALL( SCIPallocBufferArray(scip, &visitedOne,	   basicNScenario) );
   SCIP_CALL( SCIPallocBufferArray(scip, &visitedZero,   basicNScenario) );

   /* initialize queue data */
   SCIP_CALL( graphDivideVar(scip, consdata, &Queueone, &Queuezero, &Queuefree, &visitedOne, &visitedZero, basicVarzInd, basicNScenario) );
   /* check whether the knapsack constraint is feasible */ 
   SCIP_CALL( checkKnapCons(scip, consdata, &Queueone, cutoff) );
   if( *cutoff == FALSE )
   {
      /* check whether or not the knapsack can handle the scenario */ 
      SCIP_CALL( varFixtoZeroByKnapCons(scip, consdata, consdata->basicVarzInd, &Queueone, &Queuefree, &isFixVar, nfixedvars, cutoff) );
      if( *cutoff == FALSE )
      {
         if( isFixVar == TRUE )
         {
            SCIP_CALL( graphDivideVar(scip, consdata, &Queueone, &Queuezero, &Queuefree, &visitedOne, &visitedZero, basicVarzInd, basicNScenario) );
         }
         /* variable fixing by graph (reverse) breadth-first search based on the dominance inequalities--local bound fixed at 0(1) by BFS(RBFS) */
         SCIP_CALL( varFixByGraphBFS(scip, consdata, basicVarzInd, &Queueone, &Queuezero, &visitedOne, &visitedZero, nfixedvars, cutoff) );
      }
   }

   /* free temporary memory */
   SCIPfreeBufferArray(scip, &(Queueone.base));
   SCIPfreeBufferArray(scip, &(Queuezero.base));
   SCIPfreeBufferArray(scip, &(Queuefree.base));
   SCIPfreeBufferArray(scip, &visitedOne);
   SCIPfreeBufferArray(scip, &visitedZero);

   return SCIP_OKAY;
}


/** initial queue (the overlap-oriented node pruning and variable fixing) */
static
SCIP_RETCODE divideVar(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,                            /**< constraint data */
   Queue*                Queueone,                            /**< queue that store the index of varible fixing to one */
   Queue*                Queuezero,                           /**< queue that store the index of varible fixing to zero */
   Queue*                Queuefree,                           /**< queue that store the index of free varible */
   int*                  basicVarzInd,                        /**< index of variable z corresponding to basic scenarios */
   int                   basicNScenario                       /**< number of scenarios with right-side hand greater than the lower bound */
   )
{
   SCIP_VAR* var;
   InitQueue(scip, Queueone);
   InitQueue(scip, Queuezero);
   InitQueue(scip, Queuefree);
   assert(Queueone->front ==Queueone->rear);
   assert(Queuezero->front==Queuezero->rear);
   assert(Queuefree->front==Queuefree->rear);

   for( int s = 0; s < basicNScenario; s++ )
   {
      var = consdata->varz[basicVarzInd[s]];
      if( SCIPvarGetLbLocal(var) > 0.5 )
      {
         EnQueue(scip, Queueone, basicNScenario, s);
      }
      else if( SCIPvarGetUbLocal(var) < 0.5 )
      {
         EnQueue(scip, Queuezero, basicNScenario, s);
      }
      else
      {
         assert(SCIPvarGetLbLocal(var) < 0.5);
         EnQueue(scip, Queuefree, basicNScenario, s);
      }
   }
   assert((Queueone->size+Queuezero->size+Queuefree->size) == basicNScenario);

   return SCIP_OKAY;
}


/** try to update the lower bound of the node:
 * (i) determine if a better lower bound is obtained;
 * (ii) update the local bound of the continue variable. */
static
SCIP_RETCODE updateLowerBoundVarV(
   SCIP*                scip,                                 /**< SCIP data structure */
   SCIP_CONSDATA*       consdata,                             /**< constraint data */
   SCIP_CONSHDLRDATA*   conshdlrdata,                         /**< constraint handler data */
   int*                 basicVarzInd,                         /**< index of variable z corresponding to basic scenarios */
   Queue*               Queueone,                             /**< queue that store the index of varible fixing to one */
   SCIP_Real*           nodeLb                                /**< node lower bound for continuous variable */
   )
{
   SCIP_VAR* var;
   SCIP_Real nodeEps;
   SCIP_Real residuals;

   residuals = consdata->epsilon;
   for( int i = 0; i < Queueone->size; i++ )
   {
      residuals -= consdata->proba[basicVarzInd[Queueone->base[i]]];
   }

   /* compute the lower bound of the child node */
   for( int i = 0; i < consdata->dimension; i++ )
   {
      nodeEps = residuals;
      for( int j = 0; j <= consdata->KI[i]; j++ )
      {
         var = consdata->varz[consdata->sortTransRhsInd[i][j]];
         if( SCIPvarGetUbLocal(var) < 0.5 )
         {
            nodeLb[i] = consdata->randomRhs[consdata->sortTransRhsInd[i][j]][i];
            break;
         }
         else if( SCIPvarGetLbLocal(var) < 0.5 && SCIPvarGetUbLocal(var) > 0.5 )
         {
            nodeEps -= consdata->proba[consdata->sortTransRhsInd[i][j]];
            if( SCIPisLT(scip, nodeEps, 0) )
            {
               nodeLb[i] = consdata->randomRhs[consdata->sortTransRhsInd[i][j]][i];
               break;
            }
         }
      }
      assert(SCIPisGE(scip, nodeLb[i], consdata->randomRhs[consdata->sortTransRhsInd[i][consdata->KI[i]]][i]));

      /* update the local bound of the continue variable */
      if( conshdlrdata->tightContVarBound == TRUE )
      {
         clock_t start = clock();
         if( SCIPisLT(scip, nodeLb[i], SCIPvarGetLbLocal(consdata->varv[i])) )
         {
            nodeLb[i] = SCIPvarGetLbLocal(consdata->varv[i]);
         }
         else if( SCIPisGT(scip, nodeLb[i], SCIPvarGetUbLocal(consdata->varv[i])) )
         {
            continue;
         }
         else if( !SCIPisEQ(scip, nodeLb[i], SCIPvarGetLbLocal(consdata->varv[i]))
               && SCIPvarGetStatus(consdata->varv[i]) != SCIP_VARSTATUS_MULTAGGR
               && SCIPgetSubscipDepth(scip) == 0
               && SCIPvarGetProbindex(consdata->varv[i]) >= 0 )
         {
            SCIP_CALL( SCIPchgVarLb(scip, consdata->varv[i], nodeLb[i]) );

            if( !SCIPinProbing(scip) )
            {
               consdata->tightNConVarLb++;
            }
         }
         consdata->tightNConVarLbTime += (clock()-start)/(SCIP_Real)CLOCKS_PER_SEC;
      }
   }

   return SCIP_OKAY;
}


/** calculate the components of a random vector to be compared */
static
SCIP_RETCODE computeSetM(
   SCIP*                scip,                                 /**< SCIP data structure */
   SCIP_CONSDATA*       consdata,                             /**< constraint data */
   int*                 basicVarzInd,                         /**< index of variable z corresponding to basic scenarios */
   SCIP_Real*           nodeLb                                /**< node lower bound for continuous variable */
   )
{

   int dimension = consdata->dimension;
   int basicNScenario = consdata->basicNScenario;
   for( int s = 0; s < basicNScenario; s++ )
   {
      int count = 0;
      for( int i = 0; i < dimension; i++ )
      {
         if( SCIPisGT(scip, consdata->randomRhs[basicVarzInd[s]][i], nodeLb[i]) )
         {
            consdata->MiInd[s][count] = i;
            consdata->MiVal[s][count] = consdata->randomRhs[basicVarzInd[s]][i];
            count++;
         }
      }
      consdata->MiBeg[s] = count;
   }

   return SCIP_OKAY;
}


/** reduce the initial set N_1 to S_1 */
static
SCIP_RETCODE reduceSetN1(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,                            /**< constraint data */
   int*                  basicVarzInd,                        /**< index of variable z corresponding to basic scenarios */
   Queue*                Queueone,                            /**< queue that store the index of varible fixing to one */
   Queue*                QueueoneS                            /**< queue stores a set of scenarios that are fixed to one and are not dominated */
   )
{
   int ind1;
   int ind2;
   int oneSize;
   int count;
   SCIP_Bool flag;
   SCIP_Bool mark;
   SCIP_Bool* barS;
   int*  tempQueue;

   count = 0;
   oneSize = Queueone->size;
   /* allocate buffer memory */
   SCIP_CALL( SCIPallocBufferArray(scip, &barS, oneSize) );
   SCIP_CALL( SCIPallocBufferArray(scip, &tempQueue, consdata->basicNScenario) );


   for (int i = 0; i < oneSize; i++)
   {
      barS[i] = TRUE;
   }

   for (int t1 = 0; t1 < oneSize; t1++)
   {
      flag = FALSE;
      ind1 = Queueone->base[t1];
      for (int t2 = 0; t2 < oneSize; t2++)
      {
         if (t1 == t2 || barS[t2] == FALSE)
         {
            continue;
         }
         mark = TRUE;
         ind2 = Queueone->base[t2];
         for (int i = 0; i < consdata->MiBeg[ind2]; i++)
         {
            /* C_{i_2}^(N_0,N_1)(\xi^{i_1}) = 0, i.e, \xi^{i_1}_k >= \xi^{i_2}_k, for all k \in M_{i_2}(N_0, N_1) */
            if (SCIPisGT(scip, consdata->MiVal[ind2][i], consdata->randomRhs[basicVarzInd[ind1]][consdata->MiInd[ind2][i]]))
            {
               mark = FALSE;
               break;
            }
         }
         if (mark == TRUE)
         {
            barS[t1] = FALSE;
            flag = TRUE;
            break;
         }
      }
      if (flag == FALSE)
      {
         tempQueue[count] = ind1;
         count++;
      }
   }

   InitQueue(scip, QueueoneS);
   for (int i = 0; i < count; i++)
   {
      EnQueue(scip, QueueoneS, consdata->basicNScenario, tempQueue[i]);
   }

   /* free memory of arrays */
   SCIPfreeBufferArray(scip, &tempQueue);
   SCIPfreeBufferArray(scip, &barS);

   return SCIP_OKAY;
}


/** check whether there exits some i \in S_1 such that \xi^i \leq \xi(N_0,N_1) */
static
SCIP_RETCODE checkSuffCondition(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,                            /**< constraint data */
   int*                  basicVarzInd,                        /**< index of variable z corresponding to basic scenarios */
   Queue*                Queueone,                            /**< queue that store the index of varible fixing to one */
   SCIP_Bool*            cutoff                               /**< pointer to store if a cutoff was detected */
   )
{
   /* cutoff */
   int omega;
   for (int i = 0; i < Queueone->size; i++)
   {
      omega = Queueone->base[i];
      assert(SCIPvarGetLbLocal(consdata->varz[basicVarzInd[omega]]) > 0.5);
      if (consdata->MiBeg[omega] == 0)
      {
         (*cutoff) = TRUE;
         if( !SCIPinProbing(scip) )
         {
            consdata->cutoffTimesAOPF++;
         }
         break;
      }
   }

   return SCIP_OKAY;
}


/** fix variables z that satisfies certain conditions to one */
static
SCIP_RETCODE varFixtoOne(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,                            /**< constraint data */
   int*                  basicVarzInd,                        /**< index of variable z corresponding to basic scenarios */
   Queue*                QueueoneS,                           /**< queue stores a set of scenarios that are fixed to one and are not dominated */
   Queue*                Queuefree,                           /**< queue that store the index of free varible */
   SCIP_Bool*            isFixVar,                            /**< whether a variable is fixed */
   int*                  nfixedvars,                          /**< pointer to store the number of fixed variables */
   SCIP_Bool*            cutoff                               /**< pointer to store if a cutoff was detected */
   )
{
   SCIP_Bool fixed;
   SCIP_Bool infeasible;

   int i;
   int omega;
   SCIP_VAR* var;
   SCIP_Bool mark;
   for( int s1 = 0; s1 < Queuefree->size; s1++ )
   {
      omega = Queuefree->base[s1];
      var = consdata->varz[basicVarzInd[omega]];
      if( SCIPvarGetLbLocal(var) > 0.5 )
      {
         continue;
      }
      for( int s2 = 0; s2 < QueueoneS->size; s2++ )
      {
         mark = TRUE;
         i = QueueoneS->base[s2];
         for( int t = 0; t < consdata->MiBeg[i]; t++ )
         {
            /* C_{i}^(N_0,N_1)(\xi^{omega}) = 0, i.e, \xi^{i}_k <= \xi^{omega}_k, for all k \in M_{i}(N_0, N_1) */
            if( SCIPisLT(scip, consdata->randomRhs[basicVarzInd[omega]][consdata->MiInd[i][t]], consdata->MiVal[i][t]) )
            {
               mark = FALSE;
               break;
            }
         }
         if( mark == TRUE )
         {
            (*isFixVar) = TRUE;
            SCIP_CALL( SCIPfixVar(scip, var, 1.0, &infeasible, &fixed) );
            if( infeasible )
            {
               if( !SCIPinProbing(scip) ) 
               {
                  consdata->cutoffTimesAOPF++;
               }
               assert( SCIPvarGetUbLocal(var) < 0.5 );
               SCIPdebugMsg(scip, "-> cutoff\n");
               (*cutoff) = TRUE;
               break;
            }
            else
            {
               if( !SCIPinProbing(scip) )
               {
                  consdata->fixNOneVars++;
               }
               assert(fixed);
               (*nfixedvars)++;
            }
            break;
         }
      }
   }
   return SCIP_OKAY;
}


/** check whether there exists some i \in N_f such that \xi^i \leq \xi(N_0,N_1) */
static
SCIP_RETCODE varFixtoZero(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,                            /**< constraint data */
   Queue*                Queuefree,                           /**< queue that store the index of free varible */
   int*                  basicVarzInd,                        /**< index of variable z corresponding to basic scenarios */
   SCIP_Bool*            isFixVar,                            /**< whether a variable is fixed */
   int*                  nfixedvars,                          /**< pointer to store the number of fixed variables */
   SCIP_Bool*            cutoff                               /**< pointer to store if a cutoff was detected */
   )
{
   SCIP_Bool fixed;
   SCIP_Bool infeasible;

   int omega;
   for( int i = 0; i < Queuefree->size; i++ )
   {
      omega = Queuefree->base[i];
      if( SCIPvarGetUbLocal(consdata->varz[basicVarzInd[omega]]) < 0.5 )
      {
         continue;
      }
      if( consdata->MiBeg[omega] == 0 )
      {
         (*isFixVar) = TRUE;
         SCIP_CALL( SCIPfixVar(scip, consdata->varz[basicVarzInd[omega]], 0.0, &infeasible, &fixed) );
         if( infeasible )
         {
            if( !SCIPinProbing(scip) )            
            {
               consdata->cutoffTimesAOPF++;
            }
            assert( SCIPvarGetLbLocal(consdata->varz[basicVarzInd[omega]]) > 0.5 );
            SCIPdebugMsg(scip, "-> cutoff\n");
            (*cutoff) = TRUE;
            break;
         }
         else
         {
            if( !SCIPinProbing(scip) )
            {
               consdata->fixNZeroVars++;
            }
            assert(fixed);
            (*nfixedvars)++;
         }
      }
   }

   return SCIP_OKAY;
}

static
int SloveSubMIPExactFixing(
   int            dimension,           /**< pointer to store the dimension of random vector on exit */
   int            nScenario,
   SCIP_Real**    rhsData,             /**< pointer to store the random right-hand side matrix on exit */
   SCIP_Real*     proba,               /**< pointer to store the scenario probability vector on exit */
   int*           basicVarzInd,        /**< the index of variable z corresponding to basic scenarios */
   Queue          Queueone,
   Queue          Queuezero,
   Queue          Queuefree,
   SCIP_Real*     nodeLb,
   SCIP_Real*     optValSubProb
)
{
   int** 	      MJ;
   int*           NumMJ;
   char 			   name[SCIP_MAXSTRLEN];
   SCIP_Real* 	   maxN0;
   SCIP_VAR** 		varz;
   SCIP_VAR***		varw;
   SCIP_CONS* 		cons;
   SCIP_Bool      flag;
   SCIP_STATUS    solutionstatus;
   (*optValSubProb) = -1.0;

   /* Setting up the SCIP environment */
   SCIP* scip = NULL;
   /* Creating the SCIP environment */
   SCIP_CALL( SCIPcreate(&scip) );
   /* include default plugins */
   SCIP_CALL( SCIPincludeDefaultPlugins(scip) );
   /* turns off the SCIP output */
   SCIP_CALL( SCIPsetIntParam(scip, "display/verblevel", 0) );

   /* compute xi^{N_0} */   
   SCIP_CALL( SCIPallocBufferArray(scip, &maxN0, dimension) );
   for( int i = 0; i < dimension; i++ )
   {
      // maxN0[i] = 0;
      maxN0[i] = nodeLb[i];
      for( int s = Queuezero.front; s < Queuezero.rear; s++ )
      {
         flag = SCIPisLT(scip, maxN0[i], rhsData[basicVarzInd[Queuezero.base[s]]][i]);
         maxN0[i] = flag ? rhsData[basicVarzInd[Queuezero.base[s]]][i] : maxN0[i];  
      }
   }
   
   /* compute the M_j */
   SCIP_CALL( SCIPallocBufferArray(scip, &NumMJ, Queueone.size) );
   SCIP_CALL( SCIPallocBufferArray(scip, &MJ, Queueone.size) );
   for( int i = 0; i < Queueone.size; i++ )
   {
      SCIP_CALL( SCIPallocBufferArray(scip, &(MJ[i]), dimension) );
   }
   
   for( int i = 0; i < Queueone.size; i++ )
   {
      NumMJ[i] = 0;
      for( int k = 0; k < dimension; k++ )
      {  
         flag = SCIPisLE(scip, rhsData[basicVarzInd[Queueone.base[i]]][k]-maxN0[k], 0);
         if( !flag )
         {
            MJ[i][NumMJ[i]] = k;
            NumMJ[i]++;
         }
      }
      if( NumMJ[i] == 0 )
      {
         (*optValSubProb) = SCIP_DEFAULT_INFINITY;
         break;
      }
   }

   if( SCIPisEQ(scip, (*optValSubProb), SCIP_DEFAULT_INFINITY) )
   {
      SCIPfreeBufferArray(scip, &maxN0);
      for( int i = 0; i < Queueone.size; i++ )
      {
         SCIPfreeBufferArray(scip, &MJ[i]);
      }
      SCIPfreeBufferArray(scip, &MJ);
      SCIPfreeBufferArray(scip, &NumMJ);

      /* freeing scip */
      SCIP_CALL( SCIPfree(&scip) );
      BMScheckEmptyMemory();

      return 1;
   }

   /* creating the SCIP Problem */
   SCIP_CALL( SCIPcreateProbBasic(scip, "ccpsubporblem") );

   /* set objective sense */
   SCIP_CALL( SCIPsetObjsense(scip, SCIP_OBJSENSE_MINIMIZE) );

   /* variables z */
   SCIP_CALL( SCIPallocBufferArray(scip, &varz, Queuefree.size) );
   for( int i = 0; i < Queuefree.size; i++ )
   {
      int j = Queuefree.base[i]; 
      (void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "z#%d", j);
      
      /* create the SCIP_VAR object */
      SCIP_CALL( SCIPcreateVar(scip,
               &(varz[i]),              // returns new index
               name,   						 // name
               0.0,                     // lower bound
               1.0,                     // upper bound
               proba[basicVarzInd[j]],  // objective
               SCIP_VARTYPE_BINARY,     // variable type
               TRUE,                    // initial
               FALSE,                   // forget the rest ...
               NULL, NULL, NULL, NULL, NULL) );  /*lint !e732 !e747*/

      /* add the SCIP_VAR object to the scip problem */
      SCIP_CALL( SCIPaddVar(scip, varz[i]) );
   }

   /* variable w */
   SCIP_CALL( SCIPallocBufferArray(scip, &varw, Queueone.size) );
   for( int i = 0; i < Queueone.size; i++ )
   {
      int j = Queueone.base[i];
      SCIP_CALL( SCIPallocBufferArray(scip, &(varw[i]), NumMJ[i]) );

      for ( int k = 0; k < NumMJ[i]; k++ )
      {
         (void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "w#%d#%d", j, MJ[i][k]);
         
         /* create the SCIP_VAR object */
         SCIP_CALL( SCIPcreateVar(scip,
                  &(varw[i][k]),             // returns new index
                  name,   						 	// name
                  0.0,                     	// lower bound
                  1.0,                      	// upper bound
                  0.0,                       // objective
                  SCIP_VARTYPE_BINARY,       // variable type
                  TRUE,                    	// initial
                  FALSE,                   	// forget the rest ...
                  NULL, NULL, NULL, NULL, NULL) );  /*lint !e732 !e747*/

         /* add the SCIP_VAR object to the scip problem */
         SCIP_CALL( SCIPaddVar(scip, varw[i][k]) );
      }
   }

   /* add first constraint */
   for( int i = 0; i < Queueone.size; i++ )
   {
      cons = NULL;
      (void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "first_%d", i);

      /* we first create an empty inequality constraint and then add the variables */
      SCIP_CALL( SCIPcreateConsBasicLinear(
               scip,
               &cons,                   					/* pointer to hold the created constraint */
               name,  											/* name of constraint */
               0,                      					/* number of nonzeros in the constraint */
               NULL,                   					/* array with variables of constraint entries */
               NULL,                   					/* array with coefficients of constraint entries */
               1,                                     /* left hand side of constraint */
               1 ));   					                  /* right hand side of constraint */
      
      for( int k = 0; k < NumMJ[i]; k++ )
      {
         SCIP_CALL( SCIPaddCoefLinear(scip, cons, varw[i][k], 1) );
      }
      /* add the constraint to scip */
      SCIP_CALL( SCIPaddCons(scip, cons) );
      /* release this constraint */
      SCIP_CALL( SCIPreleaseCons(scip, &cons) );
   }

   /* add second constraint */
   for( int i = 0; i < Queueone.size; i++ )
   {
      int indexi = Queueone.base[i];
      for( int j = 0; j < Queuefree.size; j++ )
      {
         int indexj = Queuefree.base[j];

         flag = FALSE;
         for( int k = 0; k < NumMJ[i]; k++ )
         {
            if( SCIPisGE(scip, rhsData[basicVarzInd[indexj]][MJ[i][k]], rhsData[basicVarzInd[indexi]][MJ[i][k]]) )
            {
               flag = TRUE;
               break;
            }
         }
         if( flag == TRUE )
         {
            cons = NULL;
            (void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "second_%d_%d", i, j);
            /* we first create an empty inequality constraint and then add the variables */
            SCIP_CALL( SCIPcreateConsBasicLinear(
                     scip,
                     &cons,                   					/* pointer to hold the created constraint */
                     name,  											/* name of constraint */
                     0,                      					/* number of nonzeros in the constraint */
                     NULL,                   					/* array with variables of constraint entries */
                     NULL,                   					/* array with coefficients of constraint entries */
                     -SCIPinfinity(scip),                   /* left hand side of constraint */
                     0));   					                  /* right hand side of constraint */
            
            SCIP_CALL( SCIPaddCoefLinear(scip, cons, varz[j], -1.0) );

            for( int k = 0; k < NumMJ[i]; k++ )
            {
               if( SCIPisGE(scip, rhsData[basicVarzInd[indexj]][MJ[i][k]], rhsData[basicVarzInd[indexi]][MJ[i][k]]) )
               {
                  SCIP_CALL( SCIPaddCoefLinear(scip, cons, varw[i][k], 1.0) );
               }
            }

            /* add the constraint to scip */
            SCIP_CALL( SCIPaddCons(scip, cons) );
            /* release this constraint */
            SCIP_CALL( SCIPreleaseCons(scip, &cons) );
         }
      }
   }

   /* free memory of defaul data arrays */
   for( int i = 0; i < Queueone.size; i++ )
   {
      for( int k = 0; k < NumMJ[i]; k++ )
      {
         SCIP_CALL( SCIPreleaseVar(scip, &(varw[i][k])) );
      }
      SCIPfreeBufferArray(scip, &(varw[i]));
   }
   SCIPfreeBufferArray(scip, &varw);

   for( int i = 0; i < Queuefree.size; i++ )
   {
      SCIP_CALL( SCIPreleaseVar(scip, &(varz[i])) );
   }
   SCIPfreeBufferArray(scip, &varz);
   
   SCIPfreeBufferArray(scip, &maxN0);
   for( int i = 0; i < Queueone.size; i++ )
   {
      SCIPfreeBufferArray(scip, &MJ[i]);
   }
   SCIPfreeBufferArray(scip, &MJ);
   SCIPfreeBufferArray(scip, &NumMJ);

   SCIP_CALL( SCIPsolve(scip) );
   solutionstatus = SCIPgetStatus(scip);
   if( solutionstatus == SCIP_STATUS_OPTIMAL )
   {
      (*optValSubProb) = SCIPgetPrimalbound(scip);
   }
   else if( solutionstatus == SCIP_STATUS_INFEASIBLE )
   {
      SCIPinfoMessage(scip, NULL, "This problem is INFEASIBLE !!!");
   }
   else
   {
      SCIPinfoMessage(scip, NULL, "Something went wrong during the optimization !!!");
      assert(0);
   }

   /* freeing scip */
   SCIP_CALL( SCIPfree(&scip) );
   BMScheckEmptyMemory();

   return 1;
}


/** the overlap-oriented node pruning and variable fixing (including approximation and exact approaches) */
 static
SCIP_RETCODE overlapNodePruneVarFix(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,                            /**< constraint data */
   SCIP_CONSHDLRDATA*    conshdlrdata,                        /**< constraint handler data */
   int*                  basicVarzInd,                        /**< index of variable z corresponding to basic scenarios */
   int*                  nfixedvars,                          /**< number of fixed variables */
   SCIP_Bool*            cutoff                               /**< pointer to store if a cutoff was detected */
   )
{

   int    round;
   int    fixNVarz;
   int    basicNScenario;
   Queue  Queueone;
   Queue  Queuezero;
   Queue  Queuefree;
   Queue  QueueoneS;
   Queue  newQueueoneS;
   Queue  QueuezeroExact;
   Queue  QueueoneExact;
   Queue  QueuefreeExact;
   SCIP_Bool flag;
   SCIP_Bool mark;
   SCIP_Bool isFixVar;
   SCIP_Bool fixed;
   SCIP_Bool infeasible;
   SCIP_Real residuals;
   SCIP_Real optValSubProb;

   assert(scip != NULL);
   assert(consdata != NULL);
   assert(conshdlrdata != NULL);
   assert(nfixedvars != NULL);

   round = 1;
   fixNVarz = 0;
   flag = TRUE;
   basicNScenario = consdata->basicNScenario;

   /* allocate buffer memory */
	SCIP_CALL( SCIPallocBufferArray(scip, &(Queueone.base),  basicNScenario) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(Queuezero.base), basicNScenario) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(Queuefree.base), basicNScenario) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(QueueoneS.base), basicNScenario) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(newQueueoneS.base), basicNScenario) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(QueuezeroExact.base), basicNScenario) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(QueueoneExact.base), basicNScenario) );
	SCIP_CALL( SCIPallocBufferArray(scip, &(QueuefreeExact.base), basicNScenario) );
   InitQueue(scip, &QueueoneS);
   InitQueue(scip, &newQueueoneS);

   /* step 1: the approximation approach for implementing overlap-oriented node pruning and variable fixing */
   while( flag == TRUE )
   {
      mark = FALSE;
      isFixVar = FALSE;
      /* update partition (N_0, N_1, N_f) */
      SCIP_CALL( divideVar(scip, consdata, &Queueone, &Queuezero, &Queuefree, basicVarzInd, basicNScenario) );
      /* check whether the knapsack constraint is feasible */
      SCIP_CALL( checkKnapCons(scip, consdata, &Queueone, cutoff) );
      if( *cutoff )
         break;
      /* try to update the lower bound of the node: (i) modify the local bound of the continue variable; (ii) determine if a better lower bound is obtained) */
      SCIP_CALL( updateLowerBoundVarV(scip, consdata, conshdlrdata, basicVarzInd, &Queueone, consdata->nodeLb) );
      /* calculate the components of a random vector to be compared -- M_s(N_0, N_1) */
      SCIP_CALL( computeSetM(scip, consdata, basicVarzInd, consdata->nodeLb) );
      /* reduce the initial set N_1 to S_1 */
      if( round == 1 )
      {
         SCIP_CALL( reduceSetN1(scip, consdata, basicVarzInd, &Queueone, &QueueoneS) );
      }
      else
      {
         /* acceleration techniques */
         SCIP_CALL( reduceSetN1(scip, consdata, basicVarzInd, &QueueoneS, &QueueoneS) );
         /* continue to reduce set S_1 for each round based on the updated M_i */
         for( int i = 0; i < QueueoneS.size; i++ )
         {
            int s = QueueoneS.base[i];
            if( consdata->MiBeg[s] != consdata->roundMiBeg[s] )
            {
               EnQueue(scip, &newQueueoneS, basicNScenario, s);
            }
         }
      }
      /* check whether there exists some i \in S_1 such that \xi^i \leq \xi(N_0,N_1) */
      SCIP_CALL( checkSuffCondition(scip, consdata, basicVarzInd, &Queueone, cutoff) );
      if( *cutoff )
         break;
      fixNVarz = Queueone.size+Queuezero.size;
      if( fixNVarz == consdata->basicNScenario )
         break;
      /* fix variables z that satisfies certain conditions to one */
      if( round == 1 )
      {
         SCIP_CALL( varFixtoOne(scip, consdata, basicVarzInd, &QueueoneS, &Queuefree, &isFixVar, nfixedvars, cutoff) );
      }
      else
      {
         SCIP_CALL( varFixtoOne(scip, consdata, basicVarzInd, &newQueueoneS, &Queuefree, &isFixVar, nfixedvars, cutoff) );
      }
      if( *cutoff )
         break;
      if( isFixVar == TRUE )
      {
         mark = TRUE;
         isFixVar = FALSE;
         /* update partition (N_0, N_1, N_f) */
         SCIP_CALL( divideVar(scip, consdata, &Queueone, &Queuezero, &Queuefree, basicVarzInd, basicNScenario) );
         /* check whether the knapsack constraint is feasible */
         SCIP_CALL( checkKnapCons(scip, consdata, &Queueone, cutoff) );
         if( *cutoff )
            break;
      }
      /* check whether or not the knapsack can handle the scenario */
      SCIP_CALL( varFixtoZeroByKnapCons(scip, consdata, basicVarzInd, &Queueone, &Queuefree, &isFixVar, nfixedvars, cutoff) );
      if( isFixVar == TRUE )
      {
         /* update partition (N_0, N_1, N_f) */
         SCIP_CALL( divideVar(scip, consdata, &Queueone, &Queuezero, &Queuefree, basicVarzInd, basicNScenario) );
      }
      /* check whether there exists some i \in N_f such that \xi^i \leq \xi(N_0,N_1) */
      SCIP_CALL( varFixtoZero(scip, consdata, &Queuefree, basicVarzInd, &isFixVar, nfixedvars, cutoff) );
      if( *cutoff || (isFixVar == FALSE && mark == FALSE) )
      {
         break;
      }
      else
      {
         int s;
         for( int i = 0; i < QueueoneS.size; i++ )
         {
            s = QueueoneS.base[i];
            consdata->roundMiBeg[s] = consdata->MiBeg[s];
         }
      }
      InitQueue(scip, &newQueueoneS);
      round++;
   }
   if( consdata->maxRoundPRA < round )
   {
      consdata->maxRoundPRA = round;
   }

   /* step2: the exact approach for implementing overlap-oriented node pruning and variable fixing */
   if( conshdlrdata->BASE_DB_EOPF == TRUE && (*cutoff) == FALSE && Queuefree.size > 0 && Queueone.size > 0 && !SCIPinProbing(scip) && !(SCIPgetSubscipDepth(scip) > 0) )
   {
      clock_t exactTimeStart = clock();

      residuals = consdata->epsilon;      
      for( int i = 0; i < Queueone.size; i++ )
         residuals -= consdata->proba[basicVarzInd[Queueone.base[i]]];
      
      SloveSubMIPExactFixing(consdata->dimension, basicNScenario, consdata->randomRhs, consdata->proba, consdata->basicVarzInd,
                              Queueone, Queuezero, Queuefree, consdata->nodeLb, &optValSubProb);
      assert( !SCIPisEQ(scip, optValSubProb, -1) );
   
      if( SCIPisGT(scip, optValSubProb, residuals) )
      {
         *cutoff = TRUE;
         consdata->cutoffTimesExact++;
      }
      else
      {
         SCIP_Real residualsExact;
         for( int i = 0; i < Queuefree.size; i++ )
         {
            int s = Queuefree.base[i];
            SCIP_VAR* var = consdata->varz[basicVarzInd[s]];
            if( SCIPvarGetLbLocal(var) > 0.5 || SCIPvarGetUbLocal(var) < 0.5 )
               continue;
      
            InitQueue(scip, &QueuefreeExact);
            for( int j = 0; j < Queuefree.size; j++ )
            {
               if( Queuefree.base[j] != s )
                  EnQueue(scip, &QueuefreeExact, basicNScenario, Queuefree.base[j]);
            }
            
            InitQueue(scip, &QueuezeroExact);
            for( int j = 0; j < Queuezero.size; j++ )
               EnQueue(scip, &QueuezeroExact, basicNScenario, Queuezero.base[j]);
            EnQueue(scip, &QueuezeroExact, basicNScenario, s);
            
            SloveSubMIPExactFixing(consdata->dimension, basicNScenario, consdata->randomRhs, consdata->proba, consdata->basicVarzInd,
                                 Queueone, QueuezeroExact, QueuefreeExact, consdata->nodeLb, &optValSubProb);
            assert( !SCIPisEQ(scip, optValSubProb, -1) );

            if( SCIPisGT(scip, optValSubProb, residuals) )
            {
               // fixing to one
               SCIP_CALL( SCIPfixVar(scip, var, 1.0, &infeasible, &fixed) );
               if( infeasible )
               {
                  assert( SCIPvarGetUbLocal(var) < 0.5 );
                  SCIPdebugMsg(scip, "-> cutoff\n");
                  (*cutoff) = TRUE;
                  break;
               }
               else
               {
                  if( !SCIPinProbing(scip) )
                  {
                     consdata->fixNOneVarsExact++;
                  }
                  assert(fixed);
                  (*nfixedvars)++;
               }
            }
            else
            {
               InitQueue(scip, &QueueoneExact);
               for( int j = 0; j < Queueone.size; j++ )
                  EnQueue(scip, &QueueoneExact, basicNScenario, Queueone.base[j]);
               EnQueue(scip, &QueueoneExact, basicNScenario, s);
                              
               residualsExact = residuals - consdata->proba[basicVarzInd[s]];

               SloveSubMIPExactFixing(consdata->dimension, basicNScenario, consdata->randomRhs, consdata->proba, consdata->basicVarzInd,
                                    QueueoneExact, Queuezero, QueuefreeExact, consdata->nodeLb, &optValSubProb);
               assert( !SCIPisEQ(scip, optValSubProb, -1) );

               if( SCIPisGT(scip, optValSubProb, residualsExact) )
               {
                  // fixing to zero
                  SCIP_CALL( SCIPfixVar(scip, var, 0.0, &infeasible, &fixed) );
                  if( infeasible )
                  {
                     assert( SCIPvarGetLbLocal(var) > 0.5 );
                     SCIPdebugMsg(scip, "-> cutoff\n");
                     (*cutoff) = TRUE;
                     break;
                  }
                  else
                  {
                     if( !SCIPinProbing(scip) )
                     {
                        consdata->fixNZeroVarsExact++;
                     }
                     assert(fixed);
                     (*nfixedvars)++;
                  }
               }
            }
         }
      }
      consdata->exactTime += (clock()-exactTimeStart)/(SCIP_Real)CLOCKS_PER_SEC;
   }
   
   /* free temporary memory */
   SCIPfreeBufferArray(scip, &(Queueone.base));
   SCIPfreeBufferArray(scip, &(Queuezero.base));
   SCIPfreeBufferArray(scip, &(Queuefree.base));
   SCIPfreeBufferArray(scip, &(QueueoneS.base));
   SCIPfreeBufferArray(scip, &(newQueueoneS.base));
   SCIPfreeBufferArray(scip, &(QueuezeroExact.base));
   SCIPfreeBufferArray(scip, &(QueueoneExact.base));
   SCIPfreeBufferArray(scip, &(QueuefreeExact.base));

   return SCIP_OKAY;
}


/** Dominance-based branching with overlap-oriented node pruning and variable fixing */
static
SCIP_RETCODE dominanceBranchWithOPF(
   SCIP*                 scip,                                /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,                            /**< constraint data */
   SCIP_CONSHDLRDATA*    conshdlrdata,                        /**< constraint handler data */
   int*                  basicVarzInd,                        /**< the index of variable z corresponding to basic scenarios */
   SCIP_RESULT*          result                               /**< pointer to store the result of the fixing */
   )
{
   int   nfixedvars;
   SCIP_Bool cutoff;

   assert(*result == SCIP_DIDNOTRUN);

   nfixedvars = 0;
   cutoff = FALSE;
   if( consdata->BASE_DB == TRUE )
   {
      SCIP_CALL( dominanceBranch(scip, consdata, basicVarzInd, &nfixedvars, &cutoff) );
   }
   else if( consdata->BASE_DB_OPF == TRUE )
   {
      SCIP_CALL( overlapNodePruneVarFix(scip, consdata, conshdlrdata, basicVarzInd, &nfixedvars, &cutoff) );
   }
   SCIPdebugMsg(scip, "fixed %d variables locally\n", nfixedvars);

   if( cutoff )
      *result = SCIP_CUTOFF;
   else if( nfixedvars > 0 )
      *result = SCIP_REDUCEDDOM;
   else
      *result = SCIP_DIDNOTFIND;

   return SCIP_OKAY;
}

/** domain propagation method of constraint handler */
static
SCIP_DECL_CONSPROP(consPropCCP)
{
   assert( scip != NULL );
   assert( conshdlr != NULL );
   assert( strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0 );
   assert( conss != NULL );
   assert( result != NULL );
   assert(SCIPisTransformed(scip));

   *result = SCIP_DIDNOTRUN;

   /* loop through all constraints */
   for( int c = 0; c < nconss; ++c )
   {
      SCIP_CONS*         cons;
      SCIP_CONSDATA*     consdata;
      SCIP_CONSHDLRDATA* conshdlrdata;

      cons = conss[c];
      assert(cons != NULL);
      assert(nconss == 1);
      SCIPdebugMsg(scip, "propagating chance constraint <%s>.\n", SCIPconsGetName(cons));

      consdata     = SCIPconsGetData(cons);
      conshdlrdata = SCIPconshdlrGetData(conshdlr);
      assert(consdata != NULL);
      assert(conshdlrdata != NULL);

      if( consdata->BASE_DB || consdata->BASE_DB_OPF )
      {
         /* do not run again in repropagation, since the path to the root might have changed */
         if( SCIPinRepropagation(scip) )
            return SCIP_OKAY;

         /* do nothing if we are in a strongbranchprobing node */
         if( conshdlrdata->isCloseProbing && scip->lp->strongbranchprobing )
            return SCIP_OKAY;

         SCIP_CALL( dominanceBranchWithOPF(scip, consdata, conshdlrdata, consdata->basicVarzInd, result) );
      }

      consdata->npropagations++;
   }

   return SCIP_OKAY;
}


/** separate mixing inequalities */
static
SCIP_RETCODE separateCCP(
   SCIP*                      scip,                           /**< SCIP pointer */
   SCIP_CONSHDLR*             conshdlr,                       /**< constraint handler */
   SCIP_VAR**                 varz,                           /**< scenario variable z */
   SCIP_VAR**                 varv,                           /**< introducing variable v */
   int                        nScenario,                      /**< number of scenarios */
   int                        dimension,                      /**< dimension of random vector */
   int*                       KI,                             /**< index vector of lower bound for variables v */
   SCIP_Real**                randomRhs,                      /**< sorted transpose of random right-hand side matrix */
   int**                      sortTransRhsInd,                /**< index of sorted transpose of random right-hand side matrix */
   SCIP_SOL*                  sol,                            /**< solution to be separated */
   int*                       nGen,                           /**< output: pointer to store number of added rows */
   SCIP_Bool*                 cutoff                          /**< output: pointer to store whether we detected a cutoff */
   )
{
   SCIP_VAR*  var;
   SCIP_Real* vlbmixcoefs;
   SCIP_Real* vlbmixsols;
   SCIP_Real* cutcoefs;
   SCIP_Real* vlbcoefs;
   SCIP_Real* vlbconsts;
   SCIP_Real  lb;
   SCIP_Real  cutrhs;
   SCIP_Real  maxabscoef;
   SCIP_Real  activity;
   SCIP_Real  lastcoef;
   SCIP_Real  maxactivity;
   SCIP_Real  coef;
   SCIP_Real  number;
   char       name[SCIP_MAXSTRLEN];
   int*       vlbmixinds;
   int*       vlbmixsigns;
   int*       cutinds;
   int        vlbmixsize;
   int        cutnnz;
   int        maxabsind;
   int        maxabssign;
   int        i;
   int        j;
   int        s;

   *cutoff = FALSE;
   assert( scip != NULL );
   assert( nGen != NULL );
   assert( cutoff != NULL );

   /* allocate temporary memory */
   SCIP_CALL( SCIPallocBufferArray(scip, &vlbmixcoefs, nScenario+dimension) );
   SCIP_CALL( SCIPallocBufferArray(scip, &vlbmixsols,  nScenario+dimension) );
   SCIP_CALL( SCIPallocBufferArray(scip, &vlbmixinds,  nScenario+dimension) );
   SCIP_CALL( SCIPallocBufferArray(scip, &vlbmixsigns, nScenario+dimension) );
   SCIP_CALL( SCIPallocBufferArray(scip, &cutcoefs,    nScenario+dimension) );
   SCIP_CALL( SCIPallocBufferArray(scip, &cutinds,     nScenario+dimension) );
   SCIP_CALL( SCIPallocBufferArray(scip, &vlbcoefs,    nScenario+dimension) );
   SCIP_CALL( SCIPallocBufferArray(scip, &vlbconsts,   nScenario+dimension) );

   for( i = 0; i < dimension && !(*cutoff); i++ )
   {

      cutnnz = 0;
      vlbmixsize = 0;
      var = varv[i];
      assert(SCIPvarGetType(var) != SCIP_VARTYPE_BINARY);

      /* get variable lower bounds */
      lb = SCIPvarGetLbGlobal(var);

      for( s = 0; s < KI[i]; s++ )
      {
         vlbcoefs[s]  = randomRhs[sortTransRhsInd[i][KI[i]]][i]-randomRhs[sortTransRhsInd[i][s]][i];
         vlbconsts[s] = randomRhs[sortTransRhsInd[i][s]][i];
      }

      maxabscoef = 0.0;
      maxabsind  = -1;
      maxabssign = 0;

      /* stop if the upper bound is equal to the LP solution value of variable(v)*/
      if( SCIPisFeasEQ(scip, SCIPvarGetUbLocal(var), SCIPgetSolVal(scip, sol, var)) )
         continue;

      assert( SCIPisFeasLE(scip, lb, SCIPvarGetUbLocal(var)) );

      for( s = 0; s < KI[i]; s++ )
      {
         /* variable z index */
         j = sortTransRhsInd[i][s];

         assert(vlbcoefs[s] <= 0);
         assert( SCIPvarIsBinary(varz[j]) );

         maxactivity = vlbconsts[s];

         if( SCIPisFeasLE(scip, maxactivity, lb) )
         {
            /* this variable bound constraint is redundant */
            continue;
         }
         coef = lb - maxactivity;
         vlbmixsigns[vlbmixsize] = 1;

         vlbmixcoefs[vlbmixsize] = REALABS(coef);
         vlbmixinds[vlbmixsize]  = j;
         vlbmixsols[vlbmixsize]  = 1-SCIPgetSolVal(scip, sol, varz[j]);

         /* update the maximal coefficient if needed */
         if(maxabscoef < vlbmixcoefs[vlbmixsize])
         {
            maxabscoef = vlbmixcoefs[vlbmixsize];
            maxabsind  = vlbmixinds[vlbmixsize];
            maxabssign = vlbmixsigns[vlbmixsize];
         }
         vlbmixsize += 1;
      }

      /* stop if no variable lower bounds information exists */
      if( vlbmixsize == 0 )
      {
         continue;
      }
      /* stop if the current solution value of the transformed continuous variable is larger than the maximal coefficient */
      number = SCIPgetSolVal(scip, sol, var) - lb;
      if( SCIPisFeasGT(scip, number, maxabscoef) )
      {
         continue;
      }
      /* sort the lp solutions in non-increasing order */
      SCIPsortDownRealRealIntInt(vlbmixsols, vlbmixcoefs, vlbmixinds,  vlbmixsigns, vlbmixsize);

      /* add the continuous variable */
      cutcoefs[cutnnz] = -1;
      cutinds[cutnnz]  = i;
      cutrhs = -lb;
      cutnnz++;

      activity = -(SCIPgetSolVal(scip, sol, var) - lb);
      lastcoef = 0;

      /* loop over the variables and add the variable to the cut if its coefficient is larger than that of the last variable */
      for( int v = 0; v < vlbmixsize; v++ )
      {
         SCIP_Real solval;
         solval = vlbmixsols[v];

         /* stop if we can not find a violated cut */
         if( activity + solval*(maxabscoef-lastcoef) < 0.0 || SCIPisFeasZero(scip, solval) )
            break;
         else
         {
            /* skip if we have already added a variable with bigger coefficient */
            if( SCIPisLE(scip, vlbmixcoefs[v], lastcoef) )
               continue;
            else
            {
               activity += (vlbmixcoefs[v]-lastcoef) * solval;
               cutrhs   -= vlbmixcoefs[v] -lastcoef;
               cutcoefs[cutnnz] = lastcoef - vlbmixcoefs[v];
               cutinds[cutnnz]  = vlbmixinds[v];
               cutnnz++;
               lastcoef = vlbmixcoefs[v];
            }
         }
      }

      /* add the variable with maximal coefficient to make sure the cut is strong enough */
      if( SCIPisGT(scip, maxabscoef, lastcoef) )
      {
         cutrhs -= maxabssign*(maxabscoef - lastcoef);
         cutcoefs[cutnnz] = !maxabssign ? maxabscoef - lastcoef : lastcoef - maxabscoef;
         cutinds[cutnnz]  = maxabsind;
         cutnnz++;
      }

      /* add the cut if the violtion is good enough */
      if( cutnnz > 2 && SCIPisEfficacious(scip, activity) )
      {
         SCIP_ROW *row;
         (void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "mixing%d_%d", SCIPgetNLPs(scip), *nGen);
         SCIP_CALL( SCIPcreateEmptyRowConshdlr(scip, &row, conshdlr, name, -SCIPinfinity(scip), cutrhs, FALSE, FALSE, TRUE) );
         SCIP_CALL( SCIPcacheRowExtensions(scip, row) );

         SCIP_CALL( SCIPaddVarToRow(scip, row, varv[cutinds[0]], cutcoefs[0]) );
         for( int v = 1; v < cutnnz; v++ )
         {
            SCIP_CALL( SCIPaddVarToRow(scip, row, varz[cutinds[v]], cutcoefs[v]) );
         }
         SCIP_CALL( SCIPflushRowExtensions(scip, row) );

         /* set cut rank */
         SCIProwChgRank(row, 1);
         // SCIPdebug( SCIPprintRow(scip, row, NULL) );
         SCIP_CALL( SCIPaddRow(scip, row, FALSE, cutoff) );
         SCIP_CALL( SCIPreleaseRow(scip, &row) );
         ++(*nGen);

         if( *cutoff )
            break;
      }
   }

   /* free temporary memory */
   SCIPfreeBufferArray(scip, &vlbcoefs);
   SCIPfreeBufferArray(scip, &vlbconsts);
   SCIPfreeBufferArray(scip, &vlbmixcoefs);
   SCIPfreeBufferArray(scip, &vlbmixsigns);
   SCIPfreeBufferArray(scip, &vlbmixsols);
   SCIPfreeBufferArray(scip, &vlbmixinds);
   SCIPfreeBufferArray(scip, &cutcoefs);
   SCIPfreeBufferArray(scip, &cutinds);

   return SCIP_OKAY;
}


/** separation method of constraint handler for LP solutions */
static
SCIP_DECL_CONSSEPALP(consSepalpCCP)
{
   int nGen = 0;
   int c;

   assert( scip != NULL );
   assert( conshdlr != NULL );
   assert( strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0 );
   assert( conss != NULL );
   assert( result != NULL );

   *result = SCIP_DIDNOTRUN;

   /* loop through all constraints */
   for( c = 0; c < nconss; ++c )
   {
      SCIP_CONSDATA* consdata;
      SCIP_CONS* cons;
      SCIP_Bool cutoff;

      cons = conss[c];
      assert( cons != NULL );
      SCIPdebugMsg(scip, "separating LP solution for chance constraint <%s>.\n", SCIPconsGetName(cons));

      consdata = SCIPconsGetData(cons);
      assert( consdata != NULL );

      /* call the cut separation */
      SCIP_CALL( separateCCP(scip, conshdlr, consdata->varz, consdata->varv, consdata->nScenario, consdata->dimension, consdata->KI,
            consdata->randomRhs, consdata->sortTransRhsInd, NULL, &nGen, &cutoff) );

      /* adjust result code */
      if( cutoff )
      {
         *result = SCIP_CUTOFF;
         return SCIP_OKAY;
      }
   }
   if( nGen > 0 )
      *result = SCIP_SEPARATED;
   else
      *result = SCIP_DIDNOTFIND;

   SCIPdebugMsg(scip, "separated %d cuts.\n", nGen);

   return SCIP_OKAY;
}


/** separation method of constraint handler for arbitrary primal solutions */
static
SCIP_DECL_CONSSEPASOL(consSepasolCCP)
{
   int nGen = 0;
   int c;

   assert( scip != NULL );
   assert( conshdlr != NULL );
   assert( strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0 );
   assert( conss != NULL );
   assert( result != NULL );

   *result = SCIP_DIDNOTRUN;

   /* loop through all constraints */
   for( c = 0; c < nconss; ++c )
   {
      SCIP_CONSDATA* consdata;
      SCIP_CONS* cons;
      SCIP_Bool cutoff;

      cons = conss[c];
      assert( cons != NULL );
      SCIPdebugMsg(scip, "separating solution for chance constraint <%s>.\n", SCIPconsGetName(cons));

      consdata = SCIPconsGetData(cons);
      assert( consdata != NULL );

      SCIP_CALL( separateCCP(scip, conshdlr, consdata->varz, consdata->varv, consdata->nScenario, consdata->dimension, consdata->KI,
            consdata->randomRhs, consdata->sortTransRhsInd, NULL, &nGen, &cutoff) );

      if( cutoff )
      {
         *result = SCIP_CUTOFF;
         return SCIP_OKAY;
      }
   }
   if( nGen > 0 )
      *result = SCIP_SEPARATED;
   else
      *result = SCIP_DIDNOTFIND;

   return SCIP_OKAY;
}


/** presolving method of constraint handler */
static
SCIP_DECL_CONSPRESOL(consPresolCCP)
{
   int   ncliquebdchgs;
   SCIP_Bool infeasible;
   SCIP_VAR* twovars[2];
   SCIP_CONS*         cons;
   SCIP_CONSDATA*     consdata;

   assert(result != NULL);
   assert(conss != NULL);
   assert(nconss > 0);

   *result = SCIP_DIDNOTRUN;
   for( int c = 0; c < nconss; ++c )
   {
      cons = conss[c];
      consdata = SCIPconsGetData(cons);

      assert(cons != NULL);
      assert(nconss == 1);
      assert(consdata != NULL);

      /* propagator can only be applied during presolving stage */
      if( SCIPgetStage(scip) > SCIP_STAGE_PRESOLVING )
         return SCIP_OKAY;
      /* do not run if we are in the child nodes */
      if( SCIPgetDepth(scip) > 0 )
         return SCIP_OKAY;
      /* do not run propagator in a sub-SCIP */
      if( SCIPgetSubscipDepth(scip) > 0 )
         return SCIP_OKAY;
      
      for( int head = 0; head < consdata->basicNScenario; head++ )
      {
         twovars[0] = consdata->varz[consdata->basicVarzInd[head]];
         
         for( int j = 0; j < consdata->vInSize[head]; j++ )
         {
            int tail = consdata->vIn[head][j];
            if( consdata->edgeTH[tail][head] == TRUE )
            {
               twovars[1] = consdata->varz[consdata->basicVarzInd[tail]];

               SCIP_CALL( SCIPgetNegatedVar(scip, twovars[1], &twovars[1]) );
               SCIP_CALL( SCIPaddClique(scip, twovars, NULL, 2, FALSE, &infeasible, &ncliquebdchgs) );
               
               if( infeasible )
               {
                  SCIPinfoMessage(scip, NULL, "new clique of the dominance inequalities led to infeasibility\n");
               }
            }
         }
      }
   }

   *result = SCIP_SUCCESS;

   return SCIP_OKAY;
}

/** constraint display method of constraint handler */
static
SCIP_DECL_CONSPRINT(consPrintCCP)
{
   SCIPinfoMessage(scip, file, "Generate chance constraint of constraint handler" );

   return SCIP_OKAY;
}


/** output the Related information in solution process */
static
SCIP_DECL_CONSEXIT(consExitCCP)
{
   int c;

   assert( scip != NULL );
   assert( conshdlr != NULL );
   assert( strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0 );

   SCIPdebugMsg(scip, "exiting chance constraint handler <%s>.\n", SCIPconshdlrGetName(conshdlr));
   /* avoid output for subscips */
   if( SCIPgetSubscipDepth(scip) > 0 )
   {
      return SCIP_OKAY;
   }

   /* loop through all constraints */
   for ( c = 0; c < nconss; ++c )
   {
      SCIP_CONSDATA* consdata;
      consdata = SCIPconsGetData(conss[c]);
      assert( consdata != NULL );
      assert( conss != NULL );
      SCIPinfoMessage(scip, NULL, "\n Branching Statistics...\n");
      SCIPinfoMessage(scip, NULL, "\n Total number of propagations -> %lld\n", consdata->npropagations);
      SCIPinfoMessage(scip, NULL, "\n Number of cutoffs of KnaCons -> %lld\n", consdata->cutoffNKC);
      SCIPinfoMessage(scip, NULL, "\n Total number of variables fixed to 1 by the propagator -> %lld\n", consdata->fixNOneVars);
      SCIPinfoMessage(scip, NULL, "\n Total number of variables fixed to 0 by the propagator -> %lld\n",  consdata->fixNZeroVars);
      SCIPinfoMessage(scip, NULL, "\n Maximum number of rounds of the approximation algorithm -> %lld\n", consdata->maxRoundPRA);
      SCIPinfoMessage(scip, NULL, "\n Runtime of the exact algorithm -> %.4f s\n", consdata->exactTime);
      SCIPinfoMessage(scip, NULL, "\n Total number of variables fixed to 1 in the exact algorithm -> %lld\n",   consdata->fixNOneVarsExact);
      SCIPinfoMessage(scip, NULL, "\n Total number of variables fixed to 0 in the exact algorithm -> %lld\n",  consdata->fixNZeroVarsExact);
      SCIPinfoMessage(scip, NULL, "\n Number of cutoffs in the exact algorithm -> %lld\n", consdata->cutoffTimesExact);
      SCIPinfoMessage(scip, NULL, "\n Number of cutoffs in the approximation algorithm -> %lld\n", consdata->cutoffTimesAOPF);

      SCIPinfoMessage(scip, NULL, "\n PS:  \n");
   }
   return SCIP_OKAY;
}


/** creates the handler for chance constraints and includes it in SCIP */
SCIP_RETCODE SCIPincludeConshdlrCCP(
      SCIP*                 scip                              /**< SCIP data structure */
      )
{
   SCIP_CONSHDLR* conshdlr = NULL;
   SCIP_CONSHDLRDATA* conshdlrdata;

   /* create constraint handler data */
   SCIP_CALL( conshdlrdataCreate(scip, &conshdlrdata) );

   /* include constraint handler */
   SCIP_CALL( SCIPincludeConshdlrBasic(scip, &conshdlr, CONSHDLR_NAME, CONSHDLR_DESC,
            CONSHDLR_ENFOPRIORITY, CONSHDLR_CHECKPRIORITY, CONSHDLR_EAGERFREQ, CONSHDLR_NEEDSCONS,
            consEnfolpCCP, consEnfopsCCP, consCheckCCP, consLockCCP, conshdlrdata) );

   assert( conshdlr != NULL );

   SCIP_CALL( SCIPsetConshdlrDelete (scip, conshdlr, consDeleteCCP) );
   SCIP_CALL( SCIPsetConshdlrExit   (scip, conshdlr, consExitCCP) );
   SCIP_CALL( SCIPsetConshdlrCopy   (scip, conshdlr, conshdlrCopyCCP, consCopyCCP) );
   SCIP_CALL( SCIPsetConshdlrTrans  (scip, conshdlr, consTransCCP) );
   SCIP_CALL( SCIPsetConshdlrSepa   (scip, conshdlr, consSepalpCCP, consSepasolCCP, CONSHDLR_SEPAFREQ, CONSHDLR_SEPAPRIORITY, CONSHDLR_DELAYSEPA) );
   SCIP_CALL( SCIPsetConshdlrProp   (scip, conshdlr, consPropCCP, CONSHDLR_PROPFREQ, CONSHDLR_DELAYPROP, CONSHDLR_PROP_TIMING) );
   SCIP_CALL( SCIPsetConshdlrPresol (scip, conshdlr, consPresolCCP, CONSHDLR_MAXPREROUNDS, CONSHDLR_PRESOLTIMING) ); 
   SCIP_CALL( SCIPsetConshdlrPrint  (scip, conshdlr, consPrintCCP) );
   SCIP_CALL( SCIPsetConshdlrFree   (scip, conshdlr, consFreeCCP) );

   SCIP_CALL( SCIPaddBoolParam(scip,
            "constraints/" CONSHDLR_NAME "/isCloseProbing", "should probing be turned off in domain propagation?",
            &conshdlrdata->isCloseProbing, FALSE, DEFAULT_ISCLOSEPROBING, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip,
            "constraints/" CONSHDLR_NAME "/tightContVarBound", "should the bounds of the continue variable v be updated in the domain propagation?",
            &conshdlrdata->tightContVarBound, FALSE, DEFAULT_TIGHTCONTVARBOUND, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip,
            "constraints/" CONSHDLR_NAME "/BASE_DB_EOPF", "should the exact approach for implementing overlap-oriented node pruning and variable fixing be used in domain propagation?",
            &conshdlrdata->BASE_DB_EOPF, FALSE, DEFAULT_BASE_DB_EOPF, NULL, NULL) );

   return SCIP_OKAY;
}


/** creates and captures a chance constraint */
SCIP_RETCODE SCIPcreateConsCCP(
   SCIP*                      scip,                           /**< SCIP data structure */
   SCIP_CONS**                cons,                           /**< pointer to hold the created constraint */
   const char*                name,                           /**< name of constraint */
   SCIP_Bool                  BASE_DB,                        /**< should the dominance-based branching be used to solve the problem? */
   SCIP_Bool                  BASE_DB_OPF,                    /**< should the dominance-based branching with overlap-oriented node pruning and variable fixing be used to solve the problem? */
   SCIP_VAR**                 varz,                           /**< scenario variable z */
   SCIP_VAR**                 varv,                           /**< introducing variable v */
   int                        nScenario,                      /**< number of scenarios */
   int                        dimension,                      /**< dimension of random vector */
   SCIP_Real*                 proba,                          /**< scenario probability vector */
   SCIP_Real                  epsilon,                        /**< confidence parameter */
   int*                       KI,                             /**< index vector of lower bound for variables v */
   int*                       basicVarzInd,                   /**< index of variable z corresponding to basic scenarios */
   int                        basicNScenario,                 /**< number of essential scenarios */
   SCIP_Real**                randomRhs,                      /**< random right-hand side matrix */
   SCIP_Real**                stRhs,                          /**< strengthened random right-hand side matrix */
   SCIP_Real**                sortTransRhs,                   /**< sorted transpose of random right-hand side matrix */
   int**                      sortTransRhsInd,                /**< index of sorted transpose of random right-hand side matrix */
   int**                      vIn,                            /**< matrix that stores the incoming  arc's tail of all vertices */
   int*                       vInSize,                        /**< vecotr that stores the number of the incoming  arc's tail for a vertice */
   int**                      vOut,                           /**< matrix that stores the outcoming arc's head of all vertices */
   int*                       vOutSize,                       /**< vecotr that stores the number of the outcoming arc's head for a vertice */
   SCIP_Bool**                edgeTH                          /**< the edges existing in the graph (tail-head) */
)
{
   SCIP_CONSHDLR* conshdlr;
   SCIP_CONSDATA* consdata;

   /* find the chance constraint handler */
   conshdlr = SCIPfindConshdlr(scip, CONSHDLR_NAME);
   if( conshdlr == NULL )
   {
      SCIPerrorMessage("chance constraint handler not found\n");
      return SCIP_PLUGINNOTFOUND;
   }

   /* create constraint data */
   SCIP_CALL( consdataCreate(scip, &consdata, conshdlr, BASE_DB, BASE_DB_OPF, varz, varv,
               nScenario, dimension, proba, epsilon, KI, basicVarzInd, basicNScenario, randomRhs, stRhs,
					sortTransRhs, sortTransRhsInd, vIn, vInSize, vOut, vOutSize, edgeTH) );

   /* create constraint */
   SCIP_CALL( SCIPcreateCons(scip, cons, name, conshdlr, consdata, FALSE, TRUE, FALSE, FALSE, TRUE, FALSE,
            FALSE, FALSE, FALSE, TRUE) );

   return SCIP_OKAY;
}


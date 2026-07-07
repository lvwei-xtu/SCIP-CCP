/**@file   chancecons_ccp.h
 * @brief  constraint handler for Chance-constrained Program with Random Right-hand Side
 * @author Wei Lv
 * @author Wei-Kun Chen
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __CCP_CHANCECONS_CCP_H__
#define __CCP_CHANCECONS_CCP_H__

#include <time.h>
#include "queue.h"

#include "scip/lp.h"
#include "scip/set.h"
#include "scip/var.h"
#include "scip/scip.h"
#include "scip/scipshell.h"
#include "scip/struct_scip.h"
#include "scip/scipdefplugins.h"

/** creates the handler for chance constraints and includes it in SCIP */
SCIP_RETCODE SCIPincludeConshdlrCCP(
   SCIP*                 scip                            /**< SCIP data structure */
);

/** creates and captures a chance constraint */
SCIP_RETCODE SCIPcreateConsCCP(
   SCIP*                      scip,                      /**< SCIP data structure */
   SCIP_CONS**                cons,                      /**< pointer to hold the created constraint */
   const char*                name,                      /**< name of constraint */
   SCIP_Bool                  BASE_DB,                   /**< should the dominance-based branching be used to solve the problem? */
   SCIP_Bool                  BASE_DB_OPF,               /**< should the dominance-based branching with overlap-oriented node pruning and variable fixing be used to solve the problem? */
   SCIP_VAR**                 varz,                      /**< scenario variable z */
   SCIP_VAR**                 varv,                      /**< introducing variable v */
   int                        nScenario,                 /**< number of scenarios */
   int                        dimension,                 /**< dimension of random vector */
   SCIP_Real*                 proba,                     /**< scenario probability vector */
   SCIP_Real                  epsilon,                   /**< confidence parameter */
   int*                       KI,                        /**< index vector of lower bound for variables v */
   int*                       basicVarzInd,              /**< index of variable z corresponding to basic scenarios */
   int                        basicNScenario,            /**< number of essential scenarios */
   SCIP_Real**                randomRhs,                 /**< random right-hand side matrix */
   SCIP_Real**                stRhs,                     /**< strengthened random right-hand side matrix */
   SCIP_Real**                sortTransRhs,              /**< sorted transpose of random right-hand side matrix */
   int**                      sortTransRhsInd,           /**< index of sorted transpose of random right-hand side matrix */
   int**                      vIn,                       /**< matrix that stores the incoming  arc's tail of all vertices */
   int*                       vInSize,                   /**< vecotr that stores the number of the incoming arc's tail for a vertice */
   int**                      vOut,                      /**< matrix that stores the outcoming arc's head of all vertices */
   int*                       vOutSize,                  /**< vecotr that stores the number of the outcoming arc's head for a vertice */
   SCIP_Bool**                edgeTH                     /**< the edges existing in the graph (tail-head) */   
);

#endif

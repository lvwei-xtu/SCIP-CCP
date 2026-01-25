/**@file   reader_ccls.h
 * @brief  Chance-constrained versions of the lot-sizing (CCLS) file reader
 * @author Wei Lv
 * @author Wei-Kun Chen
 *
 * This file implements the reader/parser used to read CCLS problems.
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_READER_CCLS_H__
#define __SCIP_READER_CCLS_H__


#include <time.h>
#include "scip/scip.h"
#include <scip/scipdefplugins.h>

#include "graph.h"
#include "processdata.h"
#include "chancecons_ccp.h"

/** includes the chance-constrained versions of the lot-sizing file reader into SCIP */
SCIP_RETCODE SCIPincludeReaderCCLS(
   SCIP*                 scip                /**< SCIP data structure */
);

#endif

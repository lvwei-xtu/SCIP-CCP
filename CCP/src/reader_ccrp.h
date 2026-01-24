/**@file   reader_ccrp.h
 * @brief  Chance-constrained version of the resource planning (CCRP) file reader
 * @author Wei Lv
 * @author Wei-Kun Chen
 *
 * This file implements the reader/parser used to read CCRP problems.
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_READER_CCRP_H__
#define __SCIP_READER_CCRP_H__


#include <time.h>
#include "scip/scip.h"
#include <scip/scipdefplugins.h>

#include "graph.h"
#include "processdata.h"
#include "chancecons_ccp.h"

/** includes the resource planning problem file reader into SCIP */
SCIP_RETCODE SCIPincludeReaderCCRP(
   SCIP*                 scip                /**< SCIP data structure */
);

#endif

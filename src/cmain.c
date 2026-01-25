/**@file   CCP/src/cmain.c
 * @brief  Main file for Chance-constrained Program with Random Right-hand Side example (CCP)
 * @author Wei Lv
 * @author Wei-Kun Chen
*/

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <stdio.h>

/* include SCIP components */
#include "scip/scip.h"
#include "scip/scipshell.h"
#include "scip/scipdefplugins.h"

#include "reader_ccrp.h"
#include "reader_ccmpp.h"
#include "reader_ccls.h"
#include "chancecons_ccp.h"

/** creates a SCIP instance with default plugins, evaluates command line parameters, runs SCIP appropriately,
 *  and frees the SCIP instance
 */
static SCIP_RETCODE runShell(
   int                         argc,                   /**< number of shell parameters */
   char**                      argv,                   /**< array with shell parameters */
   const char*                 defaultsetname          /**< name of default settings file */
)
{
	SCIP* scip = NULL;

	/*********
	 * Setup *
	 *********/

	/* initialize SCIP */
	SCIP_CALL( SCIPcreate(&scip) );

	SCIPinfoMessage(scip, NULL, "***************************************\n");
	SCIPinfoMessage(scip, NULL, "*         SCIP-based CCP Solver       *\n");
	SCIPinfoMessage(scip, NULL, "*                                     *\n");
	SCIPinfoMessage(scip, NULL, "*    Wei Lv and Wei-Kun Chen (2024)   *\n");
	SCIPinfoMessage(scip, NULL, "***************************************\n");

	/* output execution command */
	for (int i = 0; i < argc; i++)
	{
		SCIPinfoMessage(scip, NULL, "%2s ", argv[i]);
	}
	SCIPinfoMessage(scip, NULL, "\n");

	/* we explicitly enable the use of a debug solution for this main SCIP instance */
	SCIPenableDebugSol(scip);

	/* include reader */
	SCIP_CALL( SCIPincludeReaderCCRP(scip) );
	SCIP_CALL( SCIPincludeReaderCCMPP(scip) );
	SCIP_CALL( SCIPincludeReaderCCLS(scip) );

	/* include chance constraint handler */
	SCIP_CALL( SCIPincludeConshdlrCCP(scip) );

	/* include default SCIP plugins */
	SCIP_CALL( SCIPincludeDefaultPlugins(scip) );

	/**********************************
	 * Process command line arguments *
	 **********************************/
	SCIP_CALL( SCIPprocessShellArguments(scip, argc, argv, defaultsetname) );

	/********************
	 * Deinitialization *
	 ********************/
	SCIP_CALL( SCIPfree(&scip) );

	BMScheckEmptyMemory();

	return SCIP_OKAY;
}

/** main method starting SCIP */
int main(
	int argc,   						/**< number of arguments from the shell */
	char **argv 						/**< array of shell arguments */
)
{
	SCIP_RETCODE retcode;

	retcode = runShell(argc, argv, "scip.set");
	if( retcode != SCIP_OKAY )
	{
		SCIPprintError(retcode);
		return -1;
	}

	return 0;
}

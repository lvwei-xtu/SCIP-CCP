/**@file   queue.h
 * @brief  queue structrue for Chance-constrained Program with Random Right-hand Side
 * @author Wei Lv
 * @author Wei-Kun Chen
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __CCP_QUEUE__
#define __CCP_QUEUE__

#include "scip/scip.h"

typedef struct queue
{
   int* base;                          /**< store the base address of the space */
   int front;                          /**< pointer to head */
   int rear;                           /**< Pointer to rear */
   int size;                           /**< number of data elements stored */
}Queue;

typedef struct queuedouble
{
   SCIP_Real* base;                    /**< store the base address of the space */
   int front;                          /**< pointer to head */
   int rear;                           /**< Pointer to rear */
   int size;                           /**< number of data elements stored */
}QueueDouble;


/** initialize queue */
SCIP_Bool InitQueue(
   SCIP*    scip,                      /**< SCIP data structure */
   Queue*   Q                          /**< pointer to store queue */
);


/** circular queue join */
SCIP_Bool EnQueue(
   SCIP*    scip,                      /**< SCIP data structure */
   Queue*   Q,                         /**< pointer to store queue */
   int      nScenarion,                /**< the number of scenarios */
   int      e                          /**< element of entry into the queue */
);


/** cyclic queue out */
int DeQueue(
   Queue*   Q,                         /**< pointer to store queue */
   int      nScenarion                 /**< the number of scenarios */
);


/** initialize (double) queue */
SCIP_Bool InitQueueDouble(
   SCIP*          scip,                /**< SCIP data structure */
   QueueDouble*   Q                    /**< pointer to store queue */
);


/** join the circular (double) queue */
SCIP_Bool EnQueueDouble(
   SCIP*          scip,                /**< SCIP data structure */
   QueueDouble*   Q,                   /**< pointer to store queue */
   int            nScenarion,          /**< the number of scenarios */
   SCIP_Real      e                    /**< element of entry into the queue */
);

/** cyclic (double) queue out */
SCIP_Real DeQueueDouble(
   SCIP*          scip,                /**< SCIP data structure */
   QueueDouble*   Q,                   /**< pointer to store queue */
   int            nScenarion           /**< the number of scenarios */
);

/** determining whether a queue is empty */
SCIP_Bool QueueEmpty(
   Queue*   Q                          /**< pointer to store queue */
);

/** The union of A and B (A U B): the set of all elements of A and B */
SCIP_Bool SetUnion(
	SCIP*                scip,               		/**< SCIP data structure */
	Queue* 					A,								/**< queue A */
	Queue* 					B, 							/**< queue B */
	Queue* 					Union							/**< the union of A and B */
);


#endif /* __CCP_QUEUE__ */

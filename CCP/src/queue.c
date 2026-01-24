/**@file   queue.c
 * @brief  queue structrue for Chance-constrained Program with Random Right-hand Side
 * @author Wei Lv
 * @author Wei-Kun Chen
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <assert.h>
#include "queue.h"


/** initialize queue */
SCIP_Bool InitQueue(
   SCIP*    scip,                      /**< SCIP data structure */
   Queue*   Q                          /**< pointer to store queue */
)
{
   /* set the head and tail pointer to zero */
   Q->front = Q->rear = 0;
   /* set size to zero */
   Q->size = 0;

   return TRUE;
}


/** join the circular queue */
SCIP_Bool EnQueue(
   SCIP*    scip,                      /**< SCIP data structure */
   Queue*   Q,                         /**< pointer to store queue */
   int      nScenarion,                /**< the number of scenarios */
   int      e                          /**< element of entry into the queue */
)
{
   /* (loop) Queue full */
   assert(Q->size < nScenarion);

   /* inserting elements at the end of the queue */
   Q->base[Q->rear] = e;

   /* end-of-queue pointer plus one */
   Q->rear = (Q->rear+1)%nScenarion;

   Q->size++;

   return TRUE;
}


/** cyclic queue out */
int DeQueue(
   Queue*   Q,                         /**< pointer to store queue */
   int      nScenarion                 /**< the number of scenarios */
)
{
   int e = -1;
   /* queue empty */
   assert(Q->size >  0);

   /* exporting header elements */
   e = Q->base[Q->front];

   /* plus one to the head pointer */
   Q->front = (Q->front+1)%nScenarion;
   Q->size--;

   assert(e != -1);

   return e;
}


/** initialize (double) queue */
SCIP_Bool InitQueueDouble(
   SCIP*          scip,                /**< SCIP data structure */
   QueueDouble*   Q                    /**< pointer to store queue */
)
{
   /* set the head and tail pointer to zero */
   Q->front = Q->rear = 0;
   /* set size to zero */
   Q->size = 0;

   return TRUE;
}

/** join the circular queue */
SCIP_Bool EnQueueDouble(
   SCIP*          scip,                /**< SCIP data structure */
   QueueDouble*   Q,                   /**< pointer to store queue */
   int            nScenarion,          /**< the number of scenarios */
   SCIP_Real      e                    /**< element of entry into the queue */
)
{
   /* (loop) Queue full */
   assert(Q->size < nScenarion);

   /* inserting elements at the end of the queue */
   Q->base[Q->rear] = e;

   /* end-of-queue pointer plus one */
   Q->rear = (Q->rear+1)%nScenarion;

   Q->size++;

   return TRUE;
}


/** cyclic queue out */
SCIP_Real DeQueueDouble(
   SCIP*          scip,                /**< SCIP data structure */
   QueueDouble*   Q,                   /**< pointer to store queue */
   int            nScenarion           /**< the number of scenarios */
)
{
   SCIP_Real e = -1.0;
   /* queue empty */
   assert(Q->size > 0);

   /* exporting header elements */
   e = Q->base[Q->front];

   /* plus one to the head pointer */
   Q->front = (Q->front+1)%nScenarion;
   Q->size--;

   assert(e != -1.0);

   return e;
}


/** determining whether a queue is empty */
SCIP_Bool QueueEmpty(
   Queue*   Q                          /**< pointer to store queue */
)
{
   if ( Q->front == Q->rear )
   {
      return TRUE;
   }

   return FALSE;
}


/** The union of A and B: the set of all elements of A and B */
SCIP_Bool SetUnion(
	SCIP*                scip,               		/**< SCIP data structure */
	Queue* 					A,								/**< queue A */
	Queue* 					B, 							/**< queue B */
	Queue* 					Union							/**< the union of A and B */
	)
{
   int i = 0;
	int j = 0;
	int allSize;
	InitQueue(scip, Union);

	allSize = A->size+B->size;
	for( i=0; i < A->size; i++ )
	{
      for( j=0; j < B->size; j++ )
		{
			/* elements belonging to both A and B, skipped  */
      	if( A->base[i] == B->base[j] )
         {
            break;
         }
      }
      if( j == B->size )
		{
			/* elements that belong to A but not to B are stored in Union */
         EnQueue(scip, Union, allSize, A->base[i]);
      }
   }
   for( j = 0; j < B->size; j++ )
	{
		/* All elements of B, into Union */
		EnQueue(scip, Union, allSize, B->base[j]);
   }

	return TRUE;
}

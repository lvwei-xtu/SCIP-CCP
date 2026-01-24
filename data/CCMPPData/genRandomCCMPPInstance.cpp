/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   genRandomCCMPPInstance.cpp
 * @brief  generate a random CCMPP instance
 * @author Wei lv
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <iostream>
#include <string>
#include <sstream>
using namespace std;

#define SCIP_RAND_MAX RAND_MAX
/** returns a random number between 0 and SCIP_RAND_MAX */
   static
int getRand(
      unsigned int*         seedp               /**< pointer to seed value */
      )
{
   return rand_r(seedp);
}

/** returns a random real between minrandval and maxrandval */
   static
double getRandomDouble(
      double              minrandval,         /**< minimal value to return */
      double             maxrandval,         /**< maximal value to return */
      unsigned int*         seedp               /**< pointer to seed value */
      )
{
   double randnumber;

   randnumber = (double) getRand(seedp)/(double)SCIP_RAND_MAX;
   assert(randnumber >= 0.0);
   assert(randnumber <= 1.0);

   /* we multiply minrandval and maxrandval separately by randnumber in order to avoid overflow if they are more than
    * SCIP_REAL_MAX apart
    */
   return minrandval*(1.0 - randnumber) + maxrandval*randnumber;
}

/** returns a random integer between minrandval and maxrandval */                                                                                     
   static
int getRandomInt(
      int                   minrandval,         /**< minimal value to return */
      int                   maxrandval,         /**< maximal value to return */
      unsigned int*         seedp               /**< pointer to seed value */
      )
{
   double randnumber;

   randnumber = (double) getRand(seedp)/(SCIP_RAND_MAX+1.0);
   assert(randnumber >= 0.0);
   assert(randnumber < 1.0);

   /* we multiply minrandval and maxrandval separately by randnumber in order to avoid overflow if they are more than INT_MAX
    * apart
    */
   return (int) (minrandval*(1.0 - randnumber) + maxrandval*randnumber + randnumber);
}

int main(int argc, char** argv)
{
   int    m; 
   int    s;
   int    stageNum;
   int    scenNum;
   int    fileNum;
   int    coalCostLb;
   int    coalCostUb;
   int    nuclearCostLb;
   int    nuclearCostUb;
   int    demandLb; 
   int    demandUb; 
   int    randomInt;
   double sum;
   double coeffLb; 
   double coeffUb; 

   unsigned int seed;
   FILE *file;

   if ( argc != 4 )
   {
      printf("usage: %s <filename> <n> <d>.\n", argv[0]);
      return 1;
   }

   stageNum = atoi(argv[1]);
   scenNum  = atoi(argv[2]);
   fileNum  = atoi(argv[3]);
   seed = 5;

   double *coeff  = new double[scenNum];

   assert( stageNum > 0 );
   assert( scenNum > 0 );

   coalCostLb = 100;  
   coalCostUb = 300; 
   nuclearCostLb = 100;
   nuclearCostUb = 200;
   coeffLb = 1; 
   coeffUb = 100; 
   demandLb = 300;
   demandUb = 700;
   int e1   = getRandomInt(100, 500, &seed);
   double r = getRandomDouble(0.7, 0.99, &seed);

   string a;
   for(s = 0; s < scenNum; s++)
   {
      coeff[s] = getRandomDouble(coeffLb, coeffUb, &seed);
      sum += coeff[s]; 
   }

   for(int k = 0; k < fileNum; k++)
   {
      stringstream ss;
      ss << stageNum;
      ss << "-";
      ss << scenNum;
      ss << "-";
      ss << k;
      ss << ".ccmpp";
      ss >> a;

      /* open file */
      file = fopen(a.c_str(), "w");
      if ( file == NULL )
      {
         printf("Could not open file1\n");
         return 1;
      }

      fprintf(file, "%d\n", stageNum);
      fprintf(file, "%d\n", scenNum);

      fprintf(file,"[ ");
      for(m = 0; m < stageNum; m++)
      {
         randomInt = getRandomInt(coalCostLb, coalCostUb, &seed);
         if(m == stageNum -1 )
            fprintf(file,"%5d\t" "%s\n", randomInt, " ] ");
         else
            fprintf(file,"%5d" "%s", randomInt, ",");
      }
      fprintf(file,"\n");

      fprintf(file,"[ ");
      for(m = 0; m < stageNum; m++)
      {
         randomInt = getRandomInt(nuclearCostLb, nuclearCostUb, &seed);
         if( m == stageNum -1 )
            fprintf(file,"%5d\t" "%s\n", randomInt, " ] ");
         else
            fprintf(file,"%5d" "%s", randomInt, ",");
      }
      fprintf(file,"\n");

      fprintf(file,"[ ");
      for(m = 0; m < stageNum; m++)
      {
         if( m == stageNum -1 )
            fprintf(file,"%8.3f\t" "%s\n", e1*pow(r, m), " ] ");
         else
            fprintf(file,"%8.3f" "%s", e1*pow(r,m), ",");
      }
      fprintf(file,"\n");

      for(s = 0; s < scenNum; s++)
      {
         if(s == 0)
         {
            fprintf(file,"[ ");
            double pi = coeff[s]/sum; 
            fprintf(file,"%7.5f\t", pi);
            fprintf(file,"  [ ");
         }
         else
         {
            fprintf(file,"  ");
            double pi = coeff[s]/sum; 
            fprintf(file,"%7.5f\t", pi);
            fprintf(file,"  [ ");
         }
         for(m = 0; m < stageNum; m++)
         {
            randomInt = getRandomInt(demandLb, demandUb, &seed);
            if( m == stageNum -1 && s != scenNum -1 )
               fprintf(file,"%5d\t" "%s\n", randomInt, " ] ");
            else if( m == stageNum -1 && s == scenNum -1 )
               fprintf(file,"%5d\t" "%s\n", randomInt, " ] ] ");
            else
               fprintf(file,"%5d" "%s", randomInt, ",");
         }
      } 
      fclose(file);
   } 
   delete []coeff;
   return 0;
}



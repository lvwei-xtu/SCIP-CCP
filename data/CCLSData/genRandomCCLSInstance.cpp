/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   genRandomCCLSInstance.cpp
 * @brief  generate a random CCLS instance
 * @author Wei lv
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
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


/** returns a random double number between minrandval and maxrandval */
static
double getRandomDouble(
   int                   minrandval,         /**< minimal value to return */
   int                   maxrandval,         /**< maximal value to return */
   unsigned int*         seedp,               /**< pointer to seed value */
   int                   n
   )
{
   double number = minrandval + (double) ((maxrandval - minrandval) * (double) getRand(seedp)/(SCIP_RAND_MAX));
   return floor(number) + (floor((number - floor(number))* pow(10,n) ))/ pow(10, n);
}

int main(int argc, char** argv)
{
   int m; 
   int s;
   int stageNum;
   int scenNum;
   int fileNum;
   double sum;
   double setCostLb;
   double setCostUb; 
   double pdCostLb; 
   double pdCostUb;
   double demandLb; 
   double demandUb; 
   double holdCostLb; 
   double holdCostUb; 
   double randomDouble;
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
   scenNum = atoi(argv[2]);
   fileNum = atoi(argv[3]);
   seed = 5;
   
   double *coeff  = new double[scenNum];
   
   assert( stageNum > 0 );
   assert( scenNum > 0 );

   setCostLb = 1;
   setCostUb = 1000; 
   pdCostLb  = 1; 
   pdCostUb = 10;
   demandLb = 1; 
   demandUb = 100;
   holdCostLb = 1;
   holdCostLb = 5;
   coeffLb = 1; 
   coeffUb = 100; 

   string a;
   for(s = 0; s < scenNum; s++)
   {
      coeff[s] = getRandomDouble(coeffLb, coeffUb, &seed, 3);
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
      ss << ".ccls";
      ss >> a;
      //printf("%s\n", a.c_str());

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
         randomDouble = getRandomDouble(setCostLb, setCostUb, &seed, 4);
         if(m == stageNum -1 )
            fprintf(file,"%10.3f\t" "%s\n", randomDouble, " ] ");
         else
            fprintf(file,"%10.3f" "%s", randomDouble, ",");
      }
      fprintf(file,"\n");
      
      fprintf(file,"[ ");
      for(m = 0; m < stageNum; m++)
      {
         randomDouble = getRandomDouble(pdCostLb, pdCostUb, &seed, 4);
         if( m == stageNum -1 )
            fprintf(file,"%8.3f\t" "%s\n", randomDouble, " ] ");
         else
            fprintf(file,"%8.3f" "%s", randomDouble, ",");
      }
      fprintf(file,"\n");

      fprintf(file,"[ ");
      for(m = 0; m < stageNum; m++)
      {
         randomDouble = getRandomDouble(holdCostLb, holdCostUb, &seed, 4);
         if( m == stageNum -1 )
            fprintf(file,"%8.3f\t" "%s\n", randomDouble, " ] ");
         else
            fprintf(file,"%8.3f" "%s", randomDouble, ",");
      }
      fprintf(file,"\n");
      
      vector<vector <double>> array;
      for(m = 0; m < stageNum; m++)
      {
         std::vector<double> rr;
         int times = 0;
         for(s = 0; s < scenNum; s++)
         {
            seed = seed + times;
            randomDouble = getRandomDouble(demandLb, demandUb, &seed, 4);
            int size = rr.size();
            bool flag = true;
            for(int i = 0; i < size; i++)
            {
               if(abs(rr[i] - randomDouble) < 1e-3)
               {
                  flag = false;
                  break;
               }       
            }
            if(flag == true)   
               rr.push_back(randomDouble);
            else
            {
               s--;
               times++;
            }
         }
         array.push_back(rr);
      }

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
#if 1
         for(m = 0; m < stageNum; m++)
         {
            //randomDouble = getRandomDouble(demandLb, demandUb, &seed);
            if( m == stageNum -1 && s != scenNum -1 )
               fprintf(file,"%10.3f\t" "%s\n", array[m][s], " ] ");
            else if( m == stageNum -1 && s == scenNum -1 )
               fprintf(file,"%10.3f\t" "%s\n", array[m][s], " ] ] ");
            else
               fprintf(file,"%10.2f" "5%s", array[m][s], ",");
         }
#endif
      } 
      fclose(file);
   } 
   delete []coeff;
   return 0;
}



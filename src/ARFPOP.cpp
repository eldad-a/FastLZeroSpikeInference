/* -*- compile-command: "R CMD INSTALL .." -*- */

#include <vector>
#include <stdio.h>
#include "funPieceListLog.h"
#include <math.h>


#include <stdlib.h>

FILE *stream;



void ARFPOP
  (double *data_vec, int data_count,
   double penalty,
   // the following matrices are for output.
   // cost_mat and intervals_mat store the optimal cost and number of intervals
   // at each time point, for the up and down cost models.
   // end_vec and mean_vec store the best model up to and including the
   // last data point.
   double gam, // decay parameter
   double *cost_mat, // cost matrix
   int *end_vec, //data_count
   double *mean_vec,//data_count
   int *intervals_mat,//data_count
   bool *constraint,
   int *success){
  
  double MAX = 1e200;
  double EPS = 1e-10;
  bool pipeOut = 0;
  
  if (pipeOut) {
    stream = freopen( "/Users/jewellsean/Desktop/costFunc.out", "w", stdout );
  }
  
  
  double min_mean=0, max_mean;
  // double scale = pow(gam, data_count + 1);
  // if (scale < INFINITY) {
  //   max_mean = 0;
  //   for(int data_i=0; data_i<data_count; data_i++){
  //     double data = data_vec[data_i];
  //     if(data > max_mean){
  //       max_mean = data;
  //     }
  //   }
  //   
  //   max_mean *= (gam + 1) / scale;
  // } else {
  //   max_mean = INFINITY;
  // }
  
  max_mean = INFINITY;
  std::vector<PiecewiseSquareLoss> cost_model_mat(data_count);
  PiecewiseSquareLoss *cost, *cost_prev;
  PiecewiseSquareLoss min_prev_cost, scaled_prev_cost, min_prev_cost_scaled;
  int verbose=0;
  for(int data_i=0; data_i<data_count; data_i++){
    cost = &cost_model_mat[data_i];
    // Alg 3, ln 4
    if(data_i==0){
      cost->piece_list.emplace_back
      (0.5, - data_vec[0], data_vec[0] * data_vec[0] / 2,
       min_mean, max_mean, -1, false);
      
      // if (pipeOut) {
      //   printf("=cost at data point i = %d \n", data_i);
      //   cost -> print();
      // }
      

    }else{ // Alg 3 ln 6 - 8
      
      printf("=before-trim at data i = %d \n", data_i);
      cost_prev -> print();
      
      
      
      scaled_prev_cost.set_to_scaled_of(cost_prev, gam, EPS, verbose);
      
      // printf("=after-trim at data i = %d \n", data_i);
      // scaled_prev_cost.print();
      
      if (*constraint) {
        min_prev_cost.set_to_min_less_of(&scaled_prev_cost, verbose);
      } else {
        min_prev_cost.set_to_unconstrained_min_of(&scaled_prev_cost, verbose);
      }
      
      min_prev_cost.set_prev_seg_end(data_i-1);
      min_prev_cost.add(0.0, 0.0, penalty);
      
      min_prev_cost_scaled.set_to_scaled_of(&min_prev_cost, gam, 0.0, verbose);
      
      // printf("=min_prev_cost_scaled at data_i__%d\n", data_i);
      // min_prev_cost_scaled.print();
      // printf("=scaled_prev_cost at data_i__%d\n", data_i);
      // scaled_prev_cost.print();
      
      
      cost->set_to_min_env_of(&min_prev_cost_scaled, &scaled_prev_cost, verbose);
      
      // printf("=min_min at data_i__%d\n", data_i);
      // cost -> print();
      
      
      int status = cost->check_min_of(&min_prev_cost_scaled, &scaled_prev_cost);

      try {
        if(status){
          printf("Lambda = %.20e \t Gamma = %.100e\n", penalty, gam);
          printf("BAD MIN ENV CHECK data_i=%d status=%d\n", data_i, status);
          cost->set_to_min_env_of(&min_prev_cost_scaled, &scaled_prev_cost, false);
          printf("=min_prev_cost_scaled\n");
          min_prev_cost_scaled.print();
          printf("=scaled_prev_cost + %f\n", penalty);
          scaled_prev_cost.print();
          printf("=new cost model\n");
          cost->print();
          throw status;
        }
      } catch(int e) {
        printf("An exception occured %d \n", e);
      }
      
      // if (pipeOut) {
      //   printf("=cost at data point i = %d \n", data_i);
      //   cost -> print();
      // }
      
      
      cost->add
        (0.5,
         - data_vec[data_i], data_vec[data_i] * data_vec[data_i] / 2);

    try {
        cost -> checkStable(MAX);
      } catch(double e) {
      *success = 0;
      printf("Numerically unstable. Choose a smaller lambda.\n");
      return;
  }
    } 
    cost_prev = cost;
    
  }
  

  
  printf("All cost functions assembled\n");
  
  
  // Decoding the cost_model_vec, and writing to the output matrices.
  double best_cost, best_mean, prev_mean;
  int prev_seg_end=data_count;
  
  for(int i=0; i< data_count; i++){
    cost = &cost_model_mat[i];
    
    printf("Cost function at data_i %i\n", i);
    cost -> print();
    
    intervals_mat[i] = cost->piece_list.size();
    cost->Minimize
      (&best_cost, &best_mean,
       &prev_seg_end, &prev_mean);
    
    
    cost_mat[i] = best_cost;
  }
  
  printf("------ start decoding -----\n");
  
  
  // first step
  cost = &cost_model_mat[data_count - 1];
  cost->Minimize
    (&best_cost, &best_mean,
     &prev_seg_end, &prev_mean);
  
  int prev_seg_old = data_count - 1;
  int out_i=0;
  double mean = best_mean;

  // loop over all prev. changepoints
  double temp_mean;
  while(prev_seg_old >= 0){
    
    printf("next pt %i\n", prev_seg_old);
    
    if (prev_seg_old < data_count - 1) {
      cost = &cost_model_mat[prev_seg_end];
      // cost -> findMean
      //   (mean, &prev_seg_end, &prev_mean);
      
      cost->Minimize
        (&best_cost, &best_mean,
         &prev_seg_end, &prev_mean);
      
    }
    
    
    for (int t = prev_seg_old; t > prev_seg_end; t--){
      mean_vec[out_i] = mean;
      end_vec[out_i] = prev_seg_end;
      mean /= gam;
      out_i++;
    }
    
    prev_seg_old = prev_seg_end;
    
    if (prev_seg_old >= 0) {
      if(prev_mean < INFINITY){
        //equality constraint inactive
        mean = prev_mean;
      }
    }
    
  }
  
  if (pipeOut) { fclose( stream );}
}

Format of the data in the CCMPPData files.
-The file name is ``T-n-k.ccrp'', where T is the number of periods，n is the scenario size, and k is an index for the instance (0,1,2,3,4).
- In the file:
  - The number on the first line is T: the number of periods
  - The number on the second line is n: the scenario size
  - The third line provides an array of the capital costs per megawatt for coal power plants (that data c_t, t=1,...,T) 
  - The fourth line provides an array of the capital costs per megawatt for nuclear power plants (that data n_t, t=1,...,T) 
  - The fifth line provides an array of the electric capacity from existing resources (that data e_t, t=1,...,T) 
  - For each of the following n lines: the first number is the probability of a scenario, and each array represents the set of electricity demands in a scenario. 
      The format the kth array is [d_1^k, d_2^k, ..., d_T^k] (i.e., an array start is indicated by '[', there are then T numbers, separated by commas, 
      and the end of the array is indicated by ']'.)

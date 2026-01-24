Format of the data in the CCRPData files:
-The file name is ``|I|-|J|-n-k.ccrp'', where |I| and |J| are the numbers of resources and customers, respectively, 
n is the scenario size, and k is an index for the instance (0,1,2,3,4).
- In the file:
  - The number on the first line is |I|: the number of resources
  - The number on the second line is |J|: the number of customers
  - For each of the following |J| lines (one per customer): Array of service rates for that customer, if served by each
	  	server (i.e., for customer row j, element i of this array is \mu_{ij}). The array format is [ \mu_j1, \mu_j2, ...
	  	\mu_{j|I|}]. A service rate less than zero (-1, specifically) indicates that server is not able to serve that customer
	   	type.
  - The next line provides an array of costs per server (that data c_i, i=1,...,|I|) 
	- For each of the following n lines: an array representing the set of customer demands in a scenario. The format of the kth array
  is [lambda_1^k, lambda_2^k, ..., lambda_|J|^k] (i.e., an array start is indicated by '[', there are then |J| numbers,
  seaparated by commas, and the end of the array is indicated by ']'.)
# testingmlp
Testing memory-level parallelism

## Usage

```
make
./testingmlp
```


## Sample output

```
$ ./testingmlp
Initializing array made of 33554432 64-bit words.
Applying Sattolo's algorithm.
Surgery on the long cycle.
Verifying the neighboring distance...
mindist = 335544 vs 335544
Time to sum up the array (linear scan) 0.011 s (x 8 = 0.084 s), bandwidth = 24256.2 MB/s
Legend:
  BandW: Implied bandwidth (assuming 64-byte cache line) in MB/s
  % Eff: Effectiness of this lane count compared to the prior, as a % of ideal
  Speedup: Speedup factor for this many lanes versus one lane
---------------------------------------------------------------------
- # of lanes --- time (s) ---- BandW -- ns/hit -- % Eff -- Speedup --
---------------------------------------------------------------------
           1     2.719856        753      81.1       0%        1.0
           2     1.366652       1499      40.7     100%        2.0
           3     0.915235       2238      27.3      99%        3.0
           4     0.690618       2965      20.6      98%        3.9
           5     0.556533       3680      16.6      97%        4.9
           6     0.469215       4365      14.0      94%        5.8
           7     0.406813       5034      12.1      93%        6.7
           8     0.359100       5703      10.7      94%        7.6
           9     0.323974       6321       9.7      88%        8.4
          10     0.296101       6917       8.8      86%        9.2
          11     0.278978       7341       8.3      64%        9.7
          12     0.269778       7591       8.0      40%       10.1
```

# turbocalc

Estimate CPU turbo frequency. Deliver single-core and multi-core max turbo frequency.

## Usage

```
Usage: turbocalc [Options]
Options:
  -c, --compensate       Compensate result for the physical BCLK spread-spectrum drop
  -f, --format <csv|txt> Output format (default: txt)
  -i, --iterations <M>   Millions of iterations per thread (default: 200)
  -r, --runs <count>     Number of test runs (default: 5)
  -m, --max_tcores       Find the max count of single-core turbo capable cores (basic)
  -M, --max_tcores_full  Find the max count of single-core turbo capable cores (thorough)
  -v, --verbose          Display more details (only works for txt format)
  -h, --help             Display this help message
```

## Examples

```
docker run --rm --security-opt seccomp=unconfined --cap-add SYS_ADMIN ug1ybob/turbocalc:0.0.1 -f csv >turbo.csv
```
```
docker run --rm --security-opt seccomp=unconfined --cap-add SYS_ADMIN ug1ybob/turbocalc:0.0.1 -m
```


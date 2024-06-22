## Macro benchmarks
> The goal of these experiments is to measure the macrobench of BUDAlloc.  
> We prepared 5 experiments: apache2, nginx, parsec, SPEC2006, and SPEC2017

## How to run
### Apache2
   ```
   cd macrobench/apache2
   ./install.sh
   ./bench_apache2.sh
   ```

### Nginx
   ```
   cd macrobench/nginx
   ./install.sh
   ./bench_nginx.sh
   ```

### SPEC2006 / SPEC2017
```
# Before run, you should install SPEC2006 / SPEC2017 in home directory

$ cd SPEC{version}
$ bench_spec{version}.sh
```

### parsec
```
$ install.sh
$ bench_parsec.sh
```
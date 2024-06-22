set -eu

[ -f php-7.4.0.tar.gz ] || \
    wget "https://www.php.net/distributions/php-7.4.0.tar.gz"
[ -d php-7.4.0.tar.gz ] || \
    tar xf php-7.4.0.tar.gz

cd php-7.4.0

[ -f Makefile ] || \
    ./configure
[ -f sapi/cli/php ] || \
    make -j`nproc`
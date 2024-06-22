set -eu

[ -f php-7.0.7.tar.gz ] || \
    wget "https://www.php.net/distributions/php-7.0.7.tar.gz"
[ -d php-7.0.7.tar.gz ] || \
    tar xf php-7.0.7.tar.gz

cd php-7.0.7
patch -p1 -s < ../disable_custom_allocator.patch

[ -f Makefile ] || \
    ./configure --enable-zip
[ -f sapi/cli/php ] || \
    make -j`nproc`
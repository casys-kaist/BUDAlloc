set -eu

[ -f php-5.4.44.tar.gz ] || \
    wget "https://www.php.net/distributions/php-5.4.44.tar.gz"
[ -d php-5.4.44.tar.gz ] || \
    tar xf php-5.4.44.tar.gz

rm php-5.4.44.tar.gz

cd php-5.4.44
patch -p1 -s < ../disable_custom_allocator.patch

[ -f Makefile ] || \
    ./configure
[ -f sapi/cli/php ] || \
    make -j`nproc`
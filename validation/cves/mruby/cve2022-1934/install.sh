if [ ! -d "mruby" ]
then
    git clone https://github.com/mruby/mruby.git
fi

cd mruby
git stash
git pull
git checkout -f ac79849

rake clean

# export CFLAGS="-g -O0 -lpthread -fsanitize=address"
# export CXXFLAGS="-g -O0 -lpthread -fsanitize=address"
# export LDFLAGS="-fsanitize=address"
rake -j

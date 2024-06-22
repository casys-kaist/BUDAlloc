if [ ! -d "mruby" ]
then
    git clone https://github.com/mruby/mruby.git
fi

cd mruby
git stash
git pull
git checkout -f 191ee25


rake clean

rake -j

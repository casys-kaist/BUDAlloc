if [ ! -d "mruby" ]
then
    git clone https://github.com/mruby/mruby.git
fi

cd mruby
git stash
git pull
git checkout -f bf5bbf0

rake clean

rake -j

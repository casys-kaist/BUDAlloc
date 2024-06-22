if [ -d "patch" ]
then
rm -rf patch
fi

if [ -e "patch-2.7.6.tar.gz" ]
then
  rm patch-2.7.6.tar.gz
fi

if [ -e "master.zip" ]
then
  rm master.zip
fi
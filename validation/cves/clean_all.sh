
# iterate all directories and run $ sudo clean_all.sh

for dir in */ ; do
  if [ -f "${dir}clean_all.sh"  ]; then
    echo "Running clean_all.sh in ${dir}"

    # Run build_all.py script
    (cd "$dir" && bash clean_all.sh)
  else
    echo "No clean_all.sh found in ${dir}"
  fi
done

# if results directory exists, then clean up!!
if [ -d "results" ]; then 
  rm -rf results
  echo "Removed results directory";
fi
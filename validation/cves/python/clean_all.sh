for dir in */ ; do
    # Check if the clean.sh script exists in the directory
    if [ -f "${dir}clean.sh" ]; then
        echo "Running clean.sh in ${dir}"
        # Run the install.sh script
        (cd "$dir" && bash clean.sh)
    else
        echo "No clean.sh found in ${dir}"
    fi
done
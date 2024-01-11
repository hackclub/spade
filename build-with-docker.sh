for dep in docker chcon echo whoami grep; do
    if ! command -v $dep > /dev/null; then
        printf "%s not found, please install %s\n" "$dep" "$dep"
        exit 1
    fi
done

echo Dependencies found successfully

if id -nG $(whoami) | grep -qw "docker"; then
    echo User in docker group, continuing
else
    echo User $(whoami) not in the docker group. Please add user $(whoami) to the docker group and try again
    exit 1
fi


docker build ./docker | tee ./dockerBuildLog.txt
chcon -R -t container_file_t ./
docker run -it --rm --volume ./:/root/spade $(docker images | awk '{print $3}' | awk 'NR==2')
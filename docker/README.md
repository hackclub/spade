# Building with Docker
 - Use the script in the root directory `build-with-docker.sh`
 
## Note for Mac users
On macOS there has been issues reported with the `build-with-docker.sh` script. If you encounter issues please comment out this line: `chcon -R -t container_file_t ./` by adding a `#` sign in front of the line.


### Thank Yous
 - Random person on Github for explaining the correct packages to install to cross compile for ARM https://github.com/johnwalicki/RaspPi-Pico-Examples-Fedora

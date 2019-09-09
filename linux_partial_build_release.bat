# Clear the workspace
rm -Rf build/

# We need theses folders to be present
mkdir -p build

# Build the bot
cd build

# Generate a Makefile.
# Use 'cmake -DCMAKE_BUILD_TYPE=Debug ../' if debuginfo is needed
cmake ../

# Build.
make
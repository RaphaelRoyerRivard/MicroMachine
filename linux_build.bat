# Clear the workspace
rm -Rf build/
rm -Rf api/
rm -Rf sc2-libvoxelbot/
rm -Rf /usr/local/include
rm -Rf /usr/local/lib

# We need theses folders to be present
mkdir -p build
mkdir -p /usr/local/include
mkdir -p /usr/local/include/sc2-libvoxelbot
mkdir -p /usr/local/lib
mkdir -p /usr/local/lib/sc2api

# Clone and build libvoxelbot (for combat simulator)
if [ ! -d "sc2-libvoxelbot" ] ; then
	git clone --recursive https://github.com/RaphaelRoyerRivard/sc2-libvoxelbot
fi
cd sc2-libvoxelbot
git submodule update --force --recursive --init --remote
mkdir build
cd build
cmake ../
make -j8

cd ../

# Install libvoxelbot lib
cp -R libvoxelbot /usr/local/include/sc2-libvoxelbot
cp -R build /usr/local/include/sc2-libvoxelbot

ls -la /usr/local/include/sc2-libvoxelbot

# Install SC2 API headers
cp -R s2client-api/include/sc2api /usr/local/include
cp -R s2client-api/include/sc2utils /usr/local/include
cp -R build/s2client-api/generated/s2clientprotocol /usr/local/include

# Install protobuf headers
cp -R s2client-api/contrib/protobuf/src/google /usr/local/include/sc2api

# Install SC2 API libraries
cp build/s2client-api/bin/libcivetweb.a /usr/local/lib/sc2api/libcivetweb.a
cp build/s2client-api/bin/libprotobuf.a /usr/local/lib/sc2api/libprotobuf.a
cp build/s2client-api/bin/libsc2api.a /usr/local/lib/sc2api/libsc2api.a
cp build/s2client-api/bin/libsc2lib.a /usr/local/lib/sc2api/libsc2lib.a
cp build/s2client-api/bin/libsc2protocol.a /usr/local/lib/sc2api/libsc2protocol.a
cp build/s2client-api/bin/libsc2utils.a /usr/local/lib/sc2api/libsc2utils.a

# Build the bot
cd ../
ls -la /usr/local/include/sc2api
ls -la /usr/local/lib/sc2api
cd build

# Generate a Makefile.
# Use 'cmake -DCMAKE_BUILD_TYPE=Debug ../' if debuginfo is needed
cmake ../

# Build.
make

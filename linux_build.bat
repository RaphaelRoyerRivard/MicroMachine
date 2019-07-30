# We need theses folders to be present
mkdir -p build
mkdir -p api
mkdir -p api/include
mkdir -p api/lib
mkdir -p api/lib/sc2api

# Clone and build libvoxelbot (for combat simulator)
if [ ! -d "sc2-libvoxelbot" ] ; then
	git clone --recursive https://github.com/RaphaelRoyerRivard/sc2-libvoxelbot
fi
cd sc2-libvoxelbot
mkdir build
cd build
cmake ../
make -j8

cd ../

# Install SC2 API headers
cp -R s2client-api/include/sc2api ../api/include
cp -R s2client-api/include/sc2utils ../api/include
cp -R build/s2client-api/generated/s2clientprotocol ../api/include

# Install protobuf headers
cp -R s2client-api/contrib/protobuf/src/google ../api/include/sc2api

# Install SC2 API libraries
cp build/s2client-api/bin/libcivetweb.a ../api/lib/sc2api/libcivetweb.a
cp build/s2client-api/bin/libprotobuf.a ../api/lib/sc2api/libprotobuf.a
cp build/s2client-api/bin/libsc2api.a ../api/lib/sc2api/libsc2api.a
cp build/s2client-api/bin/libsc2lib.a ../api/lib/sc2api/libsc2lib.a
cp build/s2client-api/bin/libsc2protocol.a ../api/lib/sc2api/libsc2protocol.a
cp build/s2client-api/bin/libsc2utils.a ../api/lib/sc2api/libsc2utils.a

# # Clone and build the api
# if [ ! -d "s2client-api" ] ; then
# 	git clone --recursive https://github.com/RaphaelRoyerRivard/s2client-api
# fi
# cd s2client-api   
# mkdir -p build && cd build
# cmake ../
# make
# cd ../

# # Install SC2 API headers
# cp -R include/sc2api ../api/include
# cp -R include/sc2utils ../api/include
# cp -R build/generated/s2clientprotocol ../api/include

# # Install protobuf headers
# cp -R contrib/protobuf/src/google ../api/include/sc2api

# Install SC2 API libraries
# cp build/bin/libcivetweb.a ../api/lib/sc2api/libcivetweb.a
# cp build/bin/libprotobuf.a ../api/lib/sc2api/libprotobuf.a
# cp build/bin/libsc2api.a ../api/lib/sc2api/libsc2api.a
# cp build/bin/libsc2lib.a ../api/lib/sc2api/libsc2lib.a
# cp build/bin/libsc2protocol.a ../api/lib/sc2api/libsc2protocol.a
# cp build/bin/libsc2utils.a ../api/lib/sc2api/libsc2utils.a

# Build the bot
cd ../
ls -la api/include/sc2api
ls -la api/lib/sc2api
cd build

# Generate a Makefile.
# Use 'cmake -DCMAKE_BUILD_TYPE=Debug ../' if debuginfo is needed
cmake ../

# Build.
make
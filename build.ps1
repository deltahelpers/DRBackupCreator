Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++"
cmake --build build -j2
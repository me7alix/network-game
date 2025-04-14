mkdir -p build
cc src/client.c -o build/client -lraylib
cc src/server.c -o build/server -lraylib

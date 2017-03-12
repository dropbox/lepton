#include <cstring>
#ifndef EMSCRIPTEN

extern int app_main(int argc, char ** argv);
extern int benchmark(int argc, char ** argv);

int main (int argc, char **argv) {
    for (int i = 1; i < argc; ++i){
        if (strstr(argv[i], "-benchmark") != 0) {
            for (int j = i; j + 1 < argc; ++j) {
                argv[j] = argv[j + 1];
            }
            --argc;
            return benchmark(argc, argv);
        }
    }
    return app_main(argc, argv);
}

#endif

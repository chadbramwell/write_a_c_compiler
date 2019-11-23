#include "test.h"
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    // if -test is specified, ignore rest of command-line
    for (int i = 1; i < argc; ++i)
    {
        if (0 == strcmp(argv[i], "-test"))
        {
            if (i + 1 < argc)
            {
                int folder_index = atoi(argv[i + 1]);
                return run_tests_on_folder(folder_index);
            }
            return run_all_tests();
        }
    }

    // check for specific file to test
    if (argc >= 2)
    {
        const char* test_file = argv[1];
        bool verbose = false;
        if (argc == 3)
        {
            if (argv[2][0] == '-' &&
                argv[2][1] == 'v' &&
                argv[2][2] == 0)
            {
                verbose = true;
            }
        }
        return run_specific_test(test_file, verbose);
    }
    
    return run_specific_test(NULL, false);
}

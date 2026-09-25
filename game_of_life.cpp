#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The maximum length of a 64-bit signed int is 20 characters, so this should be
// more than enough to handle two separated by a space.
constexpr int LINE_MAX = 256;

int main()
{
    char buffer[LINE_MAX];

    gets_s(buffer, sizeof(buffer));
    if (strcmp(buffer, "#Life 1.06"))
    {
        printf("Invalid header: %s\n", buffer);
        exit(-1);
    }

    while (true)
    {
        char* const getsResult = gets_s(buffer, sizeof(buffer));
        if (!getsResult)
        {
            if (feof(stdin))
            {
                break;
            }
            else
            {
                puts("Error reading stdin");
                exit(-1);
            }
        }

        int64_t x = 0;
        int64_t y = 0;
        const int scanResult = sscanf_s(buffer, "%" SCNd64 " %" SCNd64, &x, &y);
        if (scanResult == EOF)
            break;

        if (scanResult != 2)
        {
            printf("Error scanning line: %s\n", buffer);
            exit(-1);
        }
    }

    return 0;
}
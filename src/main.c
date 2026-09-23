#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "expand.h"
#include "executor.h"

#define MAX_INPUT 1024

int main(void)
{
    char input[MAX_INPUT];

    /* Install SIGCHLD handler for background processes */
    setup_background_handler();

    while (1)
    {
        printf("shellforge$ ");
        fflush(stdout);

        /*
         * Read command from user
         */
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("\n");
            break;
        }

        /*
         * Remove trailing newline
         */
        input[strcspn(input, "\n")] = '\0';

        /*
         * Ignore empty input
         */
        if (strlen(input) == 0)
            continue;

        /*
         * Tokenize input
         */
        token_list tokens;

        token_list_init(&tokens);

        lexer(input, &tokens);

        /*
         * Parse tokens into pipeline
         */
        pipeline_t pipeline;

        pipeline_init(&pipeline);

        if (!parse(&tokens, &pipeline))
        {
            pipeline_free(&pipeline);
            continue;
        }

        /*
         * Expand variables
         */
        expand_variables(&pipeline);

        /*
         * Execute command / pipeline
         *
         * Background commands return immediately.
         */
        execute_pipeline(&pipeline);

        /*
         * Free allocated memory
         */
        pipeline_free(&pipeline);
    }

    return 0;
}

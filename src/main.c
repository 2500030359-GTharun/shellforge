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

    while (1)
    {
        printf("shellforge$ ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("\n");
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0)
            continue;

        /* Lexer */
        token_list tokens;
        token_list_init(&tokens);

        lexer(input, &tokens);

        /* Parser */
        pipeline_t pipeline;
        pipeline_init(&pipeline);

        if (!parse(&tokens, &pipeline))
        {
            pipeline_free(&pipeline);
            continue;
        }

        /* Variable expansion */
        expand_variables(&pipeline);

        /* Executor */
        execute_pipeline(&pipeline);

        /* Free parsed pipeline */
        pipeline_free(&pipeline);
    }

    return 0;
}

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

int execute_external(char **argv);
int execute_command(command_t *command);
int execute_pipeline(pipeline_t *pipeline);

#endif

/* $Id$ */
/* dispatch test suite. */

#include "config.h"
#include "clibrary.h"

#include "inn/dispatch.h"
#include "inn/vector.h"
#include "libtest.h"

/* Prototypes for command callbacks. */
static void command_0(struct cvector *, void *);
static void command_1(struct cvector *, void *);
static void command_4(struct cvector *, void *);

/* Test command dispatch table. */
const struct dispatch commands[] = {
    { "0", command_0, 0, 0, NULL },
    { "1", command_1, 1, 1, NULL },
    { "4", command_4, 2, 4, NULL }
};

/* Global string indicating the last command handler that was called. */
static const char *last_handler = NULL;

/* Global flag indicating whether the last handler was okay. */
static bool last_args_valid = false;

/* Command that takes 0 arguments. */
static void
command_0(struct cvector *command, void *cookie)
{
    last_handler = "0";
    last_args_valid = true;
    if (command->count != 1 || strcmp(command->strings[0], "0") != 0)
        last_args_valid = false;
    if (cookie != &last_handler)
        last_args_valid = false;
}

/* Command that takes 1 argument. */
static void
command_1(struct cvector *command, void *cookie)
{
    last_handler = "1";
    last_args_valid = true;
    if (command->count != 2 || strcmp(command->strings[0], "1") != 0)
        last_args_valid = false;
    if (cookie != &last_handler)
        last_args_valid = false;
}

/* Command that takes 2-4 arguments. */
static void
command_4(struct cvector *command, void *cookie)
{
    last_handler = "4";
    last_args_valid = true;
    if (command->count < 3 || command->count > 5)
        last_args_valid = false;
    if (strcmp(command->strings[0], "4") != 0)
        last_args_valid = false;
    if (cookie != &last_handler)
        last_args_valid = false;
}

/* Handler for unknown commands. */
static void
command_unknown(struct cvector *command UNUSED, void *cookie)
{
    last_handler = "unknown";
    last_args_valid = (cookie == &last_handler);
}

/* Handler for syntax errors. */
static void
command_syntax(struct cvector *command UNUSED, void *cookie)
{
    last_handler = "syntax";
    last_args_valid = (cookie == &last_handler);
}

int
main(void)
{
    struct cvector *command;

    test_init(18);

    command = cvector_new();
    cvector_resize(command, 6);
    last_handler = NULL;
    cvector_add(command, "0");
    dispatch(command, commands, ARRAY_SIZE(commands), command_unknown,
             command_syntax, &last_handler);
    ok_string(1, "0", last_handler);
    ok(2, last_args_valid);
    command->strings[0] = "1";
    cvector_add(command, "arg1");
    dispatch(command, commands, ARRAY_SIZE(commands), command_unknown,
             command_syntax, &last_handler);
    ok_string(3, "1", last_handler);
    ok(4, last_args_valid);
    command->strings[0] = "4";
    dispatch(command, commands, ARRAY_SIZE(commands), command_unknown,
             command_syntax, &last_handler);
    ok_string(5, "syntax", last_handler);
    ok(6, last_args_valid);
    cvector_add(command, "arg2");
    dispatch(command, commands, ARRAY_SIZE(commands), command_unknown,
             command_syntax, &last_handler);
    ok_string(7, "4", last_handler);
    ok(8, last_args_valid);
    cvector_add(command, "arg3");
    dispatch(command, commands, ARRAY_SIZE(commands), command_unknown,
             command_syntax, &last_handler);
    ok_string(9, "4", last_handler);
    ok(10, last_args_valid);
    cvector_add(command, "arg4");
    dispatch(command, commands, ARRAY_SIZE(commands), command_unknown,
             command_syntax, &last_handler);
    ok_string(11, "4", last_handler);
    ok(12, last_args_valid);
    cvector_add(command, "arg5");
    dispatch(command, commands, ARRAY_SIZE(commands), command_unknown,
             command_syntax, &last_handler);
    ok_string(13, "syntax", last_handler);
    ok(14, last_args_valid);
    command->strings[0] = "5";
    dispatch(command, commands, ARRAY_SIZE(commands), command_unknown,
             command_syntax, &last_handler);
    ok_string(15, "unknown", last_handler);
    ok(16, last_args_valid);
    cvector_resize(command, 0);
    dispatch(command, commands, ARRAY_SIZE(commands), command_unknown,
             command_syntax, &last_handler);
    ok_string(17, "unknown", last_handler);
    ok(18, last_args_valid);

    return 0;
}

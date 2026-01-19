#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
struct termios orig_termios;
char **history = NULL;
int historyPos = 0;
int historyBuffSize = LSH_TOK_BUFSIZE;

void disableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode()
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

char *lsh_read_line(void)
{
    int bufsize = LSH_RL_BUFSIZE;
    int position = 0;
    int historyIndex = -1;
    char *buffer = malloc(bufsize);
    int c;

    if (!buffer)
    {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        c = getchar();

        // ENTER
        if (c == '\n')
        {
            putchar('\n');
            buffer[position] = '\0';
            return buffer;
        }

        // CTRL+D
        if (c == 4)
        {
            buffer[position] = '\0';
            return buffer;
        }

        // BACKSPACE
        if (c == 127)
        {
            if (position > 0)
            {
                position--;
                buffer[position] = '\0';
                printf("\b \b");
            }
            continue;
        }

        // ARROW KEYS
        if (c == 27) // ESC
        {
            int c1 = getchar(); // [
            int c2 = getchar(); // A/B

            if (c1 == '[')
            {
                // UP arrow
                if (c2 == 'A' && historyPos > 0)
                {
                    if (historyIndex == -1)
                        historyIndex = historyPos - 1;
                    else if (historyIndex > 0)
                        historyIndex--;

                    printf("\33[2K\r> ");
                    strcpy(buffer, history[historyIndex]);
                    position = strlen(buffer);
                    printf("%s", buffer);
                }

                // DOWN arrow
                else if (c2 == 'B')
                {
                    if (historyIndex != -1)
                    {
                        historyIndex++;
                        if (historyIndex >= historyPos)
                        {
                            historyIndex = -1;
                            buffer[0] = '\0';
                            position = 0;
                        }
                        else
                        {
                            strcpy(buffer, history[historyIndex]);
                            position = strlen(buffer);
                        }

                        printf("\33[2K\r> %s", buffer);
                    }
                }
            }
            continue;
        }

        // NORMAL CHARACTER
        buffer[position++] = c;
        putchar(c);

        if (position >= bufsize)
        {
            bufsize += LSH_RL_BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if (!buffer)
            {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

char **lsh_split_line(char *line)
{
    int bufsize = LSH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char *)); // allocating memory blocks for array of string
    char *token;

    if (!tokens)
    {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    char *linePtr = malloc(strlen(line) + 1);
    strcpy(linePtr, line);

    token = strtok(line, LSH_TOK_DELIM); // returns the first substring till it hits a delimter, replaces the delimiter with '\0' in memory
    while (token != NULL)
    {
        tokens[position] = token;
        position++;

        if (position >= bufsize)
        {
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;

    if (tokens[0] != NULL)
    {
        if (history == NULL)
        {
            char **temp = realloc(history, historyBuffSize * sizeof(char *));
            if (!temp)
            {
                perror("lsh");
                return tokens;
            }
            else
            {
                history = temp;
            }
        }
        history[historyPos] = linePtr;
        historyPos++;
        if (historyPos >= historyBuffSize)
        {
            historyBuffSize += LSH_TOK_BUFSIZE;
            char **temp = realloc(history, historyBuffSize * sizeof(char *));
            if (!temp)
            {
                perror("lsh");
            }
            else
            {
                history = temp;
            }
        }
        history[historyPos] = NULL;
    }

    return tokens;
}

int lsh_num_args(char **args)
{
    int num = 0;
    int i;
    for (i = 0; args[i] != NULL; i++)
    {
        num++;
    }
    return num;
}

int lsh_launch(char **args)
{
    pid_t pid, wpid;
    int status;

    pid = fork(); // after fork(), a child process is created and both the processes follow instructions after this line
    // fork() returns a positive value(pid) or -1(failure) to the parent
    // fork() returns 0 to the child
    if (pid == 0)
    {
        // Child process
        if (execvp(args[0], args) == -1)
        {
            // execvp() takes a program name and argument list, replaces the current process with that program, and never returns if successful. If it fails, it returns -1 and sets errno.
            perror("lsh");
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        // Error forking
        perror("lsh");
    }
    else
    {
        // Parent process
        do
        {
            wpid = waitpid(pid, &status, WUNTRACED);
            // Hey shell, pause and wait until the child process whose PID is pid finishes or stops.
            // Store its result in status
        } while (!WIFEXITED(status) && !WIFSIGNALED(status)); // keeps the shell waiting until the child process either exits normally or is terminated by a signal. Cause waitpid() may return multiple times for different child states,
    }
    return 1;
}

// Function declarations for builtin shell commands
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_say(char **args);
int lsh_history();

// List of builtin commands followed by their corresponding functions
char *builtin_str[] = {
    "cd",
    "help",
    "exit",
    "say",
    "history"};

int (*builtin_func[])(char **) = {
    &lsh_cd,
    &lsh_help,
    &lsh_exit,
    &lsh_say,
    &lsh_history};

int lsh_num_builtins()
{
    return sizeof(builtin_str) / sizeof(char *);
}

// Builtin function implementations
int lsh_cd(char **args)
{
    if (args[1] == NULL)
    {
        fprintf(stderr, "lsh: expected argument to \"cd\"\n");
    }
    else
    {
        if (chdir(args[1]) != 0)
        {
            perror("lsh");
        }
    }
    return 1;
}

int lsh_help(char **args)
{
    int i;
    printf("KJ's LSH\n");
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");

    for (i = 0; i < lsh_num_builtins(); i++)
    {
        printf(" %s\n", builtin_str[i]);
    }

    printf("Use the man command for information on other programs.\n");
    return 1;
}

int lsh_exit(char **args)
{
    return 0;
}

int lsh_say(char **args)
{
    if (args[1] == NULL)
    {
        fprintf(stderr, "lsh: expected argument to \"say\"");
    }
    else
    {
        int num = lsh_num_args(args);
        size_t size = (num - 2) + 1;
        int i;
        for (i = 1; i < num; i++)
        {
            size += strlen(args[i]);
        }
        char *res = malloc(size);
        if (!res)
        {
            perror("malloc");
            return 1;
        }
        strcpy(res, args[1]);
        i = 2;
        while (args[i] != NULL)
        {
            strcat(res, " ");
            strcat(res, args[i]);
            i++;
        }
        if (write(STDOUT_FILENO, res, strlen(res)) == -1)
        {
            perror("lsh");
        }
        free(res);
    }
    printf("\n");
    return 1;
}

int lsh_history()
{
    if (history == NULL)
    {
        return 1;
    }
    int i = 0;
    while (history[i + 1] != NULL)
    {
        printf("%s\n", history[i]);
        i++;
    }
    return 1;
}

int lsh_execute(char **args)
{
    int i;

    if (args[0] == NULL)
    {
        return 1;
    }

    for (i = 0; i < lsh_num_builtins(); i++)
    {
        if (strcmp(args[0], builtin_str[i]) == 0)
        {
            return (*builtin_func[i])(args);
        }
    }
    return lsh_launch(args);
}

void lsh_loop(void) // (void) is important to not allow passing any arguments, blank might allow passing
{
    char *line;  // pointer to a char -> an array of char -> string
    char **args; // pointer to a char* -> an array of (array of char) -> array of string
    int status;

    do
    {
        printf("> ");
        line = lsh_read_line();
        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
    } while (status);
}

int main(int argc, char **argv)
{
    enableRawMode();
    lsh_loop();
    return EXIT_SUCCESS;
}

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Splits inputs for pipes. Need to free contents of output array.
char **tokenize_pipes(char *input) {
    unsigned int i;
    int quote = 0;
    int num_pipes = 0;
    char *substr;
    
    for (i = 0; i < strlen(input); i++) {
        if (quote) {
            if (input[i] == '"') {
                quote = 0;
            }
        }
        else {
            if (input[i] == '|') {
                num_pipes++;
            }
            else if (input[i] == '"') {
                quote = 1;
            }
        }
    }
    
    char** tokens = calloc(num_pipes + 2, sizeof(char *));
    if (!tokens) {
        fprintf(stderr, "malloc failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    quote = 0;
    int tok_start = 0;
    int count = 0;
    
    for (i = 0; i < strlen(input); i++) {
        if (input[i] == '|' && !quote) {
            substr = calloc(i - tok_start + 1, sizeof(char));
            if (!substr) {
                fprintf(stderr, "malloc failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            
            strncpy(substr, input + tok_start, i - tok_start);
            substr[i - tok_start] = '\0';
            
            tokens[count] = substr;
            
            tok_start = i + 1;
            count++;
        }
        else if (input[i] == '"') {
            quote = !quote;
        }
    }
    
    substr = calloc(i - tok_start + 1, sizeof(char));
    if (!substr) {
        fprintf(stderr, "malloc failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    strncpy(substr, input + tok_start, i - tok_start);
    substr[i - tok_start] = '\0';
    
    tokens[count] = substr;
    tokens[num_pipes + 1] = NULL;
    
    return tokens;
}

// Cleans up an input and tokenizes it into an array.
char **tokenize(char *input) {
    unsigned int i;
    int new_index = 0;
    int quote = 0;
    int num_tokens = 0;
    char *substr;
    
    for (i = 0; i < strlen(input); i++) {
        if (quote) {
            if (input[i] == '"') {
                quote = 0;
            }
            input[new_index] = input[i];
            new_index++;
        }
        else {
            if (input[i] == ' ' || input[i] == 10 || input[i] == '\t') {
                if (i > 0 && !(input[i - 1] == ' ' || input[i - 1] == 10 || 
                    input[i - 1] == '\t')) {
                    input[new_index] = ' ';
                    new_index++;
                }
            }
            else {
                if (i == 0 || input[i - 1] == ' ') {
                    num_tokens++;
                }
                if (input[i] == '"') {
                    quote = 1;
                }
                input[new_index] = input[i];
                new_index++;
            }
        }
    }
    if (input[new_index - 1] == ' ') {
        input[new_index - 1] = '\0';
    }
    else {
        input[new_index] = '\0';
    }
    
    char** tokens = calloc(num_tokens + 1, sizeof(char *));
    if (!tokens) {
        fprintf(stderr, "malloc failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    quote = 0;
    int tok_start = 0;
    int count = 0;
    
    for (i = 0; i < strlen(input); i++) {
        if (input[i] == ' ' && !quote) {
            substr = calloc(i - tok_start + 1, sizeof(char));
            if (!substr) {
                fprintf(stderr, "malloc failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            
            if (input[tok_start] != '"') {
                strncpy(substr, input + tok_start, i - tok_start);
                substr[i - tok_start] = '\0';
            }
            else {
                strncpy(substr, input + tok_start + 1, i - tok_start - 2);
                substr[i - tok_start - 2] = '\0';
            }
            
            tokens[count] = substr;
            
            tok_start = i + 1;
            count++;
        }
        else if (input[i] == '"') {
            quote = !quote;
        }
    }
    
    substr = calloc(i - tok_start + 1, sizeof(char));
    if (!substr) {
        fprintf(stderr, "malloc failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if (input[tok_start] != '"') {
        strncpy(substr, input + tok_start, i - tok_start);
        substr[i - tok_start] = '\0';
    }
    else {
        strncpy(substr, input + tok_start + 1, i - tok_start - 2);
        substr[i - tok_start - 2] = '\0';
    }
    tokens[count] = substr;
    tokens[num_tokens] = NULL;
    
    return tokens;
}

// Changes directory.
void exec_cd(char **tokens) {
    int stat = 0;
    
    if (tokens[1]) {
        if (strcmp(tokens[1], "~") == 0) {
            stat = chdir(getenv("HOME"));
        }
        else
        {
            stat = chdir(tokens[1]);
        }
    }
    else {
        stat = chdir(getenv("HOME"));
    }
    if (stat) {
        fprintf(stderr, "chdir failed: %s\n", strerror(errno));
    }
    return;
}

// Splits inputs for redirections. Need to free contents of output array.
char **tokenize_redirects(char *input) {
    char** tokens = calloc(3, sizeof(char *));
    if (!tokens) {
        fprintf(stderr, "malloc failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    int quote = 0;
    int input_idx = -1;
    int output_idx = -1;
    unsigned int i;
    char *temp;
    
    // Find the redirect signs
    for (i = 0; i < strlen(input); i++) {
        if (!quote) {
            if (input[i] == '<') {
                input_idx = i;
            }
            else if (input[i] == '>') {
                output_idx = i;
            }
            else if (input[i] == '"') {
                quote = 1;
            }
        }
        else {
            if (input[i] == '"') {
                quote = 0;
            }
        }
    }
    
    // Create the new substrings, if necessary.
    if (input_idx != -1) {
        if (output_idx != -1 && output_idx - input_idx > 0) {
            temp = calloc(output_idx - input_idx - 1, sizeof(char));
            if (!temp) {
                fprintf(stderr, "malloc failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            strncpy(temp, input + input_idx + 1, output_idx - input_idx - 1);
            temp[output_idx - input_idx - 1] = '\0';
            tokens[1] = temp;
        }
        else {
            temp = calloc(strlen(input) - input_idx - 1, sizeof(char));
            if (!temp) {
                fprintf(stderr, "malloc failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            strncpy(temp, input + input_idx + 1, strlen(input) - input_idx - 1);
            temp[strlen(input) - input_idx - 1] = '\0';
            tokens[1] = temp;
        }
    }
    else {
        tokens[1] = NULL;
    }
    
    if (output_idx != -1) {
        if (input_idx != -1 && input_idx - output_idx > 0) {
            temp = calloc(input_idx - output_idx - 1, sizeof(char));
            if (!temp) {
                fprintf(stderr, "malloc failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            strncpy(temp, input + output_idx + 1, input_idx - output_idx - 1);
            temp[input_idx - output_idx - 1] = '\0';
            tokens[2] = temp;
        }
        else {
            temp = calloc(strlen(input) - output_idx - 1, sizeof(char));
            if (!temp) {
                fprintf(stderr, "malloc failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            strncpy(temp, input + output_idx + 1, strlen(input) - output_idx - 1);
            temp[strlen(input) - output_idx - 1] = '\0';
            tokens[2] = temp;
        }
    }
    else {
        tokens[2] = NULL;
    }
    
    // Get actual command
    int lim;
    if (input_idx != -1) {
        if (output_idx != -1) {
            lim = input_idx < output_idx ? input_idx : output_idx;
        }
        else {
            lim = input_idx;
        }
    }
    else {
        if (output_idx != -1) {
            lim = output_idx;
        }
        else {
            lim = strlen(input);
        }
    }
    
    temp = calloc(lim + 1, sizeof(char));
    if (!temp) {
        fprintf(stderr, "malloc failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    strncpy(temp, input, lim);
    temp[lim] = '\0';
    tokens[0] = temp;
    
    return tokens;
}

// Invokes a command, assuming it has no pipes.
int invoke(char *input, int will_fork) {
    // First, we want to determine whether we need any redirects.
    char **files = tokenize_redirects(input);
    unsigned int i;
    char *infile = NULL;
    char *outfile = NULL;
    int status;
    
    // Clean up the array contents.
    char **tokens = tokenize(files[0]);
    char **infiles;
    if (files[1]) {
        infiles = tokenize(files[1]);
        infile = infiles[0];
    }
    char **outfiles;
    if (files[2]) {
        outfiles = tokenize(files[2]);
        outfile = outfiles[0];
    }

    if (strcmp(tokens[0], "cd") == 0 || strcmp(tokens[0], "chdir") == 0) {
        exec_cd(tokens);
    }
    else if (strcmp(tokens[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    }
    else {
        if (will_fork) {
            pid_t pid = fork();
            if (pid == -1) {
                fprintf(stderr, "fork failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            else if (pid == 0) {
                if (infile) {
                    int in_fd = open(infile, O_RDONLY);
                    dup2(in_fd, STDIN_FILENO);
                    close(in_fd);
                }
                if (outfile) {
                    int out_fd = open(outfile, O_CREAT | O_TRUNC | O_WRONLY,
                                      S_IRUSR | S_IWUSR);
                    dup2(out_fd, STDOUT_FILENO);
                    close(out_fd);
                }
                if (execvp(tokens[0], tokens) == -1) {
                    fprintf(stderr, "execvp failed: %s\n", strerror(errno));
                }
            }
            else {
                wait(&status);
            }
        }
        else {
            if (infile) {
                int in_fd = open(infile, O_RDONLY);
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            if (outfile) {
                int out_fd = open(outfile, O_CREAT | O_TRUNC | O_WRONLY, 
                                  S_IRUSR | S_IWUSR);
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }
            if (execvp(tokens[0], tokens) == -1) {
                fprintf(stderr, "execvp failed: %s\n", strerror(errno));
            }
        }
    }
    
    for (i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    free(tokens);
    
    if (files[1]) {
        for (i = 0; infiles[i] != NULL; i++) {
            free(infiles[i]);
        }
        free(infiles);
    }
    
    if (files[2]) {
        for (i = 0; outfiles[i] != NULL; i++) {
            free(outfiles[i]);
        }
        free(outfiles);
    }
    
    for (i = 0; i < 3; i++) {
        if (files[i]) {
            free(files[i]);
        }
    }
    free(files);
    
    return 0;
}

// Invokes a command, assuming it has pipes.
int pipe_invoke(char *input) {
    int i;
    int filedes[2];
    char **pipe_tokens = NULL;
    int status;
    int num_cmds = 0;
    
    pipe_tokens = tokenize_pipes(input);
    for (i = 0; pipe_tokens[i] != NULL; i++) {
        num_cmds++;
    }
    
    if (num_cmds == 1) {
        return invoke(pipe_tokens[0], 1);
    }
    
    int allfiledes[num_cmds - 1][2];
    // Make file descriptor array
    
    for (i = 0; i < num_cmds - 1; i++){
        pipe(filedes);
        allfiledes[i][0] = filedes[0];
        allfiledes[i][1] = filedes[1];
    }
    
    // Run all piped processes
    pid_t pid;
    for (i = 0; i < num_cmds; i++) {
        pid = fork();
        
        if (pid == -1) {
            fprintf(stderr, "fork failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        // If it's the child, change read/write locations and invoke the process
        else if (pid == 0) {
            // If it's not the first process, change the read location
            if (i > 0) {
                close(allfiledes[i - 1][1]);
                dup2(allfiledes[i - 1][0], STDIN_FILENO);
                close(allfiledes[i - 1][0]);
            }
            // If it's not the last process, change the write location
            if (i < num_cmds - 1) {
                close(allfiledes[i][0]);
                dup2(allfiledes[i][1], STDOUT_FILENO);
                close(allfiledes[i][1]);
            }
            invoke(pipe_tokens[i], 0);
            // Should never get here cause execvp will be called
            exit(EXIT_FAILURE);
        }
        else {
            // Close the file descriptors we've used already
            if(i > 0) {
                close(allfiledes[i-1][1]);
                close(allfiledes[i-1][0]);
            }
            continue;
        }
    }

    // Wait for all children to finish (only main process should ever
    // get here)
    assert(pid != 0);
    for(i = 0; i < num_cmds; i++) {
        wait(&status);
    }
    
    return 0;
}

int main() {
    while(1) {
        char *uname = getlogin();
        if (!uname) {
            fprintf(stderr, "getlogin failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        char *cwd = getcwd(0, 0);
        if (!cwd) {
            fprintf(stderr, "getcwd failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        fflush(stdout);
        fprintf(stdout, "%s:%s> ", uname, cwd);
        char buf[1024];
        fgets(buf, 1024, stdin);
        int status = pipe_invoke(buf);
        if (status) {
            fprintf(stderr, "something failed. \n");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

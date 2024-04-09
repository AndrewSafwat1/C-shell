#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/limits.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_WORDS 100
#define MAX_WORD_LENGTH 100
#define CHILD_ID 0
#define SHELL_BUILTIN 0
#define EXECUTABLE_OR_ERROR 1
#define CD 'c'
#define ECHO 'e'
#define EXPORT 'x'
#define FIRST_WORD 0
#define SECOND_WORD 1
#define FIRST_LETTER 0
#define EXIT_TERMINAL 'q'
#define FOREGROUND 0
#define BACKGROUND 1


void shell (void);
void setup_environment();
void execute_shell_bultin(char words[MAX_WORDS][MAX_WORD_LENGTH], int word_count, char command_type);
void execute_command(char words[MAX_WORDS][MAX_WORD_LENGTH], int word_count, char command_type, int status);
int evaluate_expression(char command_type);
char get_command_type(char words[MAX_WORDS][MAX_WORD_LENGTH], int word_count);
bool are_equal(char word1[], char word2[]);
int check_status(char words[][MAX_WORD_LENGTH], int word_count);
void reap_child_zombie();
void on_child_exit();
void write_to_log_file(const char *message);
void reset_color ();
void green_color ();

char export_value[MAX_WORD_LENGTH];
char export_variable[MAX_WORD_LENGTH];
int index_of_variable = 0;

int main(void){
    signal(SIGCHLD, on_child_exit);
    setup_environment();
    shell();
    return 0;
}

void on_child_exit() {
    // Call the reap_child_zombie() function to handle zombie child processes
    reap_child_zombie();

    // Write a message to the log file
    write_to_log_file("Child terminated");
}

void reap_child_zombie() {
    int status;
    pid_t pid;
    
    pid = wait(&status);
    if (pid == -1) {
    } else {
        if (WIFEXITED(status)) {
            WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            WTERMSIG(status);
        }
    }
}

void write_to_log_file(const char *message) {
    FILE *log_file = fopen("logfile.txt", "a");
    if (log_file == NULL) {
        return;
    }
    fprintf(log_file, "%s\n", message);
    fclose(log_file);
}

void setup_environment(){
    char *homeDir = getenv("HOME");
    chdir(homeDir);
}


void shell(void){
    char command_type;
    do{
        char cwd[PATH_MAX];
        getcwd(cwd, sizeof(cwd));
        printf(":");
        green_color();
        printf("%s", cwd);
        reset_color();
        printf("$ ");

        char line[MAX_WORD_LENGTH * MAX_WORDS]; // Maximum line length
        char words[MAX_WORDS][MAX_WORD_LENGTH]; // Array of character arrays to store words
        int word_count = 0; // Counter for the number of words

        fgets(line, sizeof(line), stdin); // Read the whole line
        
        // Parse the line and split it into words
        char *token = strtok(line, " \n");
        while (token != NULL && word_count < MAX_WORDS) {
            strcpy(words[word_count], token); // Copy the word into the array of words
            word_count++;
            token = strtok(NULL, " \n");
        }

        bool export = false;
        for(int i = 0 ; i < word_count; i++){
            if(words[i][FIRST_LETTER] == '$'){
                for(int k = 1; words[i][k] !='\0' && k-1 < index_of_variable; k++){
                    if(words[i][k] != export_variable[k-1])
                        break;
                    if(k-1 == index_of_variable - 1)
                        export = true;
                }
                if(export){
                    export = false;
                    strcpy(words[i], export_value);
                }
            }
        }


        command_type = get_command_type(words, word_count);
        int input_type = evaluate_expression(command_type);
        int status = check_status(words, word_count);

        switch(input_type){
            case SHELL_BUILTIN:
                execute_shell_bultin(words, word_count, command_type);
                break;
            case EXECUTABLE_OR_ERROR:
                execute_command(words, word_count, command_type, status);
                break;
        }

        printf("\n");
    }
    while (command_type != EXIT_TERMINAL);
}


void execute_shell_bultin(char words[][MAX_WORD_LENGTH], int word_count, char command_type){
    bool export = false;
    int index = 0;
    switch(command_type){
        case CD:
            if(word_count == 1 || word_count == 2 && (are_equal(words[SECOND_WORD], "./") || are_equal(words[SECOND_WORD], ".")))
                return;
            else if (word_count == 2){
                chdir(words[SECOND_WORD]);
            }
            else if (word_count > 2)
                printf("too many arguments");
            break;
        case ECHO:
            for(int i = 1; i < word_count; i++){
                for(int j = 0; words[i][j] != '\0'; j++){
                    if(words[i][j] == '"')
                        continue;
                    else if (words[i][j] == '$'){
                        for(int k = j+1; words[i][k] != '\0' && k-j-1 < index_of_variable; k++){
                            if(words[i][k] != export_variable[k-j-1])
                                break;
                            if(k-j-1 == index_of_variable - 1)
                                export = true;
                        }
                        continue;
                    } else if (export){
                        for(int k = 0 ; export_value[k] != '\0'; k++)
                            printf("%c", export_value[k]);
                        j+=(index_of_variable-1);
                        export = false;
                        continue;
                    }
                    printf("%c", words[i][j]); 
                }
            }
            break;
        case EXPORT:
            for(; words[SECOND_WORD][index] != '='; index++){
                export_variable[index] = words[SECOND_WORD][index];
            }
            index_of_variable = index;
            export_variable[index] = '\0';
            index ++;
            int temp = index;
            if(words[SECOND_WORD][index] == '"'){
                index++;
                temp++;
                for(;words[SECOND_WORD][index] != '\0' && words[SECOND_WORD][index] != '"'; index++){
                    export_value[index-temp] = words[SECOND_WORD][index];
                }
                export_value[index-temp] = '\0';
            }else {
                for(; words[SECOND_WORD][index] != '\0'; index++){
                    export_value[index-temp] = words[SECOND_WORD][index];
                }
                export_value[index-temp] = '\0';
            }

            break;
    }
}


void execute_command(char words[][MAX_WORD_LENGTH], int word_count, char command_type, int status){
    char **argument_list = malloc((word_count+1) * sizeof(char*));
    argument_list[word_count] = NULL;

    for(int i = 0; i < word_count; i++) {
        argument_list[i] = malloc(MAX_WORD_LENGTH * sizeof(char));
        strcpy(argument_list[i], words[i]);
    }
    
    if(status == BACKGROUND){
        for(int i = 1; i < word_count; i++){
            if(are_equal(argument_list[i], "&")){
                for(int j = i ; j < word_count-1; j++){
                    strcpy(argument_list[j], argument_list[j+1]);
                }
                argument_list[word_count-1] = NULL;
                break;
            }
        }
    }

    int id = fork();

    if(id != CHILD_ID && status == FOREGROUND)
        wait(NULL);

    if (id == CHILD_ID){
        execvp(words[FIRST_WORD], argument_list);
        printf("Command not found");
        exit(0);
    }

    // Free allocated memory
    for (int i = 0; argument_list[i] != NULL; i++) {
        free(argument_list[i]);
    }
    free(argument_list);
}

int evaluate_expression(char command_type){
    if(command_type == EXIT_TERMINAL || command_type == ECHO || command_type == CD || command_type == EXPORT){
        return SHELL_BUILTIN;
    } else {
        return EXECUTABLE_OR_ERROR;
    }
    return 0;
}

char get_command_type(char words[][MAX_WORD_LENGTH], int word_count){
    if(are_equal(words[FIRST_WORD], "cd"))
        return CD;
    else if(are_equal(words[FIRST_WORD], "echo"))
        return ECHO;
    else if(are_equal(words[FIRST_WORD], "export"))
        return EXPORT;
    else if(are_equal(words[FIRST_WORD], "exit"))
        return EXIT_TERMINAL;
}

bool are_equal(char word1[], char word2[]){
    return strcmp(word1, word2) == 0;
}

int check_status(char words[][MAX_WORD_LENGTH], int word_count){
    for(int i = 0 ; i < word_count; i++){
        if(are_equal(words[i], "&"))
            return BACKGROUND;
    }
    return FOREGROUND;
}

void green_color () {
  printf("\033[0;32m");
  
}

void reset_color () {
  printf("\033[0m");
}
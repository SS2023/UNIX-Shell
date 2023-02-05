#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <sys/types.h>
#include <fcntl.h>

// Memory is dynamically allocated to user commands and arguments through standard input or from a file (SH_RL_BUFSIZE)
// Memory is dynamically allocated to tokenized sections of user inputs (SH_TOK_BUFSIZE)
// Current path is stored and updated every shell loop (MAX_BUF)
// Tokenizing is based on (SH_TOK_DELIM)
#define SH_RL_BUFSIZE 1024
#define MAX_BUF 200
#define SH_TOK_BUFSIZE 64
#define SH_TOK_DELIM " \t\r\n\a"

// Function prototypes
char *sh_read_line(void);
char **sh_split_line(char *line);
int sh_launch(char **args);
int sh_execute(char **args);
void sh_loop(void);
void get_home_dir(void);
int my_read(void);

char current_path[MAX_BUF]; 	// Updates every shell loop and keeps the current directory
char *home_dir = NULL; 		// Stores the Home directory of the user

int main(int argc, char const *argv[])
{
	get_home_dir(); 
	sh_loop();

  	return 0;
}

/**
 * This function finds out the home directory of the user and store it in home_dir
 */
void get_home_dir(void)
{
  	home_dir = getenv("HOME"); // Pull the environment variable "HOME"

  	// If the user has rename the home dir, read it from the system user database 
  	if (home_dir == NULL)
  	{
    		uid_t uid = getuid();
    		struct passwd *pw = getpwuid(uid);

    		if (pw == NULL)
    		{
      			printf("Failed\n");
      			exit(EXIT_FAILURE);
    		}

    		home_dir = pw->pw_dir;
  	}
}

/**
 * Home directory is replaced with '~'.
 */
char *str_replace(char *orig, char *rep, char *with)
{
  	char *result;  // the return string
  	char *ins;     // the next insert point
  	char *tmp;     // varies
  	int len_rep;   // length of rep (the string to remove)
  	int len_with;  // length of with (the string to replace rep with)
  	int len_front; // distance between rep and end of last rep
  	int count;     // number of replacements

   
  	// sanity checks and initialization
  	if (!orig || !rep)
	{
    		return NULL;
	}

  	len_rep = strlen(rep);

  	if (len_rep == 0)
	{
    		return NULL; // empty rep causes infinite loop during count
	}

  	if (!with)
	{
    		with = (char*)"";
	}

  	len_with = strlen(with);

   	// count the number of replacements needed
  	ins = orig;

  	for (count = 0; (tmp = strstr(ins, rep)); ++count)
  	{
    		ins = tmp + len_rep;
  	}

  	tmp = result = (char*)malloc(strlen(orig) + (len_with - len_rep) * count + 1);

  	if (!result)
	{
    		return NULL;
	}

  	
   	// first time through the loop, all the variable are set correctly, after that
  	// tmp points to the end of the result string points to the next occurrence of rep in orig
  	// points to the remainder of orig after "end of rep"
  	while (count--)
  	{
    		ins = strstr(orig, rep);
    		len_front = ins - orig;
    		tmp = strncpy(tmp, orig, len_front) + len_front;
    		tmp = strcpy(tmp, with) + len_with;
    		orig += len_front + len_rep; 	// move to next "end of rep"
  	}

  	strcpy(tmp, orig);
  	return result;
}

/**
 * sh_loop starts the shell process. Only exit command from the user returns from this function
 * For every shell command user inputs, the do-while loop runs an iteration
 */
void sh_loop(void)
{
  	char *line; 			// User input returns to the 'line'
  	char **args; 			// Pointers to the tokenized line returns to the 'args'.
  	int status; 			// Determines the exit condition from the do-while loop
  	char *ch = (char*)"$ "; 	// Use to append at the user prompt
  	chdir(home_dir); 		// Required to start the shell from the home directory of
                   			// the user. This change the current directory to the home directory
  	
  	 // Save the current file discriptors for the standard output and input so
  	 // they can be restore back to the terminal after redirecting is done.
  	int saved_stdout = dup(1); 
  	int saved_stdin = dup(0);

  	do
  	{
    		dup2(saved_stdout, 1); // Redirect the standard output back the terminal
    		dup2(saved_stdin, 0);  // Redirect the standard input back the terminal

    		memset(current_path, '\0', sizeof(char) * MAX_BUF);
    		getcwd(current_path, MAX_BUF); // Get the current path
    		char *temp = (char*)str_replace(current_path, home_dir, (char*)"~"); // Replace the home dir with '~'
    		strcpy(current_path,temp);
    		free(temp);
    		strncat(current_path, ch, 2); // Append '$' at the end of the user prompt

    		printf("1730sh:%s", current_path); // User prompt
    		fflush(stdout);
    		line = sh_read_line(); // Funtion to read the line and returns to the line
    		args = sh_split_line(line); // Function takes the line and tokenized and returns to thoes pointers of tokens
    		status = sh_execute(args);  // Take thoes ages and execute them.

    		free(line); // Free the line because memory is allocated dynamically
    		free(args); // Free the args because memory is allocated dynamically
  	} while (status);
}

/**
 * sh_read_line function read the input from the user
 */
char *sh_read_line(void)
{
  	
  	// memory alocation for storing the input from the user 
  	int bufsize = SH_RL_BUFSIZE;
  	int position = 0;
  	char *buffer = (char*)malloc(sizeof(char) * bufsize);
  	int c;

  	if (!buffer)
  	{
    		fprintf(stderr, "1730sh: allocation error\n");
    		exit(EXIT_FAILURE);
  	}

  	while (1)
  	{
    
    		c = my_read(); // Read a character
    		fflush(stdout);
    
    		if (c == EOF || c == '\n') // If EOF, replace it with a null character and return.
    		{
      			buffer[position] = '\0';
      			return buffer;
    		}

    		else
    		{
      			buffer[position] = c;
    		}

    		position++;
    
    		if (position >= bufsize) // If buffer is exceeded, reallocate.
    		{
      			bufsize += SH_RL_BUFSIZE;
      			buffer = (char*)realloc(buffer, bufsize);
      			if (!buffer)
      			{
        			fprintf(stderr, "1730sh: allocation error\n");
        			exit(EXIT_FAILURE);
      			}
    		}
  	}
}

/**
 * Read a charactor from the stdin
 */
int my_read(void)
{
  	static char buf[BUFSIZ];
  	static char *bufp = buf;
  	static int i = 0;

  	if (i == 0)
  	{
    		i = read(0, buf, 1);
    		bufp = buf;
  	}

  	if ( --i >= 0 )
  	{
    		return  *bufp++;
  	}

  	return EOF;
}

/**
 * sh_split_line function take the line and tokenize by strtok function
 */
char **sh_split_line(char *line)
{
  	
  	// memory allocation for stor the tokens
  	int bufsize = SH_TOK_BUFSIZE, position = 0;
  	char **tokens = (char**)malloc(bufsize * sizeof(char *));
  	char *token;

  	if (!tokens)
  	{
    		fprintf(stderr, "1730sh: allocation error\n");
    		exit(EXIT_FAILURE);
  	}

  	token = strtok(line, SH_TOK_DELIM); // Tokenize

  	while (token != NULL)
  	{
    		tokens[position] = token;
    		position++;

    		if (position >= bufsize)
    		{
      			bufsize += SH_TOK_BUFSIZE;
			
			// if buffer is exceeded, reallocate.
      			tokens = (char**)realloc(tokens, bufsize * sizeof(char *));
      			
			if (!tokens)
      			{
        			fprintf(stderr, "1730sh: allocation error\n");
        			exit(EXIT_FAILURE);
      			}
    		}

    		token = strtok(NULL, SH_TOK_DELIM);
  	}

  	tokens[position] = NULL;
  	return tokens;
}

/**
 * fork() and execvp() are used to duplicate existing process, then replacing that process with a new process
 * In args tokenized user input is present, The 1st token is taken as the command and others are taken as
 * arguments for that command
 */
int sh_launch(char **args)
{
  	pid_t pid;
  	int status;

  	pid = fork();

  	if (pid == 0)
  	{
    		// Child process
    		if (execvp(args[0], args) == -1)
    		{
      			perror("1730sh:");
    		}

    		exit(EXIT_FAILURE);
  	}

  	else if (pid < 0)
  	{
    		// Error forking
    		perror("1730sh:");
  	}

  	else
  	{
    		// Parent process
    		do
    		{
      			waitpid(pid, &status, WUNTRACED); // Wait for the child process to finish
    		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
  	}

  	return 1;
}


// change directory and exit command are inbuilt.
int lsh_cd(char **args);
int lsh_exit(char **args);


//builtin commands.
char *builtin_str[] = {
   	(char*)"cd",
    	(char*)"exit"};

int (*builtin_func[])(char **) = {
    	&lsh_cd,
    	&lsh_exit};

int lsh_num_builtins()
{
  	return sizeof(builtin_str) / sizeof(char *);
}

/**
* Builtin function implementations.
* Change directory is done by chdir function.
*/
int lsh_cd(char **args)
{
  	if (args[1] == NULL)
  	{
    		fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  	}

  	else
  	{
    		if (chdir(args[1]) != 0) // Changing the directory
    		{
      			perror("lsh");
    		}
  	}

  	getcwd(current_path, MAX_BUF);
  	return 1;
}

/**
 * exit command is returning zero to the status effectively terminating the loop.
 */
int lsh_exit(char **args)
{
  	return 0;
}

/**
 * inbuilt commands are called (cd and exit)
 * if those funtions are called with out going to the sh_lauch function it directly goes to
 * those implimentations. Then it checks for redirections (>,>>,<). If used the std 
 * input and outputs are changed and call sh_lauch after that. Then redirection occurs.
 * For every loop std input output restores back to the terminal.
 */
int sh_execute(char **args)
{
  	int i;

  	if (args[0] == NULL) // Checks for empty commands
  	{
    		// An empty command was entered.
    		return 1;
  	}

  	for (i = 0; i < lsh_num_builtins(); i++) // Checks for build in functions and get the return
  	{
    		if (strcmp(args[0], builtin_str[i]) == 0)
    		{
      			return (*builtin_func[i])(args);
    		}
  	}

  	
  	// Checks if the redirection has used and changed
  	i = 0;
  	while (args[i] != NULL) 
  	{
    		if (strcmp(args[i], ">") == 0)
    		{
      			int file = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0777); // Open the file for overwite
      			dup2(file, STDOUT_FILENO); // Change the std output to the opened file
      			close(file);
      			args[i] = NULL;
      			break;
    		}

    		if (strcmp(args[i], ">>") == 0)
    		{
      			int file = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0777); // Open the file for append
      			dup2(file, STDOUT_FILENO); // Change the std output to the opened file
      			close(file);
      			args[i] = NULL;
      			break;
    		}

    		if (strcmp(args[i], "<") == 0)
    		{
      			int file = open(args[i + 1], O_RDONLY, 0777); // Open the file containing inputs

      			if (file == -1)
      			{
        			fprintf(stderr, "1730sh: Input file not found!\n");
        			return 1;
      			}

      			dup2(file, STDIN_FILENO); // Change the std input to the opened file
      			close(file);
      			args[i] = NULL;
      			break;
    		}

    		i++;
  	}

  	return sh_launch(args); // Call the sh_launch function with the args
}


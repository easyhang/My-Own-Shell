#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>


#define MAX_SUB_COMMANDS	10
#define MAX_ARGS		10

struct SubCommand 
{
	char *line;
	char *argv[MAX_ARGS];
};

struct Command
{
	char *stdin_redirect;
	char *stdout_redirect;
	int background;
	struct SubCommand sub_commands[MAX_SUB_COMMANDS];
	int num_sub_commands;
};

void printArgs(char **argv)
{
        int i = 0;
        while(1)
        {
		if (!argv[i] || i >= MAX_ARGS) break;
                printf("argv[%d] = '%s'\n", i, argv[i]);
                i++;
        }
}

void readArgs(char *in, char **argv, int size)
{
        int i = 0;
        char *dup;
        char *token;
        dup = strdup(in);

        token = strtok(dup, " ");
        while (token != NULL)
        {
                argv[i] = token;
                token = strtok(NULL, " ");
                i++;

                if (i >= size)
                break;
        }
        argv[i] = NULL;
}


void readCommand(char *line, struct Command *command)
{
	char *dup ;
	dup = strdup(line);
	dup[strlen(dup)-1] = 0;
	
	// Allocate lines
	int i = 0;
	char *token;
	token = strtok (dup, "|");
	while(1)
	{
		if (token == NULL) break;
		command->sub_commands[i].line = token;
		token = strtok(NULL, "|");
		i++;
	}

	// Store the number of lines
	command->num_sub_commands = i-1;

	// Allocate argvs
	int j;
	for (j=0; j<i; j++)
	readArgs(command->sub_commands[j].line, 
		 command->sub_commands[j].argv, MAX_ARGS);
}

void readRedirectsAndBackground(struct Command *command)
{
	// read input file name
	int i = 0,k = command->num_sub_commands;
	while(1)
	{
		if (strcmp(command->sub_commands[k].argv[i] , "<") == 0)
		{
			command->stdin_redirect = command->sub_commands[k].argv[i+1];
			while(command->sub_commands[k].argv[i+2])
			{
				command->sub_commands[k].argv[i] = command->sub_commands[k].argv[i+2];
	                	i++;
			}
			command->sub_commands[k].argv[i] = NULL;
		}
		i++;
		if (command->sub_commands[k].argv[i] == NULL) break;
	}
	
	// read output file name
	int j = 0;
	while(1)
        {
                if (strcmp(command->sub_commands[k].argv[j] , ">") == 0)
                {
                        command->stdout_redirect = command->sub_commands[k].argv[j+1];
                        while(command->sub_commands[k].argv[j+2])
                        {
                                command->sub_commands[k].argv[j] = command->sub_commands[k].argv[j+2];
                                j++;
                        }
                        command->sub_commands[k].argv[j] = NULL;
                }
                else j++;
                if (command->sub_commands[k].argv[j] == NULL) break;
        }

	//read the background flag
	if (strcmp(command->sub_commands[k].argv[j-1], "&") == 0) 
	{
		command->background = 1;
		command->sub_commands[k].argv[j-1] = 0;

	}
	else command->background = 0;
}

int executeSubcommands(struct Command *command, int index)
{
	execvp(command->sub_commands[index].argv[0],command->sub_commands[index].argv);
        fprintf(stderr, "%s: Command not found\n", command->sub_commands[index].argv[0]);
	return -1;
}
	

void executeCommands()
{
	char s[1024];
	int err = 0;	
	int background = 0;
	
	while(1)
	{
		struct Command *command =(struct Command *) malloc(sizeof(struct Command)) ;
		
		printf("$ ");
	        fgets(s, sizeof s, stdin);
	
		readCommand(s, command);
		if (command->sub_commands[0].argv[0] == NULL) 
		{
			if (background == 0) continue;
			else 
			{
				printf("[%d]   finished\n", background);
				background = 0;
				continue;
			}
		}
		readRedirectsAndBackground(command);

		if (background != 0)
		{
			printf("[%d]  finished\n", background);
			background = 0;
		}

		int num = command->num_sub_commands + 1;
        	int ret[num];
        	int i = 0;
		
		if (num == 1)
		{
			ret[i] = fork();
                	if (ret[i] < 0) printf("fork child[%d] failed\n", i+1);

                	if (ret[i] == 0)
                	{
				// if < is detected, redirect the input
                                if (command->stdin_redirect != NULL)
                                {
                                	close(0);
                                        int fd;
                                        fd = open(command->stdin_redirect, O_RDONLY);
                                        if (fd < 0)
                                        {
                                        	fprintf(stderr, "%s: File not found\n", command->stdin_redirect);
						exit(1);
                                        }
                                }
				
				// if ">" is detected, redirect the output
                                if (command->stdout_redirect != NULL)
                                {
                                	close(1);
                                        int fd;
                                        fd = open(command->stdout_redirect, O_WRONLY | O_CREAT, 0660 );
                                        if (fd < 0)
                                        {
                                        	fprintf(stderr, "%s: Cannot create file\n", command->stdout_redirect);
						exit(1);
                                        }
                                }
				err = executeSubcommands(command, i);
                	}

                	if (ret[i] > 0)
                	{
                       		if (command->background == 0)
                        	waitpid(ret[i], NULL, WUNTRACED);
				else
				{
					printf("[%d]\n", ret[0]);
					background = ret[0];
				}
                	}
		}

        	if (num > 1)
        	{
			//create pipes
			int fds[num][2];
                	int j = 0;
                	for (j = 0; j < num-1; j++)
                	{
                 		pipe(fds[j]);
                	}
			
			while(1)
			{
				if (i == 0 ) ret[i] = fork();
				if (ret[i] < 0) printf("fork child[%d] failed\n", i+1);

				if (ret[i] == 0)
				{
					if (i == 0)
					{
						// if < is detected, redirect the input
						if (command->stdin_redirect)
						{
							close(0);
							int fd;
							fd = open(command->stdin_redirect, O_RDONLY, 0660 );
							if (fd < 0)
							{
								fprintf(stderr, "%s: File not found\n", command->stdin_redirect);
								exit(1);
							}
						}

						//redirect output
                                        	close(fds[i][0]);
                                        	dup2(fds[i][1],1);
						close(fds[i][1]);
						j = 1;
						while(1)
						{
							close(fds[j][0]);	
							close(fds[j][1]);
							j++;
							if (j >= num-1) break;	
						}
					}
			
					if (i > 0 && i < num-1)
					{
						//redirect input
						dup2(fds[i-1][0],0);
						close(fds[i][0]);

						//redirect output
                                                dup2(fds[i][1],1);
                                                close(fds[i][1]);

						for (j=0; j<num-1; j++) 
						{
							if (j!=i || j!=i-1)
							{
								close(fds[j][0]);
								close(fds[j][1]);
							}
						}
					}
					
					if (i == num-1 )
					{
						// if ">" is detected, redirect the output
                                                if (command->stdout_redirect)
                                                {
                                                        close(1);
                                                        int fd;
                                                        fd = open(command->stdout_redirect, O_WRONLY | O_CREAT, 0660 );
                                                        if (fd < 0)
                                                        {
                                                                fprintf(stderr, "%s: Cannot create file\n", command->stdout_redirect);
								exit(1);
                                                        }
                                                }

						//redirect input
                                      		close(fds[i-1][1]);
					  	dup2(fds[i-1][0],0);
                                        	close(fds[i-1][0]);
						j = 0;
						while(1)
						{
							close(fds[j][0]);
                                                        close(fds[j][1]);
                                                        j++;
                                                        if (j >= num-1 ) break;
						}
					}
					err = executeSubcommands(command, i);
					if (err == -1) break;
				}
		
				if (ret[i] > 0)
				{
					int k;	
					for(k = 0; k < i; k++)	close(fds[k][1]);
		 			if (command->background == 0)
					waitpid(ret[i], NULL, WUNTRACED);
					i++;
					if (i >= num) 
					{
						if (command->background == 1)
						{
							printf("[%d]\n", ret[i-1]);
							background = ret[i-1];
						}
						break;
					}
					ret[i] = fork();
				}
			}
		}
		if (err == -1)	break;
	}
}	
	
int main()
{
//	struct Command *command =(struct Command *) malloc(sizeof(struct Command)) ;
	executeCommands();
	return 0;
}	

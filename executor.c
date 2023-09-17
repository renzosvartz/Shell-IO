/* Renzo Moron-Svartz */
/* UID: 114145086 */
/* grace id: renzosvartz */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>    /* fork(), getpid(), getppid(), chdir */
#include <sys/types.h> /* pid_t */
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>  /* wait() */
#include <sysexits.h>
#include <err.h>
#include "command.h"
#include "executor.h"

#define SUCCESS 0
#define FAILURE -1
#define OUT_TO 0
#define IN_FROM 1
#define YES 1
#define NO 0
#define CHECK -2

static void print_tree(struct tree *t);
static int nested_pipe(int, int);
static int is_nested(int);
static void print_node(struct tree *t);

/* Executes the commands in the tree created by the lexer and parser,
 * handling I/O redirection, piping, and && */
int execute(struct tree *t) {
	pid_t cmd_pid, subshell_pid, left_pid, right_pid;
        int status, input, output, pipe_fd[2];

	/*fprintf(stderr, "tree\n");	
	print_tree(t);

	fprintf(stderr, "node\n");
	print_node(t);*/

	/* Process the tree based on the conjunction*/
	if (strcmp(conj[t->conjunction], "err") == 0) {
		
		/*fprintf(stderr, "\t\t\tPID %d is processing an argument %s\n", getpid(), t->argv[0]);*/

		if (strcmp(t->argv[0], "exit") == 0) { /* exit */
                        exit(0);
        	}
        	else if (strcmp(t->argv[0], "cd") == 0) /* cd */
        	{
                	if (t->argv[1] == NULL) { /* directory not present, chdir to HOME */
                       		chdir(getenv("HOME"));
                	}
                	else { /* error catching */
                        	if (chdir(t->argv[1]) == -1) {
                                	perror(t->argv[1]);
                        	}
                	}
        	}
        	else { /* CMD */

			/* fork to execute cmd */
			if ((cmd_pid = fork()) < 0) {
                                err(EX_OSERR, "fork error1\n");
                        }

                        if (cmd_pid == 0) { /* Child to exec */

				/*fprintf(stderr, "\t\t\tPPID's %d Child PID %d is exec %s\n", getppid(), getpid(), t->argv[0]);*/

				/* Redirect I/O */
				if (t->input) {
					/*fprintf(stderr, "Redir. I\n");*/
                               		input = open(t->input, O_RDONLY);	
					if (dup2(input, STDIN_FILENO) < 0) {
                                        	err(EX_OSERR, "dup2 error1\n");
                                	}
                        	}
				if (t->output) {
					/*fprintf(stderr, "Redir. O\n");*/
                       			output = open(t->output, O_WRONLY);
                                        if (dup2(output, STDOUT_FILENO) < 0) {
                                                err(EX_OSERR, "dup2 error2\n");
                                        }
                                }
			
				/* Execute command */
                                execvp(t->argv[0], t->argv);
                                fprintf(stderr, "Failed to execute %s\n", t->argv[0]);
				exit(EX_OSERR);
                        }
                        else { /* parent process waits */
                        
				/*fprintf(stderr, "\t\t\tPID %d created a child %d to exec %s\n", getpid(), cmd_pid, t->argv[0]);*/

			       	wait(&status);
				
				if (WIFEXITED(status) && !WEXITSTATUS(status)) {
					/*fprintf(stderr, "\t\t\tPID %d's exec %s worked\n", cmd_pid, t->argv[0]);*/
					return SUCCESS;
				}
				else {
					/*fprintf(stderr, "\t\t\tPID %d's exec %s didn't work\n", cmd_pid, t->argv[0]);*/
					return FAILURE;
				}
			}
		}
	}
	else if (strcmp(conj[t->conjunction], "&&") == 0) {	
		
		/*fprintf(stderr, "\t\tPID %d is processing an AND\n", getpid());*/
	
		/* Process AND, conditionally */
		if (execute(t->left) == SUCCESS) {
			/*fprintf(stderr, "\t\tPID %d's AND LHS passed\n", getpid());*/
			return execute(t->right);
		}
		else {
			/*fprintf(stderr, "\t\tPID %d's AND LHS failed\n", getpid());*/
			return FAILURE;
		}
	}
	else if (strcmp(conj[t->conjunction], "()") == 0) {

		/* fork to complete subshell processes */
		if ((subshell_pid = fork()) < 0) {
             		err(EX_OSERR, "fork error2\n");
               	}

                if (subshell_pid == 0) { /* Child (subshell) */
			/*fprintf(stderr, "\t\tPID %d is processing the subshell\n", getpid());*/
        		
			/* Redirect I/O */
                	if (t->input) {
                        	/*fprintf(stderr, "Redir. I\n");*/
                        	input = open(t->input, O_RDONLY);
                        	if (dup2(input, STDIN_FILENO) < 0) {
                                	err(EX_OSERR, "dup2 error1\n");
                        	}
                	}
                	if (t->output) {
                        	/*fprintf(stderr, "Redir. O\n");*/
                        	output = open(t->output, O_WRONLY);
                        	if (dup2(output, STDOUT_FILENO) < 0) {
                                	err(EX_OSERR, "dup2 error2\n");
                        	}
                	}
 
	        	exit(execute(t->left));
			
            	}
              	else { /* Original shell */

			/*fprintf(stderr, "\t\tPID %d created a child %d to subshell\n", getpid(), subshell_pid);*/
			wait(&status);
              		if (WIFEXITED(status) && !WEXITSTATUS(status)) {
				/*fprintf(stderr, "\t\tPID %d's subshell worked\n", subshell_pid);*/
                  		return SUCCESS;
                       	}
                       	else {
				/*fprintf(stderr, "\t\tPID %d's subshell didn't work\n", subshell_pid);*/
                        	return FAILURE;
                       	}
               	}		
	}
	else if (strcmp(conj[t->conjunction], "|") == 0) {
	
		/* Upper pipe / nested pipe */
		if (t->left->conjunction == PIPE) {	

			if (t->left->output || t->right->input) {
                                printf("Ambiguous output redirect.\n");
                                exit(0);
                        }	

			if (pipe(pipe_fd) < 0) { /* pipe */
                                err(EX_OSERR, "pipe error1\n");
                        }

                        if ((right_pid = fork()) < 0) { /* fork to process the RHS of the pipe */
                         	err(EX_OSERR, "fork error4\n");
                        }

                        if (right_pid == 0) { /* right child reads from the pipe */

				close(pipe_fd[1]); /* Close write end */
                               	
				/* Read from the read end instead of stdin */
                               	if (dup2(pipe_fd[0], STDIN_FILENO) < 0) {
                                      	err(EX_OSERR, "dup2 error5\n");
                               	}
                               	close(pipe_fd[0]);

				/* handle nesting by linking stdout to upper pipe */
                              	if (is_nested(CHECK)) {
                                        /*fprintf(stderr, "Rewiring lower OUT to %d\n", nested_pipe(OUT_TO, CHECK));*/
                                  	if (dup2(nested_pipe(OUT_TO, CHECK) + 1, STDOUT_FILENO) < 0) {
                                          	err(EX_OSERR, "dup2 error8\n");
                                   	}
                                }
			

                               	exit(execute(t->right)); /* traverse into the RHS of the pipe */
                        }
                        else {
				is_nested(YES);
				nested_pipe(OUT_TO, pipe_fd[0]);
				execute(t->left);
                             	close(pipe_fd[0]);
                         	close(pipe_fd[1]);
				wait(&status);
				is_nested(NO);
                        }
		}
		else {
			/* Lower pipe / Non-nested pipe */
			if (t->left->output || t->right->input) {
				printf("Ambiguous output redirect.\n");
				exit(0);
			}

			if (pipe(pipe_fd) < 0) { /* pipe */
                        	err(EX_OSERR, "pipe error2\n");
                	}
			/*if (is_nested(CHECK)) {
				nested_pipe(IN_FROM, pipe_fd[1]);
			}*/

                	if ((left_pid = fork()) < 0) { /* fork to process LHS of pipe */
                        	err(EX_OSERR, "fork error5\n");
                	}

                	if (left_pid == 0) { /* Left child writes to the pipe */
                        
				close(pipe_fd[0]); /* Close read end */

                        	/* Write to the write end instead of stdout */
                        	if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
					err(EX_OSERR, "dup2 error6\n");
                        	}
                        	close(pipe_fd[1]);

                        	exit(execute(t->left)); /* traverse into the LHS of the pipe */
                	}
                	else {
		
				/* fork to process the RHS of the pipe */
                        	if ((right_pid = fork()) < 0) {
                                	err(EX_OSERR, "fork error6\n");
                        	}

                        	if (right_pid == 0) { /* right child reads from the pipe */

                                	close(pipe_fd[1]); /* Close write end */

                                	/* Read from the read end instead of stdin */
                                	if (dup2(pipe_fd[0], STDIN_FILENO) < 0) {
                                        	err(EX_OSERR, "dup2 error7\n");
                                	}
                                	close(pipe_fd[0]);

					/* handle nesting by linking stdout to upper pipe */
					if (is_nested(CHECK)) {
						/*fprintf(stderr, "Rewiring lower OUT to %d\n", nested_pipe(OUT_TO, CHECK));*/
						if (dup2(nested_pipe(OUT_TO, CHECK) + 1, STDOUT_FILENO) < 0) {
                                                	err(EX_OSERR, "dup2 error8\n");
                                        	}
					}
                                	exit(execute(t->right));
                        	}
                        	else {
					close(nested_pipe(OUT_TO, CHECK) + 1);
                                	close(pipe_fd[0]);
                                	close(pipe_fd[1]);
                                	wait(&status);
					wait(&status);
                        	}
                	}
		}
	}
	
   return 0;
}

/* Helper function stores fd's for nested piping */
static int nested_pipe(int option1, int option2) {
        static int out_to = 0;
	static int in_from = 0;

	if (option1 == OUT_TO) {
		if (option2 == CHECK) {
			return out_to;
		}
		else {
			out_to = option2;
		}
	}
	else {
		if (option2 == CHECK) {
                        return in_from;
                }
                else {
                        in_from = option2;
                }
        }

	return FAILURE;	
}

/* Helper function sets/returns if nested pipe */
static int is_nested(int option) {
	static int status = NO;

	if (option == CHECK) {
		return status;
	}
	else {
		status = option;
		return FAILURE;
	}
}

static void print_tree(struct tree *t) {
   if (t != NULL) {
      print_tree(t->left);

      if (t->conjunction == NONE) {
         printf("NONE: %s, ", t->argv[0]);
      } else {
         printf("%s, ", conj[t->conjunction]);
      }
      printf("IR: %s, ", t->input);
      printf("OR: %s\n", t->output);

      print_tree(t->right);
   }
}

/* Helper function prints current node */
static void print_node(struct tree *t) {
        
	printf("PID: %d ", getpid());

        if (t != NULL) {
                if (t->conjunction == NONE) {
                        printf("CMD: %s, ", t->argv[0]);
                }
                else {
                        printf("CONJ: %s, ", conj[t->conjunction]);
                }
        }

	printf("IR: %s, ", t->input);
      	printf("OR: %s\n", t->output);

	printf("\n");
}

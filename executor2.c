/* Renzo Moron-Svartz */
/* UID: 11415086 */
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

int execute(struct tree *t) {
	pid_t cmd_pid, subshell_pid, left_pid, right_pid;
        int status, input, output, pipe_fd[2];
	
	print_node(t);

	/* Process the tree based on the conjunction*/
	if (strcmp(conj[t->conjunction], "err") == 0) {
		
		fprintf(stderr, "\t\t\tPID %d is processing an argument %s\n", getpid(), t->argv[0]);

		if (strcmp(t->argv[0], "exit") == 0) { /* exit */
                        exit(0);
        	}
        	else if (strcmp(t->argv[0], "cd") == 0) /* cd */
        	{
                	if (t->argv[1] == NULL) {
                       		chdir(getenv("HOME"));
                	}
                	else {
                        	if (chdir(t->argv[1]) == -1) {
                                	perror(t->argv[1]);
                        	}
                	}
        	}
        	else { /* CMD */

			if ((cmd_pid = fork()) < 0) {
                                err(EX_OSERR, "fork error1");
                        }

                        if (cmd_pid == 0) { /* Exec */

				fprintf(stderr, "\t\t\tChild PID %d is exec %s\n", getpid(), t->argv[0]);

				/* Redirect I/O */
				if (t->input) {
					fprintf(stderr, "Redir. I\n");
                               		input = open(t->input, O_RDONLY);	
					if (dup2(input, STDIN_FILENO) < 0) {
                                        	err(EX_OSERR, "dup2 error1");
                                	}
                        	}
				if (t->output) {
					fprintf(stderr, "Redir. O\n");
                       			output = open(t->output, O_WRONLY);
                                        if (dup2(output, STDOUT_FILENO) < 0) {
                                                err(EX_OSERR, "dup2 error2");
                                        }
                                }
			
				/* Execute command */
                                execvp(t->argv[0], t->argv);
                                fprintf(stderr, "\t\t\tFailed to execute %s\n", t->argv[0]);
				exit(EX_OSERR);
                        }
                        else { /* parent process wait */
                        
				fprintf(stderr, "\t\t\tPID %d created a child %d to exec %s\n", getpid(), cmd_pid, t->argv[0]);

			       	wait(&status);
				
				if (WIFEXITED(status) && !WEXITSTATUS(status)) {
					fprintf(stderr, "\t\t\tPID %d's exec %s worked\n", cmd_pid, t->argv[0]);
					return SUCCESS;
				}
				else {
					fprintf(stderr, "\t\t\tPID %d's exec %s didn't work\n", cmd_pid, t->argv[0]);
					return FAILURE;
				}
			}
		}
	}
	else if (strcmp(conj[t->conjunction], "&&") == 0) {	
		
		fprintf(stderr, "\t\tPID %d is processing an AND\n", getpid());

		if (execute(t->left) == SUCCESS) {
			fprintf(stderr, "\t\tPID %d's AND LHS passed\n", getpid());
			return execute(t->right);
		}
		else {
			fprintf(stderr, "\t\tPID %d's AND LHS failed\n", getpid());
		}
	}
	else if (strcmp(conj[t->conjunction], "()") == 0) {

		if ((subshell_pid = fork()) < 0) {
             		err(EX_OSERR, "fork error2");
               	}

                if (subshell_pid == 0) { /* Subshell */
			fprintf(stderr, "\tPID %d is processing the subshell\n", getpid());
                 	execute(t->left);
            	}
              	else { /* Original shell */

			fprintf(stderr, "\tPID %d created a child %d to subshell\n", getpid(), subshell_pid);
			wait(&status);
              		if (WIFEXITED(status) && !WEXITSTATUS(status)) {
				fprintf(stderr, "\tPID %d's subshell worked\n", subshell_pid);
                  		return SUCCESS;
                       	}
                       	else {
				fprintf(stderr, "\tPID %d's subshell didn't work\n", subshell_pid);
                        	return FAILURE;
                       	}
               	}		
	}
	else if (strcmp(conj[t->conjunction], "|") == 0) {
	
		if (t->left->conjunction == PIPE) {	
			fprintf(stderr, " Nested PID %d is processing a nested pipe\n", getpid());

			is_nested(YES);
			if (pipe(pipe_fd) < 0) {
                                err(EX_OSERR, "pipe error1");
                        }
			nested_pipe(OUT_TO, pipe_fd[0]);			

			fprintf(stderr, " Pre-nest\n");
			execute(t->left);
                        fprintf(stderr, " Post-nest\n");

			if ((left_pid = fork()) < 0) {
                                err(EX_OSERR, "fork error3");
                        }

                        if (left_pid == 0) { /* Left tree will write to the pipe */

				fprintf(stderr, " Nested PPID's %d Left child %d is piping\n", getppid(), getpid());

                                close(pipe_fd[0]); /* Close read end */

                                /* Write to the write end instead of stdout */
                                if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
                                        err(EX_OSERR, "dup2 error3");
                                }
                                close(pipe_fd[1]);
	
				fprintf(stderr, " Piping in from %d\n", nested_pipe(IN_FROM, CHECK));
				if (dup2(nested_pipe(IN_FROM, CHECK), STDIN_FILENO) < 0) {
					err(EX_OSERR, "dup2 error4");
				}
				fprintf(stderr, " Nested Left\n");
                                execute(t->left);

                        }
                        else { /* Right tree will read from the pipe */
				
				fprintf(stderr, " Nested PID %d created a left child %d for ", getpid(), left_pid);
				
				if (t->left->conjunction == NONE) {
					fprintf(stderr, "%s\n", t->left->argv[0]);
				}
				else {
					fprintf(stderr, "%s\n", conj[t->left->conjunction]);
				}
				
                                if ((right_pid = fork()) < 0) {
                                        err(EX_OSERR, "fork error4");
                                }

                                if (right_pid == 0) {
                                	
					fprintf(stderr, " Nested PPID's %d Right child %d is piping\n", getppid(), getpid());
					close(pipe_fd[1]); /* Close write end */
                                        /* Read from the read end instead of stdin */
                                        if (dup2(pipe_fd[0], STDIN_FILENO) < 0) {
                                                err(EX_OSERR, "dup2 error5");
                                        }
                                        close(pipe_fd[0]);

					fprintf(stderr, " Nested Right\n");
                                        execute(t->right);
                                }
                                else {
					fprintf(stderr, " Nested PID %d created a right child %d for ", getpid(), right_pid);
					
					if (t->right->conjunction == NONE) {
                                        	fprintf(stderr, "%s\n", t->right->argv[0]);
                                	}
                                	else {
                 	                  	fprintf(stderr, "%s\n", conj[t->right->conjunction]);
         	                       	}
	
                                        close(pipe_fd[0]);
                                        close(pipe_fd[1]);
                                        wait(&status);
                                        wait(&status);
					
					is_nested(NO);
					fprintf(stderr, "PID %d done with Nested PIPE\n", getpid());
                                }
                        }

	
		}
		else {

			fprintf(stderr, "PID %d is processing a pipe\n", getpid());
			
			if (pipe(pipe_fd) < 0) {
                        	err(EX_OSERR, "pipe error2");
                	}
			if (is_nested(CHECK)) {
				nested_pipe(IN_FROM, pipe_fd[1]);
			}

                	if ((left_pid = fork()) < 0) {
                        	err(EX_OSERR, "fork error5");
                	}

                	if (left_pid == 0) { /* Left tree will write to the pipe */
                        
				fprintf(stderr, "PPID's %d Left child %d is piping STDOUT into %d\n", getppid(), getpid(), pipe_fd[1]);

				close(pipe_fd[0]); /* Close read end */
                        	/* Write to the write end instead of stdout */
                        	if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
					err(EX_OSERR, "dup2 error6");
                        	}
                        	close(pipe_fd[1]);

				fprintf(stderr, "PID %d done Write piping\n", getpid());
                        	execute(t->left);

                	}
                	else { /* Right tree will read from the pipe */

				fprintf(stderr, "PID %d created a left child %d for ", getpid(), left_pid);
		
				if (t->left->conjunction == NONE) {
                                        fprintf(stderr, "%s\n", t->left->argv[0]);
                                }
                                else {
                                        fprintf(stderr, "%s\n", conj[t->left->conjunction]);
                                }

		
                        	if ((right_pid = fork()) < 0) {
                                	err(EX_OSERR, "fork error6");
                        	}

                        	if (right_pid == 0) {

					fprintf(stderr, "PPID's %d Right child %d is piping STDIN from %d\n", getppid(), getpid(), pipe_fd[0]);
                                	close(pipe_fd[1]); /* Close write end */
                                	/* Read from the read end instead of stdin */
                                	if (dup2(pipe_fd[0], STDIN_FILENO) < 0) {
                                        	err(EX_OSERR, "dup2 error7");
                                	}
                                	close(pipe_fd[0]);

					if (is_nested(CHECK)) {
						fprintf(stderr, "PID %d is nested-ly redirecting STDOUT to %d\n", getpid(), nested_pipe(OUT_TO, CHECK));
						if (dup2(nested_pipe(OUT_TO, CHECK), STDOUT_FILENO) < 0) {
                                                	err(EX_OSERR, "dup2 error8");
                                        	}
					}
					
					fprintf(stderr, "PID %d done Read (and Nest) piping\n", getpid());
                                	execute(t->right);
                        	}
                        	else {
			
					fprintf(stderr, "PID %d created a right child %d for ", getpid(), right_pid);

					if (t->right->conjunction == NONE) {
                                                fprintf(stderr, "%s\n", t->right->argv[0]);
                                        }
                                        else {
                                                fprintf(stderr, "%s\n", conj[t->right->conjunction]);
                                        }


                                	close(pipe_fd[0]);
                                	close(pipe_fd[1]);
                                	wait(&status);
					wait(&status);
					fprintf(stderr, "PID %d done with PIPE\n", getpid());
                        	}
                	}
		}
	}
	
   return 0;
}

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

	
}

static int is_nested(int option) {
	static int status = NO;

	if (option == CHECK) {
		return status;
	}
	else {
		status = option;
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

	printf("\n");
}

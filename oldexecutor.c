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

#define ENABLE 1
#define DISABLE 0
#define CHECK -2
#define LEFT -1
#define RIGHT 1
#define SUCCESS 0
#define FAILURE -1
#define GET -3
#define JUST_SET -4
#define SET -5
#define NOT_SET -6
#define RESET -7

static void print_tree(struct tree *t);
static void print_node(struct tree *t);
static int check_pipe(int);
static int check_side(int);
static int nested_pipe(struct tree *t, int);
static int has_nested_pipe(struct tree *t);

int execute(struct tree *t) {
	pid_t pid, pid1, pid2;
        int status, input, output;	
	int pipe_fd[2]; /* pipe */

	/* t = NULL */
	/*printf("Printing tree inorder\n");*/
	/*print_tree(t);
	printf("\n\n\n\n");*/
	/*printf("Printing current tree node\n");*/
	/*printf("\n\n\n\n");*/
	/*print_node(t);*/

	/* Process the tree based on the conjunction*/
	if (strcmp(conj[t->conjunction], "err") == 0) { /* NONE */
		
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
        	else { /* fork to execute command such as cat */

			if (check_pipe(CHECK)) {
				/*fprintf(stderr, "We piping\n");*/

				/* Ambiguity */
				if (check_side(CHECK) == LEFT) {
					/*fprintf(stderr, "LEFT\n");*/
					if (t->input) {
						/*fprintf(stderr, "redirecting I\n");*/
						input = open(t->input, O_RDONLY);
                                             	if (dup2(input, STDIN_FILENO) < 0) {
                                               		err(EX_OSERR, "dup2 error");
                                               	}
                                   	}
                                }
				else {       
					/*fprintf(stderr, "RIGHT\n");*/
		       			if (t->output) {
						/*fprintf(stderr, "redirecting O\n");*/
                                              	output = open(t->output, O_WRONLY);
                                              	if (dup2(output, STDOUT_FILENO) < 0) {
                                                     	err(EX_OSERR, "dup2 error");
                                               	}
                                       	}
				}

				/* Execute command */
				/*fprintf(stderr, "%s\n", t->argv[0]);*/
                               	fprintf(stderr, "%d tries %s:\n", getpid(), t->argv[0]);
				execvp(t->argv[0], t->argv);
                              	fprintf(stderr, "Failed to execute %s\n", t->argv[0]);
				exit(EX_OSERR);
			}
	                else { 
				/*printf("No pipe\n");*/

				if ((pid = fork()) < 0) {
                                	err(EX_OSERR, "fork error");
                        	}

                        	if (pid == 0) { /* child process exec */

					if (!check_pipe(CHECK)) {
						/* Redirect I/O */
						if (t->input) {
                                			input = open(t->input, O_RDONLY);
                                			if (dup2(input, STDIN_FILENO) < 0) {
                                        			err(EX_OSERR, "dup2 error");
                                			}
                        			}
						if (t->output) {
                                        		input = open(t->output, O_WRONLY);
                                        		if (dup2(output, STDOUT_FILENO) < 0) {
                                                		err(EX_OSERR, "dup2 error");
                                        		}
                                		}
					}	
			
					/* Execute command */
                               		fprintf(stderr, "%d tries %s:\n", getpid(), t->argv[0]);
                                	execvp(t->argv[0], t->argv);
                                	fprintf(stderr, "Failed to execute %s\n", t->argv[0]);
					exit(EX_OSERR);
                        	}
                        	else { /* parent process wait */
                                	wait(&status);
				 	if (WIFEXITED(status) && !WEXITSTATUS(status)) {
						return SUCCESS;
					}
					else {
						return FAILURE;
					}
				}
			}
        	}
	}
	else if (strcmp(conj[t->conjunction], "&&") == 0) {
		if (execute(t->left) == SUCCESS) {
			execute(t->right);
		}
	}
	else if (strcmp(conj[t->conjunction], "()") == 0) {

		if ((pid = fork()) < 0) {
             		err(EX_OSERR, "fork error");
               	}

                if (pid == 0) { /* child process exec */
			fprintf(stderr, "%d (%s) made %d (%s)\n", getppid(), conj[t->conjunction], getpid(), conj[t->left->conjunction]);
                 	execute(t->left);
            	}
              	else { /* parent process wait */
               		wait(&status);
              		if (WIFEXITED(status) && !WEXITSTATUS(status)) {
                  		return SUCCESS;
                       	}
                       	else {
                        	return FAILURE;
                       	}
               	}		
	}
	else if (strcmp(conj[t->conjunction], "|") == 0) {
		/* Create a pipe to pipe left tree and right tree */
		if (pipe(pipe_fd) < 0) {
      			err(EX_OSERR, "pipe error");
		}

		fprintf(stderr, "%d made R: %d and W: %d\n", getpid(), pipe_fd[0], pipe_fd[1]);
	
		check_pipe(ENABLE);

		fprintf(stderr, "%d checking for nested pipes\n", getpid());
		nested_pipe(t->left, pipe_fd[0]); /* Check and set nested pipe */
		
		if ((pid1 = fork()) < 0) {
                	err(EX_OSERR, "fork error");
        	}

        	if (pid1 == 0) { /* Left tree will write to the pipe */

                	close(pipe_fd[0]); /* Close read end */

                	/* Write to the write end instead of stdout */
                	if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
                        	err(EX_OSERR, "dup2 error");
                	}
                	close(pipe_fd[1]);

			check_side(LEFT);

			fprintf(stderr, "%d (%s) made %d (%s) Left\n", getppid(), conj[t->conjunction], getpid(), conj[t->left->conjunction]);
			/*fprintf(stderr, "%d Went left\n", getpid());*/
                	execute(t->left);

        	}
        	else { /* Right tree will read from the pipe */

                	if ((pid2 = fork()) < 0) {
                        	err(EX_OSERR, "fork error");
                	}

                	if (pid2 == 0) {
				wait(NULL);		

				/*fprintf(stderr, "Child 2 piping\n");*/

				if (nested_pipe(NULL, CHECK) == SUCCESS) {
					fprintf(stderr, "%d rewired it's W to R: %d\n", getpid(), nested_pipe(NULL, GET));
                                	if (dup2(nested_pipe(NULL, GET), STDOUT_FILENO) < 0) {
                                        	err(EX_OSERR, "dup2 error");
                                	}
				}                        
				else {
					fprintf(stderr, "%d does not rewire\n", getpid());
				}	
				close(pipe_fd[1]); /* Close write end */

                        	/* Read from the read end instead of stdin */
                        	if (dup2(pipe_fd[0], STDIN_FILENO) < 0) {
                                	err(EX_OSERR, "dup2 error");
                        	}
                       	 	/*close(pipe_fd[0]);*/

				check_side(RIGHT);

				fprintf(stderr, "%d (%s) made %d (%s) Right\n", getppid(), conj[t->conjunction], getpid(), conj[t->right->conjunction]);
				/*fprintf(stderr, "%d Went right\n", getpid());*/
                        	execute(t->right);
                	}
                	else {
				close(pipe_fd[0]);
         			close(pipe_fd[1]);
                        	wait(&status);
				check_pipe(DISABLE);
				/*fprintf(stderr, "done\n");*/
                	}
	        }
	}
	
   /*print_tree(t);*/

   return 0;
}

static int check_pipe(int option) {
        static int pipe_in_use = 0;

        switch (option) {
                case ENABLE:
                        pipe_in_use = 1;
                        break;
                case DISABLE:
                        pipe_in_use = 0;
                        break;
                case CHECK:
                        return pipe_in_use;
        }

        return pipe_in_use;
}

static int check_side(int option) {
        static int side = 0;

        switch (option) {
                case LEFT:
                        side = LEFT;
                        break;
                case RIGHT:
                        side = RIGHT;
                        break;
                case CHECK:
                        return side;
        }

        return side;
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
	printf("%d ", getpid());
	
	if (t != NULL) {
		if (t->conjunction == NONE) {
         		printf("CMD: %s, ", t->argv[0]);
		} 
		else {
         		printf("CONJUNCTION: %s, ", conj[t->conjunction]);
      		}
      		printf("IR: %s, ", t->input);
      		printf("OR: %s\n", t->output);	
	}
}

static int nested_pipe(struct tree *t, int option) {
	static int pipe_id = 0;
	static int status = NOT_SET;

	if (!t) {

		if (option == GET) {
			return pipe_id;
		}		
		else if (option == CHECK) {
			if (status == JUST_SET) {
				status = SET;
				return FAILURE;
			}
			else {
				return SUCCESS;
			}
		}
		else if (option == RESET) {
			status = NOT_SET;
		}
        }
	else {
	
		if (has_nested_pipe(t) == SUCCESS) {
			status = JUST_SET;
			pipe_id = option;
			fprintf(stderr, "stored %d\n", option);
		}
	}

}

static int has_nested_pipe(struct tree *t) {
	if (t == NULL) {
		fprintf(stderr, "%d no nested pipes\n", getpid());
		return FAILURE;
	}
	else if (t->conjunction == PIPE) {
		fprintf(stderr, "%d found nested pipes\n"), getpid();
        	return SUCCESS;
	}
	else {
		return has_nested_pipe(t->left);
	}
}

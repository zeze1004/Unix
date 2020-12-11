#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdbool.h> 


#define FALSE 0
#define TRUE 1

#define EOL   1
#define ARG   2
#define AMPERSAND 3  // &

#define LEFT 4       // <
#define RIGHT 5      // >
#define PIPE 6       // |

#define FOREGROUND 0
#define BACKGROUND 1

static char   input[512];
static char   tokens[1024];
char      *ptr, *tok;


int get_token(char **outptr)
{
   int type;

   *outptr = tok;
   while ((*ptr == ' ') || (*ptr == '\t')) ptr++;

   *tok++ = *ptr;
   
   switch (*ptr++) {
      case '\0' : type = EOL;    // 엔터
         // fprintf(stderr, "EOL\n");
         break;

      case '&': type = AMPERSAND; 
         // fprintf(stderr, "&\n");
         break;

      case '<': type = LEFT; 
         // fprintf(stderr, "LEFT\n");
         break;

      case '>': type = RIGHT; 
         // fprintf(stderr, "RIGHT\n");
         break;

      case '|': type = PIPE; 
         // fprintf(stderr, "PIPE\n");
         break;
   
      default : type = ARG;   // 위의 연산자 입력하지 않았을 시
         while ((*ptr != ' ') && (*ptr != '&') &&
         (*ptr != '\t') && (*ptr != '\0')&&
         (*ptr != '<') && (*ptr != '>') && (*ptr != '|'))
         *tok++ = *ptr++;
         // fprintf(stderr, "re-command!!!!\n");
   }
   *tok++ = '\0';
   return(type);
}

int execute(char **comm, int how)
{
   int pid;

   if ((pid = fork()) < 0) {
      fprintf(stderr, "minish : fork error\n");
      return(-1);
   }
   else if (pid == 0) {
      execvp(*comm, comm);
      fprintf(stderr, "execute_minish : command not found\n");
      exit(127);
   }
   /* Background execution */
   if (how == BACKGROUND) {   
      printf("[%d]\n", pid);
      return 0;
   }      
   /* Foreground execution */
   while (waitpid(pid, NULL, 0) < 0)
      if (errno != EINTR) return -1;
   return 0;
}

// 리디렉션 ">" 
int right_exec(char **comm, char **ccomm, int how)
{
   // fprintf(stderr, "right_exec\n");
   int pid1;
   int fd;

   if ((pid1 = fork()) < 0) {
      fprintf(stderr, "minish : fork error\n");
      return (-1);
   }
   
   if (pid1 == 0) {
      fd = open(*ccomm, O_RDWR|O_CREAT|S_IROTH, 0644);
      if (fd < 0) {
         perror("error");
         exit(-1);
      }
      // fd(abc.txt)파일 디스크립터를 1(STDOUT_FILENO)번 파일 디스크립터로 복사
      dup2(fd, STDOUT_FILENO);   
      close(fd);  
      execvp(*comm, comm); // execvp(경로, )
      fprintf(stderr, "minish : command not found\n");
      exit(127);  // execvp()되지 않으면 비정상 종료
   }
   /* Background execution */
   if (how == BACKGROUND) {   
      printf("[%d]\n", pid1);
      return 0;
   }      
   /* Foreground execution */
   while (waitpid(pid1, NULL, 0) < 0)
      if (errno != EINTR) return -1;
   wait();
}

// 리디렉션 "<"
int left_exec(char **comm, char **ccomm, int how)
{
   // fprintf(stderr, "left_exec\n");
   int   pid1;
   int   fd;

   if ((pid1 = fork()) < 0) {
      fprintf(stderr, "minish : fork error\n");
      return(-1);
   }
   if (pid1 == 0) {
      fd = open(*ccomm, O_RDONLY);
      if (fd < 0) {
         perror("error");
         exit(-1);
      }
      dup2(fd, STDIN_FILENO);
      close(fd);
      execvp(*comm, comm);
      fprintf(stderr, "minish : command not found\n");
      exit(127);
   }
   /* Background execution */
   if (how == BACKGROUND) {   
      printf("[%d]\n", pid1);
      return 0;
   }      
   /* Foreground execution */
   while (waitpid(pid1, NULL, 0) < 0)
      if (errno != EINTR) return -1;
   wait();
}

int parse_and_execute(char *input)
{
   char   *arg[1024];
   int   type, how;
   int   quit = FALSE;
   int   narg = 0;
   int   finished = FALSE;

   char   *right_arg[1024];
   char   *left_arg[1024];
   int   left_narg = 0, right_narg = 0;
   bool left = true, right = true;
   bool direction; // true: right ">",  false: left "<"

   ptr = input;
   tok = tokens;
   while (!finished) {
      // get_token에서 input(문자)을 type(정수)로 return 
      if((right == true && left == true)) {
         type = get_token(&arg[narg]);
         // fprintf(stderr, "both\n");
      } else if(right) {
         // fprintf(stderr, "right\n");
         type = get_token(&right_arg[right_narg]);
      } else {
         // fprintf(stderr, "left\n");
         type = get_token(&left_arg[left_narg]);
      }  
      switch (type) {
      case ARG :  
         if((right == true && left == true)) {
            narg++;
         } else if(right) {
            right_narg++;
         } else 
            left_narg++;
         break;
      case RIGHT :
         //fprintf(stderr, "RIGHT\n");
         right = true;
         if(left) {
            direction = true;
         }
         left = false;
         break;
      case LEFT :
         //fprintf(stderr, "LEFT\n");
         left = true;
         if(right) {
            direction = false;
         }
         right = false;
         break;
      case EOL :
      case AMPERSAND:
         if(!strcmp(arg[0], "quit")) quit = TRUE;
         // cd 구현 X
         else {
            how = FOREGROUND;
            arg[narg] = NULL;
            if((right == true || direction == true)) {
               right_arg[right_narg] = NULL;
            } else if((left == true || direction == true)) {
               left_arg[left_narg] = NULL;
            }
         
            if(narg != 0) {
               if(right == true) {
                  right_exec(arg, right_arg, how);
               } else if (left == true) { 
                  left_exec(arg, left_arg, how);
               } else execute(arg, how);
            }
         }
         narg = 0;
         if(type == EOL)
            finished = TRUE;
         break; 
      }
   }
   return quit;
}



int main(void)
{
   char *arg[1024];
   int quit;

   printf("msh start # \n");
   
   while (gets(input)) {
      quit = parse_and_execute(input);
      if (quit) break;
      printf("msh # ");
   }
}
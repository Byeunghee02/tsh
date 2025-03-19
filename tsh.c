/*
 * Copyright(c) 2023-2024 All rights reserved by Heekuck Oh.
 * 이 프로그램은 한양대학교 ERICA 컴퓨터학부 학생을 위한 교육용으로 제작되었다.
 * 한양대학교 ERICA 학생이 아닌 이는 프로그램을 수정하거나 배포할 수 없다.
 * 프로그램을 수정할 경우 날짜, 학과, 학번, 이름, 수정 내용을 기록한다.
 * ㄴ20250316 컴퓨터학과 2021073563 최병희 표준입출력 리다이렉션 기능 추가.
 * ㄴ20250319 컴퓨터학과 2021073563 최병희 파이프 명령 실행 기능 추가 및 오류 처리 추가.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 80 /* 명령어의 최대 길이 */

/*
 * cmdexec - 명령어를 파싱해서 실행한다.
 * 스페이스와 탭을 공백문자로 간주하고, 연속된 공백문자는 하나의 공백문자로 축소한다.
 * 작은 따옴표나 큰 따옴표로 이루어진 문자열을 하나의 인자로 처리한다.
 * 기호 '<' 또는 '>'를 사용하여 표준 입출력을 파일로 바꾸거나,
 * 기호 '|'를 사용하여 파이프 명령을 실행하는 것도 여기에서 처리한다.
 */
static void cmdexec(char *cmd)
{
    char *argv[MAX_LINE / 2 + 1]; /* 명령어 인자를 저장하기 위한 배열 */
    int argc = 0;                 /* 인자의 개수 */
    char *p, *q;                  /* 명령어를 파싱하기 위한 변수 */
    p = cmd;
    p += strspn(p, " \t");

    /*
     * 명령어에 '|' 기호가 있는지 확인하고, 있다면 자식 프로세스와 손자 프로세스를 생성한다.
     * '|'의 왼쪽 명령어는 손자 프로세스에서 실행 후 출력 결과는 파이프를 통해 자식 프로세스로 넘겨준다.
     * '|'의 오른쪽 명령어들은 자식 프로세스에서 손자 프로세스의 입력을 받아 cmdexec함수를 재귀적으로 실행한다.
     */
    if (strpbrk(p, "|"))
    {
        int fd[2];          /* 파이프 입출력을 위한 File Descriptor */
        char *left, *right; /* '|' 를 기준으로 왼쪽 명령어와 오른쪽 명령어를 각각 저장 */
        left = strsep(&p, "|");
        right = p;

        if (pipe(fd) < 0)
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        pid_t pid;
        if ((pid = fork()) < 0)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            pid_t childPid;
            if ((childPid = fork()) < 0)
            {
                perror("child fork");
                exit(EXIT_FAILURE);
            }
            else if (childPid == 0)
            {
                close(fd[0]);
                if (dup2(fd[1], STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    close(fd[1]);
                    exit(EXIT_FAILURE);
                }
                close(fd[1]);
                cmdexec(left);
            }
            else
            {
                waitpid(childPid, NULL, 0); // 손자 프로세스를 기다림
                close(fd[1]);
                if (dup2(fd[0], STDIN_FILENO) < 0)
                {
                    perror("dup2");
                    close(fd[0]);
                    exit(EXIT_FAILURE);
                }
                close(fd[0]);
                cmdexec(right);
            }
        }
        else
        {
            waitpid(pid, NULL, 0); // 자식 프로세스를 기다림
        }
    }

    /*
     * 명령어 앞부분 공백문자를 제거하고 인자를 하나씩 꺼내서 argv에 차례로 저장한다.
     * 작은 따옴표나 큰 따옴표로 이루어진 문자열을 하나의 인자로 처리한다.
     */
    else
    {
        do
        {
            /*
             * 공백문자, 큰 따옴표, 작은 따옴표가 있는지 검사한다.
             */
            q = strpbrk(p, " \t\'\"");
            /*
             * 공백문자가 있거나 아무 것도 없으면 공백문자까지 또는 전체를 하나의 인자로 처리한다.
             */
            if (q == NULL || *q == ' ' || *q == '\t')
            {
                q = strsep(&p, " \t");
                if (*q)
                    argv[argc++] = q;
            }
            /*
             * 작은 따옴표가 있으면 그 위치까지 하나의 인자로 처리하고,
             * 작은 따옴표 위치에서 두 번째 작은 따옴표 위치까지 다음 인자로 처리한다.
             * 두 번째 작은 따옴표가 없으면 나머지 전체를 인자로 처리한다.
             */
            else if (*q == '\'')
            {
                q = strsep(&p, "\'");
                if (*q)
                    argv[argc++] = q;
                q = strsep(&p, "\'");
                if (*q)
                    argv[argc++] = q;
            }
            /*
             * 큰 따옴표가 있으면 그 위치까지 하나의 인자로 처리하고,
             * 큰 따옴표 위치에서 두 번째 큰 따옴표 위치까지 다음 인자로 처리한다.
             * 두 번째 큰 따옴표가 없으면 나머지 전체를 인자로 처리한다.
             */
            else
            {
                q = strsep(&p, "\"");
                if (*q)
                    argv[argc++] = q;
                q = strsep(&p, "\"");
                if (*q)
                    argv[argc++] = q;
            }
        } while (p);
        argv[argc] = NULL;

        /*
         * 기호 '<' 또는 '>'를 사용하여 표준 입출력을 파일로 바꾼다.
         * 파싱된 명령어에 '<' 또는 '>'가 있으면 그 다음 인자를 파일명으로 받고,
         * 파일을 open()함수로 열고 FileDescriptor를 받아 dup2()함수를 호출한다.
         * 이후 파싱된 명령어에서 '<', '>'과 파일명을 배열에서 삭제한다.
         */

        char *inFileName = NULL, *outFileName = NULL;      /* 파일 입출력을 위한 파일 명 변수 */
        int inFileDescriptor = -1, outFileDescriptor = -1; /* 파일 입출력을 위한 파일 디스크립터 */

        for (int i = 0; i < argc; i++)
        {
            if (strcmp(argv[i], ">") == 0)
            {
                outFileName = argv[i + 1];
                outFileDescriptor = open(outFileName, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (outFileDescriptor < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(outFileDescriptor, STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    close(outFileDescriptor);
                    exit(EXIT_FAILURE);
                }
                close(outFileDescriptor);
                for (int j = i; j < argc - 2; j++)
                {
                    argv[j] = argv[j + 2];
                }
                argc -= 2;
                argv[argc] = NULL;
            }
            else if (strcmp(argv[i], "<") == 0)
            {
                inFileName = argv[i + 1];
                inFileDescriptor = open(inFileName, O_RDONLY);
                if (inFileDescriptor < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(inFileDescriptor, STDIN_FILENO) < 0)
                {
                    perror("dup2");
                    close(inFileDescriptor);
                    exit(EXIT_FAILURE);
                }
                close(inFileDescriptor);
                for (int j = i; j < argc - 2; j++)
                {
                    argv[j] = argv[j + 2];
                }
                argc -= 2;
                argv[argc] = NULL;
            }
            else
            {
                i++; /* >, <가 없으면 다음 인자로 넘어간다.*/
            }
        }
    }

    /*
     * argv에 저장된 명령어를 실행한다.
     */
    if (argc > 0)
        if (execvp(argv[0], argv))
        {
            perror("execvp");
            fprintf(stderr, "Failed to execute command: %s\n", argv[0]);
            exit(EXIT_FAILURE);
        }
}

/*
 * 기능이 간단한 유닉스 셸인 tsh (tiny shell)의 메인 함수이다.
 * tsh은 프로세스 생성과 파이프를 통한 프로세스간 통신을 학습하기 위한 것으로
 * 백그라운드 실행, 파이프 명령, 표준 입출력 리다이렉션 일부만 지원한다.
 */
int main(void)
{
    char cmd[MAX_LINE + 1]; /* 명령어를 저장하기 위한 버퍼 */
    int len;                /* 입력된 명령어의 길이 */
    pid_t pid;              /* 자식 프로세스 아이디 */
    bool background;        /* 백그라운드 실행 유무 */

    /*
     * 종료 명령인 "exit"이 입력될 때까지 루프를 무한 반복한다.
     */
    while (true)
    {
        /*
         * 좀비 (자식)프로세스가 있으면 제거한다.
         */
        pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0)
            printf("[%d] + done\n", pid);
        /*
         * 셸 프롬프트를 출력한다. 지연 출력을 방지하기 위해 출력버퍼를 강제로 비운다.
         */
        printf("tsh> ");
        fflush(stdout);
        /*
         * 표준 입력장치로부터 최대 MAX_LINE까지 명령어를 입력 받는다.
         * 입력된 명령어 끝에 있는 새줄문자를 널문자로 바꿔 C 문자열로 만든다.
         * 입력된 값이 없으면 새 명령어를 받기 위해 루프의 처음으로 간다.
         */
        len = read(STDIN_FILENO, cmd, MAX_LINE);
        if (len == -1)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }
        cmd[--len] = '\0';
        if (len == 0)
            continue;
        /*
         * 종료 명령이면 루프를 빠져나간다.
         */
        if (!strcasecmp(cmd, "exit"))
            break;
        /*
         * 백그라운드 명령인지 확인하고, '&' 기호를 삭제한다.
         */
        char *p = strchr(cmd, '&');
        if (p != NULL)
        {
            background = true;
            *p = '\0';
        }
        else
            background = false;
        /*
         * 자식 프로세스를 생성하여 입력된 명령어를 실행하게 한다.
         */
        if ((pid = fork()) == -1)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        /*
         * 자식 프로세스는 명령어를 실행하고 종료한다.
         * ~ 파일 출력을 위해 출력 버퍼를 강제로 비운다.
         */
        else if (pid == 0)
        {
            cmdexec(cmd);
            fflush(stdout);
            exit(EXIT_SUCCESS);
        }
        /*
         * 포그라운드 실행이면 부모 프로세스는 자식이 끝날 때까지 기다린다.
         * 백그라운드 실행이면 기다리지 않고 다음 명령어를 입력받기 위해 루프의 처음으로 간다.
         */
        else if (!background)
            waitpid(pid, NULL, 0);
    }
    return 0;
}

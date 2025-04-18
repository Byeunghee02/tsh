/*
 * Copyright(c) 2023-2024 All rights reserved by Heekuck Oh.
 * 이 프로그램은 한양대학교 ERICA 컴퓨터학부 학생을 위한 교육용으로 제작되었다.
 * 한양대학교 ERICA 학생이 아닌 이는 프로그램을 수정하거나 배포할 수 없다.
 * 프로그램을 수정할 경우 날짜, 학과, 학번, 이름, 수정 내용을 기록한다.
 * ㄴ20250316 컴퓨터학부 2021073563 최병희 표준입출력 리다이렉션 기능 추가.
 * ㄴ20250319 컴퓨터학부 2021073563 최병희 파이프 명령 실행 기능 추가 및 오류 처리 추가.
 * ㄴ20250323 컴퓨터학부 2021073563 최병희 이중리다이렉션 오류 해결.
 * ㄴ20250324 컴퓨터학부 2021073563 최병희 파이프 위치 조정. 오류 해결.
 * ㄴ20250325 컴퓨터학부 2021073563 최병희 리펙토링, 반복되는 코드를 새로운 함수로 정리.
 * ㄴ20250326 컴퓨터학부 2021073563 최병희 코드 주석 추가.
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


void controlError(char *errorMessage)   /* 오류 처리 함수 */
{
    perror(errorMessage);
    exit(EXIT_FAILURE);
}

/*
 * removeElement - 배열의 원하는 위치에서 원하는 갯수 만큼 삭제한다.
 * 배열과 배열의 갯수, 삭제할 위치의 인덱스, 삭제할 갯수를 인자로 받아 해당 원소들을 삭제.
 * 바뀐 배열의 갯수를 리턴.
 */
int removeElement(char **arr, int arrCount, int idx, int removeCount)
{
    for (int i = idx; i < arrCount; i++)
        arr[i] = arr[i + removeCount];
    arrCount -= removeCount;
    arr[arrCount] = NULL;
    return arrCount;
}

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
    fflush(stdout);

    /*
     * 명령어에 '|' 기호가 있는지 확인하고, 있다면 자식 프로세스와 손자 프로세스를 생성한다.
     * '|'의 왼쪽 명령어는 손자 프로세스에서 실행 후 출력 결과는 파이프를 통해 자식 프로세스로 넘겨준다.
     * '|'의 오른쪽 명령어들은 자식 프로세스에서 손자 프로세스의 입력을 받아 cmdexec함수를 재귀적으로 실행한다.
     */
    if (strpbrk(p, "|"))
    {
        char *left, *right; /* '|' 를 기준으로 왼쪽 명령어와 오른쪽 명령어를 각각 저장 */
        left = strsep(&p, "|");
        right = p;

        pid_t pid = fork(); /* 자식프로세스 생성 */
        if (pid < 0)
            controlError("fork");
        else if (pid == 0)  /* 자식프로세스 */
        {
            int fd[2]; /* 자식 프로세스와 손자 프로세스간 통신을 위한 File Descriptor */
            if (pipe(fd) < 0)
                controlError("pipe");

            pid_t childPid = fork();    /* 손자프로세스 생성 */
            if (childPid < 0)
                controlError("child fork");
            else if (childPid == 0)     /* 손자프로세스 */
            {
                close(fd[0]);
                if (dup2(fd[1], STDOUT_FILENO) < 0) /* 손자프로세스의 표준 출력을 파이프로 연결 */
                {
                    close(fd[1]);
                    controlError("child dup2");
                }
                fflush(stdout);
                close(fd[1]);
                cmdexec(left);  /* 손자프로세스에서 왼쪽 명령어 실행 */
                exit(EXIT_SUCCESS);
            }
            else
            {
                waitpid(childPid, NULL, 0); // 손자 프로세스를 기다림
                close(fd[1]);
                if (dup2(fd[0], STDIN_FILENO) < 0)  /* 자식프로세스의 표준 입력을 파이프로 연결 */
                {
                    close(fd[0]);
                    controlError("dup2");
                }
                fflush(stdout);
                close(fd[0]);
                cmdexec(right); /* 자식프로세스에서 오른쪽 명령어 실행 */
                exit(EXIT_SUCCESS);
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
         * 파일을 open()함수로 열고 FileDescriptor를 받아 dup2()함수를 호출해 표준 입력을 파일로 리다이렉션.
         * 이후 파싱된 명령어에서 '<', '>'과 파일명을 배열에서 삭제한다.
         */

        char *inFileName = NULL, *outFileName = NULL;      /* 파일 입출력을 위한 파일 명 변수 */
        int inFileDescriptor = -1, outFileDescriptor = -1; /* 파일 입출력을 위한 파일 디스크립터 */

        for (int i = 0; i < argc; i++)
        {
            if (strcmp(argv[i], ">") == 0)
            {
                outFileName = argv[i + 1];
                outFileDescriptor = open(outFileName, O_WRONLY | O_CREAT | O_TRUNC, 0666);  /* 파일이 없으면 생성하고, 있으면 덮어쓰기 */
                if (outFileDescriptor < 0)
                    controlError("out file open");
                if (dup2(outFileDescriptor, STDOUT_FILENO) < 0) /* 표준 출력을 파일로 리다이렉션 */
                {
                    close(outFileDescriptor);
                    controlError("out file dup2");
                }
                fflush(stdout);
                close(outFileDescriptor);
                argc = removeElement(argv, argc, i, 2); /* 명령어에서 '<', '>'와 파일명 삭제 */
                i--;
            }
            else if (strcmp(argv[i], "<") == 0)
            {
                inFileName = argv[i + 1];
                inFileDescriptor = open(inFileName, O_RDONLY);  /* 파일을 읽기 전용으로 열기 */
                if (inFileDescriptor < 0)
                    controlError("in file open");
                if (dup2(inFileDescriptor, STDIN_FILENO) < 0)   /* 표준 입력을 파일로 리다이렉션 */
                {
                    close(inFileDescriptor);
                    controlError("in file dup2");
                }
                fflush(stdout);
                close(inFileDescriptor);

                argc = removeElement(argv, argc, i, 2); /* 명령어에서 '<', '>'와 파일명 삭제 */
                i--;
            }
        }
    }
    /*
     * argv에 저장된 명령어를 실행한다.
     */
    fflush(stdout);
    if (argc > 0)
        if (execvp(argv[0], argv))
        {
            fprintf(stderr, "Failed to execute command: %s\n", argv[0]);
            fflush(stdout);
            controlError("execvp");
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
        while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
        {
            printf("[%d] + done\n", pid);
        }
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
            controlError("main read");
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
            controlError("main fork");
        /*
         * 자식 프로세스는 명령어를 실행하고 종료한다.
         */
        else if (pid == 0)
        {
            cmdexec(cmd);
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

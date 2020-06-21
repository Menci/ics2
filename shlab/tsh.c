/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and login ID here>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <fcntl.h>
#include <assert.h>
#include <spawn.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE 1024   /* max line size */
#define MAXARGS 128    /* max args on a command line */
#define MAXJOBS 16     /* max jobs at any point in time */
#define MAXJID 1 << 16 /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output */
int nextjid = 1;         /* next job ID to allocate */
char sbuf[MAXLINE];      /* for composing sprintf messages */

struct job_t
{                          /* The job struct */
    pid_t pid;             /* job PID */
    int jid;               /* job ID [1, 2, ...] */
    int state;             /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE]; /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */

/* Function prototypes */

/* Here are the functions that you will implement */
int eval(char *cmdline);
bool builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv)
{
    char c;
    int emit_prompt = 1; /* emit prompt (default) */

#define prompt() ((void)(emit_prompt && write(STDOUT_FILENO, prompt, sizeof(prompt) - 1)))

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF)
    {
        switch (c)
        {
        case 'h': /* print help message */
            usage();
            break;
        case 'v': /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':            /* don't print a prompt */
            emit_prompt = 0; /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */

    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGINT);
    sigaddset(&sigmask, SIGTSTP);
    sigaddset(&sigmask, SIGCHLD);
    sigaddset(&sigmask, SIGQUIT);
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
    int fd_signal = signalfd(-1, &sigmask, SFD_NONBLOCK);

    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

    /* Initialize the job list */
    initjobs(jobs);

    int fd_epoll = epoll_create1(0);
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = fd_signal;
    epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_signal, &ev);
    ev.data.fd = STDIN_FILENO;
    epoll_ctl(fd_epoll, EPOLL_CTL_ADD, STDIN_FILENO, &ev);

    prompt();

    bool read_input = true;

    char cmdline[MAXLINE];
    int cmdline_length = 0;
    while (1)
    {
        struct epoll_event event;
        epoll_wait(fd_epoll, &event, 1, -1);
        if (event.data.fd == fd_signal)
        {
            struct signalfd_siginfo siginfo;
            assert(read(fd_signal, &siginfo, sizeof(siginfo)) == sizeof(siginfo));
            switch (siginfo.ssi_signo)
            {
            case SIGINT:
            case SIGTSTP:
                if (fgpid(jobs))
                {
                    kill(-fgpid(jobs), siginfo.ssi_signo);
                }
                break;
            case SIGCHLD:
            {
                pid_t pid;
                int status;
                while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
                {
                    struct job_t *job = getjobpid(jobs, pid);
                    if (WIFEXITED(status))
                    {
                        if (WEXITSTATUS(status) == 127)
                        {
                            // Print "./bogus: Command not found"
                            // Don't print the '\n' in cmdline
                            write(STDOUT_FILENO, job->cmdline, strlen(job->cmdline) - 1);
                            const char *msg = ": Command not found\n";
                            write(STDOUT_FILENO, msg, strlen(msg));
                        }
                        deletejob(jobs, pid);
                    }
                    else if (WIFSIGNALED(status))
                    {
                        printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), (int)pid, (int)WTERMSIG(status));
                        deletejob(jobs, pid);
                    }
                    else if (WIFSTOPPED(status))
                    {
                        printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), (int)pid, (int)WSTOPSIG(status));
                        job->state = ST;
                    }
                    else
                        abort();
                }
                break;
            }
            case SIGQUIT:
                printf("Terminating after receipt of SIGQUIT signal\n");
                exit(1);
            default:
                abort();
            }
        }
        else if (event.data.fd == STDIN_FILENO)
        {
            // C-d in a empty line means EOF, C-d in a non-empty line means flashing the buffer.
            char ch;
            while (read(STDIN_FILENO, &ch, 1) == 1)
            {
                cmdline[cmdline_length++] = ch;
                if (ch == '\n')
                    break;
            }
            if (cmdline_length == 0)
            {
                // EOF
                fflush(stdout);
                exit(0);
            }
            else if (cmdline[cmdline_length - 1] == '\n')
            {
                cmdline[cmdline_length] = '\0';

                eval(cmdline);
                // If the command is NOT to run a foreground job, print the prompt.
                if (!fgpid(jobs))
                {
                    prompt();
                }
                fflush(stdout);

                cmdline_length = 0;
            }
        }
        else
            abort();

        // Don't read the stdin if there's a foreground job.
        if (read_input && fgpid(jobs) != 0)
        {
            ev.data.fd = STDIN_FILENO;
            epoll_ctl(fd_epoll, EPOLL_CTL_DEL, STDIN_FILENO, &ev);
            read_input = false;
        }
        else if (!read_input && fgpid(jobs) == 0)
        {
            ev.data.fd = STDIN_FILENO;
            epoll_ctl(fd_epoll, EPOLL_CTL_ADD, STDIN_FILENO, &ev);
            read_input = true;
            prompt();
        }
    }

    exit(0); /* control never reaches here */
}

/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
int eval(char *cmdline)
{
    char *argv[MAXARGS];
    bool bg = parseline(cmdline, argv);
    if (argv[0] == NULL)
        return 0;
    if (builtin_cmd(argv))
        return 0;

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);

    // Restore signals
    sigset_t sigset;
    sigemptyset(&sigset);
    posix_spawnattr_setsigmask(&attr, &sigset);

    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK);
    posix_spawnattr_setpgroup(&attr, 0);

    pid_t pid;
    assert(posix_spawn(&pid, argv[0], NULL, &attr, argv, NULL) == 0);

    posix_spawnattr_destroy(&attr);

    int jid = addjob(jobs, pid, bg ? BG : FG, cmdline);
    if (bg)
    {
        printf("[%d] (%d) %s", jid, (int)pid, cmdline);
    }

    return jid;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv)
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'')
    {
        buf++;
        delim = strchr(buf, '\'');
    }
    else
    {
        delim = strchr(buf, ' ');
    }

    while (delim)
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'')
        {
            buf++;
            delim = strchr(buf, '\'');
        }
        else
        {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;

    if (argc == 0) /* ignore blank line */
        return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
    {
        argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
bool builtin_cmd(char **argv)
{
    if (argv[0] == NULL)
        return false;

    if (strcmp(argv[0], "jobs") == 0)
        listjobs(jobs);
    else if (strcmp(argv[0], "quit") == 0)
    {
        size_t i;
        for (i = 0; i < MAXJOBS; i++)
            if (jobs[i].pid != 0)
                kill(-jobs[i].pid, SIGQUIT);
        exit(0);
    }
    else if (strcmp(argv[0], "fg") == 0 || strcmp(argv[0], "bg") == 0)
    {
        do_bgfg(argv);
    }
    else
        return false;

    return true;
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{
    struct job_t *job = NULL;
    if (argv[1])
    {
        if (argv[1][0] == '%')
        {
            int jid;
            if (sscanf(&argv[1][1], "%d", &jid) == 1)
            {
                job = getjobjid(jobs, jid);
                if (!job)
                {
                    printf("%s: No such job\n", argv[1]);
                    return;
                }
            }
        }
        else
        {
            int pid;
            if (sscanf(&argv[1][0], "%d", &pid) == 1)
            {
                job = getjobpid(jobs, pid);
                if (!job)
                {
                    printf("(%s): No such process\n", argv[1]);
                    return;
                }
            }
        }
    }
    else
    {
        printf("%s: argument must be a PID or %%jobid\n", argv[0]);
        return;
    }

    if (!job)
    {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }

    job->state = **argv == 'b' ? BG : FG;
    kill(-job->pid, SIGCONT);

    if (job->state == BG)
    {
        printf("[%d] (%d) %s", job->jid, (int)job->pid, job->cmdline);
    }
}

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job)
{
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
    int i, max = 0;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid > max)
            max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == 0)
        {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(jobs[i].cmdline, cmdline);
            if (verbose)
            {
                printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == pid)
        {
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs) + 1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG)
            return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid == jid)
            return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
        {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid != 0)
        {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state)
            {
            case BG:
                printf("Running ");
                break;
            case FG:
                printf("Foreground ");
                break;
            case ST:
                printf("Stopped ");
                break;
            default:
                printf("listjobs: Internal error: job[%d].state=%d ",
                       i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* MARK NAME Daniele Cássia Silva Diniz */

/****************************************************************
 * Shell xv6 simplificado
 *
 * Este codigo foi adaptado do codigo do UNIX xv6 e do material do
 * curso de sistemas operacionais do MIT (6.828).
 ***************************************************************/

#define MAXARGS 10

/* Todos comandos tem um tipo.  Depois de olhar para o tipo do
 * comando, o código converte um *cmd para o tipo específico de
 * comando. */
struct cmd {
    int type; /* ' ' (exec)
                 '|' (pipe)
                 '<' or '>' (redirection) */
};

struct execcmd {
    int type;             // ' '
    char *argv[MAXARGS];  // argumentos do comando a ser exec'utado
};

struct redircmd {
    int type;         // < ou >
    struct cmd *cmd;  // o comando a rodar (ex.: um execcmd)
    char *file;       // o arquivo de entrada ou saída
    int mode;         // o modo no qual o arquivo deve ser aberto
    int fd;           // o número de descritor de arquivo que deve ser usado
};

struct pipecmd {
    int type;           // |
    struct cmd *left;   // lado esquerdo do pipe
    struct cmd *right;  // lado direito do pipe
};


int fork1(void);               // Fork mas fechar se ocorrer erro.
struct cmd *parsecmd(char *);  // Processar o linha de comando.



/* MARK START debug
DEBUGGER PARA DEBUGAR*/

// Function prototype declarations
void printcmd(struct cmd *c);

void printexeccmd(struct execcmd *c) {
    printf("exec command:\n");
    printf("  argv: ");
    for (int i = 0; c->argv[i] != NULL; i++) {
        printf("'%s' ", c->argv[i]);
    }
    printf("\n");
}

void printredircmd(struct redircmd *c) {
    printf("redirection command:\n");
    printf("  file: '%s', mode: %d, fd: %d\n", c->file, c->mode, c->fd);
    printf("  subcommand:\n");
    printcmd(c->cmd);
}

void printpipecmd(struct pipecmd *c) {
    printf("pipe command:\n");
    printf("  left side command:\n");
    printcmd(c->left);
    printf("  right side command:\n");
    printcmd(c->right);
}

void printcmd(struct cmd *c) {
    if (!c) {
        printf("NULL command\n");
        return;
    }

    switch (c->type) {
        case ' ':
            printexeccmd((struct execcmd *)c);
            break;
        case '>':
        case '<':
            printredircmd((struct redircmd *)c);
            break;
        case '|':
            printpipecmd((struct pipecmd *)c);
            break;
        default:
            printf("Unknown command type\n");
    }
}

/* MARK END debug */

void runls() {
    DIR *d;
    struct dirent *dir;
    d = opendir(".");

    if (d) {
        while ((dir = readdir(d)) != NULL) {
            printf("%s\n", dir->d_name);
        }
        closedir(d);
    } else {
        perror("Failed to open directory");
        exit(EXIT_FAILURE);
    }
}

/* Executar comando cmd.  Nunca retorna. */
void runcmd(struct cmd *cmd) {
    int p[2], r;
    struct execcmd *ecmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if (cmd == 0) exit(0);

    switch (cmd->type) {
        default:
            fprintf(stderr, "tipo de comando desconhecido\n");
            exit(-1);

        case ' ':
            ecmd = (struct execcmd *)cmd;
            if (ecmd->argv[0] == 0) exit(0);
            /* MARK START task2
             * TAREFA2: Implemente codigo abaixo para executar
             * comandos simples. */
            execvp(ecmd->argv[0], ecmd->argv);

            /* MARK END task2 */
            break;

        case '>':
        case '<':
            rcmd = (struct redircmd *)cmd;
            /* MARK START task3
             * TAREFA3: Implemente codigo abaixo para executar
             * comando com redirecionamento. */
            // Abre arquivo para redirecionamento
            int fd = open(rcmd->file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
            if (fd < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            // Redireciona stdin ou stdout
            if (cmd->type == '<')
                dup2(fd, STDIN_FILENO);
            else
                dup2(fd, STDOUT_FILENO);
            close(fd);
            /* MARK END task3 */
            runcmd(rcmd->cmd);
            break;

        case '|':
            pcmd = (struct pipecmd *)cmd;
            /* MARK START task4
             * TAREFA4: Implemente codigo abaixo para executar
             * comando com pipes. */
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
            // Fork um processo filho para o comando esquerdo
            pid_t pid = fork1();
          if (pid == 0) {
              // Processo filho: feche a extremidade de leitura do pipe e 
              // redirecione o stdout para pipe
              close(pipefd[0]);
              dup2(pipefd[1], STDOUT_FILENO);
              close(pipefd[1]);
              // Execute o comando esquerdo
              runcmd(pcmd->left);
              exit(EXIT_FAILURE);  // O processo filho não deve continuar
            } 
            else {
              // Processo pai: bifurque outro processo filho pela direita
              pid = fork1();
              if (pid == 0) {
                  // Processo filho: feche o fim do pipe de gravação e redirecione o stdin
                  close(pipefd[1]);
                  dup2(pipefd[0], STDIN_FILENO);
                  close(pipefd[0]);
                  // Execute o comando direito
                  runcmd(pcmd->right);
                  exit(EXIT_FAILURE);  // O processo filho não deve continuar
              } else {
                  // Processo pai: feche ambas as extremidades do pipe e aguarde
                  close(pipefd[0]);
                  close(pipefd[1]);
                  wait(NULL);
                  wait(NULL);
              }
            }
            /* MARK END task4 */
            break;
    }
    exit(0);
}

int getcmd(char *buf, int nbuf) {
    if (isatty(fileno(stdin))) fprintf(stdout, "$ ");
    memset(buf, 0, nbuf);
    fgets(buf, nbuf, stdin);
    if (buf[0] == 0)  // EOF
        return -1;
    return 0;
}

int main(void) {
    static char buf[100];
    int r;

    // Ler e rodar comandos.
    while (getcmd(buf, sizeof(buf)) >= 0) {
        /* MARK START task1 */
        /* TAREFA1: O que faz o if abaixo e por que ele é necessário?
         * Insira sua resposta no código e modifique o fprintf abaixo
         * para reportar o erro corretamente.
        RESPOSTA:
        ->  O código verifica se o comando inserido começa com 'cd ', é
        necessário para identificar se a entrada é o referido comando. Se sim,
        tenta mudar o diretório usando a função chdir(). Se chdir() retorna um
        valor menor que zero, significa que a mudança de diretório falhou. Nesse
        caso, o imprime-se um erro.
        */
        if (buf[0] == 'c' && buf[1] == 'd' &&
            buf[2] == ' ') {  // olho se o camando é o 'cd '
            buf[strlen(buf) - 1] =
                0;  // coloco 0 na ultima posição do buf (remover o \n ?)
            if (chdir(buf + 3) < 0)  // tento navegar para o dir
                fprintf(stderr, "bash: cd: %s: No such file or directory\n",
                        buf + 3);
            continue;
        }
        /* MARK END task1 */

        if (fork1() == 0) {
            struct cmd *c = parsecmd(buf);
            // printcmd(c); //debug
            runcmd(c);
        }
        wait(&r);
    }
    exit(0);
}

/**
cria um novo processo e retorna o `pid`
Se der erro, retorna -1 e imprime o erro
*/
int fork1(void) {
    int pid;

    pid = fork();
    if (pid == -1) perror("fork");
    return pid;
}

/****************************************************************
 * Funcoes auxiliares para criar estruturas de comando
 ***************************************************************/

/**
Cria/Retorana um bloco de memória[0] e coloca o type como ' '
*/
struct cmd *execcmd(void) {
    struct execcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = ' ';
    return (struct cmd *)cmd;
}

struct cmd *redircmd(struct cmd *subcmd, char *file, int type) {
    struct redircmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = type;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->mode = (type == '<') ? O_RDONLY : O_WRONLY | O_CREAT | O_TRUNC;
    cmd->fd = (type == '<') ? 0 : 1;
    return (struct cmd *)cmd;
}

struct cmd *pipecmd(struct cmd *left, struct cmd *right) {
    struct pipecmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = '|';
    cmd->left = left;
    cmd->right = right;
    return (struct cmd *)cmd;
}

/****************************************************************
 * Processamento da linha de comando
 ***************************************************************/

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

/**
Separa os comando nos divisores de pipe ou redirecionamento
EXEMPLO:
    ps                   es
    cat n.txt  | grep "Oi"
FINAL:
    q        eq  ps           es
    cat n.txt    |  grep "Oi"
*/
int gettoken(char **ps, char *es, char **q, char **eq) {
    char *s;
    int ret;

    s = *ps;
    while (s < es && strchr(whitespace, *s)) s++;
    if (q) *q = s;
    ret = *s;
    switch (*s) {
        case 0:
            break;
        case '|':
        case '<':
            s++;
            break;
        case '>':
            s++;
            break;
        default:
            ret = 'a';
            while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
                s++;
            break;
    }
    if (eq) *eq = s;

    while (s < es && strchr(whitespace, *s)) s++;
    *ps = s;
    return ret;
}

/**
Retorna se o comando em ps inicia com algum char contido em toks
*/
int peek(char **ps, char *es, char *toks) {
    char *s;

    s = *ps;
    // ignora espaços vazios no inicio da string
    while (s < es && strchr(whitespace, *s)) s++;
    *ps = s;
    return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);

/* Copiar os caracteres no buffer de entrada, comeando de s ate es.
 * Colocar terminador zero no final para obter um string valido. */
char *mkcopy(char *s, char *es) {
    int n = es - s;
    char *c = malloc(n + 1);
    assert(c);
    strncpy(c, s, n);
    c[n] = 0;
    return c;
}

/**
recebe inicio da string e retorna arvore de execução
*/
struct cmd *parsecmd(char *s) {
    char *es;
    struct cmd *cmd;

    es = s + strlen(s);  // aponta para o fim da string
    cmd = parseline(
        &s, es);  // organizar a arvore de execução/argumentos dos comandos
    peek(&s, es, "");  // Da uma limpada no comeco da string
    if (s != es) {     // A entrada não foi interamente consumida
        fprintf(stderr, "leftovers: %s\n", s);
        exit(-1);
    }
    return cmd;
}

/**
Retorna o comando com os argv
*/
// ps = tem permição para alterar o valor de *s
struct cmd *parseline(char **ps, char *es) {
    struct cmd *cmd;
    cmd = parsepipe(ps, es);
    return cmd;
}

/**
Retorna o comando com os argv
*/
struct cmd *parsepipe(char **ps, char *es) {
    struct cmd *cmd;

    cmd = parseexec(ps, es);
    if (peek(ps, es, "|")) {
        gettoken(ps, es, 0, 0);
        cmd = pipecmd(cmd, parsepipe(ps, es));
    }
    return cmd;
}

/**
Trata os casos em que o comando é um redirecionamento
*/
struct cmd *parseredirs(struct cmd *cmd, char **ps, char *es) {
    int tok;
    char *q, *eq;

    while (peek(ps, es, "<>")) {
        tok = gettoken(ps, es, 0, 0);
        if (gettoken(ps, es, &q, &eq) != 'a') {
            fprintf(stderr, "missing file for redirection\n");
            exit(-1);
        }
        switch (tok) {
            case '<':
                cmd = redircmd(cmd, mkcopy(q, eq), '<');
                break;
            case '>':
                cmd = redircmd(cmd, mkcopy(q, eq), '>');
                break;
        }
    }
    return cmd;
}

/**
Retorna execcmd, com o type e os parametros dele no argv
*/
struct cmd *parseexec(char **ps, char *es) {
    char *q, *eq;
    int tok, argc;
    struct execcmd *cmd;
    struct cmd *ret;

    ret = execcmd();
    cmd = (struct execcmd *)ret;  // tem type e argv

    argc = 0;
    ret = parseredirs(ret, ps, es);
    while (!peek(ps, es, "|")) {  // verifica se o comando não é um pipe
        if ((tok = gettoken(ps, es, &q, &eq)) == 0) break;
        if (tok != 'a') {
            fprintf(stderr, "syntax error\n");
            exit(-1);
        }
        cmd->argv[argc] = mkcopy(q, eq);
        argc++;
        if (argc >= MAXARGS) {
            fprintf(stderr, "too many args\n");
            exit(-1);
        }
        ret = parseredirs(ret, ps, es);
    }
    cmd->argv[argc] = 0;
    return ret;
}

// vim: expandtab:ts=2:sw=2:sts=2

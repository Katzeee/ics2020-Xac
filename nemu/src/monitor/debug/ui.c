#include <isa.h>
#include "expr.h"
#include "watchpoint.h"
#include <memory/paddr.h>

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint64_t);
int is_batch_mode();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);
static int cmd_si(char *args);
static int cmd_info(char *args);
static int cmd_x(char *args);
static int cmd_p(char *args);
static int cmd_w(char *args);
static int cmd_d(char *args);

static struct {
  char *name;
  char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
	{ "si", "Step N commands", cmd_si },
	{ "info", "info r for register, info w for watchpoint", cmd_info },
	{ "x", "Print continuous 4N bytes data from the given address", cmd_x },
	{ "p", "Print the answer of the expression", cmd_p },
	{ "w", "Pause the program when the value of the watchpoint changes", cmd_w },
	{ "d", "Delete the Nth watchpoint", cmd_d }

  /* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_info(char *args)
{
  char *arg = strtok(NULL, " ");
	if (arg == NULL) {
		Log("no enough arguments");
		return 0;
	}
	if (strcmp(arg, "r") == 0) {
		isa_reg_display();
//		printf("eax\t0x%-10x\necx\t0x%-10x\nedx\t0x%-10x\nebx\t0x%-10x\nesp\t0x%-10x\nebp\t0x%-10x\nesi\t0x%-10x\nedi\t0x%-10x\n", cpu.eax, cpu.ecx, cpu.edx, cpu.ebx, cpu.esp, cpu.ebp, cpu.esi, cpu.edi);
	} else if (strcmp(arg, "w") == 0) {
		Log("print watchpoint");
	}
	return 0;
}

static int cmd_x(char *args)
{
	char *arg = strtok(NULL, " ");
	int n;
	if (arg == NULL) {
		printf("No enough arguments!\n");
		return 0;
	}
	n = atoi(arg);
	Log("N = %d", n);
	arg = strtok(NULL, " ");
	Log("The second argument is %s", arg);
	char arghead[3] = {0};
	strncpy(arghead, arg, 2); //获得表达式前两个字符，判断表达式的形式
	Log("The first two letter is %s", arghead);
	if (arghead[0] == '$') {
		Log("register branch"); //寄存器
		char regname[6] = "R_";
	  strncat(regname, arg + 1, 3); //防止缓冲区溢出
		Log("%s", regname);
	} else if (strcmp(arghead, "0x") == 0) {
		Log("base 16 branch"); //16进制数
		paddr_t addr;
		sscanf(arg, "%x", &addr);
		Log("addr is 0x%x", addr);
		for (int i = 1 ; i <= n ; ++ i) { //连续输出n个
			word_t result = paddr_read(addr + i, 4); //以四字节为输出格式
			printf("0x%08x    ", result);
			if (i % 4 == 0) { //每输出4个则换行
				printf("\n");
			}
		}
		printf("\n"); //全部输出完成后换行
	} else {
		printf("Invalid arguments!\n");
	}
	return 0;
}

static int cmd_p(char *args)
{
	return 0;
}

static int cmd_w(char *args)
{
	return 0;
}

static int cmd_d(char*args)
{
	return 0;
}

static int cmd_si(char *args)
{
	char *arg = strtok(NULL, " ");
	int n;
	if (arg == NULL) {
		n = 1;
	}
	else {
		n = atoi(args);
	}
	cpu_exec(n);
	Log("%d", n); //测试打印n
	return 0;
}

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void ui_mainloop() {
  if (is_batch_mode()) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef HAS_IOE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

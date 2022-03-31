# pa1
## RTFSC
选择了x86作为isa进行实验，第一步要完成x86寄存器的实现
需要更改`./nemu/include/isa/x86.h`
考虑到`_16`访问`_32`的低16bits部分，则将原`struct { } gpr[8]`改为`union`
以及要考虑到用寄存器名字能访问和`_32`相同的位置地址，所以用匿名union将两个再包裹起来
但是ecx在eax后4个字节处，因此还需要用struct包裹所有寄存器名字的定义
至此x86的寄存器定义完成
```bash
typedef struct {
	union {
 	 union {
  	 uint32_t _32;
   	 uint16_t _16;
   	 uint8_t _8[2];
  	} gpr[8];
	 struct {
  	rtlreg_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
	 };
	};

  vaddr_t pc;
} x86_CPU_state;
```
## 基础设施
### `cmd_help`  
首先在`cmd_table`结构体中添加命令的name，description和handler
该结构体用于实现help命令打印命令相关信息
### `cmd_si`
根据`./nemu/src/monitor/cpu-exec.c`中函数`cpu_exec(uint64_t n)`发现pc寄存器的变化在for循环中和传入的n有关
因此只需调用该函数传入相应的n则可以做到单步执行
用strtok函数获取并截断每次传入的参数，其中第一次调用解析字符串在`./nemu/src/monitor/debug/ui.c`的`ui_mainloop()`中
因此获取si N命令中的N只需通过`strtok(NULL, " "); //以空格为分隔符获取第二段字符串的起始地址`来获取
当arg为空时，n默认为1，否贼通过函数atoi将字符串转为int并调用`cpu_exec(n)`已完成`cmd_si`，最终代码如下
```c
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
	Log("%d", n);
	return 0;
}

```
### `cmd_info`
要注意对arg为NULL的检查必须放在最前面，因为没有办法对一个NULL进行strcmp，以及同时要注意r和w需要用双引号，这样才会被编译器解释为`char*`，用单引号将会被解释为`int`
#### `info r`
实现info r非常简单，只需注意%-10x可以左对齐并占10个位置以16进制输出数据即可
调用`./nemu/src/isa/x86/reg.c`中的`isa_reg_display()`完成
其代码应为
```c
void isa_reg_display() {
		printf("eax\t0x%-10x\necx\t0x%-10x\nedx\t0x%-10x\nebx\t0x%-10x\nesp\t0x%-10x\nebp\t0x%-10x\nesi\t0x%-10x\nedi\t0x%-10x\n", cpu.eax, cpu.ecx, cpu.edx, cpu.ebx, cpu.esp, cpu.ebp, cpu.esi, cpu.edi);
}
```
### `cmd_x`
想要printf一个`char *arg`的变量，采取`printf("%s", arg);`的形式输出，因为字符串在直接作为参数传递时会被解释成首字符的地址
将0输出成00即采用`%02x`的形式，右对齐，宽度为2且不够时补0
若考虑表达式仅为一个十六进制数，则可以直接调用`paddr_read`进行内存的访问，注意需要包含头文件`<memory/paddr.h>``cmd_x()`的部分代码如下
```c
paddr_t addr;
sscanf(arg, "%x", &addr);
Log("addr is 0x%x", addr);
for (int i = 1 ; i <= n ; ++ i) {
	word_t result = paddr_read(addr + i - 1, 4);
	printf("0x%08x    ", result);
	if (i % 4 == 0) {
		printf("\n");
	}
}
printf("\n");
```
打印寄存器中的值调用`/nemu/src/isa/x86`里的`word_t isa_reg_str2val(const char *s, bool *success)`函数，接收寄存器的名字，返回其值，只需要遍历所有寄存器名称并一一比较即可，若失败则将success置为false，代码如下
```c
word_t isa_reg_str2val(const char *s, bool *success) {
	//通过名字取寄存器值
	*success = false;
	for(int i = 0 ; i < 8 ; i++)
	{
		if(strcmp(regsl[i], s) == 0)
		{
			*success = true;
			return reg_l(i);
		}
	}
  return 0;
}
```
对于`cmd_x`,需要考虑本次输入是要查询内存的值还是寄存器的值,可以根据第二个参数的前两位判断是哪一个再进行下一步操作,访问内存的部分上面已经给出，下面给出访问寄存器部分的代码
```c
bool success = false;
word_t reg_value = isa_reg_str2val(arg + 1, &success);
Log("%x", reg_value);

```
## 表达式求值
### 词法分析
首先需要在`./nemu/src/monitor/debug/expr.c`中补全算术表达式的正则表达式并添加`token_type`
```c
enum {
  TK_NOTYPE = 256,
	TK_EQ,
	TK_NUMBER,
};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {

  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
	{"-", '-'},						// minus
	{"/", '/'},						// divide
  {"==", TK_EQ},        // equal
	{"\\*", '*'},					// multiply
	{"\\(", '('},					// left parentheses
	{"\\)", ')'},					// right parentheses
	{"[0-9]+", TK_NUMBER}, //numbers
}
```
注意`+`的正则表达式是`\+`且c中字符串`\`写为`"\\"`因此加对应`"\\+"`
之后在`make_token`函数中补全代码，首先是每次进入时初始化`token`数组，补全switch语句，并在每次匹配上之后将数组下标加一，注意匹配到空格时需要减一再加一保证数组下标不变
```c
static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;
	memset(tokens, 0, sizeof(tokens));

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
				//char substr[32] = { 0 };
				//strncpy(substr, substr_start, substr_len);
        switch (rules[i].token_type) {
					case '+': 
						tokens[nr_token].type = '+';
						strncpy(tokens[nr_token].str, substr_start, substr_len);
						break;
					case '-':
						tokens[nr_token].type = '-';
						strncpy(tokens[nr_token].str, substr_start, substr_len);
						break;
					case '*':
						tokens[nr_token].type = '*';
						strncpy(tokens[nr_token].str, substr_start, substr_len);
						break;
					case '/':
						tokens[nr_token].type = '/';
						strncpy(tokens[nr_token].str, substr_start, substr_len);
						break;
					case TK_NUMBER:
						tokens[nr_token].type = TK_NUMBER;
						if(substr_len <= 32) {
						  strncpy(tokens[nr_token].str, substr_start, substr_len);
							break;
						} else {
							Log("Number is too large!");
							return 0;
						}
					case '(':
						tokens[nr_token].type = '(';
						strncpy(tokens[nr_token].str, substr_start, substr_len);
						break;
					case ')':
						tokens[nr_token].type = ')';
						strncpy(tokens[nr_token].str, substr_start, substr_len);
						break;
					case TK_NOTYPE:
						break;
          default: break;
        }
				++ nr_token;

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

```
测试采用`cmd_p`调用`expr`函数，`expr`调用`make_token`函数
```c
static int cmd_p(char *args)
{
  bool success = false;
  word_t result;
	result = expr(args, &success);
	return result;
}
```
在测试`make_token`函数的过程中，发现如果有空格则会使表达式无法识别，本以为是`switch`的问题，经检查才发现是如果还和之前的`cmd_info`等一样采取`strtok`来传递`expr`会导致空格作为分隔符无法获得后面的所有参数，因此直接传递`args`作为参数
### 递归求值
首先实现`check_parentheses()`，只需考虑左右括号是否匹配以及第一个左括号匹配的是不是最右边的括号，代码如下，注意总共有三种情况考虑，其中两种是good expression
```c
int check_parentheses(int p, int q) {
	/*返回-1表示bad expression，返回0表示good expression但不是被括号包裹，范围1表示good expression且被括号包裹*/
	int n = 0; //用来记录未被匹配的左括号数量
	bool flag = 0; 
	for (int i = p ; i < q ; i++) { //检查到倒数第二个位置
		if (tokens[i].type == '(') {
			++ n;
		} else if (tokens[i].type == ')') {
			-- n;
		}
		//每完成一次比较后检查n的值
		if (n < 0) { //说明出现了'())'的情况
			return -1; //bad expession
		} else if (tokens[p].type == '(' && n == 0) { 
			//说明开头的第一个括号被中间的右括号匹配了，并不是与最后一个括号匹配
			//此时要确保第一个是左括号，但是后面的表达式依然有可能是bad expression
			//因此此时不能直接return 0;应设flag变量去通知最后不能return 1;
			flag = 1;
		}
	}
	if (n == 0 && tokens[q].type != '(' && tokens[q].type != ')') { //表达式没有括号或者前面的括号匹配完了
		return 0;
	} else if (n != 1) { //在前面有括号的情况下全部检查完毕后若n不为1则说明不匹配
		return -1; //bad expression
	} else if (tokens[q].type != ')') { //在n=1的情况下，因为只检查到了倒数第二个位置，还需检查最后一个位置是不是右括号，以及这个右括号是不是和第一个左括号匹配
		return -1;
	} else if (flag == 1 || tokens[p].type != '(') { //说明最后的右括号不与第一个左括号匹配
		//第一个不是左括号，或者第一个是左括号但是被匹配掉了的情况
		return 0;
	}
	return 1;
}
```
获取主运算符的位置，首先括号内的不可能是主运算符，再就是加减优先级低，只要出现加减一定是比之前的任何运算符更晚计算，对于乘除，只有当前面所出现运算不是加减才可以移动主运算符指针
```c
int main_op(int p, int q) { //返回主运算符的下标
	int n = 0; //用来判断在括号内还是括号外
	int k = -1; //用来记录主运算符的位置，默认没有运算符
	for (int i = p ; i <= q ; ++ i) {
		if (tokens[i].type == '(') {
			++ n;
		} else if (tokens[i].type == ')') {
			-- n;
		}
		if (n != 0) { //括号内的运算符没有意义
			continue;
		}
		switch (tokens[i].type) {
			case '+':
			case '-': k = i; break;
			case '*':
			case '/': 
				if (k == -1 || tokens[k].type =='*' || tokens[k].type == '/') {
					k = i;
				}
				break;
			default: break;
		}
	}
	return k;
}
```
### 表达式求值测试
修改`nemu/tools/gen-expr/gen-expr.c`中的函数
首先是定义每次往buf的什么位置写入数据的index，bufp，同时注意在main函数中，每次循环结束都需要初始化buf和bufp
对于`gen_rand_expr()`有
```c
static inline void gen_rand_expr() {
	switch(rand() % 3) {
		//case 0: buf[bufp++] = '1'; break;
    case 0: gen_num(); break;

		case 1: buf[bufp++] = '('; gen_rand_expr(); buf[bufp++] = ')'; break;
		default: gen_rand_expr(); gen_rand_op(); gen_rand_expr(); break;
	}
}
```
对于`gen_rand_num()`有
```c
static inline void gen_num() {
	sprintf(buf + bufp, "%u", rand() % 10000);
	bufp = strlen(buf);
}
```
对于`gen_rand_op()`有
```c
static inline void gen_rand_op() {
	switch(rand() % 4) {
		case 0: buf[bufp++] = '+'; break;
		case 1: buf[bufp++] = '-'; break;
		case 2: buf[bufp++] = '*'; break;
		case 3: buf[bufp++] = '/'; break;
	}
}
```
同时要注意对于`fscanf(fp, "%d", &result)`的输出需要检查，若输出为-1则说明读取失败 
修改`main()`函数进行表达式求值的验证，在验证过程中发现若将`expr`在初始化之前调用则会发生Segmentation fault,不明原因，因此将验证放在初始化之后进行,同时注意修改。`nemu/tools/gen-expr/gen-expr.c`中token数组的长度。
对m`main()`函数进行如下修改，对表达式求值进行检验
```
#include"./monitor/debug/expr.h"
#include<stdio.h>
#include<stdlib.h>
void init_monitor(int, char *[]);
void engine_start();
int is_exit_status_bad();

int main(int argc, char *argv[]) {	
	  /* Initialize the monitor. */
  init_monitor(argc, argv);

  /* Start engine. */
  engine_start();

	FILE* fp = NULL;	
	fp = fopen("/home/xac/dev/ics2020-Xac/nemu/tools/gen-expr/input", "r");
	if (fp == NULL)
	{
		printf("null\n");
		exit(EXIT_FAILURE);
	}

	
	int res;
	char* exp = (char*)malloc(sizeof(char)*1000);
	char* buf = (char*)malloc(sizeof(char)*1000);

	size_t len;
	ssize_t read;
	bool success = false;
	if((read = getline(&buf, &len, fp)) != -1)
	{
			printf("getline:%zu\n", read);
			sscanf(buf, "%d %s", &res, exp);
			printf("%d\n", res);
			expr("-2955", &success);
	}


  return is_exit_status_bad();
}
```
注意此时对单目运算符`-`和`*`还没有进行区分
## 监视点
### 完善表达式求值

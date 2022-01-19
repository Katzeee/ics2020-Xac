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
若考虑表达式仅为一个十六进制数，则可以直接调用`paddr_read`进行内存的访问，注意需要包含头文件`<memory/paddr.h>`部分代码如下
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


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
实现info r非常简单，只需注意%-10x可以左对齐并占10个位置以16进制输出数据即可
要注意对arg为NULL的检查必须放在最前面，因为没有办法对一个NULL进行strcmp，以及同时要注意r和w需要用双引号，这样才会被编译器解释为`char*`，用单引号将会被解释为`int`

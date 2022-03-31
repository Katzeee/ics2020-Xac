#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static int bufp = 0; //用于指示每次往哪里写数据
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static inline void gen_rand_op() {
	switch(rand() % 4) {
		case 0: buf[bufp++] = '+'; break;
		case 1: buf[bufp++] = '-'; break;
		case 2: buf[bufp++] = '*'; break;
		case 3: buf[bufp++] = '/'; break;
	}
}

static inline void gen_num() {
	sprintf(buf + bufp, "%u", rand() % 10000);
	bufp = strlen(buf);
}

static inline void gen_rand_expr() {
	switch(rand() % 3) {
		//case 0: buf[bufp++] = '1'; break;
    case 0: gen_num(); break;

		case 1: buf[bufp++] = '('; gen_rand_expr(); buf[bufp++] = ')'; break;
		default: gen_rand_expr(); gen_rand_op(); gen_rand_expr(); break;
	}
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 10;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
		//每次循环都需要初始化buf和bufp
		memset(buf, '\0', strlen(buf));
		bufp = 0;

    gen_rand_expr();
    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) {
			i--;
			continue;
		}

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    if (fscanf(fp, "%d", &result) == -1) {
			pclose(fp);
			i--;
			continue;
		}
		pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}

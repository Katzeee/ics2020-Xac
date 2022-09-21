void init_monitor(int, char *[]);
void engine_start();
int is_exit_status_bad();

#include <stdio.h>
#include <string.h>
#include "monitor/debug/expr.h"

void test_monitor() {
  FILE* fd = fopen("/home/xac/ics2020-Xac/nemu/tools/gen-expr/input.txt", "r");

  if (fd == NULL) 
    return;
  char tmp[65532];
  word_t res = 0;
  bool suc = false;
  memset(tmp, 0, strlen(tmp));
  while(fscanf(fd, "%d %[^\n]", &res, tmp) != EOF) {
    if (expr(tmp, &suc) != res) {
      printf("%u, %s\n", res, tmp);
      assert(0);
    }
    memset(tmp, 0, strlen(tmp));
    suc = false;
  }
  fclose(fd);
}

int main(int argc, char *argv[]) {	


	/* Initialize the monitor. */
  init_monitor(argc, argv);

  /* Start engine. */
  engine_start();

  test_monitor();


 return is_exit_status_bad();

}

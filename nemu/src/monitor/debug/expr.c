#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256,
	TK_EQ,
	TK_NUMBER,
  /* TODO: Add more token types */

};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
	{"-", '-'},						// minus
	{"\\*", '*'},					// multiply
	{"/", '/'},						// divide
	{"\\(", '('},					// left parentheses 
	{"\\)", ')'},					// right parentheses 
	{"==", TK_EQ},        // equal
	{"[0-9]+", TK_NUMBER}, //numbers
};


bool check_parentheses(int, int);

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;
	memset(tokens, 0, sizeof(tokens)); //初始化token数组

  while (e[position] != '\0') { //只要字符串不空
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
							return false;
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
						-- nr_token; //检测到空格时需要减去1抵消下面的加1
						break;
          default: break;
        }
				++ nr_token; //每次处理完一个便将下标+1

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


word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
	Log("%d", check_parentheses(0, nr_token-1));
  /* TODO: Insert codes to evaluate the expression. */

  return 0;
}

bool check_parentheses(int p, int q) {
	int n = 0; //用来记录未被匹配的左括号数量
	for (int i = p ; i < q ; i++) { //检查到倒数第二个位置
		if (tokens[i].type == '(') {
			++ n;
		} else if (tokens[i].type == ')') {
			-- n;
		}
		//每完成一次比较后检查n的值
		if (n < 0) { //说明出现了'())'的情况
			return false;
		} else if (n == 0) { //说明开头的第一个括号并不是与最后一个括号匹配
			return false;
		}
	}
	if (n != 1 || tokens[q].type != ')') { //全部检查完毕后若n不为1则说明不匹配，因为只检查到了倒数第二个位置，还需检查最后一个位置是不是右括号
		return false;
	}
	return true;
}

word_t eval(int p, int q) {
	if (q < p) {
		assert(0);
	} else if (p == q) {
		return 0;
	} else if (check_parentheses(p, q) == true) {
		return eval(p + 1, q - 1);
	} else {
	}
	return 0;
}



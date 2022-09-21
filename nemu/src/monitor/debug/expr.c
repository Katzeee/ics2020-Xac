#include <isa.h>
#include <stdlib.h>

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


int check_parentheses(int, int);
int main_op(int , int); 
word_t eval(int, int);

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

static Token tokens[24] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;
	memset(tokens, 0, sizeof(tokens)); // initialize token

  while (e[position] != '\0') { //只要字符串不空
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        //Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
        //   i, rules[i].regex, position, substr_len, substr_len, substr_start); // debug info

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
						-- nr_token; // if detect space, decrese by 1 to neutralize the incresing after
						break;
          default: break;
        }
				++ nr_token; // have recognized one token, increse the index

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
  /* TODO: Insert codes to evaluate the expression. */
	int check_res = check_parentheses(0, nr_token-1); 
	if (check_res == -1) {
		Log("bad expression!");
		return 0;
	} else if (check_res == 0) {
		Log("Good expression with the main operator at %d", main_op(0, nr_token-1));
	} else if (check_res == 1) {
		Log("The whole expression is in parentheses");
	}
	uint32_t eval_res = eval(0, nr_token - 1);
	Log("The result of the expression is %d",eval_res);
  return 0;
}

int check_parentheses(int p, int q) {
	/* 返回-1表示bad expression，返回0表示good expression但不是被括号包裹，范围1表示good expression且被括号包裹 */
	/* return -1 means bad expression,
	 * return 0 means good expression but not surrounded by parentheses
	 * return 1 means good expression and surrounded by parentheses */
	int n = 0; // record the number of left patrentheses haven't been matched 用来记录未被匹配的左括号数量
	bool flag = 0; 
	for (int i = p ; i < q ; i++) { // until the last 2nd position 检查到倒数第二个位置
		if (tokens[i].type == '(') {
			++ n;
		} else if (tokens[i].type == ')') {
			-- n;
		}
		// check the value of n 每完成一次比较后检查n的值
		if (n < 0) { // means '())' 说明出现了'())'的情况
			return -1; //bad expession
		} else if (tokens[p].type == '(' && n == 0) { 
			/* means the ( at the first position is matched but not with the last position
			 * cannot deduct the finnal result because we don't know the last position's item
			 * so we cannot directly return 0, but set flag to 1 to tell not to return 1 */
			// 说明开头的第一个括号被中间的右括号匹配了，并不是与最后一个括号匹配
			//此时要确保第一个是左括号，但是后面的表达式依然有可能是bad expression
			//因此此时不能直接return 0;应设flag变量去通知最后不能return 1;
			flag = 1;
		}
	}
	if (n == 0 && tokens[q].type != '(' && tokens[q].type != ')') { // expression has no parenthese or all matched 表达式没有括号或者前面的括号匹配完了
		return 0;
	} else if (n != 1) { // all parenthese except the last item are matched but n != 1 means bad expression 在前面有括号的情况下全部检查完毕后若n不为1则说明不匹配
		return -1; //bad expression
	} else if (tokens[q].type != ')') { // check whether the last postion is right parenthese and is match with the first position's left parenthese 在n=1的情况下，因为只检查到了倒数第二个位置，还需检查最后一个位置是不是右括号，以及这个右括号是不是和第一个左括号匹配
		return -1;
	} else if (flag == 1 || tokens[p].type != '(') { // means the last parenthese is not matched with the first one 说明最后的右括号不与第一个左括号匹配
		// first item isn't left parenthese or has been matched with other right parenthese 第一个不是左括号，或者第一个是左括号但是被匹配掉了的情况
		return 0;
	}
	return 1;
}

int main_op(int p, int q) { // return the index of main operator 返回主运算符的下标
	int n = 0; // determine the op is surounded by parenthese or not 用来判断在括号内还是括号外
	int k = -1; // record the main op position, default no op 用来记录主运算符的位置，默认没有运算符
	for (int i = p ; i <= q ; ++ i) {
		if (tokens[i].type == '(') {
			++ n;
		} else if (tokens[i].type == ')') {
			-- n;
		}
		if (n != 0) { // means we are in parenthese and the operators in parenthese are meaningless 括号内的运算符没有意义
			continue;
		}
		switch (tokens[i].type) {
			case '+':
			case '-': k = i; break; // + and - have lowest priority 位置靠后的加减运算符必改变k指针位置
			case '*':
			case '/': 
				if (k == -1 || tokens[k].type =='*' || tokens[k].type == '/') {
					// * and / is the main op only if the main op now is not + or -
					// 位置靠后的乘除运算符需要进行判断当前所指的运算符是否不是加减
					k = i;
				}
				break;
			default: break;
		}
	}
	return k;
}

word_t eval(int p, int q) {
	if (q < p) {
		assert(0); //bad expression
	} else if (p == q) {
		int num = atoi(tokens[p].str);
		return num;
	} else if (check_parentheses(p, q) == 1) {
		return eval(p + 1, q - 1);
	} else {
		int op = main_op(p, q);
		int val1 = eval(p, op - 1);
		int val2 = eval(op + 1, q);
		switch (tokens[op].type) {
			case '+': return val1 + val2;
			case '-': return val1 - val2;
			case '*': return val1 * val2;
			case '/': return val1 / val2;
			default: assert(0);
		}
	}
}



#include <stdio.h>
int main(void) {
double _A = 10;
double _B = 4;
double _C;
double _RESULT;
_A = 5 + _A;
_B = _B - 2;
_C = _B + _A;
_C = _C * 8;
_C = _C / 3;
_C = (long long)_C % 5;
_RESULT = (_A + 2 - _B * _C) / (long long)3 % (long long)5;
printf("%g", _RESULT);
fputc('\n', stdout);
return 0;
return 0;
}

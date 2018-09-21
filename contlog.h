#include <string.h>

typedef int contlog_t;
#define FLS(val) (sizeof(val) == sizeof(long long) ? flsll(val) :	\
		  sizeof(val) == sizeof(long) ? flsl(val) :		\
		  sizeof(val) == sizeof(int) ? fls(val) :		\
		  fls(val & ((1 << 8*sizeof(val)) - 1)))
#define FFS(val) (sizeof(val) == sizeof(long long) ? ffsll(val) :	\
		  sizeof(val) == sizeof(long) ? ffsl(val) :		\
		  ffs(val))

#define SIGNED(T) (-(T)1 < 0)
#define SGNBIT_POS(T) (8*sizeof(T) - 1)
#define MINVAL(T) (SIGNED(T) ? -(T)1 << SGNBIT_POS(T) : 0)
#define MAXVAL(T) (~MINVAL(T))
#define MAX2PWR(T) ((T)1 << (SGNBIT_POS(T) - SIGNED(T)))

static inline int
lobit(contlog_t operand)
{
  unsigned int invpos = 8*sizeof(contlog_t) - (SIGNED(contlog_t) ? 1 : 0);
  return (operand ? FFS(operand) - 1 : invpos);
}


/* use continued logrithms, as described by Gosper,
 * but with logs a, b, c, d, ... represented as bits
 * 11^a 00^b 11^c 00^d ...
 * so that x < y for rationals x and y iff rep(x) < rep(y),
 * where rep(x) is the integer that stores the representation of x.
 */

static inline int
sum_overflows(contlog_t a, contlog_t b)
{
  return (SIGNED(contlog_t) ? (a >> SGNBIT_POS(contlog_t) == 0) : 1)?
    (b > MAXVAL(contlog_t)-a) : (b < MINVAL(contlog_t)-a);
}

contlog_t *contlog_fold(contlog_t operand, contlog_t box[], int nDims);

/*
 * Translate operand into fraction frac[] = {denom, numer}.
 */
static int
contlog_decode(contlog_t operand, contlog_t frac[])
{
  const unsigned int maxbits = 8*sizeof(operand);
  int neg;
  if (SIGNED(contlog_t)) {
    neg = (operand >> (maxbits-1));
    operand ^= operand << 1;
    operand &= ~((contlog_t)1 << SGNBIT_POS(contlog_t));
  }
  else {
    neg = 0;
    operand ^= operand << 1;
  }
  frac[0] = 1;
  frac[1] = 0;
  unsigned int numer = 0;
  unsigned int invpos = maxbits - (SIGNED(contlog_t) ? 1 : 0);
  unsigned int w = lobit(operand);
  while (w < invpos) {
    operand ^= (contlog_t)1 << w;
    numer ^= 1;
    frac[numer] += frac[numer^1];
    unsigned int next_w = lobit(operand);
    frac[numer] <<= next_w - w - 1;
    w = next_w;
  }
  return neg;
}

static void
contlog_load_arg(contlog_t operand, contlog_t frac[])
{
  const unsigned int maxbits = 8*sizeof(operand);
  int neg = contlog_decode(operand, frac);
  int shift = maxbits - FLS(frac[0] | frac[1]) - 1;
  if (neg)
    frac[1] = -frac[1];
  frac[0] <<= shift;
  frac[1] <<= shift;
}

void contlog_to_frac(contlog_t operand, contlog_t *n, contlog_t *d);
contlog_t frac_to_contlog(contlog_t n, contlog_t d);
contlog_t contlog_sqrt(contlog_t operand);

#define CONTLOG_CAST(T, val) ((sizeof(T) > sizeof(val) ?	\
	((T)(val) << 8*(sizeof(T)-sizeof(val))) :	\
	(T)((val) >> 8*(sizeof(val)-sizeof(T)))))


static contlog_t
contlog_incr(contlog_t operand)
{
  contlog_t frac[2];
  contlog_load_arg(operand, frac);
  if (sum_overflows(frac[0], frac[1])) {
    frac[1] = (frac[1] + 1) >> 1;
    frac[0] >>= 1;
  }
  frac[1] += frac[0];
  return frac_to_contlog(frac[0], frac[1]);
}

static contlog_t
contlog_add(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {frac[1], frac[0], frac[0], 0};
  contlog_t *b = box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

static contlog_t
contlog_sub(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {-frac[1], frac[0], frac[0], 0};
  contlog_t *b = box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

static contlog_t
contlog_mult(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {0, frac[0], frac[1], 0};
  contlog_t *b = box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

static contlog_t
contlog_div(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {0, frac[1], frac[0], 0};
  contlog_t *b = box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

static contlog_t
contlog_backdiv(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {frac[1], 0, 0, frac[0]};
  contlog_t *b = box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

static contlog_t
contlog_atnsum(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {frac[1], frac[0], frac[0], -frac[1]};
  contlog_t *b = box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

static contlog_t
contlog_harmsum(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {0, frac[1], frac[1], frac[0]};
  contlog_t *b = box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

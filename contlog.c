#include <string.h>
#include <stdio.h>

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

/* use continued logrithms, as described by Gosper,
 * but with logs a, b, c, d, ... represented as bits
 * 11^a 00^b 11^c 00^d ...
 * so that x < y for rationals x and y iff rep(x) < rep(y),
 * where rep(x) is the integer that stores the representation of x.
 */

static void
debug_print(contlog_t operand, contlog_t box[], int nDims)
{
  int sh = 4*sizeof(contlog_t);
  __uintmax_t val = operand;
  val -= val >> sh >> sh << sh << sh;
  fprintf(stderr, " %0*jx", 2*sizeof(operand), val);
  for (int b = 0; b < (1 << nDims); ++b) {
    __intmax_t val = box[b];
    val -= val >> sh >> sh << sh << sh;
    fprintf(stderr, " %10jx", val);
  }
  fprintf(stderr, "\n");
}

static inline int
sum_overflows(contlog_t a, contlog_t b)
{
  return (SIGNED(contlog_t) ? (a >> SGNBIT_POS(contlog_t) == 0) : 1)?
    (b > MAXVAL(contlog_t)-a) : (b < MINVAL(contlog_t)-a);
}

contlog_t *
contlog_fold(contlog_t operand, contlog_t box[], int nDims)
{
  const unsigned int maxbits = 8*sizeof(contlog_t);
  contlog_t hibit = (contlog_t)1 << (maxbits - 1);
  unsigned int idx_n1;		/* posn in box of coeff of 1 in numerator */
  int b, d = nDims - 1;
  int bit_d = 1 << d;
  if (SIGNED(contlog_t)) {
    if (operand & hibit) {
      for (b = 0; b < bit_d; ++b)
	box[b^bit_d] = -box[b^bit_d];
    }
    operand ^= operand << 1;
    idx_n1 = (operand & hibit) ? bit_d : 0;
    operand <<= 1;
  }
  else {
    idx_n1 = (operand & hibit) ? bit_d : 0;
    operand ^= operand << 1;
  }

  while (operand != 0) {
    debug_print(operand, box, nDims);

    /* Swap the with-d face of the box with the without-d
     * face, and let idx_nopd identify the position of the constant
     * coefficient of the numerator in the new without-d face.
     */
    unsigned int idx_nopd = idx_n1;
    idx_n1 ^= bit_d;

    /* Find the leftmost set bit position of operand and shift the bit
       out. */
    int shift = maxbits - FLS(operand);
    operand <<= shift + 1;

    /* Shift the coeffs without operand and add to them the corresponding
     * coeffs with operand.  If necessary, divide everything by 2 first to
     * avoid overflow.
     */
    int overflow = 0;
    for (b = 0; !overflow && b < bit_d; ++b) {
      contlog_t addend = box[idx_n1^b];
      if (shift > 0) {
	addend += 1 << (shift - 1);
	addend >>= shift;
      }
      overflow = sum_overflows(addend, box[idx_nopd^b]);
    }
    shift += overflow;
    for (b = 0; b < bit_d; ++b) {
      if (shift > 0) {
	box[idx_n1^b] += 1 << (shift - 1);
	box[idx_n1^b] >>= shift;
	box[idx_nopd^b] >>= overflow;
      }
      box[idx_n1^b] += box[idx_nopd^b];
    }
  }
  debug_print(operand, box, nDims);
  return &box[idx_n1];
}

static void
contlog_load_arg(contlog_t operand, contlog_t frac[])
{
  const unsigned int maxdigits = 8*sizeof(operand);
  int neg;
  if (SIGNED(contlog_t)) {
    neg = (operand >> (maxdigits-1));
    operand ^= operand << 1;
    operand &= ~((contlog_t)1 << SGNBIT_POS(contlog_t));
  }
  else {
    neg = 0;
    operand ^= operand << 1;
  }
  frac[0] = 0;
  frac[1] = 1;
  unsigned int numer = 1;
  unsigned int invpos = maxdigits - (SIGNED(contlog_t) ? 1 : 0);
  unsigned int w = operand ? FFS(operand) - 1 : invpos;
  while (w < invpos) {
    operand ^= (contlog_t)1 << w;
    numer ^= 1;
    frac[numer] += frac[numer^1];
    unsigned int next_w = operand ? FFS(operand) - 1 : invpos;
    frac[numer] <<= next_w - w - 1;
    w = next_w;
  }
  int shift = maxdigits - FLS(frac[0] | frac[1]) - 1;
  if (neg)
    frac[0] = -frac[0];
  frac[0] <<= shift;
  frac[1] <<= shift;
}

static void
contlog_to_frac_ubound(contlog_t operand, contlog_t frac[])
{
  const unsigned int maxdigits = 8*sizeof(operand);
  operand ^= (operand << 1) | 1;
  unsigned int numer = 1;
  unsigned int invpos = maxdigits - (SIGNED(contlog_t) ? 1 : 0);
  unsigned int w = operand ? FFS(operand) - 1 : invpos;
  frac[numer] = 1 << w;
  frac[numer^1] = 1;
  while (w < invpos) {
    operand ^= (contlog_t)1 << w;
    numer ^= 1;
    frac[numer] += frac[numer^1];
    unsigned int next_w = operand ? FFS(operand) - 1 : invpos;
    frac[numer] <<= next_w - w - 1;
    w = next_w;
  }
}

void
contlog_to_frac(contlog_t operand, contlog_t *n, contlog_t *d)
{
  int neg = operand < 0;
  if (neg)
    operand = -operand;

  const unsigned int maxbits = 8*sizeof(contlog_t);
  contlog_t hibit = (contlog_t)1 << (maxbits - 1);
  int numer = 1;
  contlog_t frac[2][2] = {{1, 0}, {0, 1}};

  if (operand != 0) {
    contlog_t bound[2][2];
    contlog_to_frac_ubound(operand-1, &bound[0][0]);
    contlog_to_frac_ubound(operand, &bound[1][0]);
    
    for (;;) {
      contlog_t val[2];
      for (int b = 0; b < 2; ++b) {
	val[b] = bound[b][numer] / bound[b][numer^1];
	bound[b][numer] %= bound[b][numer^1];
      }

      if (0 == operand % 2 && bound[numer^1][numer] == 0)
	/* The lower bound of the closed interval is found.  Make
	   sure the val fields are different to escape the loop.
	 */
	--val[numer^1];
      switch (val[numer] - val[numer^1]) {
      case 0:
	if (bound[numer^1][numer] != 0)
	  break;
	/* The lower bound of the open interval is unavailable, so
	 * add a small extra cf term without passing the upper bound.
	 */
	for (int b = 0; b < 2; ++b)
	  frac[numer][b] += val[numer] * frac[numer^1][b];
	numer ^= 1;
	val[numer^1] = bound[numer^1][numer] / bound[numer^1][numer^1];
	val[numer] = val[numer^1] + 1;
	break;
      case 1:
	if (bound[numer][numer] != 0 ||
	    0 == operand % 2)
	  break;
	/* The upper bound of the open interval is unavailable, so
	 * add one or two small extra cf terms to the lower bound.
	 */
	for (int b = 0; b < 2; ++b)
	  frac[numer][b] += val[numer^1] * frac[numer^1][b];
	numer ^= 1;
	if (bound[numer][numer] >= 2 * bound[numer][numer^1])
	  val[numer^1] = 1;
	else {
	  for (int b = 0; b < 2; ++b)
	    frac[numer][b] += frac[numer^1][b];
	  numer ^= 1;
	  val[numer^1] = bound[numer^1][numer] / 
	    (bound[numer^1][numer^1] - bound[numer^1][numer]);
	}
	val[numer] = val[numer^1] + 1;
	break;
      default:
	val[numer] = val[numer^1] + 1;
	break;
      }

      for (int b = 0; b < 2; ++b)
	frac[numer][b] += val[numer] * frac[numer^1][b];
      if (val[numer^1] < val[numer])
	break;
      numer ^= 1;
    }
  }
    
  *n = frac[numer][0];
  *d = frac[numer][1];
  
  if (neg)
    *n = -*n;
  }

contlog_t
frac_to_contlog(contlog_t n, contlog_t d)
{
  contlog_t operand = 0;
  int w = 8 * sizeof(contlog_t);
  unsigned int neg = 0;

  if (SIGNED(contlog_t)) {
    if (d >> SGNBIT_POS(contlog_t)) {
      d = -d;
      neg = (n > 0);
    }
    else if (d != 0) {
      neg = (n >> SGNBIT_POS(contlog_t)) != 0;
    }
    else
      neg = 1;
    if (n >> SGNBIT_POS(contlog_t))
      n = -n;
    operand |= neg << --w;
  }

  contlog_t frac[2];
  frac[neg] = d;
  frac[neg^1] = n;
  unsigned int numer = (frac[0] < frac[1]);

  while (frac[numer^1] != 0 && w > 0) {
    int shift = FLS(frac[numer]) - FLS(frac[numer^1]);
    if (frac[numer] >= frac[numer^1] << shift)
      ++shift;
    if (shift > w)
      break;
    w -= shift;
    if (numer)
      operand |= (((contlog_t)1 << shift) - 1) << w;
    numer ^= 1;
    frac[numer] <<= shift - 1;
    frac[numer^1] -= frac[numer];
  }
  if (numer)
    operand |= (contlog_t)1 << w;
  return operand;
}

/* is n + (r/abs(r)) * sqrt(abs(r)) negative? */
static int
surd_is_neg(contlog_t n, contlog_t r)
{
  return ((r < 0) ? (n < 0 || -r > n*n) :
	  (r != 0) ? (n < 0 && r < n*n) : 0);
}

#if 0
contlog_t
surd_to_contlog(contlog_t n, contlog_t d, contlog_t r)
{
  contlog_t operand = 0;
  int w = 8 * sizeof(contlog_t);
  unsigned int neg = 0;
  unsigned int neg_surd = surd_is_neg(n, r);

  if (SIGNED(contlog_t)) {
    if (d >> SGNBIT_POS(contlog_t)) {
      d = -d;
      neg = !neg_surd;
    }
    else if (d != 0)
      neg = neg_surd;

    if (neg_surd) {
      n = -n;
      r = -r;
    }
    operand |= neg << --w;
  }

}
#endif

#define CONTLOG_CAST(T, val) ((sizeof(T) > sizeof(val) ?	\
	((T)(val) << 8*(sizeof(T)-sizeof(val))) :	\
	(T)((val) >> 8*(sizeof(val)-sizeof(T)))))

/*-----------------------------------------------------------------*\
 * Compute the integer square root and remainder
 */
typedef struct {
  contlog_t quot;
  contlog_t rem;
} contlog_div_t;

static inline contlog_div_t
isqrt(contlog_t rem)
{
  //int square = rem;
  int place = (FLS(rem) + 1) & ~1;
  contlog_div_t res = {0, rem};

  while (place > 0) {
    res.quot /= 2;
    place -= 2;
    contlog_t bit = (contlog_t)1 << place;
    contlog_t newrem = res.rem - (bit + 2*res.quot);
    if (newrem >= 0) {
      res.rem = newrem;
      res.quot += bit;
    }
  }
  return (res);
}
#if 1
contlog_t
contlog_sqrt(contlog_t operand)
{
  const unsigned int maxbits = 8*sizeof(contlog_t);
  contlog_t hibit = (contlog_t)1 << (maxbits - 1);

  contlog_t frac[2];
  contlog_load_arg(operand, frac);
  contlog_t n = frac[0];
  contlog_t d = frac[1];
  int shift = maxbits - 1;
  if (n >= d) {
    shift -= FLS(d);
    shift &= ~1;
    d <<= shift;
  }
  else {
    shift -= FLS(n);
    shift &= ~1;
    n <<= shift;
  }
  contlog_t n1 = n >> maxbits/2;
  contlog_t n2 = n - (n1 << maxbits/2);
  contlog_t d1 = d >> maxbits/2;
  contlog_t d2 = d - (d1 << maxbits/2);
  contlog_t D = n2*d2 >> maxbits/2;
  D = (D + n1*d2+n2*d1) >> maxbits/2;
  D += n1*d1;
  contlog_div_t s = isqrt(D);
  contlog_t geomean = (s.quot << (maxbits-shift)/2) + (s.rem << (maxbits-shift)/2) / (2 * s.quot + 1);
  contlog_t box[] = {frac[0], 0, 0, frac[1]};
  unsigned int idx_n1 = frac[0] >= frac[1] ? 3 : 0;
  int b;
  int w = 8 * sizeof(contlog_t) - 1;
  operand = 0;

  while (w > 0 && box[idx_n1] != 0) {
    debug_print(operand, box, 2);
    contlog_t avg = geomean + (box[idx_n1^1] - geomean) / 2;
    int shift = FLS(avg) - FLS(box[idx_n1]);
    if (avg >= box[idx_n1] << shift)
      ++shift;
    ++shift;
    if (shift > w)
      break;
    w -= shift;
    if (idx_n1)
      operand |= (((contlog_t)1 << shift) - 1) << w;
    --shift;

    geomean >>= shift;
    unsigned int idx_nopd;

    idx_nopd = idx_n1;
    idx_n1 ^= 1;
    int overflow = 0;
    for (b = 0; !overflow && b < 4; b+=2) {
      contlog_t addend = box[idx_n1^b];
      if (shift > 0) {
	addend += 1 << (shift - 1);
	addend >>= shift;
      }
      overflow = sum_overflows(addend, -box[idx_nopd^b]);
    }
    shift += overflow;
    for (b = 0; b < 4; b+=2) {
      if (shift > 0) {
	box[idx_n1^b] += 1 << (shift - 1);
	box[idx_n1^b] >>= shift;
	box[idx_nopd^b] >>= overflow;
      }
      box[idx_n1^b] -= box[idx_nopd^b];
    }

    idx_nopd = idx_n1;
    idx_n1 ^= 2;


    overflow = 0;
    for (b = 0; !overflow && b < 2; ++b) {
      contlog_t addend = box[idx_n1^b];
      if (shift > 0) {
	addend += 1 << (shift - 1);
	addend >>= shift;
      }
      overflow = sum_overflows(addend, box[idx_nopd^b]);
    }
    shift += overflow;
    for (b = 0; b < 2; ++b) {
      if (shift > 0) {
	box[idx_n1^b] += 1 << (shift - 1);
	box[idx_n1^b] >>= shift;
	box[idx_nopd^b] >>= overflow;
      }
      box[idx_n1^b] += box[idx_nopd^b];
    }
#if 0
    contlog_t mask = 0;
    for (b = 0; b < 4; ++b)
      mask |= box[b] >= 0 ? box[b] : -box[b];
    shift = maxbits - 1 - FLS(mask);
    for (b = 0; b < 4; ++b)
      box[b] <<= shift;
#endif
  }
  if (idx_n1)
    operand |= (contlog_t)1 << w;
  return operand;
}
#else

static contlog_t
gcd(contlog_t a, contlog_t b)
{
  contlog_t pair[] = {a, b};
  int bigger = a < b;
  while (pair[!bigger] != 0) {
    pair[bigger] %= pair[!bigger];
    bigger = !bigger;
  }
  return pair[bigger];
}
contlog_t
contlog_sqrt(contlog_t operand)
{
  contlog_t n,d;
  contlog_to_frac(operand, &n, &d);
  contlog_t nd = n * d;
  contlog_div_t rt_nd = isqrt(nd);
  if (rt_nd.rem == 0)
    return frac_to_contlog(rt_nd.quot, d);

  int w = 8 * sizeof(contlog_t) - 1;
  contlog_t r = 1;
  printf("\n");
  
  int numer = n >= d;
  if (!numer)
    d = n;

  n = 0;
  operand = 0;
  while (w > 0) {
    /* Find the log of (n + r*sqrt(nd)) / d */
    int shift = FLS((n + r*rt_nd.quot + r*rt_nd.rem/(2 * rt_nd.quot + 1))/d);
    if (shift > w)
      break;
    w -= shift;
    if (numer)
      operand |= (((contlog_t)1 << shift) - 1) << w;
    numer ^= 1;

    contlog_t Rd = d << (shift - 1);
    d = r*r*nd - n*n + Rd * (2*n - Rd);
    n = Rd * (Rd - n);
    r *= Rd;
    contlog_t g = gcd(gcd(n, d), r);
    n /= g;
    d /= g;
    r /= g;
    printf("n %d\td %d\tr %d\tg %d\n", n, d, r, g);
  }
  if (numer)
    operand |= (contlog_t)1 << w;
  return operand;
}
#endif
contlog_t
contlog_incr(contlog_t operand)
{
  contlog_t frac[2];
  contlog_load_arg(operand, frac);
  if (sum_overflows(frac[0], frac[1])) {
    frac[0] = (frac[0] + 1) >> 1;
    frac[1] >>= 1;
  }
  frac[0] += frac[1];
  return frac_to_contlog(frac[0], frac[1]);
}

contlog_t
contlog_add(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {frac[0], frac[1], frac[1], 0};
  contlog_t *b = box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

contlog_t
contlog_sub(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {-frac[0], frac[1], frac[1], 0};
  contlog_t *b = box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

contlog_t
contlog_mult(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {0, frac[1], frac[0], 0};
  contlog_t *b = box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

contlog_t
contlog_div(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {0, frac[0], frac[1], 0};
  contlog_t *b = box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

contlog_t
contlog_backdiv(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {frac[0], 0, 0, frac[1]};
  contlog_t *b = box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

contlog_t
contlog_atnsum(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {frac[0], frac[1], frac[1], -frac[0]};
  contlog_t *b= box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

contlog_t
contlog_harmsum(contlog_t op0, contlog_t op1)
{
  contlog_t frac[2];
  contlog_load_arg(op1, frac);
  contlog_t box[] = {0, frac[0], frac[0], frac[1]};
  contlog_t *b= box;
  b = contlog_fold(op0, b, 2);
  return frac_to_contlog(b[0], b[1]);
}

contlog_t
sscan_frac(const char *s)
{
  contlog_t operand;
  if (strchr(s, '/')) {
    __intmax_t nx, dx;
    sscanf(s, "%jd/%jd", &nx, &dx);
    contlog_t n = nx, d = dx;
    operand = frac_to_contlog(n, d);
  }
  else {
    __intmax_t opx;
    sscanf(s, "%jx", &opx);
    operand = opx;
    operand <<= 8*sizeof(contlog_t) - 4*strlen(s);
  }
  return operand;
}

void
print_frac(contlog_t operand)
{
  contlog_t n, d;
  contlog_to_frac(operand, &n, &d);
  __intmax_t nx = n;
  __intmax_t dx = d;
  __intmax_t opx = operand;
  int sh = 4*sizeof(contlog_t);
  opx -= opx >> sh >> sh << sh << sh;
  printf("%jd/%jd (%0*jx) = %18.12f\n", nx, dx, 2*sizeof(contlog_t), opx, (double)n/d);
}

static int usage(const char *cmd)
{
printf("Usage: '%s x' or '%s x op` or '%s x op y', where:\n"
       "x and y are rationals, expressed as fractions like 3/7 or as hex representations,\n"
       "and\n"
       "op is from '+', '-', '*', '/', '\\' (backward division), 'H' (harmonic sum), 'T' (arctangent sum).\n"
       "where unary ops are '+' (increment) and '/' (square root).\n",

       cmd, cmd, cmd);
    return 0;
}

static void
splat(contlog_t i, contlog_t j, contlog_t n, contlog_t d)
{
  printf("%08x\t%08x\t%d/%d\n", i, j, n, d);
}

static void
splat2(contlog_t i, contlog_t j, contlog_t k)
{
  int ni, di;
  contlog_to_frac(i, &ni, &di);
  int nj, dj;
  contlog_to_frac(j, &nj, &dj);
  int nk, dk;
  contlog_to_frac(k, &nk, &dk);

  printf("x = %08x (%d/%d), sqrt(x)=%08x (%d/%d), x/sqrt(x) = %08x (%d/%d)\n",
	 i, ni, di, j, nj, dj, k, nk, dk);
}

int main(int argc, char *argv[])
{
  if (argc == 1) {
    contlog_t i = 2;
    while (i < 1<<30) {
      int n, d;
      contlog_to_frac(i, &n, &d);
      contlog_t j = frac_to_contlog(n, d);
      if (i != j)
	splat(i, j, n, d);
      j = contlog_sqrt(i);
      contlog_t k = contlog_div(i,j);
      if (j - k > 2 || k - j > 2)
	splat2(i, j, k);
      ++i;
    }
  }

  if (argc == 1)
    return usage(argv[0]);

  int a = sscan_frac(argv[1]);
  printf("x: ");
  print_frac(a);
  if (argc == 2)
    return 0;

  char op = argv[2][0];

  if (argc == 3) {
    switch (op) {
    case '+':
      printf("x+1: ");
      print_frac(contlog_incr(a));
      break;

    case '/':
      printf("sqrt(x): ");
      contlog_t r = contlog_sqrt(a);
      print_frac(r);

      printf("x/sqrt(x): ");
      print_frac(contlog_div(a, r));
      break;

    default:
      break;
    }
    return 0;
  }
  int b = sscan_frac(argv[3]);
  printf("y: ");
  print_frac(b);
  
  switch (op) {
  case '+':
    printf("x+y: ");
    print_frac(contlog_add(a, b));
    break;
    
  case '-':
    printf("x-y: ");
    print_frac(contlog_sub(a, b));
    break;
    
  case '*':
    printf("x*y: ");
    print_frac(contlog_mult(a, b));
    break;
    
  case '/':
    printf("x/y: ");
    print_frac(contlog_div(a, b));
    break;
    
  case '\\':
    printf("x\\y: ");
    print_frac(contlog_backdiv(a, b));
    break;
    
  case 'T':
    printf("(x+y)/(1-xy): ");
    print_frac(contlog_atnsum(a, b));
    break;
    
  case 'H':
    printf("xy/(x+y): ");
    print_frac(contlog_harmsum(a, b));
    break;
    
  default:
    break;
  }

  return 0;
}

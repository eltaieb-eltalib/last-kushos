/*
 * PROJECT:     Kush 1.1
 * LICENSE:     MIT
 * PURPOSE:     C99 math and <fenv.h> functions that were "-stub" exports in
 *              ucrtbase (they returned 0: lround(100.0) == 0, cbrtf(x) == 0, ...).
 *              Modern programs (Firefox 160 and its Rust code) call them.
 *
 * On Windows long double == double, so the *l variants share the double code.
 * This file is compiled with -fno-builtin so GCC does not turn the bodies into
 * calls to themselves. It deliberately does not include <math.h>.
 */

#include <stdint.h>

double __cdecl floor(double);
double __cdecl fmod(double, double);
double __cdecl pow(double, double);
double __cdecl log(double);
double __cdecl exp(double);
double __cdecl sqrt(double);
double __cdecl frexp(double, int *);
double __cdecl trunc(double);

/* ---- bit helpers ---- */
static uint64_t d2u(double d) { union { double d; uint64_t u; } x; x.d = d; return x.u; }
static double u2d(uint64_t u) { union { double d; uint64_t u; } x; x.u = u; return x.d; }
static int k_isnan(double x) { return (d2u(x) & 0x7fffffffffffffffULL) > 0x7ff0000000000000ULL; }
static int k_isinf(double x) { return (d2u(x) & 0x7fffffffffffffffULL) == 0x7ff0000000000000ULL; }
static double k_fabs(double x) { return u2d(d2u(x) & 0x7fffffffffffffffULL); }
#define K_NAN u2d(0x7ff8000000000000ULL)
#define K_INF u2d(0x7ff0000000000000ULL)

/* ---- sign / classification ---- */
double __cdecl copysign(double x, double y)
{ return u2d((d2u(x) & 0x7fffffffffffffffULL) | (d2u(y) & 0x8000000000000000ULL)); }
float __cdecl copysignf(float x, float y) { return (float)copysign(x, y); }
double __cdecl copysignl(double x, double y) { return copysign(x, y); }

double __cdecl nan(const char *tag) { (void)tag; return K_NAN; }
float __cdecl nanf(const char *tag) { (void)tag; return (float)K_NAN; }

double __cdecl fmax(double a, double b)
{ if (k_isnan(a)) return b; if (k_isnan(b)) return a; return a > b ? a : b; }
float __cdecl fmaxf(float a, float b) { return (float)fmax(a, b); }
double __cdecl fmin(double a, double b)
{ if (k_isnan(a)) return b; if (k_isnan(b)) return a; return a < b ? a : b; }
float __cdecl fminf(float a, float b) { return (float)fmin(a, b); }
double __cdecl fdim(double a, double b)
{ if (k_isnan(a) || k_isnan(b)) return K_NAN; return a > b ? a - b : 0.0; }
float __cdecl fdimf(float a, float b) { return (float)fdim(a, b); }

/* ---- rounding ---- */
/* rint/nearbyint honour the current rounding mode via the 2^52 trick. */
double __cdecl rint(double x)
{
    const double two52 = 4503599627370496.0;
    volatile double t;
    if (k_isnan(x) || k_fabs(x) >= two52) return x;
    if (x >= 0) { t = x + two52; t -= two52; }
    else        { t = x - two52; t += two52; }
    return copysign(t, x);
}
float __cdecl rintf(float x) { return (float)rint(x); }
double __cdecl nearbyint(double x) { return rint(x); }
float __cdecl nearbyintf(float x) { return (float)rint(x); }
long long __cdecl llrint(double x) { return (long long)rint(x); }
long long __cdecl llrintf(float x) { return (long long)rint(x); }

static double k_round(double x) /* half away from zero */
{
    double t;
    if (k_isnan(x) || k_isinf(x)) return x;
    t = floor(k_fabs(x));
    if (k_fabs(x) - t >= 0.5) t += 1.0;
    return copysign(t, x);
}
long __cdecl lround(double x) { return (long)k_round(x); }
long __cdecl lroundf(float x) { return (long)k_round(x); }
long long __cdecl llround(double x) { return (long long)k_round(x); }
long long __cdecl llroundf(float x) { return (long long)k_round(x); }

float __cdecl modff(float x, float *ip)
{
    double i = trunc(x);
    if (ip) *ip = (float)i;
    if (k_isinf(x)) return (float)copysign(0.0, x);
    return (float)copysign(x - i, x);
}

/* ---- exponent ---- */
#define K_ILOGB0   (-2147483647 - 1)
#define K_ILOGBNAN 2147483647
int __cdecl ilogb(double x)
{
    int e;
    if (x == 0) return K_ILOGB0;
    if (k_isnan(x)) return K_ILOGBNAN;
    if (k_isinf(x)) return 2147483647;
    frexp(x, &e);
    return e - 1;
}
int __cdecl ilogbf(float x) { return ilogb(x); }
double __cdecl logb(double x)
{
    int e;
    if (x == 0) return -K_INF;
    if (k_isnan(x)) return x;
    if (k_isinf(x)) return K_INF;
    frexp(x, &e);
    return (double)(e - 1);
}
float __cdecl logbf(float x) { return (float)logb(x); }

/* ---- remainder ---- */
double __cdecl remquo(double x, double y, int *quo)
{
    double n, r;
    if (k_isnan(x) || k_isnan(y) || k_isinf(x) || y == 0) { if (quo) *quo = 0; return K_NAN; }
    n = rint(x / y);
    r = x - n * y;
    if (k_fabs(r) > k_fabs(y) * 0.5) { /* guard against rounding of x/y */
        double adj = (r > 0) == (y > 0) ? 1.0 : -1.0;
        n += adj; r -= adj * y;
    }
    if (quo) *quo = (int)fmod(n, 8.0);
    return r == 0 ? copysign(0.0, x) : r;
}
float __cdecl remquof(float x, float y, int *quo) { return (float)remquo(x, y, quo); }
double __cdecl remainder(double x, double y) { int q; return remquo(x, y, &q); }
float __cdecl remainderf(float x, float y) { int q; return (float)remquo(x, y, &q); }

/* ---- cube root, hyperbolic inverses ---- */
double __cdecl cbrt(double x)
{
    double a, y;
    if (x == 0 || k_isnan(x) || k_isinf(x)) return x;
    a = k_fabs(x);
    y = pow(a, 1.0 / 3.0);
    y = y - (y * y * y - a) / (3.0 * y * y); /* one Newton step */
    return copysign(y, x);
}
float __cdecl cbrtf(float x) { return (float)cbrt(x); }
double __cdecl cbrtl(double x) { return cbrt(x); }

double __cdecl log1p(double x)
{
    volatile double u = 1.0 + x;
    if (k_isnan(x)) return x;
    if (x == -1.0) return -K_INF;
    if (x < -1.0) return K_NAN;
    if (u == 1.0) return x;
    return log(u) * x / (u - 1.0); /* Goldberg's correction */
}
float __cdecl log1pf(float x) { return (float)log1p(x); }

double __cdecl expm1(double x)
{
    volatile double u = exp(x);
    if (k_isnan(x)) return x;
    if (u == 1.0) return x;
    if (u - 1.0 == -1.0) return -1.0;
    if (k_isinf(u)) return u;
    return (u - 1.0) * x / log(u);
}
float __cdecl expm1f(float x) { return (float)expm1(x); }

double __cdecl asinh(double x)
{
    double a = k_fabs(x);
    if (k_isnan(x) || k_isinf(x) || x == 0) return x;
    if (a > 1e150) return copysign(log(a) + 0.69314718055994530942, x);
    return copysign(log1p(a + a * a / (1.0 + sqrt(1.0 + a * a))), x);
}
float __cdecl asinhf(float x) { return (float)asinh(x); }
double __cdecl asinhl(double x) { return asinh(x); }

double __cdecl acosh(double x)
{
    if (k_isnan(x)) return x;
    if (x < 1.0) return K_NAN;
    if (x > 1e150) return log(x) + 0.69314718055994530942;
    return log(x + sqrt(x * x - 1.0));
}
float __cdecl acoshf(float x) { return (float)acosh(x); }
double __cdecl acoshl(double x) { return acosh(x); }

double __cdecl atanh(double x)
{
    double a = k_fabs(x);
    if (k_isnan(x)) return x;
    if (a > 1.0) return K_NAN;
    if (a == 1.0) return copysign(K_INF, x);
    return copysign(0.5 * log1p(2.0 * a / (1.0 - a)), x);
}
float __cdecl atanhf(float x) { return (float)atanh(x); }
double __cdecl atanhl(double x) { return atanh(x); }

/* ---- error function ----
 * |x| < 2: Maclaurin series (alternating, at most ~1 digit lost).
 * x >= 2: continued fraction for erfc evaluated with modified Lentz. */
static double k_erf_series(double a)
{
    double term = a, sum = a, x2 = a * a;
    int n;
    for (n = 1; n < 100; n++)
    {
        term *= -x2 / n;
        sum += term / (2 * n + 1);
        if (k_fabs(term) < 1e-17 * k_fabs(sum)) break;
    }
    return sum * 1.12837916709551257390; /* 2/sqrt(pi) */
}
static double k_erfc_cf(double x) /* x >= 2 */
{
    /* erfc(x) = exp(-x^2)/sqrt(pi) * 1/(x + (1/2)/(x + 1/(x + (3/2)/(x + 2/(x + ...))))) */
    const double tiny = 1e-300;
    double f = x, C = x, D = 0, delta;
    int n;
    for (n = 1; n < 500; n++)
    {
        double an = n * 0.5;
        D = x + an * D; if (D == 0) D = tiny; D = 1.0 / D;
        C = x + an / C; if (C == 0) C = tiny;
        delta = C * D; f *= delta;
        if (k_fabs(delta - 1.0) < 1e-16) break;
    }
    return exp(-x * x) / f * 0.56418958354775628695; /* 1/sqrt(pi) */
}
double __cdecl erf(double x)
{
    double a = k_fabs(x);
    if (k_isnan(x)) return x;
    if (a < 2.0) return copysign(k_erf_series(a), x);
    if (a > 6.0) return copysign(1.0, x);
    return copysign(1.0 - k_erfc_cf(a), x);
}
float __cdecl erff(float x) { return (float)erf(x); }
double __cdecl erfl(double x) { return erf(x); }
double __cdecl erfc(double x)
{
    if (k_isnan(x)) return x;
    if (x < 0.5) return 1.0 - erf(x);
    if (x < 2.0) return 1.0 - k_erf_series(x);
    if (x > 27.3) return 0.0;
    return k_erfc_cf(x);
}
float __cdecl erfcf(float x) { return (float)erfc(x); }
double __cdecl erfcl(double x) { return erfc(x); }

/* ---- gamma (Lanczos, g=7, n=9) ---- */
static const double k_lanczos[9] = {
    0.99999999999980993, 676.5203681218851, -1259.1392167224028,
    771.32342877765313, -176.61502916214059, 12.507343278686905,
    -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7 };
#define K_PI 3.14159265358979323846

static double k_lgamma_pos(double x) /* x >= 0.5 */
{
    double a = k_lanczos[0], t;
    int i;
    x -= 1.0;
    for (i = 1; i < 9; i++) a += k_lanczos[i] / (x + i);
    t = x + 7.5;
    return 0.91893853320467274178 + (x + 0.5) * log(t) - t + log(a); /* 0.5*log(2pi) */
}
static double k_sinpi(double x)
{
    double r, y, y2, s;
    if (x < 0) return -k_sinpi(-x); /* keep precision for small negative x */
    r = fmod(x, 2.0);
    if (r > 1.0) return -k_sinpi(r - 1.0);
    if (r > 0.5) r = 1.0 - r;
    y = K_PI * r; y2 = y * y;
    s = y * (1 - y2 / 6 * (1 - y2 / 20 * (1 - y2 / 42 * (1 - y2 / 72 * (1 - y2 / 110 * (1 - y2 / 156 * (1 - y2 / 210 * (1 - y2 / 272))))))));
    return s;
}
double __cdecl lgamma(double x)
{
    if (k_isnan(x)) return x;
    if (k_isinf(x)) return K_INF;
    if (x <= 0 && x == floor(x)) return K_INF;
    if (x < 0.5)
        return log(K_PI / k_fabs(k_sinpi(x))) - k_lgamma_pos(1.0 - x);
    return k_lgamma_pos(x);
}
float __cdecl lgammaf(float x) { return (float)lgamma(x); }
double __cdecl tgamma(double x)
{
    double s;
    if (k_isnan(x)) return x;
    if (x == 0) return copysign(K_INF, x);
    if (x < 0 && x == floor(x)) return K_NAN;
    if (k_isinf(x)) return x > 0 ? x : K_NAN;
    if (x > 171.7) return K_INF;
    if (x < 0.5)
    {
        s = k_sinpi(x);
        return K_PI / (s * exp(k_lgamma_pos(1.0 - x)));
    }
    return exp(k_lgamma_pos(x));
}
float __cdecl tgammaf(float x) { return (float)tgamma(x); }

/* ---- <fenv.h> on SSE (MXCSR); fenv_t is { _Fe_ctl, _Fe_stat } ---- */
typedef struct { unsigned long _Fe_ctl, _Fe_stat; } kush_fenv_t;
#define K_FE_INEXACT   0x01
#define K_FE_UNDERFLOW 0x02
#define K_FE_OVERFLOW  0x04
#define K_FE_DIVBYZERO 0x08
#define K_FE_INVALID   0x10
#define K_FE_ALL       0x1f

static unsigned int k_getcsr(void) { return __builtin_ia32_stmxcsr(); }
static void k_setcsr(unsigned int v) { __builtin_ia32_ldmxcsr(v); }

static unsigned long k_csr_to_flags(unsigned int c)
{
    unsigned long f = 0;
    if (c & 0x01) f |= K_FE_INVALID;
    if (c & 0x04) f |= K_FE_DIVBYZERO;
    if (c & 0x08) f |= K_FE_OVERFLOW;
    if (c & 0x10) f |= K_FE_UNDERFLOW;
    if (c & 0x20) f |= K_FE_INEXACT;
    return f;
}
static unsigned int k_flags_to_csr(unsigned long f)
{
    unsigned int c = 0;
    if (f & K_FE_INVALID)   c |= 0x01;
    if (f & K_FE_DIVBYZERO) c |= 0x04;
    if (f & K_FE_OVERFLOW)  c |= 0x08;
    if (f & K_FE_UNDERFLOW) c |= 0x10;
    if (f & K_FE_INEXACT)   c |= 0x20;
    return c;
}

int __cdecl fegetround(void) { return (int)((k_getcsr() >> 13) & 3) << 8; }
int __cdecl fesetround(int round)
{
    if (round & ~0x300) return 1;
    k_setcsr((k_getcsr() & ~0x6000u) | ((unsigned int)(round >> 8) << 13));
    return 0;
}
int __cdecl fetestexcept(int excepts) { return (int)(k_csr_to_flags(k_getcsr()) & excepts & K_FE_ALL); }
int __cdecl feclearexcept(int excepts)
{ k_setcsr(k_getcsr() & ~k_flags_to_csr((unsigned long)excepts)); return 0; }
int __cdecl fegetexceptflag(unsigned long *flagp, int excepts)
{ if (!flagp) return 1; *flagp = k_csr_to_flags(k_getcsr()) & excepts; return 0; }
int __cdecl fesetexceptflag(const unsigned long *flagp, int excepts)
{
    unsigned int c;
    if (!flagp) return 1;
    c = k_getcsr() & ~k_flags_to_csr((unsigned long)excepts);
    k_setcsr(c | k_flags_to_csr(*flagp & excepts));
    return 0;
}
int __cdecl fegetenv(kush_fenv_t *env)
{ if (!env) return 1; env->_Fe_ctl = k_getcsr() & ~0x3fu; env->_Fe_stat = k_csr_to_flags(k_getcsr()); return 0; }
int __cdecl fesetenv(const kush_fenv_t *env)
{
    if (!env) return 1;
    /* An all-zero or foreign control word means "default": round-to-nearest,
     * all exceptions masked (MXCSR 0x1f80). */
    if ((env->_Fe_ctl & 0x1f80) != 0x1f80 && (env->_Fe_ctl & ~0xffffu))
        k_setcsr(0x1f80 | k_flags_to_csr(env->_Fe_stat));
    else
        k_setcsr((env->_Fe_ctl ? (unsigned int)env->_Fe_ctl : 0x1f80u) | k_flags_to_csr(env->_Fe_stat));
    return 0;
}
int __cdecl feholdexcept(kush_fenv_t *env)
{
    if (fegetenv(env)) return 1;
    k_setcsr((k_getcsr() | 0x1f80u) & ~0x3fu); /* mask all, clear flags */
    return 0;
}
